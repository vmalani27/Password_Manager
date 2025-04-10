# password_manager.py
import struct

MASTER_PASSWORD_FILE = "masterpassword.bin"

# password_manager.py
import struct

MASTER_PASSWORD_FILE = "masterpassword.bin"

def save_password(password):
    """Save the password to a binary file."""
    with open(MASTER_PASSWORD_FILE, "wb") as f:
        # Convert the password list to a string and encode it
        password_bytes = ''.join(password).encode('utf-8')
        # Write the length of the password followed by the password bytes
        f.write(struct.pack('B', len(password_bytes)))  # Write the length as a single byte
        f.write(password_bytes)

def load_password():
    """Load the password from a binary file."""
    try:
        with open(MASTER_PASSWORD_FILE, "rb") as f:
            length = struct.unpack('B', f.read(1))[0]  # Read the length of the password
            password_bytes = f.read(length)  # Read the password bytes
            return list(password_bytes.decode('utf-8'))  # Convert bytes back to list of characters
    except OSError:
        return None  # Return None if the file does not exist or cannot be opened

def load_password():
    """Load the password from a binary file."""
    try:
        with open(MASTER_PASSWORD_FILE, "rb") as f:
            length = struct.unpack('B', f.read(1))[0]  # Read the length of the password
            password_bytes = f.read(length)  # Read the password bytes
            return list(password_bytes.decode('utf-8'))  # Convert bytes back to list of characters
    except OSError:
        return None  # Return None if the file does not exist or cannot be opened
