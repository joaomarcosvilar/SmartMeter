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

ADSreads SensorV(vREADY_PIN, 0x48);
ADSreads SensorI(iREADY_PIN, 0x4B);

void IRAM_ATTR onVReady() { SensorV.onNewDataReady(); }
void IRAM_ATTR onIReady() { SensorI.onNewDataReady(); }

LoRaEnd lora(17, 16);

MySPIFFS files;

WiFiClientSecure espClient = WiFiClientSecure();
PubSubClient MQTTclient(espClient);

TaskHandle_t CalibrationHandle = NULL;
TaskHandle_t TranslateSerialHandle = NULL;
TaskHandle_t InsertCoefficientsHandle = NULL;
TaskHandle_t SendHandle = NULL;
TaskHandle_t TimerHandle = NULL;
TaskHandle_t SerialHandle = NULL;
TaskHandle_t SelectFunctionHandle = NULL;

void vInitializeInterface(void *pvParameters);
void vCalibration(void *pvParameters);
void vInsertCoefficients(void *pvParameters);
void vInterfaceChange(void *pvParameters);
void vSend(void *pvParameters);
void vSerial(void *pvParameters);
void vSelectFunction(void *pvParameters);

EventGroupHandle_t xEventGroup;
#define ENVIO (1 << 0)
#define CALIBRATION (1 << 1)
#define INSERT_COEFICIENTS (1 << 2)
#define CHANGE_INTERFACE (1 << 3)

void vTimer(TaskHandle_t TimerHandle);

#define SAMPLES 2680
#define TimeOut 10000 // em milisegundos
#define TimeMesh 5000 // em milisegundos
#define DEGREE 3      // Grau do polinimio de calibração + 1
#define TIMER 5000

JsonDocument data;
String interface;

bool debug = true;

#define AWS_IOT_PUBLISH_TOPIC "smartmeter/power"

void setup()
{
  data = files.begin();
  debug = data["debug"];

  // Inicializa o Serial
  Serial.begin(115200);

  // Interrupções de leituras dos ADS
  attachInterrupt(digitalPinToInterrupt(vREADY_PIN), onVReady, FALLING);
  attachInterrupt(digitalPinToInterrupt(iREADY_PIN), onIReady, FALLING);

  // Inicializa os ADS
  SensorV.begin();
  SensorI.begin();
  Wire.setBufferSize(256);

  // Cria o EventGroup
  xEventGroup = xEventGroupCreate();
  TimerHandle = xTimerCreate("TimerEnvios", pdMS_TO_TICKS(TIMER), pdTRUE, (void *)0, vTimer);

  // Cria as tasks
  xTaskCreate(vSelectFunction, "vSelectFunction", configMINIMAL_STACK_SIZE, NULL, 1, &SelectFunctionHandle);
  xTaskCreate(vSerial, "Serial", 2048, NULL, 1, &SerialHandle);

  // Inicializa o timer
  if (!debug)
    xTimerStart(TimerHandle, pdMS_TO_TICKS(100));

  // Serial.println("Setup finalizado");
}

void loop()
{
}

// Task de Acionamento dos Envios
void vTimer(TaskHandle_t TimerHandle)
{
  if (interface.equals("") & (!debug))
    xTaskCreate(vInitializeInterface, "InitializeInterface", 4096, NULL, 1, &InsertCoefficientsHandle);
  else
  {
    // Serial.println("TIMER");
    xEventGroupSetBits(xEventGroup, ENVIO);
  }
}

void vSelectFunction(void *pvParameters)
{
  EventBits_t bits;
  while (1)
  {
    bits = xEventGroupWaitBits(xEventGroup, ENVIO | CALIBRATION | INSERT_COEFICIENTS, pdTRUE, pdFALSE, pdMS_TO_TICKS(100));
    if (bits & ENVIO)
    {
      xTaskCreate(vSend, "Send", 4096, NULL, 2, &SendHandle);
      xEventGroupClearBits(xEventGroup, ENVIO);
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
      xTaskCreate(vInterfaceChange, "InterfaceChange", 4096, NULL, 3, NULL);
      xEventGroupClearBits(xEventGroup, CHANGE_INTERFACE);
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
    serializeJson(data, Serial);
    Serial.println(interface);
  }

  // Inicializa o LoRaMesh
  if (data["LoRaMESH"]["Status"])
  {
    lora.begin(data);
    interface = "LoRaMESH";
  }

  // Inicializa o WiFi e conecta com o broker MQTT
  if (data["WiFi"]["Status"])
  {
    const char *ssid = data["WiFi"]["SSID"];
    const char *password = data["WiFi"]["Password"];

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print('.');
      vTaskDelay(pdMS_TO_TICKS(500));
      if ((millis() - start) > TimeOut)
      {
        Serial.println("WiFi error, initializing...");
        ESP.restart();
      }
    }
    Serial.println(String(WiFi.localIP()));
  }
  // Inicializa protocolo GSM
  if (data["PPP"]["Status"])
  {
    interface = "PPP";
  }

  if ((data["WiFi"]["Status"]) || (data["PPP"]["Status"]))
  {
    // Conexão com o Broker
    const char *AWS_IOT_ENDPOINT = data["WiFi"]["AWS_IOT_ENDPOINT"];
    Serial.println(String(AWS_IOT_ENDPOINT));
    const char *THINGNAME = data["WiFi"]["THINGNAME"];
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
      Serial.println(String(MQTTclient.state()));
      Serial.println(".");
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    // Subscribe to a topic
    MQTTclient.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

    Serial.println("AWS IoT Connected!");

    interface = "WiFi";
  }
  vTaskDelete(NULL); // Autodelete
}

void vSend(void *pvParameters)
{
  // Serial.println(interface);
  bool alt;
  String sensor;
  JsonDocument info;

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
      info[sensor][j] = rmsValue;
    }
  }
  String str;
  serializeJson(info, str);

  if (interface.equals("LoRaMESH"))
  {
    lora.sendMaster(str);
  }
  if (interface.equals("WiFi"))
  {
    const char *strWiFi = str.c_str();
    int buffer = sizeof(strWiFi);

    if (!MQTTclient.connected())
    {
      Serial.println("Não conectado ao MQTT, reiniciando conexão");
      // todo: chamar initialize interface
    }

    MQTTclient.publish(AWS_IOT_PUBLISH_TOPIC, strWiFi);
    MQTTclient.loop();
    Serial.println("Enviado.");
  }

  if (interface.equals("PPP"))
  {
  }

  vTaskDelete(NULL);
}

void vSerial(void *pvParameters)
{
  unsigned long start = millis();
  String inputString = "";
  while (1)
  {
    if (Serial.available() > 1)
    {
      inputString = Serial.readString();
      if (inputString.equals("Calibration"))
        xEventGroupSetBits(xEventGroup, CALIBRATION);
      if (inputString.equals("InsertCoefficients"))
        xEventGroupSetBits(xEventGroup, INSERT_COEFICIENTS);
      if (inputString.equals("ChangeInterface"))
        xEventGroupSetBits(xEventGroup, CHANGE_INTERFACE);
      if (debug)
      {
        if (inputString.equals("InitializeInterface"))
        {
          xTaskCreate(vInitializeInterface, "InitializeInterface", 4096, NULL, 1, &InsertCoefficientsHandle);
          xTimerStart(TimerHandle, pdMS_TO_TICKS(100));
        }
        // if (inputString.equals("DEBUG"))
        // TODO: fazer a alternância do modo debug
      }
      inputString = "";
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
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

  xTimerStop(TimerHandle, pdMS_TO_TICKS(100));
  Serial.println("OK_1");
  Serial.flush();

  int channel = 0;
  bool alt = false;
  String sensor = "V";

  unsigned long starttime = millis();
  while (true)
  {
    if (Serial.available() > 0)
    {
      String inputString = Serial.readString();
      Serial.flush();
      int limiter = inputString.indexOf('|');
      String str_channel = inputString.substring(0, limiter);
      String sensor = inputString.substring(limiter + 1);
      channel = str_channel.toInt();

      if ((!(sensor.equals("V")) && !(sensor.equals("I"))))
      {
        Serial.println("aqui");
        break;
      }

      alt = sensor.equals("I");

      if (channel >= 0 && channel < 4)
      {
        for (int contt = 0; contt < SAMPLES; contt++)
        {
          float instValue = !alt ? SensorV.readInst(channel) : SensorI.readInst(channel);
          Serial.println(String(instValue));
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
  xTimerStop(TimerHandle, 0);  // Pausa o Timer enquanto a interface é alterada

  String _interface = "", dado = "", subdado = "";
  Serial.println("Qual interface?");
  
  while (1)
  {
    if (Serial.available() > 0)
    {
      _interface = Serial.readString();
      if(_interface.equals("debug")){
        files.ChangeInterface();
      }

      if (!_interface.equals("WiFi") && !_interface.equals("LoRaMESH") && !_interface.equals("PPP"))
      {
        Serial.println("Interface inválida. Tente novamente.");
        break;
      }

      Serial.println("Qual tipo de dado quer inserir?");

      while (1)
      {
        if (Serial.available() > 0)
        {
          dado = Serial.readString();

          if (data[_interface].containsKey(dado))
          {
            break;  
          }
          else
          {
            Serial.println("Tipo de dado não identificado. Tente novamente:");
          }
        }
        vTaskDelay(pdMS_TO_TICKS(100));  
      }

      Serial.println("Insira o valor do dado:");

      while (1)
      {
        if (Serial.available() > 0)
        {
          subdado = Serial.readString();

          if (subdado.equals("false"))
          {
            files.ChangeInterface(_interface, false);
            vTaskDelete(NULL);
          }
          else if (subdado.equals("true"))
          {
            files.ChangeInterface(_interface, true);
            vTaskDelete(NULL);
          }
          else
          {
            data[_interface][dado] = subdado;
          }

          if (data[_interface][dado] == subdado)
          {
            Serial.println("Valor alterado com sucesso.");
            break;
          }
        }
        vTaskDelay(pdMS_TO_TICKS(100));  
      }

      files.ChangeInterface(_interface, dado, subdado);
      files.list("/interface.json");
      break; 
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  vTaskDelete(NULL);
}
