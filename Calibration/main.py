import os
import sys
import time
import serial
import serial.tools.list_ports

# Autoconexão
print('Search...')
ports = serial.tools.list_ports.comports(include_links=False)
for port in ports :
    print('Find port '+ port.device)

ser = serial.Serial(port.device)
if ser.isOpen():
    ser.close()

ser = serial.Serial(port.device, 115200, timeout=1)
ser.flushInput()
ser.flushOutput()
print('Connect ' + ser.name)

time.sleep(0.5) 


# Aciona as tasks de calibração e alteração de faixa do dimmer



# Todos os procedimentos abaixo deve ser feito para cada ADS e cada canal

# Inserção de dados do multimetro:


# Aquisição dos dados

i = 0
timer = []
vinst = []
while(i<1000):
    data = ser.readline().decode('utf-8').rstrip()
    data = data.split(',')

    try:    
        timer.append(int(data[0]))
        vinst.append(float(data[1]))

    except ValueError as e:
        print(f"Erro no armazenamento: {e}")


    #print("Input: " + str(data) + " - time: " + str(time) + ", vinst:" + str(vinst))
    
# Calculo dos coeficientes:
max_visnt = max(vinst)
min_visnt = min(vinst)

#print(f"Vmin = {min_visnt} e Vmax = {max_visnt}")


# Envio dos dados para o ESP32


