from machine import Pin
import json
import time
import os
from display_handler import display_text
from keypad_handler import scan_matrix

CREDENTIALS_FILE = "credentials.json"

# Load existing credentials or create an empty list if none exist
def load_credentials():
    try:
        with open(CREDENTIALS_FILE, "r") as file:
            return json.load(file)
    except OSError:  # Handle file-related errors
        display_text("No credentials found.", "", "")  # Optional: Display a message
        return []  # Return an empty list when the file doesn't exist

def save_credentials(credentials):
    with open(CREDENTIALS_FILE, "w") as file:
        json.dump(credentials, file)

# Example usage
credentials = load_credentials()

def view_credentials():
    """Display all stored credentials."""
    if not credentials:
        display_text("No Credentials", "Stored", "")
        time.sleep(2)
        return

    for i, cred in enumerate(credentials, 1):
        display_text(f"{i}: {cred['service']}", cred['username'], "")
        time.sleep(3)  # Show each credential briefly

def add_credential():
    """Add a new credential via serial input."""
    display_text("Connect to", "Serial Console", "")
    print("Enter new credential details. Type 'exit' to cancel.")

    # Prompt for service name
    service = input("Service (e.g., Gmail): ").strip()
    if service.lower() == "exit":
        return

    # Prompt for username
    username = input("Username: ").strip()
    if username.lower() == "exit":
        return

    # Prompt for password
    password = input("Password: ").strip()
    if password.lower() == "exit":
        return

    # Save new credential
    credentials.append({"service": service, "username": username, "password": password})
    save_credentials(credentials)

    display_text("Credential", "Saved", "")
    time.sleep(2)

def delete_credential():
    """Delete a credential by its index."""
    if not credentials:
        display_text("No Credentials", "to Delete", "")
        time.sleep(2)
        return

    display_text("Select Credential", "to Delete", "")
    time.sleep(2)
    for i, cred in enumerate(credentials, 1):
        display_text(f"{i}: {cred['service']}", cred['username'], "")
        time.sleep(2)

    try:
        index = int(input("Enter the index of the credential to delete: ").strip()) - 1
        if 0 <= index < len(credentials):
            deleted_cred = credentials.pop(index)
            save_credentials(credentials)
            display_text(f"Deleted", deleted_cred['service'], "")
            time.sleep(2)
        else:
            display_text("Invalid Index", "", "")
            time.sleep(2)
    except ValueError:
        display_text("Invalid Input", "", "")
        time.sleep(2)

def manage_credentials():
    """Provide options to view, add, or delete credentials."""
    options = ["View", "Add", "Delete", "Exit"]
    current_option = 0  # Start with the first option

    while True:
        display_text("Manage Credentials", f"Option {options[current_option]}", "")

        button = scan_matrix()

        # Debugging: Print the button value to check what it returns
        print(f"Button pressed: {button}")  # This line will help identify the issue

        if button == "Next":
            current_option = (current_option + 1) % len(options)  # Move to the next option in a circular way
            time.sleep(0.2)
        elif button == "Prev":
            current_option = (current_option - 1) % len(options)  # Move to the previous option in a circular way
            time.sleep(0.2)
        elif button == "OK":
            # Handle the selected option
            if options[current_option] == "View":
                view_credentials()
            elif options[current_option] == "Add":
                add_credential()
            elif options[current_option] == "Delete":
                delete_credential()
            elif options[current_option] == "Exit":
                display_text("Exiting", "", "")
                time.sleep(2)
                break


# Uncomment below line to start managing credentials
# manage_credentials()
