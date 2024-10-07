import os
import time
import serial
import serial.tools.list_ports
from math import sqrt
import numpy as np

def setChannel():
    while True:
        try:
            channel = int(input("Enter the Channel reading: "))
            if 0 <= channel < 4:
                return channel
            else:
                print("Invalid input. Channel value should be between 0 and 3.")
        except ValueError:
            print("Invalid input. Please enter a numeric value.")

def getRMS(sensor):
    while True:
        try:
            rms = float(input(f"Enter the RMS {sensor} reading: "))
            if 0.0 <= rms <= 500.0:
                return rms
            else:
                print("Invalid input. RMS value should be between 0 and 500.")
        except ValueError:
            print("Invalid input. Please enter a numeric value.")

def collect_dynamic_data(ESP,channel,sensor):
    rms = 0
    samples = 0
    sumRMS = 0
    idle_timeout = 5
    last_receive_time = time.time()

    while True:
        data = ESP.readline().decode('utf-8').rstrip()
        # print(f"Board response: {data}")
        if data == "Invalid sensor input.":
            return -1
        
        if data == "OK_2":
            print("Calibration completed.")
            break

        if data:
            last_receive_time = time.time()
            try:
                voltage = float(data)
                sumRMS += voltage*voltage
                samples += 1
                
            except ValueError as e:
                print(f"Error parsing data: {e}")
        else:    
            if time.time() - last_receive_time > idle_timeout:
                print("Time Out.")
                return -1
            
    
    rms = sqrt(sumRMS/samples)
    return rms

def calculate_coefficients(TRUErms, SensorRMS, degree = 2):
    X = np.array(SensorRMS)
    print(X)
    Y = np.array(TRUErms)
    print(Y)
    coefficients = np.polyfit(X, Y, degree)
    return coefficients

def debug(ESP):
    ESP.write(b"ChangeInterface")
    j = 0
    while True:
        send = ESP.readline().decode('utf-8').rstrip()
        print(f"Board response: {send}")  
        j+=1
        if send == "Insira o comando:":
            print("Insert command: debug-;")
            ESP.write(b"debug -;")
            return
        
        if send == "Aguarde":
            flag = True
        else:
            flag = False
            
        print(f"j: {j}")
        if j > 30 and flag:
            ESP.write(b"ChangeInterface")
            j = 0

def verificar_leitura(TRUErms, SensorRMS, nova_leitura, novo_TRUErms):
    # Inicializando variáveis para buscar o primeiro menor e o primeiro maior
    valores_menores = []
    valores_maiores = []

    # Percorrer a lista TRUErms e comparar com o novo_TRUErms
    for i, valor in enumerate(TRUErms):
        if valor < novo_TRUErms:
            valores_menores.append((valor, SensorRMS[i]))
        elif valor > novo_TRUErms:
            valores_maiores.append((valor, SensorRMS[i]))

    # Encontrando o maior valor menor que o novo_TRUErms
    if valores_menores:
        maior_menor, sensor_maior_menor = max(valores_menores, key=lambda x: x[0])
    else:
        maior_menor = None

    # Encontrando o menor valor maior que o novo_TRUErms
    if valores_maiores:
        menor_maior, sensor_menor_maior = min(valores_maiores, key=lambda x: x[0])
    else:
        menor_maior = None

    # Verificando se a nova leitura está na faixa válida
    if maior_menor is not None and menor_maior is not None:
        if not (sensor_maior_menor <= nova_leitura <= sensor_menor_maior):
            print(f"Leitura inválida: {nova_leitura} não está entre {sensor_maior_menor} e {sensor_menor_maior}")
            return False
    elif maior_menor is not None:
        if nova_leitura < sensor_maior_menor:
            print(f"Leitura inválida: {nova_leitura} < {sensor_maior_menor}")
            return False
    elif menor_maior is not None:
        if nova_leitura > sensor_menor_maior:
            print(f"Leitura inválida: {nova_leitura} > {sensor_menor_maior}")
            return False

    # Adicionando os novos valores se a leitura for válida
    # SensorRMS.append(nova_leitura)
    # TRUErms.append(novo_TRUErms)
    print(f"Leitura válida. Adicionada: TRUErms = {novo_TRUErms}, SensorRMS = {nova_leitura}")
    return True


def main():
    # Conexão com ESP
    try:
        portas = serial.tools.list_ports.comports()
        if not portas:
            print("Nenhuma porta serial disponível.")
            return
        
        print("Portas disponíveis:")
        for i,porta in enumerate(portas):
            print(f"{i+1}: {porta}")
        
        # while True:
        #     InputPorta  = input("Digite o número da porta ou a porta manualmente (ex: COM3 ou /dev/ttyUSB0): ")
        #     index = int(porta) - 1
        #     if 0 <= index < len(portas):
        #         portaSelect =portas[index]
        #         break
        #     elif porta in portas:
        #         portaSelect = InputPorta
        #         break
        #     else:
        #         print("Porta inválida.")
        #         return
        portaSelect = input("Digite o número da porta ou a porta manualmente (ex: COM3 ou /dev/ttyUSB0): ")
        
        try:
            ESP = serial.Serial(portaSelect,115200,timeout = 1)
            ESP.flushInput()
            ESP.flushOutput()
            
            print(f"Conectado à porta {ESP.name}")
            
            # Verifica se está no modo desenvolvedor do ESP
            i = 0
            while True:
                send = ESP.readline().decode('utf-8').rstrip()
                print(f"Board response: {send}")
                i += 1
                
                if send == "Inicializado":
                    break
                
                print(f"i: {i}")
                if i > 30:
                    debug(ESP)
                    i = 0
            
        except Exception as e:
            raise Exception(f"Falha ao conectar à porta {e}")    
            return
                
    except Exception as e:
        print(e)
        return

    # Calibração
    TRUErms = []
    SensorRMS = []
    sensor = "V"
    continueCalibration = True
    flagCurrent = False
    READS = 4
    
    while continueCalibration:
    
        # Seta canal de calibração 
        channel = setChannel()
        
        if input("Calibrate Current this channel? (Y/N): ").strip().upper() == "Y":
            flagCurrent = True
        else:
            flagCurrent = False
        
        for i in range(READS):
            retry = 0
            # Leitura de referência por multimetro:
            if not flagCurrent:
                rms = getRMS("Voltage")                        
                print(f"Calibrating Voltage, channel {channel}, Vrms: {rms}")
                sensor = "V"
            else:
                rms = getRMS("Current")
                print(f"Calibrating Current, channel {channel}, Irms: {rms}")
                # flagCurrent = False
                sensor = "I"  
            
            # Armazena o fetor de leituras de referência
            TRUErms.append(rms)
            
            # Solicita leituras do sensor
            ESP.write(b"Calibration")
            
            # Aguarda retorno do ESP para sincronização
            while True:
                send = ESP.readline().decode('utf-8').rstrip()
                print(f"Board response: {send}")                # DEBUG
                
                if send == "Time Out":
                    ESP.write(b"Calibration")
                
                # Envia referência de canal e sensor para o ESP
                if send == "OK_1":
                    send = f"{channel}|{sensor}"
                    print(f"Sending comand: {send}")            # DEBUG
                    ESP.write(send.encode())
                    
                    # Coleta as leituras rms do sensor
                    rms = collect_dynamic_data(ESP, channel, sensor)
                    
                    if rms == -1.0:
                        ESP.write(b"Calibration")
                        continue
                    
                    else:
                        # Tratamento da leitura
                        if retry > 2:
                            print(f"TRUE: {TRUErms}")
                            print(f"Sensor: {SensorRMS}")
                            print(rms)
                            print("Erro nas leituras, reniciando... ")
                            return
                        
                        retry += 1
                        if verificar_leitura(TRUErms, SensorRMS, rms, TRUErms[len(TRUErms)-1]):                        
                            
                            SensorRMS.append(rms)
                            break
                        else:
                            print("Repetindo leitura...")
                            ESP.write(b"Calibration")
                            continue

        # Com as leituras, faz-se o calculo dos coeficientes
        coefficients = calculate_coefficients(TRUErms, SensorRMS,2)
        
        # Envia os coeficientes para o ESP
        ESP.write(b"InsertCoefficients")
        while True:
            send = ESP.readline().decode('utf-8').rstrip()
            print(f"Board response: {send}")                # DEBUG
            
            if send == "Time Out":
                ESP.write(b"InsertCoefficients")
                continue
            
            if send == "OK":
                send = f"{channel}|{sensor}"
                print(f"Sending command: {send}")            # DEBUG
                ESP.write(send.encode())
                
            if send == "OK_":
                send = f"{coefficients[0]:.2f}|{coefficients[1]:.2f}|{coefficients[2]:.2f}"
                print(f"Enviando coeficientes: {send}")
                ESP.write(send.encode())
                time.sleep(1)
                break
            
        # Confirmação de calibração finalizada
        print(f"Calibração do canal {channel} finalizada.")
        
        # Limpa as listas
        TRUErms = []
        SensorRMS = []
    
        # Verifica se deseja calibrar outros canais
        if input("Calibrate another channel? (Y/N): ").strip().upper() == "N":
            continueCalibration = False
        
    # Finaliza comunicação com ESP
    debug(ESP)
    ESP.close()

if __name__ == "__main__":
    main()