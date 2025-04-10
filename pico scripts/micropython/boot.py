import os

MODE_FILE = "mode.txt"

def get_mode():
    """Read mode from a file (default: read-only)"""
    try:
        with open(MODE_FILE, "r") as f:
            mode = f.read().strip()
            return mode.lower() == "read-write"
    except OSError:
        return False  # Default to read-only if file is missing

# Set filesystem permissions at boot
if get_mode():
    print("Filesystem set to READ-WRITE")
else:
    print("Filesystem set to READ-ONLY")
