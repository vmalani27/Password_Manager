from display_handler import display_text
from keypad_handler import scan_matrix
import time
import asyncio
from password_handler import save_password, load_password

# Load the password from the binary file
password = load_password() or ["1", "2", "3", "4", "5", "6"]  # Default password if none is saved
entered_password = []
is_locked = True

async def set_password():
    """Set a new 6-digit password."""
    global password
    new_password = []
    display_text("Enter New", "Password:", "")
    while len(new_password) < 6:
        button = scan_matrix()
        if button and button.isdigit():
            new_password.append(button)
            display_text("Enter New", "Password:", "".join(new_password))
            await asyncio.sleep(0.2)
        elif button == "Clear":
            if new_password:
                new_password.pop()
                display_text("Enter New", "Password:", "".join(new_password))
            await asyncio.sleep(0.2)
    password = new_password
    save_password(password)  # Save the new password
    display_text("Password Set", "", "")
    await asyncio.sleep(2)

async def reset_password():
    """Reset the password to the default value."""
    global password
    password = ["1", "2", "3", "4", "5", "6"]
    save_password(password)  # Save the default password
    display_text("Password Reset", "to Default", "")
    await asyncio.sleep(2)

async def lock_device():
    """Lock the device and require the correct password to unlock."""
    global is_locked, entered_password
    is_locked = True
    entered_password = []
    display_text("Device Locked", "Enter Password", "")
    while is_locked:
        button = scan_matrix()
        if button and button.isdigit():
            entered_password.append(button)
            display_text("Enter Password", "".join(entered_password), "")
            await asyncio.sleep(0.2)
        elif button == "Clear":
            if entered_password:
                entered_password.pop()
                display_text("Enter Password", "".join(entered_password), "")
            await asyncio.sleep(0.2)
        elif button == "OK":
            if entered_password == password:
                is_locked = False
                display_text("Unlocked!", "", "")
                await asyncio.sleep(2)
                return
            else:
                display_text("Wrong Password", "", "")
                entered_password = []
                await asyncio.sleep(2)
