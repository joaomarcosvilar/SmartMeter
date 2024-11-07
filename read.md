# SmartMeter
## Sistema de monitoramento de Tensão e de Corrente com ESP32

O seguinte projeto é um sistema embarcado de monitoramento de tensão e corrente implementado com um ESP32 DEVKIT 32pin. Para seu funcionamento pleno, utilizou-se o sistema operacional de tempo real FreeRTOS. O sistema é dividido em 3 partes principais:
> Aquisição de dados;
> Envio;
> Comunicação Serial.

A construção física é dada pelo conjunto dos seguintes componentes:
> ESP32 DEVKIT 32pin;
> ADS1115;
> ZMPT101B;
> SCT013;
> LoraMesh Radioenge;

## Aquisição de dados

Os sensores ZMPT101B e SCT013 são acoplados ao conversor ADC ADS1115, sendo possível monitorar os níveis de tensão e de corrente, respectivamente. O uso do ADS1115 é justificado pelos conversores ADC da construção do ESP32 apresentarem um grande distanciamento dos valores corretos, com características de não linearidade e de OFFSET. O ADS1115 apresenta resolução de 16 bits, e podendo ser trabalhado de diversas formas, comparando os sinais de entradas ou por leituras simples ( que foi abordado no projeto). Cada fase está conectada a um sensor, e cada sensor a cada canal do ADS1115, sendo utilizado apenas os canais de 0 a 2 no ADS1115.
Quando realizada as leituras, é ainda preciso realizar a correção do valor lido do ADC para os valores reais. Para isso, é preciso realizar a calibração dos sensores na primeira inicialização do Smartmeter, ou ainda quando houver a necessidade de recalibração. Assim, em cada calibração, os coeficientes de correção serão armazenados na SPIFFS do ESP32.
As especificações de leituras são:
> V<sub>MAX<sub> = 300 V<sub>RMS<sub>
> I<sub>MIN<sub> = 1 A<sub>RMS<sub>
> I<sub>MAX<sub> = 30 A<sub>RMS<sub>
Antes de indicar o uso da calicação inserida no ESP32, é necessario indicar que a não linearidade, apenas que pequena mas existente, propicia que à faixa de calibração do sensores irá interagir diretamente com a aproximação real da leitura. Em outras palavras, caso realizada a calibração com valores de corrente de 30 A<sub>RMS<sub>, o sensor irá obter os valores mais próximos do real na faixa de 0 à 30 A<sub>RMS<sub> (Lembrando que o mesmo ocorre para o sensor de tensão).

### Calibração
