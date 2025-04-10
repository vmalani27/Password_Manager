from machine import Pin, I2C
import ssd1306
import time
import _thread
from display_handler import init_display, display_text
from menu_manager import menu
from ble_service import start_ble  # Import BLE service

# Initialize I2C for OLED Display (Change pins based on your setup)
i2c = I2C(0, scl=Pin(21), sda=Pin(20))  # Verify your I2C pins
oled = ssd1306.SSD1306_I2C(128, 64, i2c)  # Adjust resolution if needed

# Initialize Display
init_display()

# File Handling (Ensure the file path is correct)
try:
    with open("data.txt", "w") as f:
        f.write("Hello, Pico W!\n")
except OSError as e:
    print(f"File Write Error: {e}")

# Function to run BLE Service
def ble_task():
    print("Starting BLE Service...")
    start_ble()

# Function to run Menu System
def menu_task():
    print("Starting Menu System...")
    menu()

# Start BLE & Menu in separate threads (since asyncio is limited in MicroPython)
_thread.start_new_thread(ble_task, ())
_thread.start_new_thread(menu_task, ())

# Keep running main loop
while True:
    display_text("System Running", "Menu Active", "")
    time.sleep(1)
