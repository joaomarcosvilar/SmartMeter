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

def getIrms(ser, channel):
    while True:
        try:
            # Request user input
            Pn = float(input("Enter the Power of the Lamp in 220Vrms: "))
                        
            # Check if the power is within the allowed range
            if 0.0 < Pn < 500.0:
                Rn = 48400 / Pn  # Calculate nominal resistance
                ser.write(b"RMS")
                
                while True:
                    data = ser.readline().decode('utf-8').rstrip()
                    # print(f"Board response: {data}")  # Debug sync response
                    
                    if data == "OK":
                        send = f"{channel}|V"
                        ser.write(send.encode())
                        print(f"Sending command: {send}")
                        
                        while True:
                            data = ser.readline().decode('utf-8').rstrip()
                            print(f"Board response: {data}")  # Debug Vrms response
                            if data > "0.0":  # Check if the received data is not empty or zero
                                Irms = float(data) / Rn  # Calculate RMS current
                                # Irms = Irms / 6     # Cable turns on sensor
                                return Irms
            else:
                print("Invalid input. Power value should be between 0 and 500.")
        
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
            # print("Calibration completed.")
            break

        if data:
            last_receive_time = time.time()
            try:
                voltage = float(data)
                vinst.append(voltage)
                total_sum += voltage
            except ValueError as e:
                print(f"Error parsing data: {e}")
        else:
            if time.time() - last_receive_time > idle_timeout:
                print("Timeout: No data received.")
                break

    return vinst, total_sum

def calculate_coefficients(vinst, rms, total_sum):
    if len(vinst) == 0:
        raise ValueError("No data collected.")

    max_vinst = max(vinst)
    min_vinst = min(vinst)
    mean = total_sum / len(vinst)
    v_peak_peak = 2.0 * rms * sqrt(2)
    a = v_peak_peak / (max_vinst - min_vinst)
    b = -a * mean
    return a, b


    
def wait_for_confirmation():
    while True:
        confirmer = input()
        if confirmer.strip().upper() == "OK":
            break
        

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
            print(f"Calibrating Voltage, channel {channel}, Vrms: {rms}")
            sensor = "V"
        else:
            rms = getIrms(board, channel)  # Current Calibration
            print(f"Calibrating Current, channel {channel}, Irms: {rms}")
            flagCurrent = False
            sensor = "I"
        
        board.write(b"Calibration")
        
        while True:
            send = board.readline().decode('utf-8').rstrip()
            # print(f"Board response: {send}")  # Debug sync response
            
            if send == "Time Out":
                continueCalibration = False
                break
            
            if send == "OK_1":
                send = f"{channel}|{sensor}"
                # print(f"Sending command: {send}")
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
                    # print(f"Sending coefficients: {send}")
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
