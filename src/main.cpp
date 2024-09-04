#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <Adafruit_ADS1X15.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../include/ADSreads.h"
#include "../include/LoRaMEshLib.h"
#include "../include/FileSystem.h"
#include "../include/InterfaceLib.h"

#define vREADY_PIN 18
#define iREADY_PIN 19

ADSreads SensorV(vREADY_PIN, 0x48);
ADSreads SensorI(iREADY_PIN, 0x4B);

void IRAM_ATTR onVReady() { SensorV.onNewDataReady(); }
void IRAM_ATTR onIReady() { SensorI.onNewDataReady(); }

MySPIFFS files;

Interface InterComunic;

TaskHandle_t CalibrationHandle = NULL;
TaskHandle_t TranslateSerialHandle = NULL;
TaskHandle_t InsertCoefficientsHandle = NULL;
TaskHandle_t MeshSendHandle = NULL;
TaskHandle_t WiFiMQQThandle = NULL;
TimerHandle_t EnviosHandle;

void vTranslateSerial(void *pvParameters);
void vCalibration(void *pvParameters);
void vInsertCoefficients(void *pvParameters);
void vInterfaceChange(void *pvParameters);
void vTimerEnvios(TimerHandle_t EnviosHandle);

#define SAMPLES 2680
#define TimeOut 10000 // em milisegundos
#define DEGREE 3      // Grau do polinimio de calibração + 1
#define TIMER 5000   // em milisegundos

JsonDocument data;
String interface;

bool debug = true;

void setup()
{
  Serial.begin(115200);

  attachInterrupt(digitalPinToInterrupt(vREADY_PIN), onVReady, FALLING);
  attachInterrupt(digitalPinToInterrupt(iREADY_PIN), onIReady, FALLING);

  SensorV.begin();
  SensorI.begin();

  data = files.begin();
  // debug = data["debug"];
  if (!debug)
    InterComunic.begin(data);

  Wire.setBufferSize(256);

  EnviosHandle = xTimerCreate("Envios", pdMS_TO_TICKS(TIMER), pdTRUE, (void *)0, vTimerEnvios);
  xTaskCreate(vTranslateSerial, "TranslateSerial", configMINIMAL_STACK_SIZE + 3072, NULL, 1, &TranslateSerialHandle);
}

void loop()
{
}

/*
  Timer de empacotamento e envio dos dados dos sensores a cada
*/
void vTimerEnvios(TimerHandle_t EnviosHandle)
{
  while (1)
  {
    // Lê e empacota os dados dos sensores
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
    String strJson;
    if (debug)
      serializeJson(info, Serial);
    serializeJson(info, strJson);
    const char *str = strJson.c_str();
    int buffer = sizeof(str);
    if(InterComunic.send(str))
      break;

      // TODO: timeOUT
  }
}

  /*
    TASK de trandução de Serial para chamada de tasks equivalentes ao chamado.

    Função: Auxiliar na sincronia com os dispositivos e gerenciamento para tratamentos automáticos.
  */
  void vTranslateSerial(void *pvParameters)
  {
    unsigned long start = millis();
    String inputString = "";
    while (true)
    {
      if (Serial.available() > 0)
      {
        inputString = Serial.readString();
        Serial.flush();
        if (inputString.length() > 0)
        {
          // Serial.println("Received command: " + inputString); // Debugging
          if (inputString.equals("Calibration"))
            xTaskCreate(vCalibration, "Calibration", configMINIMAL_STACK_SIZE + 2048, NULL, 2, &CalibrationHandle);

          if (inputString.equals("InsertCoefficients"))
            xTaskCreate(vInsertCoefficients, "InsertCoefficients", configMINIMAL_STACK_SIZE + 2048, NULL, 2, &InsertCoefficientsHandle);

          if (inputString.equals("InterfaceChange"))
          {
            xTaskCreate(vInterfaceChange, "InterfaceChange", configMINIMAL_STACK_SIZE + 2048, NULL, 3, NULL);
          }
          if (inputString.equals("debug"))
          {
            debug = !debug;
            files.ChangeInterface("debug", !debug);
            ESP.restart();
          }

          // Debug
          if (debug)
          {
            if (inputString.equals("read all files"))
              files.readALL();

            if (inputString.equals("Intialize Interface"))
              InterComunic.begin(data);

            if (inputString.equals("Format SPIFFS"))
            {
              files.format();
            }

            if (inputString.equals("List Calibration"))
            {
              files.list("/calibration.txt");
            }

            if (inputString.equals("List Interface"))
            {
              files.list("/interface.json");
            }
            if (inputString.equals("Restart"))
            {
              ESP.restart();
            }
            if (inputString.equals("InstRead"))
            {
              for (int i = 0; i < SAMPLES; i++)
              {
                Serial.print(SensorI.readInst(0), 6);
                Serial.print("|");
              }
            }
          }
        }
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
          // Serial.println("Error: Delimitador '|' não encontrado.");
          // Serial.println(inputString);
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

            // DEBUG
            // for (int i = 0; i < DEGREE; i++)
            // {
            //   Serial.print("Coeficiente ");
            //   Serial.print(i);
            //   Serial.print(": ");
            //   Serial.println(coefficients[i]);
            // };
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
    vTaskDelete(NULL);
  }

  void vInterfaceChange(void *pvParameters)
  {
    String _interface, dado, subdado;
    Serial.println("Qual interface");
    while (1)
    {
      if (Serial.available() > 0)
      {
        _interface = Serial.readString();

        Serial.print("Qual tipo de dado quer inserir?");
        while (1)
        {
          if (Serial.available() > 0)
          {
            dado = Serial.readString();
            break;
          }
          vTaskDelay(pdMS_TO_TICKS(100));
        }
        Serial.println();
        Serial.print("Qual o dado quer inserir?");
        while (1)
        {
          if (Serial.available() > 0)
          {
            subdado = Serial.readString();
            if (subdado == data[_interface][dado])
              continue;
            break;
          }
          vTaskDelay(pdMS_TO_TICKS(100));
        }
        files.ChangeInterface(_interface, dado, subdado);
        Serial.flush();

        files.list("/interface.json"); // Debug
        break;
      }

      vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
  }