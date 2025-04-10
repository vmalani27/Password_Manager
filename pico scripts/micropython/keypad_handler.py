import machine
import time

# Define GPIO pins for rows and columns
row_pins = [5, 4, 3, 2]   # Row GPIO numbers
col_pins = [9, 8, 7, 6]   # Column GPIO numbers

# Initialize row pins as outputs (initially LOW)
rows = [machine.Pin(pin, machine.Pin.OUT, value=0) for pin in row_pins]

# Initialize column pins as inputs with pull-down resistors
cols = [machine.Pin(pin, machine.Pin.IN, machine.Pin.PULL_DOWN) for pin in col_pins]

# Map button positions to their values
button_map = {
    (0, 0): '1', (0, 1): '2', (0, 2): '3', (0, 3): 'Next',  # Row 1
    (1, 0): '4', (1, 1): '5', (1, 2): '6', (1, 3): 'Prev',  # Row 2
    (2, 0): '7', (2, 1): '8', (2, 2): '9', (2, 3): 'OK',    # Row 3
    (3, 0): '*', (3, 1): '0', (3, 2): '#', (3, 3): 'Clear'  # Row 4
}

def scan_matrix():
    """Scan the button matrix and return the pressed button."""
    for row_index, row in enumerate(rows):
        row.value(1)  # Set the current row HIGH
        for col_index, col in enumerate(cols):
            if col.value():  # Check if the column is HIGH
                row.value(0)  # Reset row before returning
                return button_map.get((row_index, col_index), None)  # Return mapped button
        row.value(0)  # Reset row
    return None  # No button pressed

# Example Usage
while True:
    button = scan_matrix()
    if button:
        print(f"Button pressed: {button}")
        time.sleep(0.3)  # Debounce delay
