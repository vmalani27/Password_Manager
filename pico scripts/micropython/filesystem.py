import machine

MODE_FILE = "mode.txt"

def set_filesystem_readonly():
    """Set filesystem mode to read-only (changes on next reboot)."""
    try:
        with open(MODE_FILE, "w") as f:
            f.write("read-only")
        print("Changes saved. Restart the device to apply read-only mode.")
    except OSError as e:
        print(f"Error: {e}")

def set_filesystem_readwrite():
    """Set filesystem mode to read-write (changes on next reboot)."""
    try:
        with open(MODE_FILE, "w") as f:
            f.write("read-write")
        print("Changes saved. Restart the device to apply read-write mode.")
    except OSError as e:
        print(f"Error: {e}")

def reboot():
    """Restart the device."""
    print("Rebooting...")
    machine.reset()
