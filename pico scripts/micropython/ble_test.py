import os
print(os.uname())

import time
import board
import adafruit_ble

try:
    ble = adafruit_ble.BLERadio()
    print("BLE adapter initialized successfully")
except RuntimeError as e:
    print(f"Error: {e}")

while True:
    time.sleep(1)  # Keep the REPL open and observe for errors
