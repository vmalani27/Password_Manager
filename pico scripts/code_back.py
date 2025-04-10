from menu_manager import menu
from display_handler import init_display, display_text
from adafruit_ble import BLERadio
from adafruit_ble.advertising.standard import ProvideServicesAdvertisement
from adafruit_ble.services.nordic import UARTService
import time
import storage

# Initialize the OLED display
init_display()

# Initialize BLE
ble = BLERadio()
uart_service = UARTService()
advertisement = ProvideServicesAdvertisement(uart_service)

# Load stored device address (if any)
try:
    with open("paired_device.txt", "r") as file:
        paired_device = file.read().strip()
except OSError:
    paired_device = None

def start_ble():
    """Start BLE advertisement and handle connection."""
    global paired_device
    display_text("Starting BLE...", "", "")
    ble.start_advertising(advertisement)

    while not ble.connected:
        if paired_device:
            display_text("Waiting for", "paired device:", paired_device)
        else:
            display_text("Connect to", "this device first", "")
        time.sleep(0.5)

    display_text("Connected!", "", "")
    with ble.connected:
        if not paired_device:
            # Store the connected device's MAC address
            paired_device = uart_service.conn.address.address
            with open("paired_device.txt", "w") as file:
                file.write(paired_device)
            display_text("Device Paired", "", "")
            time.sleep(1)
        else:
            display_text("Already Paired", "", "")
            time.sleep(1)

# Main loop
if __name__ == "__main__":
    start_ble()  # Start BLE broadcasting
    menu()       # Show menu after BLE connection
