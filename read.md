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

## Aquisição de dados

Os sensores ZMPT101B e SCT013 são acoplados ao conversor ADC ADS1115, sendo possível monitorar os níveis de tensão e de corrente, respectivamente. O uso do ADS1115 é justificado pelos conversores ADC da construção do ESP32 apresentarem um grande distanciamento dos valores corretos, com características de não linearidade e de OFFSET. O ADS1115 apresenta resolução de 16 bits, e podendo ser trabalhado de diversas formas, comparando os sinais de entradas ou por leituras simples ( que foi abordado no projeto). Cada fase está conectada a um sensor, e cada sensor a cada canal do ADS1115, sendo utilizado apenas os canais de 0 a 2 no ADS1115.
Quando realizada as leituras, é ainda preciso realizar a correção do valor lido do ADC para os valores reais. Para isso, é preciso realizar a calibração dos sensores na primeira inicialização do Smartmeter, ou ainda quando houver a necessidade de recalibração. Assim, em cada calibração, os coeficientes de correção serão armazenados na SPIFFS do ESP32.
As especificações de leituras são:
> V<sub>MAX</sub> = 300 V<sub>RMS</sub>
> 
> I<sub>MIN</sub> = 1 A<sub>RMS</sub>
> 
> I<sub>MAX</sub> = 30 A<sub>RMS</sub>

Antes de indicar o uso da calibração inserida no ESP32, é necessario indicar que a não linearidade, apenas que pequena mas existente, propicia que à faixa de calibração do sensores irá interagir diretamente com a aproximação real da leitura. Em outras palavras, caso realizada a calibração com valores de corrente de 30 A<sub>RMS</sub>, o sensor irá obter os valores mais próximos do real na faixa de 0 à 30 A<sub>RMS</sub> (Lembrando que o mesmo ocorre para o sensor de tensão).
Outrossim, a utilização do sensor de corrente SCT013 é empragada com algumas voltas do condutor que está sendo monitorada. Essa relação de voltas não está abordada no sistema desenvolvido, sendo necessário a divisão da leitura pelo número de voltas dada no dado recebido ao final do sistema ( no servidor aplicado ao MQQT, ou ao TTN).

### Calibração

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

ATENÇÃO: Caso a necessidade de calibração do sistema antes do local definitivo de uso, permanecer a mesma quantidade de voltas no sensor de corrente para que seja permanecida a proporcionalidade do sistema.

## Comunicação

A estrutura do código está baseado nas possibilidade do uso das seguintes tecnologias para envio dos pacotes de dados:
> MQQT (WiFi ou GSM)
>
> LoRaMESH

Os dados das interfaces abordadas são armazenadas no arquivo _interface.json_ na SPIFFS do ESP32, sendo possível alteração por USB Serial. 
De forma resumida, para todas as interfaces, os dados adiquiridos pelos sensores, são empacotados no formato _JSON_....
