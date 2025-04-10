from keypad_handler import scan_matrix
import json
import time
import asyncio
from display_handler import display_text

CREDENTIALS_FILE = "credentials.json"

# Load existing credentials or create an empty list if none exist
def load_credentials():
    try:
        with open(CREDENTIALS_FILE, "r") as file:
            return json.load(file)
    except OSError:  # Catching a more general file-related error
        display_text("No credentials found.", "", "")  # Optional: Display a message
        return []  # Return an empty list when the file doesn't exist

def save_credentials(credentials):
    with open(CREDENTIALS_FILE, "w") as file:
        json.dump(credentials, file)

# Example usage
credentials = load_credentials()

async def view_credentials():
    """Display all stored credentials."""
    if not credentials:
        display_text("No Credentials", "Stored", "")
        await asyncio.sleep(2)
        return

    for i, cred in enumerate(credentials, 1):
        display_text(f"{i}: {cred['service']}", cred['username'], "")
        await asyncio.sleep(3)  # Show each credential briefly

async def add_credential():
    """Add a new credential via serial input."""
    display_text("Connect to", "Serial Console", "")
    print("Enter new credential details. Type 'exit' to cancel.")

    # Prompt for service name
    print("Service (e.g., Gmail): ", end="")
    service = input().strip()
    if service.lower() == "exit":
        return

    # Prompt for username
    print("Username: ", end="")
    username = input().strip()
    if username.lower() == "exit":
        return

    # Prompt for password
    print("Password: ", end="")
    password = input().strip()
    if password.lower() == "exit":
        return

    # Save new credential
    credentials.append({"service": service, "username": username, "password": password})
    save_credentials(credentials)

    display_text("Credential", "Saved", "")
    await asyncio.sleep(2)

async def delete_credential():
    """Delete a credential by its index."""
    if not credentials:
        display_text("No Credentials", "to Delete", "")
        await asyncio.sleep(2)
        return

    display_text("Select Credential", "to Delete", "")
    await asyncio.sleep(2)
    for i, cred in enumerate(credentials, 1):
        display_text(f"{i}: {cred['service']}", cred['username'], "")
        await asyncio.sleep(2)

    print("Enter the index of the credential to delete: ", end="")
    try:
        index = int(input().strip()) - 1
        if 0 <= index < len(credentials):
            deleted_cred = credentials.pop(index)
            save_credentials(credentials)
            display_text(f"Deleted", deleted_cred['service'], "")
            await asyncio.sleep(2)
        else:
            display_text("Invalid Index", "", "")
            await asyncio.sleep(2)
    except ValueError:
        display_text("Invalid Input", "", "")
        await asyncio.sleep(2)

async def manage_credentials():
    """Provide options to view, add, or delete credentials."""
    options = ["View", "Add", "Delete", "Exit"]
    current_option = 0  # Start with the first option

    while True:
        try:
            # Display current option from the queue
            display_text("Manage Credentials", f"Option {options[current_option]}", "")

            button = scan_matrix()

            if button == "Next":
                current_option = (current_option + 1) % len(options)  # Move to the next option in a circular way
                await asyncio.sleep(0.2)
            elif button == "Prev":
                current_option = (current_option - 1) % len(options)  # Move to the previous option in a circular way
                await asyncio.sleep(0.2)
            elif button == "OK":
                # Handle the selected option
                if options[current_option] == "View":
                    await view_credentials()
                elif options[current_option] == "Add":
                    await add_credential()
                elif options[current_option] == "Delete":
                    await delete_credential()
                elif options[current_option] == "Exit":
                    display_text("Exiting", "", "")
                    await asyncio.sleep(2)
                    break
            
            await asyncio.sleep(0.1)  # Small delay to prevent busy waiting
            
        except Exception as e:
            print(f"Credentials manager error: {e}")
            display_text("Error", str(e), "")
            await asyncio.sleep(2)

# Example usage:
# Uncomment the line below to start managing credentials
# manage_credentials()
