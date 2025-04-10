import bluetooth
import time
import uos
import ubinascii
from micropython import const

# Constants for Nordic UART Service (NUS)
_UART_UUID = bluetooth.UUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
_UART_TX = bluetooth.UUID("6E400003-B5A3-F393-E0A9-E50E24DCCA9E")  # TX characteristic
_UART_RX = bluetooth.UUID("6E400002-B5A3-F393-E0A9-E50E24DCCA9E")  # RX characteristic

# Storage for bonded device address
BOND_FILE = "bonded_device.txt"
PASSKEY = "123456"  # Change this to your desired PIN


class BLEUART:
    def __init__(self):
        self.ble = bluetooth.BLE()
        self.ble.active(True)
        self.ble.config(gap_name="PicoW-BLE")  # Device name

        # Enable security settings
        self.ble.config(mitm=True, lesc=True, bond=True)
        self.ble.config(passkey=int(PASSKEY))  # Set pairing PIN

        # Register GATT service
        self.tx_handle = None
        self.tx_char = (_UART_TX, bluetooth.FLAG_NOTIFY)
        self.rx_char = (_UART_RX, bluetooth.FLAG_WRITE | bluetooth.FLAG_WRITE_NO_RESPONSE)
        self.service = (_UART_UUID, (self.tx_char, self.rx_char))

        # Register the service
        ((self.tx_handle, self.rx_handle),) = self.ble.gatts_register_services([self.service])

        # Load bonded device address if available
        self.bonded_device = self.load_bonded_device()

        # Callback for BLE events
        self.ble.irq(self.ble_irq)

        # Start advertising
        self.advertise()

    def load_bonded_device(self):
        """Load the previously bonded device's address."""
        try:
            with open(BOND_FILE, "r") as f:
                return f.read().strip()
        except OSError:
            return None  # No bonded device yet

    def save_bonded_device(self, address):
        """Save the bonded device address."""
        with open(BOND_FILE, "w") as f:
            f.write(address)

    def ble_irq(self, event, data):
        """Handle BLE events."""
        if event == 1:  # _IRQ_CENTRAL_CONNECT
            conn_handle, addr_type, addr = data
            addr_str = ubinascii.hexlify(bytes(addr), ":").decode()

            print(f"BLE Connected! Address: {addr_str}")

            # If not bonded, save the device
            if not self.bonded_device:
                self.save_bonded_device(addr_str)
                print("Device bonded and stored.")

        elif event == 2:  # _IRQ_CENTRAL_DISCONNECT
            print("BLE Disconnected, Restarting Advertising...")
            self.advertise()

        elif event == 3:  # _IRQ_GATTS_WRITE
            received_data = self.ble.gatts_read(self.rx_handle).decode("utf-8").strip()
            print(f"Received: {received_data}")
            self.send(f"Echo: {received_data}")  # Echo response

    def send(self, message):
        """Send data over BLE."""
        self.ble.gatts_notify(0, self.tx_handle, message.encode())

    def advertise(self):
        """Start BLE advertising."""
        print("Advertising BLE UART Service with PIN pairing...")
        adv_data = (
            b"\x02\x01\x06"  # Flags: General Discoverable Mode
            + b"\x03\x03\xD8\xFE"  # Complete List of 16-bit Service UUIDs (Nordic UART)
            + bytes(f"\x09\x09PicoW-BLE", "utf-8")  # Device Name (PicoW-BLE)
        )

        if self.bonded_device:
            print(f"Auto-connecting to bonded device: {self.bonded_device}")

        self.ble.gap_advertise(100, adv_data)


# Run BLE UART with PIN pairing & bonding
ble_uart = BLEUART()

# Main loop
while True:
    time.sleep(1)  # Keep the script running
