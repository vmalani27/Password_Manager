from display_handler import display_text
from keypad_handler import scan_matrix
from password_manager import set_password, reset_password, lock_device
from credentials_manager import manage_credentials
from filesystem import set_filesystem_readonly, set_filesystem_readwrite
import time
import asyncio
from hid_write import fill_credentials

# Read initial mode from file or set to default


menu_options = ["Set Password", "Reset Password", "Lock Device", "Manage Credentials", "Toggle Filesystem", "Fill Credentials"]
current_option = 0

# Read initial mode from file or set to default
try:
    with open("mode.txt", "r") as mode_file:
        filesystem_mode = mode_file.read().strip()
except OSError:  # Changed from FileNotFoundError to OSError for CircuitPython compatibility
    # If the file is not found, create it with a default value
    filesystem_mode = "Read-Only"
    with open("mode.txt", "w") as mode_file:
        mode_file.write(filesystem_mode)

async def menu():
    """Main menu loop."""
    global current_option, filesystem_mode

    while True:
        try:
            display_text("Main Menu:", menu_options[current_option], f"Mode: {filesystem_mode}")
            button = scan_matrix()

            if button == "Next":
                current_option = (current_option + 1) % len(menu_options)
                await asyncio.sleep(0.3)
            elif button == "Prev":
                current_option = (current_option - 1) % len(menu_options)
                await asyncio.sleep(0.3)
            elif button == "OK":
                if current_option == 0:  # Set Password
                    await set_password()
                elif current_option == 1:  # Reset Password
                    await reset_password()
                elif current_option == 2:  # Lock Device
                    await lock_device()
                elif current_option == 3:  # Manage Credentials
                    await manage_credentials()
                elif current_option == 4:  # Toggle Filesystem
                    if filesystem_mode == "Read-Only":
                        set_filesystem_readwrite()
                        filesystem_mode = "Read-Write"
                    else:
                        set_filesystem_readonly()
                        filesystem_mode = "Read-Only"
                    with open("mode.txt", "w") as mode_file:
                        mode_file.write(filesystem_mode)
                    await asyncio.sleep(0.3)
                elif current_option == 5:  # Fill Credentials
                    await fill_credentials()
            
            await asyncio.sleep(0.1)  # Small delay to prevent busy waiting
            
        except Exception as e:
            print(f"Menu error: {e}")
            display_text("Error", str(e), "")
            await asyncio.sleep(2)
