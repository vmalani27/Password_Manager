import uos
import time
from display_handler import display_text
from keypad_handler import scan_matrix
from password_manager import set_password, reset_password, lock_device
from credentials_manager import manage_credentials
from filesystem import set_filesystem_readonly, set_filesystem_readwrite
from hid_write import fill_credentials

menu_options = ["Set Password", "Reset Password", "Lock Device", "Manage Credentials", "Toggle Filesystem", "Fill Credentials"]
current_option = 0

# Check if mode file exists and read it
mode_filename = "mode.txt"

try:
    uos.stat(mode_filename)  # Check if file exists
    with open(mode_filename, "r") as mode_file:
        filesystem_mode = mode_file.read().strip()
except OSError:
    # If the file doesn't exist, create it with a default mode
    filesystem_mode = "Read-Only"
    with open(mode_filename, "w") as mode_file:
        mode_file.write(filesystem_mode)

def menu():
    """Main menu loop."""
    global current_option, filesystem_mode

    while True:
        display_text("Main Menu:", menu_options[current_option], f"Mode: {filesystem_mode}")
        button = scan_matrix()

        if button == "Next":
            current_option = (current_option + 1) % len(menu_options)
            time.sleep(0.3)
        elif button == "Prev":
            current_option = (current_option - 1) % len(menu_options)
            time.sleep(0.3)
        elif button == "OK":
            if current_option == 0:  # Set Password
                set_password()
            elif current_option == 1:  # Reset Password
                reset_password()
            elif current_option == 2:  # Lock Device
                lock_device()
            elif current_option == 3:  # Manage Credentials
                manage_credentials()
            elif current_option == 4:  # Toggle Filesystem
                if filesystem_mode == "Read-Only":
                    set_filesystem_readwrite()
                    filesystem_mode = "Read-Write"
                else:
                    set_filesystem_readonly()
                    filesystem_mode = "Read-Only"
                with open(mode_filename, "w") as mode_file:
                    mode_file.write(filesystem_mode)
                time.sleep(0.3)
            elif current_option == 5:  # Fill Credentials
                fill_credentials()

# Start the menu
menu()
