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
TaskHandle_t RMSreadHandle = NULL;
TaskHandle_t MeshSendHandle = NULL;

void vCalibration(void *pvParameters);
void vTranslateSerial(void *pvParameters);
void vRMSread(void *pvParameters);
void vMeshSend(void *pvParameters);

#define SAMPLES 2680
#define TimeOut 10000 // em milisegundos
#define TimeMesh 5000 // em milisegundos

void setup()
{
  Serial.begin(115200);

  attachInterrupt(digitalPinToInterrupt(vREADY_PIN), onVReady, FALLING);
  attachInterrupt(digitalPinToInterrupt(iREADY_PIN), onIReady, FALLING);

  SensorV.begin();
  SensorI.begin();

  lora.begin(9600);

  files.begin();

  Wire.setBufferSize(256);
  delay(2000);

  xTaskCreate(vTranslateSerial, "TranslateSerial", configMINIMAL_STACK_SIZE + 3072, NULL, 1, &TranslateSerialHandle);
}

void loop()
{
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
      if (inputString.length() > 0)
      {
        // Serial.println("Received command: " + inputString); // Debugging
        if (inputString.equals("Calibration"))
        {
          // xTaskNotifyGive(CalibrationHandle);
          xTaskCreate(vCalibration, "Calibration", configMINIMAL_STACK_SIZE + 2048, NULL, 2, &CalibrationHandle);
        }
        if (inputString.equals("RMS"))
        {
          // xTaskNotifyGive(RMSreadHandle);
          xTaskCreate(vRMSread, "RMSread", configMINIMAL_STACK_SIZE + 3072, NULL, 2, &RMSreadHandle);
        }
        if (inputString.equals("LoraMesh"))
        {
          xTaskCreate(vMeshSend, "MeshSend", configMINIMAL_STACK_SIZE + 3072, NULL, 1, &MeshSendHandle);
        }

        // Debug
        // if (inputString.equals("Format SPFFIS"))
        // {
        //   files.format();
        // }
        // if (inputString.equals("List Calibration"))
        // {
        //   files.list("/calibration.txt");
        // }

        inputString = "";
        Serial.flush();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Delay para liberar CPU
  }
  // A cada TimeMesh/1000 segundos ele envia um pacote de dados para o LoraMesh
  if ((millis() - start) < TimeMesh)
  {
    start = millis();
    // xTaskNotifyGive(MeshSendHandle);
    xTaskCreate(vMeshSend, "MeshSend", configMINIMAL_STACK_SIZE + 3072, NULL, 1, &MeshSendHandle);
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
  // while (true)
  // {
  // ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Aguardando notificação
  Serial.println("OK_1");

  int channel = 0;
  bool alt = false;
  String sensor = "V";

  // Leitura dos parâmetros de calibração
  unsigned long starttime = millis();
  while (true)
  {
    if (Serial.available() > 0)
    {
      String inputString = Serial.readString();
      int limiter = inputString.indexOf('|');
      String str_channel = inputString.substring(0, limiter);
      String sensor = inputString.substring(limiter + 1);
      channel = str_channel.toInt();
      inputString = "";

      if ((!(sensor.equals("V")) && !(sensor.equals("I"))))
      {
        Serial.print(sensor); Serial.print(" "); Serial.print(channel);
        Serial.println("<-Invalid sensor input.");
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

        // Leitura dos coeficientes e armazenamento
        starttime = millis();
        while (true)
        {
          if (Serial.available() > 0)
          {
            inputString = Serial.readString();
            int limiter = inputString.indexOf('|');
            String str_a = inputString.substring(0, limiter);
            String str_b = inputString.substring(limiter + 1);
            inputString = "";

            float a = str_a.toFloat();
            float b = str_b.toFloat();

            files.insCoef(sensor, channel, a, b);
            break;
          }

          if ((millis() - starttime) >= TimeOut)
          {
            // Serial.println("Time Out 1");
            break;
          }
        }
      }
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    if ((millis() - starttime) >= TimeOut)
    {
      // Serial.println("Time Out");
      break;
    }
  }
  Serial.flush();
  // }
  vTaskDelete(NULL);
}

/*
  TASK auxiliar para calibração do sensor de corrente

  FUNÇÂO: quando solicitado o RMS pela serial, a task irá aguardar qual
  o canal e o sensor ela vai retornar a leitura RMS real, puxando os coeficientes
  armazenados no calibration.txt.
*/
void vRMSread(void *pvParameters)
{
  // while (true)
  // {
  // ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Esperando notificação
  Serial.println("OK");

  int channel = 0;
  bool alt = false;

  unsigned long starttime = millis();
  while (true)
  {
    if (Serial.available() > 0)
    {
      String inputString = Serial.readString();
      int limiter = inputString.indexOf('|');
      String str_channel = inputString.substring(0, limiter);
      String sensor = inputString.substring(limiter + 1);
      channel = str_channel.toInt();
      inputString = "";

      if ((!(sensor.equals("V")) && !(sensor.equals("I"))))
      {
        // Serial.print(sensor);
        // Serial.println("<-Invalid sensor input.");
        break;
      }

      // Pega os coeficientes armazendos
      float a = files.getCoef(sensor, channel, "a");
      float b = files.getCoef(sensor, channel, "b");
      // Serial.print(a); Serial.print(" ");Serial.print(b);

      alt = sensor.equals("I");
      // Serial.println(alt);

      float rmsValue = !alt ? SensorV.readRMS(channel, a, b) : SensorI.readRMS(channel, a, b);
      Serial.println(String(rmsValue));

      break;
    }
    if ((millis() - starttime) >= TimeOut)
    {
      // Serial.println("Time Out");
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  vTaskDelete(NULL);
}

// }

/*
  TASK de empacotamento de envio dos dados dos sensores pelo LoraMesh

  FUNÇÂO: a cada TimerMesh/1000 segundos, a task vai capturar as leituras dos
  sensores em RMS, empacotar no formato JSON e depois enviar para o LoraMesh.
*/
void vMeshSend(void *pvParameters)
{
  // while (true)
  // {
  // ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Esperando notificação
  bool alt;
  String sensor;
  DynamicJsonDocument data(160);
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
      float a = files.getCoef(sensor, j, "a");
      float b = files.getCoef(sensor, j, "b");
      float rmsValue = !alt ? SensorV.readRMS(j, a, b) : SensorI.readRMS(j, a, b);
      Serial.println(rmsValue);
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