import serial
import serial.tools.list_ports
import time
from datetime import datetime, timedelta

def find_arduino():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if 'Arduino' in p.description or 'CH340' in p.description or 'Uno' in p.description:
            return p.device
    return None

port = find_arduino()
if not port:
    port = input("Port not found. Enter manually (e.g. COM10): ").strip()

print(f"Connecting to {port}...")

# Open port but disable DTR first -- this prevents auto-reset
ser = serial.Serial()
ser.port = port
ser.baudrate = 9600
ser.dtr = False  # KEY LINE -- stops reset signal
ser.open()

print("Connected without triggering reset.")
print("Waiting 2 seconds...")
time.sleep(2)

now = datetime.now() + timedelta(seconds=1)
time_str = now.strftime("%H%M%S%d%m%y")
print(f"Sending: {now.strftime('%H:%M:%S on %d/%m/%Y')}")
ser.write(time_str.encode('ascii'))
time.sleep(1)
ser.close()
print("Done! Watch display for TIME SYNCED!")
input("Press Enter to exit.")
