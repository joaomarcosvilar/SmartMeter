import os
import time
import serial
import serial.tools.list_ports
from math import sqrt

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

def getIrms(ser, channel,rms):
    idle_timeout = 5
    Flag = True
    while True:
        try:
            # Request user input
            In = float(input("Enter the Cuurent of the Lamp in 220Vrms: "))
            return In      
            # Check if the power is within the allowed range
            # if 0.0 < Pn < 1000.0:
            #     Rn = 48400 / (Pn)  # Calculate nominal resistance
            #     In = rms / Rn
            #     return In # Corrente medida em 1 volta/ Corrente medida em 6 voltas
                # ser.write(b"RMS")
                
                # while Flag:
                #     data = ser.readline().decode('utf-8').rstrip()
                #     print(f"Board response: {data}")  # Debug sync response
                    
                #     if data == "OK":
                #         send = f"{channel}|V"
                #         ser.write(send.encode())
                #         print(f"Sending command: {send}")
                #         last_receive_time = time.time()
                #         while True:
                #             data = ser.readline().decode('utf-8').rstrip()
                #             print(f"Board response: {data}")  # Debug Vrms response
                #             if data > "0.0":  # Check if the received data is not empty or zero
                #                 Irms = float(data) / Rn  # Calculate RMS current
                #                 # Irms = Irms / 6     # Cable turns on sensor
                #                 return Irms
                #             if time.time() - last_receive_time > idle_timeout:
                #                 print("Timeout: No data received.")
                #                 Flag = False
                #                 break
                                
            # else:
            #     print("Invalid input. Power value should be between 0 and 500.")
        
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

def collect_dynamic_data(ser):
    vinst = []
    total_sum = 0.0
    idle_timeout = 5
    last_receive_time = time.time()

    while True:
        data = ser.readline().decode('utf-8').rstrip()
        # print(data)
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
                vinst.append(voltage)
                total_sum += voltage
                # total_sum += voltage*voltage
            except ValueError as e:
                print(f"Error parsing data: {e}")
        else:
            if time.time() - last_receive_time > idle_timeout:
                print("Timeout: No data received.")
                break
    
    # total_sum = total_sum / len(vinst)
    # total_sum = sqrt(total_sum)
    return vinst, total_sum

def calculate_coefficients(vinst, rms, total_sum):
    if len(vinst) == 0:
        raise ValueError("No data collected.")
    print(vinst)
    max_vinst = max(vinst)
    min_vinst = min(vinst)
    mean = total_sum / len(vinst)
    v_peak_peak = 2.0 * rms * sqrt(2)
    a = v_peak_peak / (max_vinst - min_vinst)
    b = -a * mean
    
    print(f"Vmin: {min_vinst}, Vmax: {max_vinst}, mean: {mean}, V_Peak_Peak: {v_peak_peak}")
    return a, b
        

def main():
    try:
        port = find_serial_port()
        board = connect_serial(port)
    except Exception as e:
        print(e)
        return
    continueCalibration = True
    sensor = ""
    flagCurrent = False
    while continueCalibration:
        if not flagCurrent:
            channel = set_channel()
            rms = get_rms_value()  # Voltage Calibration
            Vrms = rms
            print(f"Calibrating Voltage, channel {channel}, Vrms: {rms}")
            sensor = "V"
        else:
            rms = getIrms(board, channel,Vrms)  # Current Calibration
            print(f"Calibrating Current, channel {channel}, Irms: {rms}")
            flagCurrent = False
            sensor = "I"
        
        board.write(b"Calibration")
        
        while True:
            send = board.readline().decode('utf-8').rstrip()
            print(f"Board response: {send}")  # Debug sync response
            
            if send == "Time Out":
                continueCalibration = False
                break
            
            if send == "OK_1":
                send = f"{channel}|{sensor}"
                print(f"Sending command: {send}")
                board.write(send.encode())

                vinst, total_sum = collect_dynamic_data(board)
                if not vinst:
                    print("No data was collected.")
                    if sensor == "I":
                        flagCurrent = True
                    break

                try:
                    a, b = calculate_coefficients(vinst, rms, total_sum)
                    send = f"{a}|{b}"
                    print(f"Sending coefficients: {send}")
                    board.write(send.encode())
                except ValueError as e:
                    print(e)
                
                # Transition to Current Calibration
                if sensor != "I":
                    if not flagCurrent:
                        if input("Calibrate Current channel? (Y/N): ").strip().upper() == "Y":
                            flagCurrent = True
                            break  # Exit inner loop and return to main loop
                    
                break  # Exit inner loop (voltage or current calibration)
        if not flagCurrent:
            if input("Calibrate another channel? (Y/N): ").strip().upper() == "N":
                continueCalibration = False
                break

    board.close()

if __name__ == "__main__":
    main()
