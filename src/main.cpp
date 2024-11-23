/*
Projeto: SmartMeter EndDevice
Desenvolvedor: João Marcos Vilar @github: github.com/MrSandman69
               Halysson Carvalho @github: github.com/halyssonJr

Software baseado no FreeRTOS de monitoramento, tensão e corrente, e
envio por LoRaMESH, WiFi e GSM.
*/

/*---------------------- BIBLIOTECAS ----------------------*/
// Bibliotecas principais:
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <Adafruit_ADS1X15.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"

// Comunicação
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

// Bibliotecas Auxiliares
#include "../include/ADSreads.h"
#include "../include/LoRaMEshLib.h"
#include "../include/FileSystem.h"
#include "../include/certificates.h"

/*---------------------- OBJETOS DO DISPOSITIVOS ----------------------*/
// Módúlos ADS1115: AC -> ADC
ADSreads SensorV(0x48); // 72
ADSreads SensorI(0x4B); // 75

// Loramesh
LoRaEnd lora(17, 16);

// Acesso e escrita da flash memory do ESP32
MySPIFFS files;

// Comunicação WIFI
WiFiClientSecure espClient = WiFiClientSecure();
PubSubClient MQTTclient(espClient);

WiFiClient espClientInsecure;
PubSubClient MQTTclientInsecure(espClientInsecure);

/*---------------------- FreeRTOS ----------------------*/
// Handle das tasks
TaskHandle_t CalibrationHandle = NULL;
TaskHandle_t InitializeInterfaceHandle = NULL;
TaskHandle_t SendHandle = NULL;
TaskHandle_t TimerHandle = NULL;
TaskHandle_t SerialHandle = NULL;
TaskHandle_t SelectFunctionHandle = NULL;
TaskHandle_t InterfaceChangeHandle = NULL;

// Prototipos das tasks
void vInitializeInterface(void *pvParameters);
void vCalibration(void *pvParameters);
void vInterfaceChange(void *pvParameters);
void vSend(void *pvParameters);
void vSerial(void *pvParameters);
void vSelectFunction(void *pvParameters);

EventGroupHandle_t xEventGroup;

void vTimer(TaskHandle_t TimerHandle);

// Bits do eventgroup
#define ENVIADO (1 << 0) // Status de Send finalizado
#define CALIBRATION (1 << 1)
#define CHANGE_INTERFACE (1 << 2)
#define CONTINUA (1 << 3)

/*---------------------- GLOBAIS ----------------------*/
#define TimeOut 30*1000 // em milisegundos
#define TIMER 5000

JsonDocument data;
String interface;

bool debug = true;

/*---------------------- SETUP ----------------------*/
void setup()
{
  // Flag para LOGs do sistema
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

  // EventGroup de seleção de tasks
  xEventGroup = xEventGroupCreate();

  // Cria Timer para envios
  int setTimer = 0;
  if (data["t"] > 0)
    setTimer = data["t"];
  else
    setTimer = TIMER;
  TimerHandle = xTimerCreate("TimerEnvios", pdMS_TO_TICKS(setTimer), pdFALSE, (void *)0, vTimer);

  // Inicializa a interface de comunicação dada as configurações na SPIFFS
  xTaskCreate(vInitializeInterface, "InitializeInterface", 4096, NULL, 1, &InitializeInterfaceHandle);

  if (debug)
    Serial.println("Inicializado!");
}

void loop()
{
}
/*------------------------------------------------- TASKS -------------------------------------------------*/

/*
  - Identificação: Timer de envios
  - Função: cria a task de envio a cada TIMER milisegundos. O TIMER é definido na SPIFFS.
*/
void vTimer(TaskHandle_t TimerHandle)
{
  xTaskCreate(vSend, "Send", 5 * 1024, NULL, 2, &SendHandle);
}

/*
  - Identificação: Tradutor do Serial
  - Função: monitora o Serial do ESP32, enviando para o EventGroup a task que deve ser inicializada
ou tomando decisões mais simples.
*/
void vSerial(void *pvParameters)
{
  unsigned long start = millis();
  String inputString = "";

  while (1)
  {
    if ((Serial.available() > 1))
    {
      inputString = Serial.readString();

      if (inputString.equals("calibration"))
      {
        xEventGroupSetBits(xEventGroup, CALIBRATION);
        vTaskDelete(NULL);
      }
      if (inputString.equals("config"))
      {
        xEventGroupSetBits(xEventGroup, CHANGE_INTERFACE);
        vTaskDelete(NULL);
      }

      if (inputString.equals("format"))
      {
        files.format();
        ESP.restart();
      }
      if (inputString.equals("restart"))
        ESP.restart();
      if (inputString.equals("list coeficients"))
      {
        files.list("/calibration.txt");
      }
      if (inputString.equals("list interface"))
      {
        files.list("/interface.json");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/*
  - Identificação: EventGroup de Seleção de Task
  - Função: Gerencia o acionamento das tasks de interação do usuário alinhando com os envios para
evitar conflitos.
*/
void vSelectFunction(void *pvParameters)
{
  EventBits_t bits;
  while (1)
  {
    bits = xEventGroupWaitBits(xEventGroup, CALIBRATION | CHANGE_INTERFACE | CONTINUA, pdTRUE, pdFALSE, pdMS_TO_TICKS(100));

    // Verifica se não existe ou aguarda o envio para poder tratar a String recebida da serial
    if (bits & CALIBRATION || bits & CHANGE_INTERFACE)
    {
      if (TimerHandle != NULL)
        xTimerStop(TimerHandle, 0); // Pausa o Timer enquanto a interface é alterada

      if (SendHandle != NULL)
      {
        EventBits_t bitSend;
        while (1)
        {
          bitSend = xEventGroupWaitBits(xEventGroup, ENVIADO, pdTRUE, pdFALSE, pdMS_TO_TICKS(100));
          Serial.print(".");

          if ((bits & ENVIADO) || (SendHandle == NULL))
          {
            xTimerStop(TimerHandle, 0);
            xEventGroupClearBits(xEventGroup, ENVIADO);
            break;
          }

          vTaskDelay(pdMS_TO_TICKS((100)));
        }
      }

      if (bits & CALIBRATION)
      {
        xTaskCreate(vCalibration, "Calibration", 5 * 1024, NULL, 2, &CalibrationHandle);
        xEventGroupClearBits(xEventGroup, CALIBRATION);
        vTaskDelete(NULL);
      }

      if (bits & CHANGE_INTERFACE)
      {
        xTaskCreate(vInterfaceChange, "InterfaceChange", 6 * 1024, NULL, 1, &InterfaceChangeHandle);
        xEventGroupClearBits(xEventGroup, CHANGE_INTERFACE);
        vTaskDelete(NULL);
      }
    }

    if ((bits & CONTINUA) && (CalibrationHandle == NULL) && (InterfaceChangeHandle == NULL))
    {
      xTimerStart(TimerHandle, pdMS_TO_TICKS(0));
      xEventGroupClearBits(xEventGroup, CONTINUA);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

/*
  - Identificação: TASK de inicialização da interface de comunicação
  - Função: identifica qual a interface ativa no arquivo interface.json e inicializa o
dispositivo adequado. Ela só acontece uma única vez.
*/
void vInitializeInterface(void *pvParameters)
{
  if (debug)
  {
    Serial.println("\nDados armazenados: \n");
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
      if ((millis() - start) > TimeOut)
      {
        Serial.println("\nERROR.");
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
    const char *SERV = data["wifi"]["serv"];
    // const char *SERV = "test.mosquitto.org";
    Serial.println("Servidor: " + String(SERV));
    const char *THINGNAME = data["wifi"]["name"];
    Serial.println("Thingname: " + String(THINGNAME));

    const char *AWS_IOT_SUBSCRIBE_TOPIC = data["wifi"]["subtopic"];
    int PORT = data["wifi"]["port"];
    if (PORT == 8883)
    {
      // Configure WiFiClientSecure to use the AWS IoT device credentials
      espClient.setCACert(AWS_CERT_CA);
      espClient.setCertificate(AWS_CERT_CRT);
      espClient.setPrivateKey(AWS_CERT_PRIVATE);

      // Connect to the MQTT broker on the AWS endpoint we defined earlier
      MQTTclient.setServer(SERV, PORT);

      // Create a message handler
      // client.setCallback(messageHandler);

      Serial.println("Connecting to AWS IOT");

      unsigned long start = millis();
      while (!MQTTclient.connect(THINGNAME))
      {
        Serial.println(".");
        Serial.println(String(MQTTclient.state()));
        if ((millis() - start) > TimeOut)
        {
          Serial.println("\nERROR.");
          xTaskCreate(vSelectFunction, "vSelectFunction", 2 * 1024, NULL, 1, &SelectFunctionHandle);
          xTaskCreate(vSerial, "Serial", 2048, NULL, 1, &SerialHandle);
          vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
      }

      // Subscribe to a topic
      MQTTclient.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

      Serial.println("AWS IoT Connected!");
    }
    else
    {
      MQTTclientInsecure.setServer(SERV, PORT);

      unsigned long start = millis();
      while (!MQTTclientInsecure.connect(THINGNAME))
      {
        Serial.println(".");
        Serial.println(String(MQTTclientInsecure.state()));
        if ((millis() - start) > TimeOut)
        {
          Serial.println("\nERROR.");
          xTaskCreate(vSelectFunction, "vSelectFunction", 2 * 1024, NULL, 1, &SelectFunctionHandle);
          xTaskCreate(vSerial, "Serial", 2048, NULL, 1, &SerialHandle);
          vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
      }

      MQTTclientInsecure.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
      Serial.println("Server Connected!");
    }
  }

  xTaskCreate(vSelectFunction, "vSelectFunction", 2 * 1024, NULL, 1, &SelectFunctionHandle);
  xTaskCreate(vSerial, "Serial", 2048, NULL, 1, &SerialHandle);

  // Inicializa o timer
  xEventGroupSetBits(xEventGroup, CONTINUA);

  vTaskDelete(NULL); // Autodelete
}

/*
  - Identificação: Task de envio dos dados
  - Função: Faz a leitura dos sensores, empacota os dados em uma estrutura JSON e
envia de acordo com a interface selecionada para comunicação.
*/
void vSend(void *pvParameters)
{
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

    int PORT = data["wifi"]["port"];

    // Secure
    if (PORT == 8883)
    {
      // Serial.println("Secure");
      if (!MQTTclient.connected())
      {
        const char *THINGNAME = data["wifi"]["name"];
        while (!MQTTclient.connect(THINGNAME))
        {
          Serial.println(".");
          Serial.println(String(MQTTclient.state()));
          vTaskDelay(pdMS_TO_TICKS(100));
        }
      }

      const char *TOPIC = data["wifi"]["topic"];
      if (MQTTclient.publish(TOPIC, strWiFi))
      {
        Serial.println("Enviado por WiFi.\n\tTamanho: " + String(str.length()) + "\n\tRSSI: " + String(WiFi.RSSI()));
      }

      MQTTclient.loop();
    }
    else
    {
      // Serial.println("Insecure");
      if (!MQTTclientInsecure.connected())
      {
        unsigned long start = millis();
        const char *THINGNAME = data["wifi"]["name"];
        while (!MQTTclientInsecure.connect(THINGNAME))
        {
          Serial.println(".");
          Serial.println(String(MQTTclientInsecure.state()));

          if ((millis() - start) > TimeOut)
            ESP.restart();

          vTaskDelay(pdMS_TO_TICKS(500));
        }
      }

      const char *TOPIC = data["wifi"]["topic"];
      if (MQTTclientInsecure.publish(TOPIC, strWiFi))
      {
        Serial.println("Enviado por WiFi.\n\tTamanho: " + String(str.length()) + "\n\tRSSI: " + String(WiFi.RSSI()));
      }

      MQTTclientInsecure.loop();
    }
  }

  if (interface.equals("ppp"))
  {
    // Reservado para comunicação GSM
  }

  xEventGroupSetBits(xEventGroup, ENVIADO);
  xEventGroupSetBits(xEventGroup, CONTINUA);
  vTaskDelete(NULL);
}

/*
  - Identificaçõa: Task de calibração dos sensores
  - Função: Recebe os dados de referência do usuário de voltímetro e de amperímetro,
faz o procedimento de leituras e cáculos por regressão linear para obtenção dos coeficientes
para tradução dos dados obtidos pelos sensores para valores reais.

  - Procedimento de uso:
  1. Após a conexão com o computador e iniciar a comunicação serial, chamar a TASK inserindo
"calibration" no serial. Aguarde o restante das intruções pelo serial.
  2. Indicar qual o sensor e o canal vai realizar a calibração, sendo V para os sensores
de tensão e I para os sensores de corrente. Os sensores são dividos aos canais de cada ADS1115, por
isso são identificados de 0 a 2 (3 canais). Então, utilize "V:0" para o sensor de tensão no canal 0,
assim por diante.
  3. Desligue o disjuntor da fase que vai iniciar a calibração, permanecendo a alimentação do SmartMeter
pelo USB do computador. Nesse momento, será lido os valores para o sistema desligado, tensão ou corrente.
No momento adequado, será solicitado para ligar a fase, realizar a leitura de referência (voltímetro ou
amperímetro) e inserir o valor lido no serial. ATENÇÂO: Envie o valor com um ponto no lugar da vírgula,
exemplo: CORRETO: "102.5" ERRADO: "102,5".
  4. Basta aguardar o sistema computar e calcular os coeficientes, que serão armazenado na memória do ESP.
Ao final, o sistema irá reiniciar.

Caso seja o desejo realizar para os demais sensores, basta repetir os mesmos passos.

ATENÇÃO: Caso a necessidade de calibração do sistema antes do local definitivo de uso, permanecer a mesma
quantidade de voltas no sensor de corrente para que seja permanecida a proporcionalidade do sistema.
*/
void vCalibration(void *pvParameters)
{
  Serial.println("Digite o sensor(V ou I) e canal(0 - 2) para calibração (Ex. V:0): ");
  while (true)
  {
    if (Serial.available() > 0)
    {
      String inputString = Serial.readString();
      // Serial.println("inputString: " + inputString);
      int pos = inputString.indexOf(':');
      if (pos < 0)
      {
        Serial.println("Entrada inválida.");
        xTaskCreate(vSelectFunction, "vSelectFunction", 2 * 1024, NULL, 1, &SelectFunctionHandle);
        xTaskCreate(vSerial, "Serial", 2048, NULL, 1, &SerialHandle);
        break;
      }
      String sensor = inputString.substring(0, pos);
      if (!(sensor.equals("V")) && !(sensor.equals("I")))
      {
        Serial.println("Sensor inválido.");
        xTaskCreate(vSelectFunction, "vSelectFunction", 2 * 1024, NULL, 1, &SelectFunctionHandle);
        xTaskCreate(vSerial, "Serial", 2048, NULL, 1, &SerialHandle);
        break;
      }

      String STRchannel = inputString.substring(pos + 1, pos + 2);
      uint8_t channel = STRchannel.toInt();
      if (channel < 0 || channel > 2)
      {
        Serial.println("Canal inválido.");
        xTaskCreate(vSelectFunction, "vSelectFunction", 2 * 1024, NULL, 1, &SelectFunctionHandle);
        xTaskCreate(vSerial, "Serial", 2048, NULL, 1, &SerialHandle);
        break;
      }

      // Serial.println("sensor: " + sensor + "\tcanal: " + String(channel));

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

      Serial.println("Aplicar os mesmos coeficientes nos outros canais?(y/n)");
      while (true)
      {
        if (Serial.available() > 0)
        {
          inputString = Serial.readString();
          if (inputString.equals("y"))
          {
            for (int i = 0; i++; i < 3)
            {
              files.insCoef(sensor, i, coefi);
            }
          }
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      ESP.restart();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  xEventGroupSetBits(xEventGroup, ENVIADO);
  vTaskDelete(NULL);
}

/*
  - Identificação: Task de mudança de dados de interface
  - Função: Faz a configuração de interface de comunicação, modo desenvolvedor e período
de envio dos dados.

  - Procedimento de uso:
  1. Iniciar a task inserido "config". Aguarde a mensagem para inserir o comando.
  2.1. Para mudança do modo desenvolvedor, inserir "debug -;", ele irá alternar o modo.
  2.2. Para mudar o valor do período de envios, inserir "timer -5000;" (altere o valor de
5000 para o valor desejado). ATENÇÃO: valor em milisegundos.
  2.3. Para qualquer outra mudança dos dados da interface de comunicação, obeder a ordem de:
          {interface} dado:subdado dado:subdado;
  Exemplo: 'wifi -ssid:JoaoMarcos -pwd:TestesMIC;'
    Onde:
        "-" indica o inicio da subkey
        ":" indica inicio do valor da subkey
        ";" indica final do comando

  Para que a interface seja a desejada para o envio dos dados, é preciso inserir o comando
  "status:true". Exemplo: 'wifi -status:true;'.

  As interfaces esperadas são:
    - wifi
      Dados que podem ser alterados:
        - status
        - ssid
        - pwd
        - name (nome do servidor)
        - serv (domination do servidor)
        - topic
        - subtopic
        - port
    - loramesh
        - status
        - id
        - bw (Bandwith: BW125; BW250; BW500)
        - sf (SpreadingFactor: SF_LoRa_7;SF_LoRa_8;SF_LoRa_9;SF_LoRa_10;SF_LoRa_11;SF_LoRa_12;SF_FSK)
        - crate (Coding Rate: CR4_5;CR4_6;CR4_7;CR4_8)
        - class (Classe: LoRa_CLASS_A;LoRa_CLASS_C)
        - window (Janela de envio: LoRa_WINDOW_5s;LoRa_WINDOW_10s;LoRa_WINDOW_15s)
        - pwd (senha)
        - bd (BaudeRate, default 9600)
        ATENÇÃO: para os subdados, realizar a leitura da datasheet do lora empregado, os dados
      de bw a window deve obeceder
    - ppp
        -
        -
        -

      Pode ser inseridos quandos dados necessários no mesmo envio do serial, mas ao final deve ser
      inserido ";".

*/
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
      // Serial.println("inputString cortado: " + inputString);

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
      xTaskCreate(vSelectFunction, "vSelectFunction", 2 * 1024, NULL, 1, &SelectFunctionHandle);
      xTaskCreate(vSerial, "Serial", 2048, NULL, 1, &SerialHandle);
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  xEventGroupSetBits(xEventGroup, ENVIADO);
  vTaskDelete(NULL);
}