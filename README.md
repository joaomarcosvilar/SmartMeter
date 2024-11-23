# SmartMeter
## Sistema de monitoramento de Tensão e de Corrente com ESP32

O seguinte projeto é um sistema embarcado de monitoramento de tensão e corrente implementado com um ESP32 DEVKIT 32pin. Para seu funcionamento pleno, utilizou-se o sistema operacional de tempo real FreeRTOS. O sistema é dividido em 3 partes principais:
> Aquisição de dados;
> 
> Envio;
> 
> Comunicação Serial.

A construção física é dada pelo conjunto dos seguintes componentes:
> ESP32 DEVKIT 32pin;
> 
> ADS1115;
> 
> ZMPT101B;
> 
> SCT013;
> 
> LoraMesh Radioenge;

> [!CAUTION]
> NÃO LIGUE O DISPOSITIVO SEM UTILIZAÇÃO DA ANTENA NO MODULO LORAMESH. PODERÁ TRAZER DANOS IRREVERSÍVEL PARA O DISPOSITIVO.

## Primeira Inicialização
> [!NOTE]
> 
> Para a primeira inicialização, é necessário a utilização de comunicação USB - Serial com o ESP32, assim como demais configurações ou calibração.

Todas as informações de inicialização, calibração e interfaces estão armazenadas na SPIFFS. Nessa visão, quando inicializar o módulo pela primeira vez, irá solicitar um valor de ID que servirá como identificação do dispositivo. Essa marcação é baseada pela construção abordada nos módulos LoRaMESH da Radioenge para identificar mestre e escravos.

> [!NOTE] 
>
> Vale a resalta que, baseado na construção do LoRaMESH da Radioenge, cada dispositivo portará o seu ID único. Dessa forma, é preciso ter controle da produção e quais ID já foram utilizados.

Quando armazenado e reiniciado, a seleção default para a interface selecinada é:
> Interface: LoRaMESH
>
> Configurações:
>
>       Status: true
>       Bandwith: 500KHz
>       SpreadingFactor: 7
>       Coding Rate: 4/5
>       Classe: C
>       Janela: 15 seg
>       Senha: 123
>       Baudrate: 9600
>
> Modo desenvolvedor: ativo

Todas essas informações podem ser alteradas posteriormente a inicialização. Nas próximas seções detalha os comandos para a configuração desejada.


## Comunicação

A estrutura do código está baseado nas possibilidade do uso das seguintes tecnologias para envio dos pacotes de dados:
> MQQT (WiFi ou GSM)
>
> LoRaMESH

Os dados das interfaces abordadas são armazenadas no arquivo _interface.json_ na SPIFFS do ESP32, sendo possível alteração por USB - Serial. 
De forma resumida, para todas as interfaces, os dados adiquiridos pelos sensores, são empacotados no formato _JSON_.

As configurações suportadas são:
> WIFI:

|     cmd       |     Descrição |
|      ---:       | ------------- |
| status | Status |
| ssid | SSID |
| pwd | Password |
| name | Nome do servidor |
| serv | Domínio do servidor |
| topic | Tópico |
| subtopic | Subtópico |
| port | Porta |

> [!NOTE]
>
> Os dados de configuração para o MQQT são armazenados na interface WIFI.

> LoRaMESH:


|     cmd       |     Descrição |
| ---: | ------------- |
| status | Status |
| bw | Bandwith |
| sf | SpreadingFactor |
| crate | Coding Rate |
| class | Classe |
| window | Janela |
| pwd | Senha |
| bd | Baudrate |

> GSM:

|     cmd       |     Descrição |
| ---: | ------------- |



## Comandos de configuração
Para iniciar o processo de configuração, permaneça com o ESP32 conectado ao serial monitor de escolha e envie "config". Aguarde a mensagem para inserir o comando.

> [!WARNING]
> O endline do serial monitor desejado não deve conter nenhum carctere a mais, selecione-o para none.

Após a confirmação que inicializou a etapa de configurações, poderá alterar-las usando:

> Para mudança do modo desenvolvedor, inserir "debug -;", ele irá alternar o modo.

> Para mudar o valor do período de envios, inserir "timer -5000;" (altere o valor de 5000 para o valor desejado). ATENÇÃO: valor em milisegundos.

> Para qualquer outra mudança dos dados da interface de comunicação, obeder a ordem de:

>          {interface} dado:subdado dado:subdado;

        "-" indica o inicio da subkey
        ":" indica inicio do valor da subkey
        ";" indica final do comando

Exemplo: 'wifi -ssid:JoaoMarcos -pwd:TestesMIC;'

  Para que a interface seja a desejada para o envio dos dados, é preciso inserir o comando

        "status:true"

  Exemplo: 'wifi -status:true;'.
   
> [!NOTE]
>
>  - Pode ser inseridos quandos dados necessários no mesmo envio do serial, as o final deve ser inserido ";".
> - As entradas esperadas são indetificadas na entrada do envio da configuração. Mas, para as configurações para o LoRaMESH, seguir as identificações utilizada por @elcereza na libray: https://github.com/elcereza/LoRaMESH 

> [!IMPORTANT]
> Os certificados utilizados em servidores MQQT são armazenados diretamente pelo software. Estão anexados em [Certificados](https://github.com/MrSandman69/SmartMeter/blob/main/include/certificates.h). Assim, faz-se preciso a troca e reenvio do software. 

## Calibração

De forma resumida, o processo de calibração é feita com a leitura de 3 pontos, o primeiro ponto é para o _OFFSET_ dos sensores, o segundo, quando a leitura é _zero_ (zeroRMSadc), e o terceiro, quando é acionada a rede ou a carga (maxRMSadc). Para o terceiro ponto, é requisitado ao usuário o valor real, obtido por um multimetro, que será comparado para o calculo dos coeficientes. O ESP32 irá fazer N aquisições dos dados nos três pontos, e em cada um deles irá calcular o valor _RMS_ do valor ADC. Com os valores lidos, é calculado os coeficientes de correção por regressão linear, obedecendo as seguintes expressões:
> a = maxRMS / (maxRMSadc - zeroRMSadc)
>
> b = -a * zeroRMSadc;

Para o processo, foi utilizado um múltimetro alicate para a leitura da corrente e da tensão real. Como carga foi utilizado uma lâmpada halogena de 42W da marca Elgin, e utilizado 20 voltas no sensor. É indicado utilizar cargas maiores para obtenção de valores de correntes maiores sem a necessidade de maiores voltas no sensor. Além que é preciso utilizar um computador com algum software de leitura do USB Serial compatível com o ESP32, com configuração de bauderate de 115200 e com line ending vazio ou _none_.

#### Procedimento de uso:
  1. Após a conexão com o computador e iniciar a comunicação serial, chamar a TASK inserindo "calibration" no serial. Aguarde o restante das intruções pelo serial.
  2. Indicar qual o sensor e o canal vai realizar a calibração, sendo V para os sensores de tensão e I para os sensores de corrente. Os sensores são dividos aos canais de cada ADS1115, por isso são identificados de 0 a 2 (3 canais). Então, utilize "V:0" para o sensor de tensão no canal 0, assim por diante.
  3. Desligue o disjuntor da fase que vai iniciar a calibração, permanecendo a alimentação do SmartMeter pelo USB do computador. Nesse momento, será lido os valores para o sistema desligado, tensão ou corrente. No momento adequado, será solicitado para ligar a fase, realizar a leitura de referência (voltímetro ou amperímetro) e inserir o valor lido no serial. ATENÇÂO: Envie o valor com um ponto no lugar da vírgula, exemplo: CORRETO: "102.5" ERRADO: "102,5".
  4. Basta aguardar o sistema computar e calcular os coeficientes, que serão armazenado na memória do ESP. Ao final, o sistema irá reiniciar. 

Caso seja o desejo realizar para os demais sensores, basta repetir os mesmos passos.

>[!WARNING]
>
> Caso a necessidade de calibração do sistema antes do local definitivo de uso, permanecer a mesma quantidade de voltas no sensor de corrente para que seja permanecida a proporcionalidade do sistema.

## Detalhamento do sistema

### Aquisição de dados

Os sensores ZMPT101B e SCT013 são acoplados ao conversor ADC ADS1115, sendo possível monitorar os níveis de tensão e de corrente, respectivamente. O uso do ADS1115 é justificado pelos conversores ADC da construção do ESP32 apresentarem um grande distanciamento dos valores corretos, com características de não linearidade e de OFFSET. O ADS1115 apresenta resolução de 16 bits, e podendo ser trabalhado de diversas formas, comparando os sinais de entradas ou por leituras simples ( que foi abordado no projeto). Cada fase está conectada a um sensor, e cada sensor a cada canal do ADS1115, sendo utilizado apenas os canais de 0 a 2 no ADS1115.
Quando realizada as leituras, é ainda preciso realizar a correção do valor lido do ADC para os valores reais. Para isso, é preciso realizar a calibração dos sensores na primeira inicialização do Smartmeter, ou ainda quando houver a necessidade de recalibração. Assim, em cada calibração, os coeficientes de correção serão armazenados na SPIFFS do ESP32.
As especificações de leituras são:

>[!WARNING]
>
> V<sub>MAX</sub> = 300 V<sub>RMS</sub>
> 
> I<sub>MIN</sub> = 1 A<sub>RMS</sub>
> 
> I<sub>MAX</sub> = 30 A<sub>RMS</sub>

Antes de indicar o uso da calibração inserida no ESP32, é necessario indicar que a não linearidade, apenas que pequena mas existente, propicia que à faixa de calibração do sensores irá interagir diretamente com a aproximação real da leitura. Em outras palavras, caso realizada a calibração com valores de corrente de 30 A<sub>RMS</sub>, o sensor irá obter os valores mais próximos do real na faixa de 0 à 30 A<sub>RMS</sub> (Lembrando que o mesmo ocorre para o sensor de tensão).
Outrossim, a utilização do sensor de corrente SCT013 é empregada com algumas voltas do condutor que está sendo monitorada. Essa relação de voltas não está abordada no sistema desenvolvido, sendo necessário a divisão da leitura pelo número de voltas dada no dado recebido ao final do sistema ( no servidor aplicado ao MQQT, ou ao TTN).


### Empacotamento de dados

Baseado no formato JSON, periodicamente é solicitada a leitura dos valores de todos os sensores do dispositivo. Após a leitura, armazenamento e correção, são inseridos junto ao ID do dispositivo e encaminho pelo protocolo de comunicação ativo.

Exemplo de envio:

    {
    "id": "id do dispositivo",
    "V": {
        "0": "leitura RMS do sensor V no canal 0",
        "1": "leitura RMS do sensor V no canal 1",
        "2": "leitura RMS do sensor V no canal 2"
      },
      "I": {
        "0": "leitura RMS do sensor I no canal 0",
        "1": "leitura RMS do sensor I no canal 1",
        "2": "leitura RMS do sensor I no canal 2"
      }
    }

>[!NOTE]
>
>A key ID é eliminado apenas pelo protocolo do loramesh, onde o DAP, dispositivo responsável pela comunicação com o LoRaWAN, retira dados do pacote como CRC e o ID.