from microcontroller import Pin
import displayio
import terminalio
from busio import I2C
import adafruit_displayio_ssd1306
from adafruit_display_text import LabelBase, label, bitmap_label, scrolling_label, wrap_text_to_pixels

class TextAlignment:
    TOP_LEFT = (0, 0)
    TOP_CENTER = (0.5, 0)
    TOP_RIGHT = (1.0, 0)
    MIDDLE_LEFT = (0, 0.5)
    CENTER = (0.5, 0.5)
    MIDDLE_RIGHT = (1.0, 0.5)
    BOTTOM_LEFT = (0.0, 1.0)
    BOTTOM_CENTER = (0.5, 1.0)
    BOTTOM_RIGHT = (1.0, 1.0)

class Oled:
    DEFAULT_FONT = terminalio.FONT
    
    def __init__(self, scl: Pin, sda: Pin, address: int = 0x3C, width: int = 128, height: int = 64):
        displayio.release_displays()
        i2c = I2C(scl, sda)
        display_bus = displayio.I2CDisplay (i2c, device_address=address)
        self.display = adafruit_displayio_ssd1306.SSD1306(display_bus, width=width, height=height)
        self.splash = displayio.Group()
        self.display.root_group = self.splash
        self.scrolling_labels = []
        
    def update(self) -> None:
        if not self.is_awake:
            return
        
        for label in self.scrolling_labels:
            label.update()
            
    def add_text(
        self,
        text: str,
        alignment: TextAlignment = TextAlignment.CENTER,
        justify: bool = False,
        hidden: bool = False
    ) -> label.Label:
        if justify:
            text = "\n".join(wrap_text_to_pixels(text, self.display.width, font))
        text_area = bitmap_label.Label(
            font=self.DEFAULT_FONT,
            text=text,
            color=0xFFFFFF
        )
        text_area.hidden = hidden
        self.align_text(text_area, alignment)
        self.splash.append(text_area)
        return text_area
    
    def add_scrolling_text(
        self,
        text: str,
        max_characters: int = 10,
        animate_time:float = 0.3,
        alignment: TextAlignment = TextAlignment.CENTER,
        hidden: bool = False
    ) -> label.Label:
        text_area = scrolling_label.ScrollingLabel(
            font=self.DEFAULT_FONT,
            text=text,
            color=0xFFFFFF,
            max_characters=max_characters,
            animate_time=animate_time
        )
        text_area.hidden = hidden
        self.align_text(text_area, alignment)
        self.splash.append(text_area)
        self.scrolling_labels.append(text_area)
        return text_area
    
    def align_text(self, label: LabelBase, alignment: tuple[float, float]) -> None:
        label.anchor_point = alignment
        label.anchored_position = (
            int(alignment[0] * self.display.width),
            int(alignment[1] * self.display.height)
        )
        
    def clear(self) -> None:
        while self.splash:
            self.splash.pop()
        self.scrolling_labels.clear()
            
    def remove(self, layer: displayio.Group) -> None:
        if isinstance(layer, scrolling_label.ScrollingLabel):
            self.scrolling_labels.remove(layer)
        self.splash.remove(layer)
        
    def rotate(self, degree: int) -> None:
        if degree not in (0, 90, 180, 270):
            raise ValueError(f"Invalid rotation angle: {degrees}. Must be 0, 90, 180, or 270.")
        self.display.rotation = degree
    
    @property
    def is_awake(self) -> bool:
        return self.display.is_awake
    
    def sleep(self) -> None:
        self.display.sleep()
        
    def wake(self) -> None:
        self.display.wake()
        
    def toggle_power(self) -> None:
        if self.is_awake:
            self.sleep()
        else:
            self.wake()    
