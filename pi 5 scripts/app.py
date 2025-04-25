from flask import Flask, request, jsonify, render_template, session, redirect, url_for
from flask_bcrypt import Bcrypt
from flask_cors import CORS
import sqlite3
import os
import secrets
from password_analyzer import PasswordStrengthAnalyzer
from nlp_analyzer import NLPPasswordAnalyzer
import pyperclip  # For clipboard operations
from bleak import BleakScanner, BleakClient
import asyncio
import requests
from requests.exceptions import RequestException
import sys

app = Flask(__name__)
CORS(app)  # Enable CORS for all routes
bcrypt = Bcrypt(app)
app.secret_key = secrets.token_hex(16)  # Secret key for session management

# Use absolute path for database
BASE_DIR = os.path.abspath(os.path.dirname(__file__))
DATABASE = os.path.join(BASE_DIR, 'passwords.db')
password_analyzer = PasswordStrengthAnalyzer()
nlp_analyzer = NLPPasswordAnalyzer()

# BLE Constants
ESP32_SERVICE_UUID = "0000180F-0000-1000-8000-00805F9B34FB"  # Replace with your ESP32's service UUID
ESP32_CHAR_UUID = "00002A19-0000-1000-8000-00805F9B34FB"    # Replace with your ESP32's characteristic UUID

# ESP32 Integration
ESP32_BASE_URL = "http://192.168.4.1"  # Default ESP32 AP IP address
ESP32_ENDPOINTS = {
    'status': '/api/status',
    'receive_password': '/api/receive_password',
    'credentials': '/api/credentials'
}

def is_logged_in():
    return session.get('logged_in', False)

def require_login(f):
    def decorated_function(*args, **kwargs):
        if not is_logged_in():
            return redirect(url_for('login'))
        return f(*args, **kwargs)
    decorated_function.__name__ = f.__name__
    return decorated_function

@app.route('/')
def home():
    if not is_logged_in():
        return redirect(url_for('login'))
    return render_template('index.html')

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        master_password = request.form.get('master_password')
        try:
            conn = sqlite3.connect(DATABASE)
            cursor = conn.cursor()
            cursor.execute('SELECT password_hash FROM master_password LIMIT 1')
            result = cursor.fetchone()
            conn.close()

            if result and bcrypt.check_password_hash(result[0], master_password):
                session['logged_in'] = True
                return redirect(url_for('home'))
            else:
                return render_template('login.html', error='Invalid master password')
        except sqlite3.Error as e:
            print(f"Database error in login: {str(e)}")
            return render_template('login.html', error='Database error occurred')
    return render_template('login.html')

@app.route('/setup', methods=['GET', 'POST'])
def setup():
    try:
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        cursor.execute('SELECT COUNT(*) FROM master_password')
        if cursor.fetchone()[0] > 0:
            conn.close()
            return redirect(url_for('login'))
        conn.close()
    except sqlite3.Error:
        pass

    if request.method == 'POST':
        master_password = request.form.get('master_password')
        confirm_password = request.form.get('confirm_password')

        if master_password != confirm_password:
            return render_template('setup.html', error='Passwords do not match')

        try:
            conn = sqlite3.connect(DATABASE)
            cursor = conn.cursor()
            password_hash = bcrypt.generate_password_hash(master_password).decode('utf-8')
            cursor.execute('INSERT INTO master_password (password_hash) VALUES (?)', (password_hash,))
            conn.commit()
            conn.close()
            return redirect(url_for('login'))
        except sqlite3.Error as e:
            print(f"Database error in setup: {str(e)}")
            return render_template('setup.html', error='Database error occurred')

    return render_template('setup.html')

@app.route('/logout')
def logout():
    session.pop('logged_in', None)
    return redirect(url_for('login'))

# Initialize database
def init_db():
    # Ensure the database directory exists
    os.makedirs(os.path.dirname(DATABASE), exist_ok=True)
    
    with sqlite3.connect(DATABASE) as conn:
        cursor = conn.cursor()
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS master_password (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                password_hash TEXT NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        ''')
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS credentials (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                service_name TEXT NOT NULL,
                username TEXT NOT NULL,
                password TEXT NOT NULL,
                password_hash TEXT NOT NULL,
                password_strength_score REAL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        ''')
        conn.commit()

@app.route('/add_credential', methods=['POST'])
@require_login
def add_credential():
    try:
        # Debug logging for request
        print("Received add_credential request")
        print(f"Request Content-Type: {request.headers.get('Content-Type')}")
        print(f"Request data: {request.get_data(as_text=True)}")
        
        data = request.json
        if not data:
            print("Error: No JSON data provided")
            return jsonify({'error': 'No JSON data provided'}), 400
            
        # Debug logging for parsed data
        print(f"Parsed JSON data: {data}")
        
        service_name = data.get('service_name', '').strip()
        username = data.get('username', '').strip()
        password = data.get('password', '')
        
        # Debug logging for extracted fields
        print(f"Extracted fields - service_name: {bool(service_name)}, username: {bool(username)}, password: {bool(password)}")
        
        # Validate required fields
        if not service_name:
            print("Error: Service name is required")
            return jsonify({'error': 'Service name is required'}), 400
        if not username:
            print("Error: Username is required")
            return jsonify({'error': 'Username is required'}), 400
        if not password:
            print("Error: Password is required")
            return jsonify({'error': 'Password is required'}), 400
        
        # Use NLP to suggest standardized service name
        suggested_service_name = nlp_analyzer.suggest_service_name(service_name)
        print(f"Suggested service name: {suggested_service_name}")
        
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        
        try:
            # Check if service exists
            cursor.execute('SELECT id, username FROM credentials WHERE service_name = ?', (suggested_service_name,))
            existing = cursor.fetchone()
            
            if existing:
                print(f"Service already exists with id: {existing[0]}")
                conn.close()
                return jsonify({
                    'error': 'Service already exists',
                    'existing': {
                        'id': existing[0],
                        'username': existing[1]
                    }
                }), 409
            
            # Analyze password strength using NLP
            strength_analysis = nlp_analyzer.analyze_password_strength_nlp(password)
            print(f"Password strength analysis - is_strong: {strength_analysis['is_strong']}")
            
            if not strength_analysis['is_strong']:
                print("Error: Password is too weak")
                conn.close()
                return jsonify({
                    'error': 'Password is too weak',
                    'feedback': strength_analysis['feedback'],
                    'patterns': strength_analysis['patterns']
                }), 400
            
            password_hash = bcrypt.generate_password_hash(password).decode('utf-8')
            cursor.execute(
                'INSERT INTO credentials (service_name, username, password, password_hash, password_strength_score) VALUES (?, ?, ?, ?, ?)',
                (suggested_service_name, username, password, password_hash, strength_analysis['score'])
            )
            conn.commit()
            print("Successfully added credential")
            
            return jsonify({
                'message': 'Credential added successfully',
                'service_name': suggested_service_name,
                'password_strength': strength_analysis
            })
            
        except sqlite3.Error as e:
            conn.rollback()
            print(f"Database error in add_credential: {str(e)}")
            return jsonify({'error': 'Database error occurred'}), 500
            
        finally:
            conn.close()
            
    except Exception as e:
        print(f"Unexpected error in add_credential: {str(e)}")
        print(f"Error type: {type(e)}")
        import traceback
        print(f"Traceback: {traceback.format_exc()}")
        return jsonify({'error': 'An unexpected error occurred'}), 500

@app.route('/get_credentials', methods=['GET'])
@require_login
def get_credentials():
    try:
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        cursor.execute('SELECT id, service_name, username, created_at FROM credentials ORDER BY created_at DESC')
        credentials = cursor.fetchall()
        conn.close()
        
        return jsonify({
            'credentials': [
                {
                    'id': cred[0],
                    'service_name': cred[1],
                    'username': cred[2],
                    'created_at': cred[3]
                }
                for cred in credentials
            ]
        })
    except sqlite3.Error as e:
        print(f"Database error in get_credentials: {str(e)}")
        if 'conn' in locals():
            conn.close()
        return jsonify({'error': f'Database error: {str(e)}'}), 500
    except Exception as e:
        print(f"Unexpected error in get_credentials: {str(e)}")
        if 'conn' in locals():
            conn.close()
        return jsonify({'error': f'Unexpected error: {str(e)}'}), 500

@app.route('/get_password/<int:credential_id>', methods=['GET'])
@require_login
def get_password(credential_id):
    try:
        # Input validation
        if not credential_id:
            return jsonify({
                'success': False,
                'error': 'Invalid credential ID'
            }), 400

        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        cursor.execute('SELECT id, password FROM credentials WHERE id = ?', (credential_id,))
        result = cursor.fetchone()
        conn.close()
        
        if not result:
            return jsonify({
                'success': False,
                'error': 'Credential not found'
            }), 404
        
        # Get the password
        password = result[1]
        
        try:
            # Try to copy to clipboard
            pyperclip.copy(password)
            return jsonify({
                'success': True,
                'message': 'Password copied to clipboard'
            })
        except Exception as e:
            # If clipboard operation fails, return the password in response
            print(f"Clipboard error: {str(e)}")
            print("Hint: On Linux, you need to install xclip or xsel.")
            print("You can install it using: sudo apt-get install xclip")
            return jsonify({
                'success': True,
                'message': 'Clipboard operation not available. Here is your password:',
                'password': password
            })
            
    except sqlite3.Error as e:
        print(f"Database error in get_password: {str(e)}")
        return jsonify({
            'success': False,
            'error': 'Database error occurred'
        }), 500
    except Exception as e:
        print(f"Unexpected error in get_password: {str(e)}")
        return jsonify({
            'success': False,
            'error': 'An unexpected error occurred'
        }), 500

@app.route('/search_credentials', methods=['GET'])
@require_login
def search_credentials():
    query = request.args.get('q', '').lower()
    try:
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        cursor.execute('SELECT id, service_name, username FROM credentials')
        all_credentials = cursor.fetchall()
        conn.close()
        
        # Use NLP enhanced search
        if all_credentials:
            service_names = [(cred[0], cred[1], cred[2]) for cred in all_credentials]
            search_results = nlp_analyzer.enhanced_search(query, [name for _, name, _ in service_names])
            
            # Map search results back to full credential info
            credentials = []
            for result in search_results:
                for cred_id, service_name, username in service_names:
                    if service_name == result['service']:
                        credentials.append({
                            'id': cred_id,
                            'service_name': service_name,
                            'username': username,
                            'relevance_score': result['score']
                        })
                        break
            
            return jsonify({'credentials': credentials})
        else:
            return jsonify({'credentials': []})
            
    except sqlite3.Error as e:
        print(f"Database error in search_credentials: {str(e)}")
        if 'conn' in locals():
            conn.close()
        return jsonify({'error': f'Database error: {str(e)}'}), 500
    except Exception as e:
        print(f"Unexpected error in search_credentials: {str(e)}")
        if 'conn' in locals():
            conn.close()
        return jsonify({'error': f'Unexpected error: {str(e)}'}), 500

@app.route('/update_credential/<int:credential_id>', methods=['PUT'])
@require_login
def update_credential(credential_id):
    data = request.json
    username = data.get('username')
    password = data.get('password')
    
    if not all([username, password]):
        return jsonify({'error': 'Missing required fields'}), 400
    
    # Analyze password strength
    strength_analysis = password_analyzer.analyze_strength(password)
    if not strength_analysis['is_strong']:
        return jsonify({
            'error': 'Password is too weak',
            'feedback': strength_analysis['feedback']
        }), 400
    
    password_hash = bcrypt.generate_password_hash(password).decode('utf-8')
    try:
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        cursor.execute(
            'UPDATE credentials SET username = ?, password_hash = ?, password_strength_score = ? WHERE id = ?',
            (username, password_hash, strength_analysis['score'], credential_id)
        )
        conn.commit()
        conn.close()
        
        return jsonify({
            'message': 'Credential updated successfully',
            'password_strength': strength_analysis
        })
    except sqlite3.Error as e:
        print(f"Database error in update_credential: {str(e)}")
        if 'conn' in locals():
            conn.close()
        return jsonify({'error': f'Database error: {str(e)}'}), 500
    except Exception as e:
        print(f"Unexpected error in update_credential: {str(e)}")
        if 'conn' in locals():
            conn.close()
        return jsonify({'error': f'Unexpected error: {str(e)}'}), 500

@app.route('/delete_credential/<int:credential_id>', methods=['DELETE'])
@require_login
def delete_credential(credential_id):
    try:
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        cursor.execute('DELETE FROM credentials WHERE id = ?', (credential_id,))
        conn.commit()
        conn.close()
        
        return jsonify({
            'message': 'Credential deleted successfully',
            'success': True
        })
    except sqlite3.Error as e:
        print(f"Database error in delete_credential: {str(e)}")
        if 'conn' in locals():
            conn.close()
        return jsonify({'error': f'Database error: {str(e)}'}), 500
    except Exception as e:
        print(f"Unexpected error in delete_credential: {str(e)}")
        if 'conn' in locals():
            conn.close()
        return jsonify({'error': f'Unexpected error: {str(e)}'}), 500

@app.route('/generate_password_suggestions', methods=['POST'])
@require_login
def generate_password_suggestions():
    data = request.json
    service_name = data.get('service_name', '')
    username = data.get('username', '')
    count = data.get('count', 3)
    
    try:
        suggestions = nlp_analyzer.generate_password_suggestions(service_name, username, count)
        return jsonify({
            'success': True,
            'suggestions': suggestions
        })
    except Exception as e:
        print(f"Error generating password suggestions: {str(e)}")
        return jsonify({
            'success': False,
            'error': 'Failed to generate password suggestions'
        }), 500

@app.route('/get_suggestions', methods=['GET'])
@require_login
def get_suggestions():
    try:
        query = request.args.get('q', '').lower()
        field = request.args.get('field', 'service')  # 'service' or 'username'
        
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        
        if field == 'service':
            cursor.execute('SELECT DISTINCT service_name FROM credentials')
            items = [row[0] for row in cursor.fetchall()]
        else:  # username
            cursor.execute('SELECT DISTINCT username FROM credentials')
            items = [row[0] for row in cursor.fetchall()]
            
        conn.close()
        
        # Filter suggestions based on query
        suggestions = [item for item in items if query in item.lower()]
        # Sort by relevance (exact matches first, then starts with, then contains)
        suggestions.sort(key=lambda x: (
            0 if x.lower() == query else 
            1 if x.lower().startswith(query) else 
            2
        ))
        
        return jsonify({
            'success': True,
            'suggestions': suggestions[:5]  # Limit to top 5 suggestions
        })
        
    except Exception as e:
        print(f"Error getting suggestions: {str(e)}")
        return jsonify({
            'success': False,
            'error': 'Failed to get suggestions'
        }), 500

async def find_esp32():
    devices = await BleakScanner.discover()
    for d in devices:
        if d.name and "Password Manager" in d.name:
            return d.address
    return None

async def send_password_to_esp32(password):
    address = await find_esp32()
    if not address:
        return False, "ESP32 device not found"
    
    try:
        async with BleakClient(address) as client:
            await client.write_gatt_char(ESP32_CHAR_UUID, f"TYPE:{password}".encode())
            return True, "Password sent to ESP32"
    except Exception as e:
        return False, f"Error sending password: {str(e)}"

@app.route('/send_to_device/<int:credential_id>', methods=['POST'])
@require_login
def send_to_device(credential_id):
    try:
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        cursor.execute('SELECT password FROM credentials WHERE id = ?', (credential_id,))
        result = cursor.fetchone()
        conn.close()
        
        if not result:
            return jsonify({
                'success': False,
                'error': 'Credential not found'
            }), 404
        
        password = result[0]
        success, message = asyncio.run(send_password_to_esp32(password))
        
        return jsonify({
            'success': success,
            'message': message
        })
        
    except Exception as e:
        print(f"Error in send_to_device: {str(e)}")
        return jsonify({
            'success': False,
            'error': 'An unexpected error occurred'
        }), 500

def check_esp32_connection():
    try:
        response = requests.get(f"{ESP32_BASE_URL}{ESP32_ENDPOINTS['status']}", timeout=5)
        return response.status_code == 200
    except RequestException:
        return False

@app.route('/esp32/status')
@require_login
def esp32_status():
    try:
        response = requests.get(f"{ESP32_BASE_URL}{ESP32_ENDPOINTS['status']}", timeout=5)
        return jsonify(response.json())
    except RequestException as e:
        return jsonify({'error': str(e)}), 500

@app.route('/esp32/send_password/<int:credential_id>', methods=['POST'])
@require_login
def send_password_to_esp32(credential_id):
    try:
        # Get credential from database
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        cursor.execute('SELECT service_name, username, password FROM credentials WHERE id = ?', (credential_id,))
        result = cursor.fetchone()
        conn.close()

        if not result:
            return jsonify({'error': 'Credential not found'}), 404

        service_name, username, password = result

        # Send to ESP32
        data = {
            'site': service_name,
            'username': username,
            'password': password
        }
        
        response = requests.post(
            f"{ESP32_BASE_URL}{ESP32_ENDPOINTS['receive_password']}", 
            json=data,
            timeout=5
        )
        
        if response.status_code == 200:
            return jsonify({'message': 'Password sent to ESP32 successfully'})
        else:
            return jsonify({'error': 'Failed to send password to ESP32'}), 500

    except RequestException as e:
        return jsonify({'error': str(e)}), 500
    except sqlite3.Error as e:
        return jsonify({'error': f'Database error: {str(e)}'}), 500

@app.route('/esp32/credentials')
@require_login
def get_esp32_credentials():
    try:
        response = requests.get(f"{ESP32_BASE_URL}{ESP32_ENDPOINTS['credentials']}", timeout=5)
        return jsonify(response.json())
    except RequestException as e:
        return jsonify({'error': str(e)}), 500

@app.route('/devices')
@require_login
def devices_page():
    return render_template('devices.html')

@app.route('/scan_devices')
@require_login
def scan_devices():
    try:
        print("Starting BLE device scan...")
        
        # Create a new event loop for Windows
        if sys.platform == 'win32':
            loop = asyncio.ProactorEventLoop()
            asyncio.set_event_loop(loop)
        else:
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
        
        # Create a list to store discovered devices
        discovered_devices = []
        
        # Define the callback function
        def detection_callback(device, advertisement_data):
            if device.name:
                print(f"Found device: {device.name} ({device.address})")
                discovered_devices.append({
                    'name': device.name,
                    'address': device.address,
                    'rssi': device.rssi
                })
        
        async def scan():
            # Create scanner with callback
            scanner = BleakScanner(detection_callback=detection_callback)
            
            # Start scanning
            print("Scanning for BLE devices...")
            await scanner.start()
            
            # Wait for 5 seconds
            await asyncio.sleep(5)
            
            # Stop scanning
            await scanner.stop()
            
            return discovered_devices
        
        # Run the scan
        devices = loop.run_until_complete(scan())
        
        # Close the loop
        loop.close()
        
        print(f"Found {len(devices)} devices")
        
        return jsonify({
            'success': True,
            'devices': devices
        })
        
    except Exception as e:
        print(f"Error in scan_devices: {str(e)}")
        return jsonify({
            'success': False,
            'error': f"BLE scan error: {str(e)}"
        }), 500

if __name__ == '__main__':
    init_db()
    app.run(host='0.0.0.0', port=5000, debug=True)
