from microcontroller import Pin
import digitalio
import adafruit_debouncer

class Button:
    def __init__(self, pin: Pin, pull: digitalio.Pull, short_duration_ms: int = 150, long_duration_ms: int = 500):
        button_pin = digitalio.DigitalInOut(pin)
        button_pin.direction = digitalio.Direction.INPUT
        button_pin.pull = pull
        
        self.button = adafruit_debouncer.Button(
            pin=button_pin,
            short_duration_ms=short_duration_ms,
            long_duration_ms=long_duration_ms,
            value_when_pressed=(pull is digitalio.Pull.DOWN)
        )
        
    def update(self) -> None:
        self.button.update()
        
    @property
    def value(self) -> bool:
        return self.button.value
        
    @property
    def pressed(self) -> bool:
        return self.button.pressed
    
    @property
    def released(self) -> bool:
        return self.button.released
    
    @property
    def short_count(self) -> int:
        return self.button.short_count
    
    @property
    def long_press(self) -> bool:
        return self.button.long_press
