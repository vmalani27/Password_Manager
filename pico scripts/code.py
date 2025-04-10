import board
import busio
import adafruit_ssd1306
import time
import asyncio
import supervisor
from display_handler import init_display, display_text
from menu_manager import menu
from filesystem import set_filesystem_readwrite

# Enable USB if not already enabled
if not supervisor.runtime.usb_connected:
    print("USB not connected - some features may not work")

# Initialize Display
try:
    init_display()
    print("Display initialized successfully")
except Exception as e:
    print(f"Display initialization error: {e}")

# Async main loop
async def main():
    display_text("Initializing", "Menu...", "")
    await asyncio.sleep(1)  # Display delay

    try:
        # Start Menu System
        await menu()
    except Exception as e:
        print(f"Menu error: {e}")
        display_text("Error", str(e), "")
        while True:
            await asyncio.sleep(1)

# Run the async event loop
try:
    asyncio.run(main())
except Exception as e:
    print(f"Main execution error: {e}")
    while True:
        time.sleep(1)
