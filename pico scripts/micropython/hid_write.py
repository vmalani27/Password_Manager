import json
import time
import usb_hid
import machine
from display_handler import display_text
from keypad_handler import scan_matrix

CREDENTIALS_FILE = "credentials.json"

# Configure HID Keyboard
keyboard = usb_hid.Device(
    usb_hid.Device.KEYBOARD
)

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
def type_string(data):
    """Sends keystrokes via HID Keyboard."""
    for char in data:
        keyboard.send_char(char)
    keyboard.send_key("TAB")  # Press TAB key
    keyboard.release_all()

def fill_credentials():
    """Handles credential selection and autofill."""
    if not credentials:
        display_text("No Credentials", "Stored", "")
        time.sleep(2)
        return

    display_text("Select Credential", "Use Next/OK", "")
    time.sleep(2)

    selected_index = 0  # Start at first credential

    while True:
        # Display current credential choice
        cred = credentials[selected_index]
        display_text(f"{selected_index+1}: {cred['service']}", cred['username'], "")

        button = scan_matrix()

        if button == "Next":
            selected_index = (selected_index + 1) % len(credentials)  # Cycle forward
            time.sleep(0.2)
        elif button == "Prev":
            selected_index = (selected_index - 1) % len(credentials)  # Cycle backward
            time.sleep(0.2)
        elif button == "OK":
            break  # Selection confirmed

    selected_cred = credentials[selected_index]

    display_text(f"Filling", selected_cred['service'], "")

    # Type username, press TAB, and then type the password
    type_string(selected_cred['username'])
    time.sleep(0.5)  # Short delay between username and password
    type_string(selected_cred['password'])

    display_text("Credentials", "Sent", "")
    time.sleep(2)
