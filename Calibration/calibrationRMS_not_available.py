import os
import time
import serial
import serial.tools.list_ports
from math import sqrt
import numpy as np


def listar_portas_disponiveis():
    # """Função para listar portas seriais disponíveis."""
    portas = serial.tools.list_ports.comports()
    return [porta.device for porta in portas]


def find_serial_port():
    # """Solicita ao usuário que insira a porta serial e verifica se é válida."""
    portas_disponiveis = listar_portas_disponiveis()
    
    if not portas_disponiveis:
        print("Nenhuma porta serial disponível.")
        return None

    print("Portas seriais disponíveis:")
    for i, porta in enumerate(portas_disponiveis):
        print(f"{i+1}: {porta}")
    
    while True:
        porta_input = input("Digite o número da porta ou a porta manualmente (ex: COM3 ou /dev/ttyUSB0): ")

        # Se o usuário digitar o número da porta, converte para a porta correspondente
        if porta_input.isdigit():
            porta_index = int(porta_input) - 1
            if 0 <= porta_index < len(portas_disponiveis):
                return portas_disponiveis[porta_index]
        elif porta_input in portas_disponiveis:
            return porta_input
        else:
            print("Porta inválida. Tente novamente.")


def connect_serial(port):
    # """Tenta conectar à porta serial fornecida."""
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        ser.flushInput()
        ser.flushOutput()
        print(f'Conectado à {ser.name}')
        return ser
    except Exception as e:
        raise Exception(f"Falha ao conectar à porta serial: {e}")


def get_rms_value():
    # """Solicita ao usuário o valor RMS de entrada."""
    while True:
        try:
            rms = float(input("Enter the RMS voltage reading: "))
            if 0.0 <= rms <= 500.0:
                return rms
            else:
                print("Invalid input. RMS value should be between 0 and 500.")
        except ValueError:
            print("Invalid input. Please enter a numeric value.")


def getIrms():
    # """Solicita ao usuário o valor da corrente."""
    while True:
        try:
            In = float(input("Enter the Current of the Lamp in 220Vrms: "))
            return In
        except ValueError as e:
            print(f"Invalid input. Please enter a numeric value. Error: {e}")


def set_channel():
    # """Solicita ao usuário o canal para a calibração."""
    while True:
        try:
            channel = int(input("Enter the Channel reading: "))
            if 0 <= channel < 4:
                return channel
            else:
                print("Invalid input. Channel value should be between 0 and 3.")
        except ValueError:
            print("Invalid input. Please enter a numeric value.")


def collect_dynamic_data(ser, channel, sensor):
    # """Coleta dados dinâmicos da porta serial."""
    rms = 0
    samples = 0
    sumRMS = 0
    idle_timeout = 5
    last_receive_time = time.time()

    while True:
        data = ser.readline().decode('utf-8').rstrip()
        print(f"Board response: {data}")
        if data == "Invalid sensor input.":
            break
        
        if data == "OK_2":
            print("Calibration completed.")
            break

        # if data:
        #     last_receive_time = time.time()
        #     try:
        #         voltage = float(data)
        #         sumRMS += voltage * voltage
        #         samples += 1
                
        #     except ValueError as e:
        #         print(f"Error parsing data: {e}")
        # else:    
        #     if time.time() - last_receive_time > idle_timeout:
        #         print("Time Out.")
        #         return 0.0
    
    rms = sqrt(sumRMS / samples)
    return rms


def calculate_coefficients(TRUErms, SensorRMS):
    # """Calcula os coeficientes de calibração."""
    degree = 2
    X = np.array(SensorRMS)
    print(X)
    Y = np.array(TRUErms)
    print(Y)
    coefficients = np.polyfit(X, Y, degree)
    return coefficients


def main():
    try:
        port = find_serial_port()
        if not port:
            print("Nenhuma porta selecionada. Encerrando o programa.")
            return
        
        ser = connect_serial(port)
    except Exception as e:
        print(e)
        return
    
    TRUErms = []
    SensorRMS = []
    sensor = "V"
    continueCalibration = True
    flagCurrent = False
    READS = 6    
    
    time.sleep(2)
    
    while continueCalibration:
        channel = set_channel()
        
        if input("Calibrate Current this channel? (Y/N): ").strip().upper() == "Y":
            flagCurrent = True
            READS = 3
        
        for i in range(READS):
            if not flagCurrent:
                rms = get_rms_value()
                print(f"Calibrating Voltage, channel {channel}, Vrms: {rms}")
                sensor = "V"
            else:
                rms = getIrms()
                print(f"Calibrating Current, channel {channel}, Irms: {rms}")
                flagCurrent = False
                sensor = "I"
            
            TRUErms.append(rms)
            ser.write(b"Calibration")
            
            while True:
                send = ser.readline().decode('utf-8').rstrip()
                print(f"Board response: {send}")
                
                if send == "Time Out":
                    continueCalibration = False
                    break
                
                if send == "OK_1":
                    send = f"{channel}|{sensor}"
                    print(f"Sending command: {send}")
                    ser.write(send.encode())
                    rms = collect_dynamic_data(ser, channel, sensor)
                    if rms != 0.0:
                        SensorRMS.append(rms)
                    break 

        coefficients = calculate_coefficients(TRUErms, SensorRMS)
        
        ser.write(b"InsertCoefficients")
        while True:
            send = ser.readline().decode('utf-8').rstrip()
            print(f"Board response: {send}")
            
            if send == "Time Out":
                continueCalibration = False
                break
            
            if send == "OK":
                send = f"{channel}|{sensor}"
                print(f"Sending command: {send}")
                ser.write(send.encode())
                
            if send == "OK_":
                send = f"{coefficients[0]:.2f}|{coefficients[1]:.2f}|{coefficients[2]:.2f}"
                print(f"Enviando coeficientes: {send}")
                ser.write(send.encode())
                time.sleep(1)
                break

        if not flagCurrent:
            if input("Calibrate another channel? (Y/N): ").strip().upper() == "N":
                continueCalibration = False

    ser.close()

if __name__ == "__main__":
    main()
