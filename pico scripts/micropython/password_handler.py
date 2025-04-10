import ustruct as struct  # Use ustruct in MicroPython

MASTER_PASSWORD_FILE = "masterpassword.bin"

def save_password(password):
    """Save the password to a binary file."""
    try:
        with open(MASTER_PASSWORD_FILE, "wb") as f:
            password_bytes = ''.join(password).encode('utf-8')
            f.write(struct.pack('B', len(password_bytes)))  # Write length as single byte
            f.write(password_bytes)
        print("Password saved successfully.")
    except OSError as e:
        print(f"File write error: {e}")

def load_password():
    """Load the password from a binary file."""
    try:
        with open(MASTER_PASSWORD_FILE, "rb") as f:
            length = struct.unpack('B', f.read(1))[0]  # Read length of password
            password_bytes = f.read(length)  # Read password bytes
            return list(password_bytes.decode('utf-8'))  # Convert bytes to list of characters
    except OSError as e:
        print(f"File read error: {e}")
        return None  # Return None if file doesn't exist or cannot be opened
