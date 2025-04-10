import board
import digitalio
import storage
import usb_hid
import json
import time
import asyncio
from adafruit_hid.keyboard import Keyboard
from adafruit_hid.keycode import Keycode
from adafruit_hid.keyboard_layout_us import KeyboardLayoutUS
from display_handler import display_text
from keypad_handler import scan_matrix

CREDENTIALS_FILE = "credentials.json"

# Set up the keyboard HID
keyboard = Keyboard(usb_hid.devices)
keyboard_layout = KeyboardLayoutUS(keyboard)

# Load credentials from the JSON file
def load_credentials():
    try:
        with open(CREDENTIALS_FILE, "r") as file:
            return json.load(file)
    except OSError:
        display_text("No credentials found.", "", "")
        return []

credentials = load_credentials()

# Function to type a string with the HID keyboard
async def type_string(data):
    for char in data:
        keyboard_layout.write(char)
        await asyncio.sleep(0.05)  # Small delay between characters
    keyboard.press(Keycode.TAB)
    keyboard.release_all()
    await asyncio.sleep(0.1)

async def fill_credentials():
    if not credentials:
        display_text("No Credentials", "Stored", "")
        await asyncio.sleep(2)
        return

    display_text("Select Credential", "to Fill", "")
    await asyncio.sleep(2)

    # Display all credentials on the device
    for i, cred in enumerate(credentials, 1):
        display_text(f"{i}: {cred['service']}", cred['username'], "")
        await asyncio.sleep(2)

    display_text("Press Button", "to Select", "")

    selected_index = -1

    while selected_index == -1:
        try:
            button = scan_matrix()
            if button is None:  # No button pressed
                await asyncio.sleep(0.1)  # Add small delay to prevent busy waiting
                continue  # Keep scanning

            if button.isdigit():
                index = int(button) - 1
                if 0 <= index < len(credentials):
                    selected_index = index
                else:
                    display_text("Invalid Index", "", "")
                    await asyncio.sleep(2)
            else:
                display_text("Invalid Input", "", "")
                await asyncio.sleep(2)
        except Exception as e:
            print(f"Error during credential selection: {e}")
            display_text("Error", str(e), "")
            await asyncio.sleep(2)

    selected_cred = credentials[selected_index]

    display_text(f"Filling", selected_cred['service'], "")

    # Type username, press TAB, and then type the password
    await type_string(selected_cred['username'])
    await asyncio.sleep(0.5)  # Short delay between username and password
    await type_string(selected_cred['password'])

    display_text("Credentials", "Sent", "")
    await asyncio.sleep(2)
