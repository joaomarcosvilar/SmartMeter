import os
import time
import serial
import serial.tools.list_ports
from math import sqrt
import numpy as np
# from sklearn.linear_model import LinearRegression


# OBS.: funcionando adequadamente para apenas Tensão, corrente está sendo feita manualmente
# Melhores coeficientes:    V: -9035,47880,-63221
#                           I: 1331.170044,-3398.409912,0

def find_serial_port():
    print('Searching for serial ports...')
    ports = serial.tools.list_ports.comports(include_links=False)
    if ports:
        for port in ports:
            print('Found port ' + port.device)
        return ports[0].device
    else:
        raise Exception("No serial ports found!")

def connect_serial(port):
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        ser.flushInput()
        ser.flushOutput()
        print(f'Connected to {ser.name}')
        return ser
    except Exception as e:
        raise Exception(f"Failed to connect to serial port: {e}")

def get_rms_value():
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
    idle_timeout = 5
    Flag = True
    while True:
        try:
            # Request user input
            In = float(input("Enter the Cuurent of the Lamp in 220Vrms: "))
            return In      
        
        except ValueError as e:
            print(f"Invalid input. Please enter a numeric value. Error: {e}")
            
def set_channel():
    while True:
        try:
            channel = int(input("Enter the Channel reading: "))
            if 0 <= channel < 4:
                return channel
            else:
                print("Invalid input. Channel value should be between 0 and 3.")
        except ValueError:
            print("Invalid input. Please enter a numeric value.")

def collect_dynamic_data(ser,channel,sensor):
    rms = 0
    samples = 0
    sumRMS = 0
    idle_timeout = 5
    last_receive_time = time.time()

    while True:
        data = ser.readline().decode('utf-8').rstrip()
        print(f"Board response: {data}")
        if data == "Invalid sensor input.":
            # print("Invalid sensor input.")
            break
        
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
                return 0.0
            
    
    rms = sqrt(sumRMS/samples)
    return rms

def calculate_coefficients(TRUErms, SensorRMS):
    degree = 2
    X = np.array(SensorRMS)
    print(X)
    Y = np.array(TRUErms)
    print(Y)
    coefficients = np.polyfit(X,Y,degree)
    
    return coefficients



def main():
    try:
        port = find_serial_port()
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
    
    ser.write(b"ChangeInterface")
    while True:
        send = ser.readline().decode('utf-8').rstrip()
        if send == "Qual interface?":
            ser.write(b"debug")
            break
    
    
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
                rms = getIrms()  # Corrigido
                print(f"Calibrating Current, channel {channel}, Irms: {rms}")
                flagCurrent = False
                sensor = "I"
            
            i = 0
            TRUErms.append(rms)
            ser.write(b"Calibration")
            while True:
                send = ser.readline().decode('utf-8').rstrip()
                print(f"Board response: {send}")
                i += 1
                
                 # Caso qualquer outra informação chegar, tenta novamente como 5 vezes
                if i > 5:
                    ser.write(b"Calibration")
                    i = 0
                
                if send == "Time Out":
                    continueCalibration = False
                    break
                
                if send == "OK_1":
                    send = f"{channel}|{sensor}"
                    print(f"Sending command: {send}")
                    ser.write(send.encode())
                    rms = collect_dynamic_data(ser,channel,sensor)
                    
                    if rms != 0.0:
                        if len(TRUErms) > 2:
                            # -----------------------------------------------------------------------------------------------#
                            # Faz tratamento de leitura, caso o RMS seja não equivalente à sequencia, deleta o dado
                            # Exemplo: 2,48769826	143
                                    #  2,41678483	142,5
                                    #  2,43030942	71
                            # O valor para a leitura em 142,5 Vrms está fora do intervalo de leitura do outros dois valores. 
                            # Então será deletado o TrueRMS e não adicionado o SensorRMS.
                            indexMaior = 0
                            indexMenor = 0
                            valueMaior = None
                            valueMenor = None
                            i = 0
                            
                            for v in TRUErms:
                                if v < TRUErms[-1]:
                                    if valueMenor is None or v > valueMenor:
                                        valueMenor = v
                                        indexMenor = i
                                if v > TRUErms[-1]:
                                    if valueMaior is None or v > valueMaior:
                                        valueMaior = v
                                        indexMaior = i   
                                i += 1
                            
                            # Verifica se o SensorRMS está dentro do intervalor esperado
                            if rms > TRUErms[indexMenor] and rms < TRUErms[indexMaior]:
                                SensorRMS.append(rms)
                            else:
                                TRUErms.pop()
                        
                            i = 0
                            break 
                    else:
                        SensorRMS.append(rms)                                                
            # -----------------------------------------------------------------------------------------------#
        
        
        # Calcula os coeficientes para as leituras RMS
        coefficients = calculate_coefficients(TRUErms, SensorRMS)
        
        # Sincroniza com a TASK de inserção dos coeficientes
        ser.write(b"InsertCoefficients")
        while True:
                send = ser.readline().decode('utf-8').rstrip()
                print(f"Board response: {send}")  # Debug sync response
                
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
        # Transition to Current Calibration
        if not flagCurrent:
            if sensor == "V":
                    if input("Calibrate Current this channel? (Y/N): ").strip().upper() == "Y":
                        flagCurrent = True
                    if input("Calibrate Current another channel? (Y/N): ").strip().upper() == "Y":
                        channel = set_channel()
                        continueCalibration = True
                        break
                        
            else:
                if input("Calibrate another channel? (Y/N): ").strip().upper() == "N":
                    channel = set_channel()
                    continueCalibration = False
                    break
                
    ser.write(b"ChangeInterface")
    while True:
        send = ser.readline().decode('utf-8').rstrip()
        if send == "Qual interface?":
            ser.write(b"debug")
            break
              
    ser.close()

if __name__ == "__main__":
    main()