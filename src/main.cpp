#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <Adafruit_ADS1X15.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#include "../include/ADSreads.h"
#include "../include/LoRaMEshLib.h"
#include "../include/FileSystem.h"
#include "../include/certificates.h"

ADSreads SensorV(0x48); // 72
ADSreads SensorI(0x4B); // 75

LoRaEnd lora(17, 16);

MySPIFFS files;

WiFiClientSecure espClient = WiFiClientSecure();
PubSubClient MQTTclient(espClient);

TaskHandle_t CalibrationHandle = NULL;
TaskHandle_t InitializeInterfaceHandle = NULL;
TaskHandle_t SendHandle = NULL;
TaskHandle_t TimerHandle = NULL;
TaskHandle_t SerialHandle = NULL;
TaskHandle_t SelectFunctionHandle = NULL;
TaskHandle_t InterfaceChangeHandle = NULL;

void vInitializeInterface(void *pvParameters);
void vCalibration(void *pvParameters);
void vInterfaceChange(void *pvParameters);
void vSend(void *pvParameters);
void vSerial(void *pvParameters);
void vSelectFunction(void *pvParameters);

EventGroupHandle_t xEventGroup;
#define ENVIADO (1 << 0) // Status de Send finalizado
#define CALIBRATION (1 << 1)
#define CHANGE_INTERFACE (1 << 3)

#define ENVIANDO (1 << 4) // Status de iniciando o Send
#define CONTINUA (1 << 5) // Inicializa o timer

void vTimer(TaskHandle_t TimerHandle);

#define TimeOut 10000 // em milisegundos
#define TimeMesh 5000 // em milisegundos
#define TIMER 5000

JsonDocument data;
String interface;

bool debug = true;
bool SEND = false; // Flag para verificar se a task vSend está em funcionamento

#define AWS_IOT_PUBLISH_TOPIC "smartmeter/power"

void setup()
{
  data = files.begin();
  debug = data["debug"];

  // Inicializa o Serial
  Serial.begin(115200);

  // Verifica se já identificado o device
  if (data["loramesh"]["id"] == -1)
  {
    Serial.println("Digite o ID do device: ");
    while (1)
    {
      if (Serial.available() > 0)
      {
        String strID = Serial.readStringUntil('\n');
        int ID = strID.toInt();
        if ((ID > 0) || (ID < 2046))
        {
          // TODO: precisa verificar se existe outros dispositivos com mesmo ID
          files.ChangeInterface("loramesh", "id", ID);
          ESP.restart();
        }
        else
        {
          Serial.println("ID invalido. Insira novamente: ");
        }
      }
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }

  // Inicializa os ADS
  Serial.println("Iniciando Sensor de Tensão...");
  SensorV.begin();
  Serial.println("Iniciando Sensor de Corrente...");
  SensorI.begin();

  Wire.setBufferSize(256);

  // Cria o EventGroup
  xEventGroup = xEventGroupCreate();

  // Cria Timer para envios
  int setTimer = 0;
  if (data["t"] > 0)
    setTimer = data["t"];
  else
    setTimer = TIMER;

  TimerHandle = xTimerCreate("TimerEnvios", pdMS_TO_TICKS(setTimer), pdFALSE, (void *)0, vTimer);

  xTaskCreate(vInitializeInterface, "InitializeInterface", 4096, NULL, 1, &InitializeInterfaceHandle);

  if (debug)
    Serial.println("Inicializado");
}

void loop()
{
}

// Task de Acionamento dos Envios
void vTimer(TaskHandle_t TimerHandle)
{
  if (interface.equals(""))
  {
    xTaskCreate(vInitializeInterface, "InitializeInterface", 4096, NULL, 1, &InitializeInterfaceHandle);
  }
  else
  {
    xTaskCreate(vSend, "Send", 4096, NULL, 2, &SendHandle);
  }
}

void vSerial(void *pvParameters)
{
  unsigned long start = millis();
  String inputString = "";

  while (1)
  {
    if ((Serial.available() > 1))
    {
      inputString = Serial.readString();

      if (inputString.equals("Calibration"))
        xEventGroupSetBits(xEventGroup, CALIBRATION);
      if (inputString.equals("ChangeInterface"))
        xEventGroupSetBits(xEventGroup, CHANGE_INTERFACE);
      if (inputString.equals("InitializeInterface"))
        xTaskCreate(vInitializeInterface, "InitializeInterface", 4096, NULL, 1, &InitializeInterfaceHandle);

      if (inputString.equals("Format"))
      {
        files.format();
        ESP.restart();
      }
      if (inputString.equals("List Coefficients"))
      {
        files.list("/calibration.txt");
      }
      if (inputString.equals("List Interface"))
      {
        files.list("/interface.json");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void vSelectFunction(void *pvParameters)
{
  EventBits_t bits;
  bool enviando = false;
  while (1)
  {
    bits = xEventGroupWaitBits(xEventGroup, CALIBRATION | CHANGE_INTERFACE | CONTINUA | ENVIANDO, pdTRUE, pdFALSE, pdMS_TO_TICKS(100));

    // if (bits & ENVIANDO)
    // {
    //   enviando = true;
    //   xTimerStop(TimerHandle, 0);
    //   xEventGroupClearBits(xEventGroup, ENVIANDO);
    // }

    // Verifica se não existe ou aguarda o envio para poder tratar a String recebida da serial
    if (bits & CALIBRATION || bits & CHANGE_INTERFACE)
    {
      if (TimerHandle != 0)
        xTimerStop(TimerHandle, 0); // Pausa o Timer enquanto a interface é alterada

      if (SendHandle != NULL)
      {
        EventBits_t xSend;
        while (1)
        {
          Serial.print(".");

          xSend = xEventGroupWaitBits(xEventGroup, ENVIADO, pdTRUE, pdFALSE, pdMS_TO_TICKS(100));
          if ((xSend & ENVIADO))
          {
            xTimerStop(TimerHandle, 0);
            xEventGroupClearBits(xEventGroup, ENVIANDO);
            break;
          }

          vTaskDelay(pdMS_TO_TICKS(200));
        }
      }

      // Serial.print("Aguarde");
      // if (enviando)
      // {
      //   EventBits_t xSend;
      //   while (1)
      //   {
      //     Serial.print(".");

      //     xSend = xEventGroupWaitBits(xEventGroup, ENVIADO, pdTRUE, pdFALSE, pdMS_TO_TICKS(100));
      //     if ((xSend & ENVIADO))
      //     {
      //       vTaskDelete(SendHandle);
      //       xEventGroupClearBits(xEventGroup, ENVIANDO);
      //       break;
      //     }

      //     vTaskDelay(pdMS_TO_TICKS(200));
      //   }
      // }

      if (bits & CALIBRATION)
      {
        xTaskCreate(vCalibration, "Calibration", 5 * 1024, NULL, 2, &CalibrationHandle);
        xEventGroupClearBits(xEventGroup, CALIBRATION);
      }

      if (bits & CHANGE_INTERFACE)
      {
        xTaskCreate(vInterfaceChange, "InterfaceChange", 6 * 1024, NULL, 1, &InterfaceChangeHandle);
        xEventGroupClearBits(xEventGroup, CHANGE_INTERFACE);
      }
    }

    if (bits & CONTINUA)
    {
      xTimerStart(TimerHandle, pdMS_TO_TICKS(0));
      xEventGroupClearBits(xEventGroup, CONTINUA);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/*
  TASK de inicialização da interface de envio dos dados dos sensores

  Função: identifica qual a interface ativa no arquivo interface.json e inicializa o
  dispositivo adequado. Ela só acontece uma única vez.
*/
void vInitializeInterface(void *pvParameters)
{
  if (debug)
  {
    Serial.println("Dados armazenados: \n");
    serializeJson(data, Serial);
    Serial.println();
  }

  // Inicializa o LoRaMesh
  if (data["loramesh"]["status"])
  {
    lora.begin(data);
    interface = "loramesh";
    Serial.println("Interface inicializada: " + interface);
  }

  // Inicializa o wifi e conecta com o broker MQTT
  if (data["wifi"]["status"])
  {
    const char *ssid = data["wifi"]["ssid"];
    const char *password = data["wifi"]["pwd"];

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print('.');
      vTaskDelay(pdMS_TO_TICKS(800));
      if ((millis() - start) > TimeOut)
      {
        Serial.println();
        Serial.println("WiFi error, initializing...");
        xTaskCreate(vSelectFunction, "vSelectFunction", 2 * 1024, NULL, 1, &SelectFunctionHandle);
        xTaskCreate(vSerial, "Serial", 2048, NULL, 1, &SerialHandle);
        vTaskDelete(NULL);
      }
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    Serial.println();
    interface = "wifi";
    Serial.println("Interface inicializada: " + interface);
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.println("SSID: " + WiFi.SSID());
  }

  // Inicializa protocolo GSM
  if (data["ppp"]["status"])
  {
    interface = "ppp";
    Serial.println("Interface inicializada: " + interface);
  }

  if ((data["wifi"]["status"]) || (data["ppp"]["status"]))
  {
    // Conexão com o Broker
    const char *AWS_IOT_ENDPOINT = data["wifi"]["serv"];
    Serial.println(String(AWS_IOT_ENDPOINT));
    const char *THINGNAME = data["wifi"]["name"];
    Serial.println(String(THINGNAME));

    const char AWS_IOT_SUBSCRIBE_TOPIC[] = "smartmeter/subpower";
    // Configure WiFiClientSecure to use the AWS IoT device credentials
    espClient.setCACert(AWS_CERT_CA);
    espClient.setCertificate(AWS_CERT_CRT);
    espClient.setPrivateKey(AWS_CERT_PRIVATE);

    // Connect to the MQTT broker on the AWS endpoint we defined earlier
    MQTTclient.setServer(AWS_IOT_ENDPOINT, 8883);

    // Create a message handler
    // client.setCallback(messageHandler);

    Serial.println("Connecting to AWS IOT");

    while (!MQTTclient.connect(THINGNAME))
    {
      Serial.println(".");
      Serial.println(String(MQTTclient.state()));
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Subscribe to a topic
    MQTTclient.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

    Serial.println("AWS IoT Connected!");
  }

  xTaskCreate(vSelectFunction, "vSelectFunction", 2 * 1024, NULL, 1, &SelectFunctionHandle);
  xTaskCreate(vSerial, "Serial", 2048, NULL, 1, &SerialHandle);

  // Inicializa o timer
  xEventGroupSetBits(xEventGroup, CONTINUA);

  vTaskDelete(NULL); // Autodelete
}

void vSend(void *pvParameters)
{
  // Altera o estado de SEND para não ocorrer outras tasks
  xEventGroupSetBits(xEventGroup, ENVIANDO);

  bool alt;
  String sensor;
  JsonDocument info;
  float coefficients[2] = {0};

  info["id"] = data["loramesh"]["id"]; // Usa-se o ID do loramesh como identificador do dispositivo de origem

  // Leituras para o sensor de tensão
  sensor = "V";
  for (int i = 0; i < 3; i++)
  {
    // Armazeno os coeficientes do canal
    for (int j = 0; j < 2; j++)
    {
      coefficients[j] = files.getCoef(sensor, i, j);
    }

    // Recebe e traduz o valor rms em ADC do sensor
    float rmsValue = SensorV.rmsSensor(i) * coefficients[0] + coefficients[1];

    // Caso em valor negativo, zera a leitura
    if (rmsValue < 0)
      rmsValue = 0.0;

    // Tratamento para não armazenar valores com mais de 2 casas decimais:
    char bufferRMS[10];
    sprintf(bufferRMS, "%.2f", rmsValue);
    info[sensor][i] = bufferRMS;
  }

  // Leituras para o sensor de tensão
  sensor = "I";
  for (int i = 0; i < 3; i++)
  {
    // Armazeno os coeficientes do canal
    for (int j = 0; j < 2; j++)
    {
      coefficients[j] = files.getCoef(sensor, i, j);
    }

    // Recebe e traduz o valor rms em ADC do sensor
    float rmsValue = SensorI.rmsSensor(i) * coefficients[0] + coefficients[1];

    // Caso em valor negativo, zera a leitura
    if (rmsValue < 0)
      rmsValue = 0.0;

    // Tratamento para não armazenar valores com mais de 2 casas decimais:
    char bufferRMS[10];
    sprintf(bufferRMS, "%.2f", rmsValue);
    info[sensor][i] = bufferRMS;
  }

  // Serializa para uma string de envio
  String str;
  serializeJson(info, str);

  // Envia por LoraMESH
  if (interface.equals("loramesh"))
  {
    // String loraConfirmation = lora.sendMaster(str);
    lora.sendMaster(str);
  }

  // Envia por WiFi
  if (interface.equals("wifi"))
  {
    const char *strWiFi = str.c_str();
    int buffer = sizeof(strWiFi);

    if (!MQTTclient.connected())
    {
      Serial.println("Não conectado ao MQTT, reiniciando conexão.");
      const char *THINGNAME = data["wifi"]["name"];
      while (!MQTTclient.connect(THINGNAME))
      {
        Serial.println(".");
        Serial.println(String(MQTTclient.state()));
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }

    if (MQTTclient.publish(AWS_IOT_PUBLISH_TOPIC, strWiFi))
    {
      Serial.println("Enviado por WiFi.\n\tTamanho: " + String(str.length()) + "\n\tRSSI: " + String(WiFi.RSSI()));
    }

    MQTTclient.loop();
  }

  if (interface.equals("ppp"))
  {
  }

  xEventGroupSetBits(xEventGroup, ENVIADO);
  xEventGroupSetBits(xEventGroup, CONTINUA);
  vTaskDelete(NULL);
}

void vCalibration(void *pvParameters)
{
  Serial.println("Digite o sensor(V ou I) e canal(0 - 2) para calibração (Ex. V:0): ");
  while (true)
  {
    if (Serial.available() > 0)
    {
      String inputString = Serial.readString();
      Serial.println("inputString: " + inputString);
      int pos = inputString.indexOf(':');
      if (pos < 0)
      {
        Serial.println("Entrada inválida.");
        break;
      }
      String sensor = inputString.substring(0, pos);
      if (!(sensor.equals("V")) && !(sensor.equals("I")))
      {
        Serial.println("Sensor inválido.");
        break;
      }

      String STRchannel = inputString.substring(pos + 1, pos + 2);
      uint8_t channel = STRchannel.toInt();
      if (channel < 0 || channel > 2)
      {
        Serial.println("Canal inválido.");
        break;
      }

      Serial.println("sensor: " + sensor + "\tcanal: " + String(channel));

      JsonDocument coef;
      float coefi[2] = {0};
      if (sensor.equals("V"))
        coef = SensorV.autocalib(channel);

      else
        coef = SensorI.autocalib(channel);

      coefi[0] = coef["a"];
      coefi[1] = coef["b"];
      files.insCoef(sensor, channel, coefi);

      Serial.println("Canal do sensor calibrado!");
      ESP.restart();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  xEventGroupSetBits(xEventGroup, CONTINUA);
  vTaskDelete(NULL);
}

void vInterfaceChange(void *pvParameters)
{

  String inputString = "";
  String _interface, dado, subdado, SUBinputString;
  unsigned long startTimer = millis();

  Serial.print("Insira o comando: ");
  while (1)
  {
    while (Serial.available() > 0)
    {
      char incomingChar = Serial.read();
      // if (debug)
      //   Serial.print("incomingChar: " + incomingChar);
      inputString += incomingChar;
      if (incomingChar == ';')
      {
        Serial.println();
        break;
      }
    }

    if (inputString.length() > 0 && inputString.endsWith(";"))
    {
      if (debug)
        Serial.println("inputString: " + inputString);

      /*inserção de dados do interface de Exemplo:
        WiFi -SSID:JoaoMarcos -Password:TestesMIC;
        "-" indica o inicio da subkey
        ":" indica inicio do valor da subkey
        ";" indica final do comando
      verificação de escrita para o tipo de leitura*/

      int pos = inputString.indexOf('-');

      _interface = inputString.substring(0, pos);
      _interface.replace(" ", "");
      if (debug)
        Serial.println("Interface: " + _interface);

      // Mudança de modo desenvolvedor debug -;
      if (_interface.equals("debug"))
      {
        files.ChangeInterface();
        ESP.restart();
      }

      // timer -5000;
      if (_interface.equals("timer"))
      {
        _interface.replace(" ", "");
        dado = inputString.substring(pos + 1, inputString.length());
        // Serial.println("dado: " + dado);
        int setTimer = dado.toInt();
        // Serial.println("Temporizador: " + setTimer);
        files.ChangeInterface(_interface, "", setTimer);
        ESP.restart();
      }

      inputString = inputString.substring(pos + 1, inputString.length());
      Serial.println("inputString cortado: " + inputString);

      while (1)
      {
        pos = inputString.indexOf('-');
        if ((pos > 0) || (inputString.length() > 0))
        {
          SUBinputString = inputString.substring(0, pos);
          inputString = inputString.substring(SUBinputString.length() + 1, inputString.length());

          pos = SUBinputString.indexOf(':');
          dado = SUBinputString.substring(0, pos);
          subdado = SUBinputString.substring(pos + 1, SUBinputString.length());

          // Faz limpeza dos " " que as Strings pode possuir
          dado.replace(" ", "");
          subdado.replace(" ", "");
          subdado.replace(";", "");

          if (debug)
          {
            Serial.println("\tdado: " + dado + "\tsubdado: " + subdado);
          }

          // Valida os dados
          bool flag = false;
          JsonVariant VariantDado;
          for (JsonPair keyValue : data.as<JsonObject>()) // Percorre cada chave do data
          {
            String key = keyValue.key().c_str(); // Pega a chave como string
            if (key.equals(_interface))          // Compara com a entrada de interface
            {
              for (JsonPair subKeyValue : keyValue.value().as<JsonObject>()) // Percorre os valores das chaves
              {
                String subkey = subKeyValue.key().c_str();
                if (subkey.equals(dado)) // Compara com a entrada do dado
                {
                  VariantDado = subKeyValue.value();
                  flag = true;
                  break;
                }
              }
              break;
            }
          }

          if (!flag) // Caso percorrida as chaves e seus valores e não encontrado corresponde, significa que a entrada está incorreta
          {
            Serial.println("Dados incorretos, tente novamente!");
            ESP.restart();
          }

          if (VariantDado.is<int>())
          {
            // Serial.println("É inteiro");
            int subdadoInt = subdado.toInt();
            files.ChangeInterface(_interface, dado, subdadoInt);
          }
          else
          {
            // Serial.println("É string");
            files.ChangeInterface(_interface, dado, subdado);
          }

          if (debug && (inputString.length() > 0))
            Serial.println("Dado ainda a ser tratado: " + inputString);
        }
        else
        {
          break;
        }
      }
      ESP.restart();
      break;
    }
    if ((millis() - startTimer) > 2 * TimeOut)
    {
      Serial.println("Tempo de espera encerrado.");
      // xTimerStart(TimerHandle, pdMS_TO_TICKS(100));
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  xEventGroupSetBits(xEventGroup, CONTINUA);
  vTaskDelete(NULL);
}