import paramiko
import os
import sys
import socket

# SSH Configuration
PI_HOST = "192.168.17.30"  # Replace with your Pi's actual IP address
PI_USER = "ppm"            # Replace with your Pi's username
PI_PASSWORD = "root"       # Replace with your Pi's password
REMOTE_DIR = "/home/ppm/ppm/"  # Remote directory on Pi

def validate_ip(ip_str):
    """Validate IP address format"""
    try:
        socket.inet_aton(ip_str)
        return True
    except socket.error:
        return False

def create_ssh_client():
    """Create and return an SSH client with improved error handling"""
    if not validate_ip(PI_HOST):
        print(f"Error: Invalid IP address format: {PI_HOST}")
        print("Please check your PI_HOST configuration")
        sys.exit(1)

    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    
    try:
        print(f"Attempting to connect to {PI_HOST} as user {PI_USER}...")
        client.connect(
            PI_HOST,
            username=PI_USER,
            password=PI_PASSWORD,
            timeout=10
        )
        print("Successfully connected to the remote host")
        return client
    except socket.timeout:
        print(f"Connection timed out. Please check if the host {PI_HOST} is reachable")
        sys.exit(1)
    except paramiko.AuthenticationException:
        print("Authentication failed. Please check your username and password")
        sys.exit(1)
    except paramiko.SSHException as ssh_exception:
        print(f"SSH exception occurred: {ssh_exception}")
        sys.exit(1)
    except socket.error as socket_error:
        print(f"Network error: {socket_error}")
        print("Please check your network connection and the remote host's availability")
        sys.exit(1)
    except Exception as e:
        print(f"Failed to connect to Pi: {str(e)}")
        sys.exit(1)

def create_sftp_client(ssh_client):
    return ssh_client.open_sftp()

def ensure_remote_directory(sftp):
    try:
        sftp.stat(REMOTE_DIR)
    except FileNotFoundError:
        sftp.mkdir(REMOTE_DIR)
    
    # Ensure templates directory exists
    try:
        sftp.stat(f"{REMOTE_DIR}/templates")
    except FileNotFoundError:
        sftp.mkdir(f"{REMOTE_DIR}/templates")

def transfer_files(sftp):
    print("Transferring files...")
    
    # Get list of all files and directories in current directory
    items = os.listdir('.')
    
    # Skip system files and directories
    skip_items = [
        '__pycache__',
        '.git',
        '.env',
        'venv',
        'modules_windows',
        '.pytest_cache',
        '.vscode',
        'build',
        'dist',
        '*.pyc',
        '*.pyo',
        '*.pyd',
        '.DS_Store'
    ]
    
    for item in items:
        # Skip hidden files and specified items
        if item.startswith('.') or item in skip_items or any(item.endswith(ext) for ext in ['.pyc', '.pyo', '.pyd']):
            print(f"Skipping {item}...")
            continue
            
        local_path = os.path.join('.', item)
        # Convert Windows path separators to Unix style for remote path
        remote_path = os.path.join(REMOTE_DIR, item).replace('\\', '/')
        
        if os.path.isdir(local_path):
            # Handle directory transfer
            try:
                print(f"Creating directory {remote_path}...")
                sftp.mkdir(remote_path)
            except IOError:
                print(f"Directory {remote_path} already exists, continuing...")
            
            # Recursively transfer directory contents
            for root, dirs, files in os.walk(local_path):
                # Skip directories in skip_items
                dirs[:] = [d for d in dirs if d not in skip_items and not d.startswith('.')]
                
                for d in dirs:
                    local_dir = os.path.join(root, d)
                    # Calculate the relative path from the source directory
                    rel_path = os.path.relpath(local_dir, local_path)
                    # Convert Windows path separators to Unix style for remote path
                    remote_dir = os.path.join(remote_path, rel_path).replace('\\', '/')
                    try:
                        print(f"Creating directory {remote_dir}...")
                        sftp.mkdir(remote_dir)
                    except IOError:
                        print(f"Directory {remote_dir} already exists, continuing...")
                
                for f in files:
                    # Skip system files
                    if f.startswith('.') or any(f.endswith(ext) for ext in ['.pyc', '.pyo', '.pyd']):
                        continue
                    
                    local_file = os.path.join(root, f)
                    # Calculate the relative path from the source directory
                    rel_path = os.path.relpath(local_file, local_path)
                    # Convert Windows path separators to Unix style for remote path
                    remote_file = os.path.join(remote_path, rel_path).replace('\\', '/')
                    print(f"Transferring {local_file} to {remote_file}...")
                    sftp.put(local_file, remote_file)
        else:
            # Transfer single file
            print(f"Transferring {item} to {remote_path}...")
            sftp.put(local_path, remote_path)

def setup_environment(ssh_client):
    # First ensure pip is up to date
    print("Updating pip...")
    stdin, stdout, stderr = ssh_client.exec_command("python3 -m pip install --upgrade pip")
    print(stdout.read().decode())
    print(stderr.read().decode())
    
    # Install requirements with verbose output
    print("Installing requirements...")
    stdin, stdout, stderr = ssh_client.exec_command(f"cd {REMOTE_DIR} && pip3 install -r requirements.txt --verbose")
    output = stdout.read().decode()
    error = stderr.read().decode()
    print(output)
    print(error)
    
    # Verify installations
    print("Verifying installations...")
    stdin, stdout, stderr = ssh_client.exec_command("python3 -c 'import flask_bcrypt; print(\"flask_bcrypt installed successfully\")'")
    print(stdout.read().decode())
    print(stderr.read().decode())
    
    # Backup existing database if it exists
    print("Backing up existing database...")
    stdin, stdout, stderr = ssh_client.exec_command(f"cd {REMOTE_DIR} && if [ -f passwords.db ]; then cp passwords.db passwords.db.backup; fi")
    print(stdout.read().decode())
    print(stderr.read().decode())
    
    # Initialize database
    print("Initializing database...")
    stdin, stdout, stderr = ssh_client.exec_command(f"cd {REMOTE_DIR} && python3 -c 'from app import init_db; init_db()'")
    print(stdout.read().decode())
    print(stderr.read().decode())

def start_application(ssh_client):
    print("Starting the application...")
    # Use nohup to keep the application running after SSH session ends
    stdin, stdout, stderr = ssh_client.exec_command(
        f"cd {REMOTE_DIR} && nohup python3 app.py > app.log 2>&1 &"
    )
    print(stdout.read().decode())
    print(stderr.read().decode())
    
    # Get the process ID
    stdin, stdout, stderr = ssh_client.exec_command("pgrep -f 'python3 app.py'")
    pid = stdout.read().decode().strip()
    if pid:
        print(f"Application started with PID: {pid}")
    else:
        print("Warning: Could not verify application start")

def main():
    print("Connecting to Pi...")
    ssh_client = create_ssh_client()
    sftp = create_sftp_client(ssh_client)
    
    print("Creating remote directory...")
    ensure_remote_directory(sftp)
    
    print("Transferring files...")
    transfer_files(sftp)
    
    print("Setting up environment...")
    setup_environment(ssh_client)
    
    start_application(ssh_client)
    
    sftp.close()
    ssh_client.close()
    print("\nDeployment completed successfully!")
    print(f"Application is running on http://{PI_HOST}:5000")
    print("You can check the application logs in the remote directory at app.log")

if __name__ == "__main__":
    main() 