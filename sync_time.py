import serial
import time
from datetime import datetime, timedelta

port = "COM7"

print(f"Connecting to {port}...")
ser = serial.Serial(port, 9600)

# Measure exact boot wait time
start = time.time()
time.sleep(1.5)
elapsed = time.time() - start

# Add elapsed + 1 extra second to compensate for transmission delay
now = datetime.now() + timedelta(seconds=elapsed + 0)
time_str = now.strftime("%H%M%S%d%m%y")
print(f"Sending: {now.strftime('%H:%M:%S on %d/%m/%Y')}")
ser.write(time_str.encode())
time.sleep(0.5)
ser.close()
print("Done! RTC synced.")
input("Press Enter to exit.")