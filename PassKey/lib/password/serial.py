try:
    from typing import Optional, Callable
except ImportError:
    pass

import json
import usb_cdc

class Serial:
    _serial = None
    _connected = False
    _data_received_callback = None
    _serial_connected_callback = None
    _initialized = False
    
    @staticmethod
    def init(data_received_callback: Callable[[dict], None], serial_connected_callback: Callable[[], None]):
        if Serial._initialized:
            raise RuntimeError("Serial already initialized")
        
        Serial._serial = usb_cdc.data
        if Serial._serial is None:
            raise RuntimeError("USB CDC data interface not available")
        
        Serial.reset_input_buffer()
        Serial.reset_output_buffer()
        Serial._serial.timeout = -1
        Serial._serial.write_timeout = -1
        
        Serial._data_received_callback = data_received_callback
        Serial._serial_connected_callback = serial_connected_callback
        Serial._initialized = True

    @staticmethod
    def update() -> Optional[dict]:
        if Serial._serial.connected and not Serial._connected:
            if Serial._serial_connected_callback:
                Serial._serial_connected_callback()
            Serial._connected = True
        elif not Serial._serial.connected:
            Serial._connected = False
        
        str_data = Serial._readline()
        if not str_data:
            return None
        
        try:
            json_data = json.loads(str_data)
            if Serial._data_received_callback:
                Serial._data_received_callback(json_data)
                
            return json_data
        except Exception as e:
            print(f"Error decoding JSON: {e}")
    
        return None
    
    @staticmethod
    def _readline() -> str:
        if Serial._serial and Serial._serial.in_waiting > 0:
            return Serial._serial.readline().rstrip().decode("utf-8")
        return None
    
    @staticmethod
    def writeline(text: str) -> None:
        if Serial._serial and Serial._serial.connected:
            Serial._serial.write(f"{text}\n".encode("utf-8"))
    
    @staticmethod
    def flush() -> None:
        if Serial._serial:
            Serial._serial.flush()
            
    @staticmethod
    def reset_input_buffer() -> None:
        if Serial._serial:
            Serial._serial.reset_input_buffer()
            
    @staticmethod
    def reset_output_buffer() -> None:
        if Serial._serial:
            Serial._serial.reset_output_buffer()
