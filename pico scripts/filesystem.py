# filesystem.py

import storage
from display_handler import display_text

def set_filesystem_readonly():
    """Set the filesystem to read-only mode."""
    try:
        storage.remount("/", readonly=True)
        print("Filesystem is now read-only.")
    except Exception as e:
        display_text("Changes saved now just restart the device")
        print(f"to Set Filesystem as read only you must disconnect and reconnect: {e}")

def set_filesystem_readwrite():
    """Set the filesystem to read-write mode."""
    try:
        storage.remount("/", readonly=False)
        print("Filesystem is now read-write.")
    except Exception as e:
        print(f"Error setting filesystem to read-write: {e}")
