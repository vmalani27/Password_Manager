import board
import digitalio
import time

# Define GPIO pins for rows and columns
rows = [
    digitalio.DigitalInOut(board.GP5),
    digitalio.DigitalInOut(board.GP4),
    digitalio.DigitalInOut(board.GP3),
    digitalio.DigitalInOut(board.GP2),
]

cols = [
    digitalio.DigitalInOut(board.GP9),
    digitalio.DigitalInOut(board.GP8),
    digitalio.DigitalInOut(board.GP7),
    digitalio.DigitalInOut(board.GP6),
]

# Map button positions to their values
button_map = {
    (0, 0): '1', (0, 1): '2', (0, 2): '3', (0, 3): 'Next',  # Row 1
    (1, 0): '4', (1, 1): '5', (1, 2): '6', (1, 3): 'Prev',  # Row 2
    (2, 0): '7', (2, 1): '8', (2, 2): '9', (2, 3): 'OK',    # Row 3
    (3, 0): '*', (3, 1): '0', (3, 2): '#', (3, 3): 'Clear'  # Row 4
}

for row in rows:
    row.switch_to_output(value=False)

for col in cols:
    col.switch_to_input(pull=digitalio.Pull.DOWN)

def scan_matrix():
    """Scan the button matrix and return the pressed button."""
    for row_index, row in enumerate(rows):
        row.value = True  # Set the current row high
        for col_index, col in enumerate(cols):
            if col.value:  # Check if the column is high
                button = button_map.get((row_index, col_index), None)
                if button:
                    print(f"Key Pressed: {button} (Row: {row_index + 1}, Col: {col_index + 1})")
                row.value = False  # Reset the row before returning
                return button  # Return the button value
        row.value = False  # Reset the row
    return None  # No button pressed
