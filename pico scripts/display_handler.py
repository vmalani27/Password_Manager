import board
import busio
from adafruit_ssd1306 import SSD1306_I2C

# Initialize I2C and OLED display
i2c = busio.I2C(board.GP17, board.GP16)  # SCL=GP17, SDA=GP16
oled = SSD1306_I2C(128, 64, i2c)  # 128x64 OLED display

def init_display():
    """Initialize the OLED display."""
    oled.fill(0)
    oled.show()

def display_text(line1="", line2="", line3=""):
    """Display text on the OLED."""
    oled.fill(0)
    oled.text(line1, 0, 0, 1)
    oled.text(line2, 0, 16, 1)
    oled.text(line3, 0, 32, 1)
    oled.show()
# Write your code here :-)
