#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <Adafruit_ADS1X15.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../include/ADSreads.h"
#include "../include/LoRaMEshLib.h"
#include "../include/FileSystem.h"

#define vREADY_PIN 18
#define iREADY_PIN 19

ADSreads SensorV(vREADY_PIN, 0x48);
ADSreads SensorI(iREADY_PIN, 0x4B);

void IRAM_ATTR onVReady() { SensorV.onNewDataReady(); }
void IRAM_ATTR onIReady() { SensorI.onNewDataReady(); }

LoRaEnd lora(17, 16);

MySPIFFS files;

TaskHandle_t CalibrationHandle = NULL;
TaskHandle_t TranslateSerialHandle = NULL;
TaskHandle_t InsertCoefficientsHandle = NULL;
TaskHandle_t MeshSendHandle = NULL;

void vCalibration(void *pvParameters);
void vTranslateSerial(void *pvParameters);
void vInsertCoefficients(void *pvParameters);
void vMeshSend(void *pvParameters);

#define SAMPLES 2680
#define TimeOut 10000 // em milisegundos
#define TimeMesh 5000 // em milisegundos
#define DEGREE 3      // Grau do polinimio de calibração + 1

void setup()
{
  Serial.begin(115200);

  attachInterrupt(digitalPinToInterrupt(vREADY_PIN), onVReady, FALLING);
  attachInterrupt(digitalPinToInterrupt(iREADY_PIN), onIReady, FALLING);

  SensorV.begin(0);
  SensorI.begin(0);

  lora.begin(9600);

  files.begin();

  Wire.setBufferSize(256);
  delay(2000);

  xTaskCreate(vTranslateSerial, "TranslateSerial", configMINIMAL_STACK_SIZE + 3072, NULL, 1, &TranslateSerialHandle);
}

void loop()
{
}

//_________________________________TODO: recomentar___________________________________

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
        {
          xTaskCreate(vCalibration, "Calibration", configMINIMAL_STACK_SIZE + 2048, NULL, 2, &CalibrationHandle);
        }

        if (inputString.equals("InsertCoefficients"))
        {
          xTaskCreate(vInsertCoefficients, "InsertCoefficients", configMINIMAL_STACK_SIZE + 2048, NULL, 2, &InsertCoefficientsHandle);
        }

        // TODO: ajustar para timer acionar o envio e quando em outra task, aguardar a outra finalizar para essa entrar
        if (inputString.equals("LoraMesh"))
        {
          xTaskCreate(vMeshSend, "MeshSend", configMINIMAL_STACK_SIZE + 3072, NULL, 1, &MeshSendHandle);
        }

        // Debug
        if (inputString.equals("Format SPFFIS"))
        {
          files.format();
        }

        if (inputString.equals("List Calibration"))
        {
          files.list("/calibration.txt");
        }

        if (inputString.equals("InstRead"))
        {
          for(int i = 0; i < SAMPLES; i++){
            Serial.print(SensorI.readInst(0), 6); Serial.print("|");
          }
        }

        // inputString = "";
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Delay para liberar CPU
  }
}

/*
  TASK de Calibração dos Sensores e seus canais

  FUNÇÂO: Funciona em conjunto com o programa calibration.py na pasta Calibration para
  calibração automática dos canais dos sensores desejados pelos usuário.

  USO: 
    1. Conecte a Carga de Calibração: Conecte uma lâmpada incandescente de potência
    conhecida (ex.: 60W) à rede elétrica de 220V, entre fase e neutro. Esta lâmpada
    servirá como referência para a calibração do sensor de corrente.

    2. Prepare o ESP32: Conecte o ESP32 ao computador. Verifique qual porta COM foi
    atribuída ao dispositivo e certifique-se de que não está em uso por outro programa.

    3. Execute o Programa de Calibração: Inicie o programa calibration.py. Ele detectará
    automaticamente o ESP32 disponível e estabelecerá a comunicação.

    4. Selecione o Canal a Ser Calibrado: O programa solicitará que você selecione qual
    fase (ou canal) será calibrada. Os canais correspondem às fases 0, 1 e 2 (total de 3
    fases). Se for a primeira calibração do ESP32, é recomendável iniciar pelo canal 0,
    calibrando um canal por vez.

    5. Medição da Tensão: Utilize um multímetro em modo de medição de tensão AC para medir
    a tensão da fase selecionada em relação ao neutro. Insira o valor medido no programa
    calibration.py.

    6. Sincronização e Cálculo: O programa sincronizará com o ESP32, coletará os dados
    brutos, calculará os coeficientes de calibração e enviará automaticamente os coeficientes
    ajustados para o ESP32.

    7. Calibração da Corrente: Após a calibração da tensão, o programa perguntará se você
    deseja calibrar o sensor de corrente para aquela fase. Se sim, insira "Y" e forneça o
    valor da potência da lâmpada conectada. O programa calculará a resistência da lâmpada,
    determinará a corrente real com base na tensão calibrada, e enviará os coeficientes de
    calibração para o sensor de corrente.

    8. Continuação ou Finalização: Ao final da calibração de um canal, você será questionado
    se deseja calibrar outro canal. Se optar por continuar, repita o processo a partir do passo 4.
    Caso contrário, o programa encerrará a sessão e desconectará automaticamente do ESP32.
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
  TASK
  FUNÇÂO:
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
      // Serial.print(channel);
      // Serial.print(" ");
      // Serial.println(sensor);
      if ((!(sensor.equals("V")) && !(sensor.equals("I"))))
      {
        // Serial.println("Error aqui");
        // Serial.println(inputString);
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
            coefficients[coefCount] = coefStr.toFloat(); // Converta para float e armazene
            coefCount++;
            Serial.println(inputString);

            // Atualize a posição de início e encontre o próximo delimitador
            startPos = delimiterPos + 1;
            delimiterPos = inputString.indexOf('|', startPos);
          }

          // Não se esqueça de pegar o último coeficiente após o último '|'
          if (coefCount < DEGREE)
          {
            String coefStr = inputString.substring(startPos);
            coefficients[coefCount] = coefStr.toFloat();
          }

          // DEBUG
          for (int i = 0; i < DEGREE; i++)
          {
            Serial.print("Coeficiente ");
            Serial.print(i);
            Serial.print(": ");
            Serial.println(coefficients[i]);
          };
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
        // vTaskDelay(pdMS_TO_TICKS(100));
        Serial.flush();
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

/*
  TASK de empacotamento de envio dos dados dos sensores pelo LoraMesh

  FUNÇÂO: a cada TimerMesh/1000 segundos, a task vai capturar as leituras dos
  sensores em RMS, empacotar no formato JSON e depois enviar para o LoraMesh.
*/
void vMeshSend(void *pvParameters)
{
  bool alt;
  String sensor;
  DynamicJsonDocument data(160);
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
      // Serial.println(rmsValue);
      data[sensor][j] = rmsValue;
    }
  }
  String str;
  serializeJson(data, str);
  serializeJson(data, Serial);

  if (lora.idRead() == 1)
  {
    lora.sendMaster(str);
    Serial.println("LoraMesh Send.");
  }
  str = "";
  vTaskDelete(NULL);
}

// }