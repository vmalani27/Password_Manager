from machine import Pin, I2C
import ssd1306

# Initialize I2C and OLED display
i2c = I2C(0, scl=Pin(17), sda=Pin(16))  # SCL=GP17, SDA=GP16
oled = ssd1306.SSD1306_I2C(128, 64, i2c)  # 128x64 OLED display

def init_display():
    """Initialize the OLED display."""
    oled.fill(0)
    oled.show()

def display_text(line1="", line2="", line3=""):
    """Display text on the OLED."""
    oled.fill(0)
    oled.text(line1, 0, 0)
    oled.text(line2, 0, 16)
    oled.text(line3, 0, 32)
    oled.show()
