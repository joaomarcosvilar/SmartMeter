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

#define vREADY_PIN 18
#define iREADY_PIN 19

ADSreads SensorV(vREADY_PIN, 0x48); // 72
ADSreads SensorI(iREADY_PIN, 0x4B); // 75

void IRAM_ATTR onVReady() { SensorV.onNewDataReady(); }
void IRAM_ATTR onIReady() { SensorI.onNewDataReady(); }

LoRaEnd lora(17, 16);

MySPIFFS files;

WiFiClientSecure espClient = WiFiClientSecure();
PubSubClient MQTTclient(espClient);

TaskHandle_t CalibrationHandle = NULL;
TaskHandle_t InsertCoefficientsHandle = NULL;
TaskHandle_t InitializeInterfaceHandle = NULL;
TaskHandle_t SendHandle = NULL;
TaskHandle_t TimerHandle = NULL;
TaskHandle_t SerialHandle = NULL;
TaskHandle_t SelectFunctionHandle = NULL;
TaskHandle_t InterfaceChangeHandle = NULL;

void vInitializeInterface(void *pvParameters);
void vCalibration(void *pvParameters);
void vInsertCoefficients(void *pvParameters);
void vInterfaceChange(void *pvParameters);
void vSend(void *pvParameters);
void vSerial(void *pvParameters);
void vSelectFunction(void *pvParameters);

EventGroupHandle_t xEventGroup;
#define ENVIADO (1 << 0) // Status de Send finalizado
#define CALIBRATION (1 << 1)
#define INSERT_COEFICIENTS (1 << 2)
#define CHANGE_INTERFACE (1 << 3)

#define ENVIANDO (1 << 4) // Status de iniciando o Send
#define CONTINUA (1 << 5) // Inicializa o timer

void vTimer(TaskHandle_t TimerHandle);

#define SAMPLES 2680
#define TimeOut 10000 // em milisegundos
#define TimeMesh 5000 // em milisegundos
#define DEGREE 3      // Grau do polinimio de calibração + 1
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

  // Interrupções de leituras dos ADS
  attachInterrupt(digitalPinToInterrupt(vREADY_PIN), onVReady, FALLING);
  attachInterrupt(digitalPinToInterrupt(iREADY_PIN), onIReady, FALLING);

  // Inicializa os ADS
  Serial.println("Iniciando Sensor de Tensão...");
  SensorV.begin();
  Serial.println("Iniciando Sensor de Corrente...");
  SensorI.begin();

  Wire.setBufferSize(256);

  // Cria o EventGroup
  xEventGroup = xEventGroupCreate();
  // xEnviando = xEventGroupCreate();

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
      if (inputString.equals("InsertCoefficients"))
        xEventGroupSetBits(xEventGroup, INSERT_COEFICIENTS);
      if (inputString.equals("ChangeInterface"))
      {
        xEventGroupSetBits(xEventGroup, CHANGE_INTERFACE);
      }
      if (inputString.equals("InitializeInterface"))
      {
        xTaskCreate(vInitializeInterface, "InitializeInterface", 4096, NULL, 1, &InitializeInterfaceHandle);
      }
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

      inputString = "";
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
    bits = xEventGroupWaitBits(xEventGroup, CALIBRATION | INSERT_COEFICIENTS | CHANGE_INTERFACE | CONTINUA | ENVIANDO, pdTRUE, pdFALSE, pdMS_TO_TICKS(100));

    if (bits & ENVIANDO)
    {
      enviando = !enviando;
      xEventGroupClearBits(xEventGroup, ENVIANDO);
    }

    // Verifica se não existe ou aguarda o envio para poder tratar a String recebida da serial
    if (bits & CALIBRATION || bits & INSERT_COEFICIENTS || bits & CHANGE_INTERFACE)
    {
      xTimerStop(TimerHandle, 0); // Pausa o Timer enquanto a interface é alterada
      Serial.print("Aguarde");
      if (enviando)
      {
        EventBits_t xSend;
        while (1)
        {
          Serial.print(".");
          if (SendHandle != NULL)
          {
            xSend = xEventGroupWaitBits(xEventGroup, ENVIADO, pdTRUE, pdFALSE, pdMS_TO_TICKS(100));
            if ((xSend & ENVIADO))
            {
              xTimerStop(TimerHandle, 0);
              vTaskDelete(SendHandle);
              xEventGroupClearBits(xEventGroup, ENVIANDO);
              break;
            }
          }

          vTaskDelay(pdMS_TO_TICKS(200));
        }
      }

      if (bits & CALIBRATION)
      {
        xTaskCreate(vCalibration, "Calibration", configMINIMAL_STACK_SIZE + 2048, NULL, 2, &CalibrationHandle);
        xEventGroupClearBits(xEventGroup, CALIBRATION);
      }

      if (bits & INSERT_COEFICIENTS)
      {
        xTaskCreate(vInsertCoefficients, "InsertCoefficients", configMINIMAL_STACK_SIZE + 2048, NULL, 2, &InsertCoefficientsHandle);
        xEventGroupClearBits(xEventGroup, INSERT_COEFICIENTS);
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

  xTaskCreate(vSelectFunction, "vSelectFunction", 2*1024, NULL, 1, &SelectFunctionHandle);
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

  info["id"] = data["loramesh"]["id"]; // Usa-se o ID do loramesh como identificador do dispositivo de origem

  float coefficients[DEGREE];
  for (int i = 0; i < 2; i++)
  {
    if (i == 0)
    {
      sensor = "V";
      alt = false;
    }
    else
    {
      sensor = "I";
      alt = true;
    }
    for (int j = 0; j < 3; j++)
    {
      for (int c = 0; c < DEGREE; c++)
      {
        coefficients[c] = files.getCoef(sensor, j, c);
      }
      float rmsValue = !alt ? SensorV.readRMS(j, coefficients) : SensorI.readRMS(j, coefficients);

      // Quando valores negativos, identifica como dispositivo desconectado
      if (rmsValue < 0)
      {
        rmsValue = 0;
      }
      while (rmsValue > 1000)
      {
        rmsValue -= 1000;
      }

      // Tratamento para não armazenar valores com mais de 2 casas decimais:
      char bufferRMS[10];
      sprintf(bufferRMS, "%.2f", rmsValue);
      info[sensor][j] = bufferRMS;
    }
  }
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

/*
  TASK de Calibração dos Sensores e seus canais

  FUNÇÂO: Funciona em conjunto com o programa calibration.py na pasta Calibration para
  calibração automática dos canais dos sensores desejados pelos usuário.

  USO:
    1. Conecte as 3 cargas resistivas nos locais devidos (ficarão em série). E aguarde
    com o multímetro na função adequada de leitura de tensão AC.

    2. Inicialize o programa "calibraiton.py". Assim que ele autoconectar com a board,
    irá solicitar qual o canal é sejado calibrar.

    3. Em seguida, irá iniciar a solicitação da tensão lida pelo sensor, se repetindo
    por 6 vezes. Essa repetção é explicada devido ao ESP32 que irá fazer a leitura da
    tensão em 6 posições desse arranjo (uma de cada, a cada duas, e as três). Então,
    em cada solicitação fazer a leitura da tensão na entrada do sensor do canal que
    está sendo calibrado.

    4. Após as 6 coletas, ele irá automaticamente enviar os coeficientes calculados
    e irá perguntar se é desejada a calibração de outro canal. Caso sim, apenas
    repetir os passos 2 e 3.

    5. Para calibração de corrente, ainda não está funcionando (26/08/24).

*/
void vCalibration(void *pvParameters)
{
  Serial.println("OK_1");
  Serial.flush();

  int channel = -1;
  bool alt = false;
  String sensor = "V";

  unsigned long starttime = millis();
  while (true)
  {
    if (Serial.available() > 1)
    {
      String inputString = Serial.readString();
      Serial.flush();
      int limiter = inputString.indexOf('|');
      String str_channel = inputString.substring(0, limiter);
      String sensor = inputString.substring(limiter + 1);
      channel = str_channel.toInt();

      if (((!(sensor.equals("V")) && !(sensor.equals("I"))) || (channel == -1)))
      {
        Serial.println(inputString);
        Serial.println("Sensor: " + sensor + "\nChannel: " + String(channel));
        break;
      }

      alt = sensor.equals("I");

      if (channel >= 0 && channel < 4)
      {
        for (int contt = 0; contt < SAMPLES; contt++)
        {
          float instValue = !alt ? SensorV.readInst(channel) : SensorI.readInst(channel);
          Serial.println(String(instValue, 5));
        }

        Serial.println("OK_2"); // Pausa a captura de dados pelo Calibration.py
        Serial.flush();
        break;
      }
    }
    if ((millis() - starttime) >= TimeOut)
    {
      break;
    }
    Serial.flush();
  }
  xEventGroupSetBits(xEventGroup, CONTINUA);
  vTaskDelete(NULL);
}

/*
  TASK de armazenamento dos coeficientes
  FUNÇÂO: armazena no calibraiton.txt os coeficientes calculados para cada
  canal e sensor acoplado. É chamada pelo software "calibration.py", mas pode
  ser chamado manualmente pelo monitor serial.
*/
void vInsertCoefficients(void *pvParameters)
{

  int channel = 0;
  bool alt = false;

  unsigned long starttime = millis();
  Serial.println("OK");

  while (true)
  {
    if (Serial.available() > 0)
    {
      String inputString = Serial.readString();
      Serial.flush();
      int limiter = inputString.indexOf('|');
      if (limiter == -1)
      {
        Serial.println("Error: Delimitador '|' não encontrado.");
        Serial.println(inputString);
        Serial.flush();
        break;
      }

      String str_channel = inputString.substring(0, limiter);
      String sensor = inputString.substring(limiter + 1);
      channel = str_channel.toInt();
      if ((!(sensor.equals("V")) && !(sensor.equals("I"))))
      {
        Serial.flush();
        break;
      }

      Serial.print("OK_");
      Serial.flush();
      starttime = millis();
      while (true)
      {
        if (Serial.available() > 0)
        {
          String inputString = Serial.readString(); // Lê a string recebida
          Serial.flush();
          int startPos = 0;
          int delimiterPos = inputString.indexOf('|');
          int coefCount = 0;
          float coefficients[DEGREE];

          while (delimiterPos > 0 && coefCount < DEGREE)
          {
            // Extraia o coeficiente da substring
            String coefStr = inputString.substring(startPos, delimiterPos);
            coefficients[coefCount] = coefStr.toFloat();
            coefCount++;
            // Serial.println(inputString);

            // Atualiza a posição de início e encontre o próximo delimitador
            startPos = delimiterPos + 1;
            delimiterPos = inputString.indexOf('|', startPos);
          }
          if (coefCount < DEGREE)
          {
            String coefStr = inputString.substring(startPos);
            coefficients[coefCount] = coefStr.toFloat();
          }
          Serial.flush();
          files.insCoef(sensor, channel, coefficients);
          break;
        }
        Serial.flush();

        if ((millis() - starttime) >= TimeOut)
        {
          // Serial.println("Time Out");
          break;
        }
      }

      break;
    }
    if ((millis() - starttime) >= TimeOut)
    {
      break;
    }
    Serial.flush();
  }
  ESP.restart();
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