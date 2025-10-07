/// BLE UUIDs and constants for ESP32 Password Manager
class BleConstants {
  BleConstants._(); // Private constructor - this is a static class
  
  /// Expected device name - primary way to identify our device during scan
  static const String expectedDeviceName = 'ESP32-GATT-Manager';
  
  /// Service UUID - This is advertised by the ESP32
  static const String serviceUuid = '6E400001-B5A3-F393-E0A9-E50E24DCCA9E';
  
  /// Write characteristic UUID (for sending commands to ESP32)
  /// Commands: request_token, auth <token>, add, get, update, delete, list, logout
  static const String rxCharacteristicUuid = '6E400002-B5A3-F393-E0A9-E50E24DCCA9E';
  
  /// Notification characteristic UUID (for receiving responses from ESP32)
  /// ESP32 sends responses like "TOKEN <hex>", "AUTH OK", "Password: xyz", etc.
  static const String notificationCharacteristicUuid = '6E400003-B5A3-F393-E0A9-E50E24DCCA9E';
  
  /// BLE connection timeout (increased for bonding)
  static const Duration connectionTimeout = Duration(seconds: 30);
  
  /// Command timeout (how long to wait for ESP32 response)
  static const Duration commandTimeout = Duration(seconds: 10);
  
  /// Preferred MTU size (ESP32 sets 256, we'll request 256)
  static const int preferredMtu = 256;
  
  /// Scan duration
  static const Duration scanDuration = Duration(seconds: 10);
  
  /// Auto-reconnect delay after disconnect
  static const Duration reconnectDelay = Duration(seconds: 2);
}

/// ESP32 command prefixes and response patterns
class Esp32Commands {
  Esp32Commands._();
  
  // Commands to send
  static const String requestToken = 'request_token';
  static String auth(String token) => 'auth $token';
  static String add(String site, String username, String password) => 
      'add $site $username $password';
  static String get(String site, String username) => 'get $site $username';
  static String update(String site, String username, String newPassword) => 
      'update $site $username $newPassword';
  static String delete(String site, String username) => 'delete $site $username';
  static const String list = 'list';
  static const String logout = 'logout';
  
  // Expected response prefixes
  static const String tokenPrefix = 'TOKEN ';
  static const String authOk = 'AUTH OK';
  static const String authFail = 'AUTH FAIL';
  static const String locked = 'LOCKED';
  static const String notAuthorized = 'NOT AUTHORIZED';
  static const String passwordPrefix = 'Password: ';
  static const String listPrefix = 'LIST:\n';
  static const String added = 'Added';
  static const String updated = 'Updated';
  static const String deleted = 'Deleted';
  static const String notFound = 'NOT FOUND';
  static const String addFail = 'ADD FAIL';
  static const String updateFail = 'UPDATE FAIL';
  static const String deleteFail = 'DELETE FAIL';
  static const String invalid = 'INVALID';
}

/*
 * ============================================================================
 * SECURITY & PRODUCTION CONSIDERATIONS FOR BLE UUIDs
 * ============================================================================
 * 
 * CURRENT STATE (Development):
 * - Using hardcoded UUIDs that are the same across all devices
 * - Device identified by name only ("ESP32-GATT-Manager")
 * - No device-specific authentication beyond session tokens
 * 
 * PRODUCTION CONCERNS:
 * 1. DEVICE UNIQUENESS
 *    - Multiple ESP32 devices in proximity will have identical UUIDs
 *    - Flutter app cannot distinguish between different physical devices
 *    - Risk: User connects to wrong password manager
 * 
 * 2. UUID SPOOFING
 *    - Attacker can clone UUIDs and device name
 *    - App will trust any device with matching name/UUIDs
 *    - Risk: Man-in-the-middle attack, credential theft
 * 
 * 3. NO DEVICE BINDING
 *    - App doesn't "remember" its specific physical device
 *    - No way to enforce one-device-per-user pairing
 *    - Risk: User accidentally connects to another user's device
 * 
 * RECOMMENDED SOLUTIONS FOR PRODUCTION:
 * 
 * Option A: Device-Specific UUIDs (Requires ESP32 firmware update)
 *    - Generate unique service UUID per device during first boot
 *    - Store UUID in ESP32 EEPROM/NVS (persistent)
 *    - Display UUID on OLED during pairing (QR code ideal)
 *    - User scans QR code to register device in app
 *    - App stores device UUID + optional friendly name
 *    - Pros: Strong device identity, prevents UUID collision
 *    - Cons: Requires firmware change, more complex pairing flow
 * 
 * Option B: Device Certificate/Public Key (Most Secure)
 *    - Generate unique ECDSA key pair on ESP32 first boot
 *    - Store private key securely, expose public key via BLE characteristic
 *    - App reads public key during pairing, verifies device via signature
 *    - All subsequent commands signed by device using private key
 *    - Pros: Cryptographic device identity, prevents spoofing
 *    - Cons: Requires crypto library on ESP32 (mbedtls), more complex
 * 
 * Option C: MAC Address Binding (Simpler but less secure)
 *    - Use ESP32's Bluetooth MAC address as device ID
 *    - Store MAC in app after first pairing
 *    - Verify MAC on every connection
 *    - Pros: Simple, no firmware change needed
 *    - Cons: MAC can be spoofed, privacy concerns (MAC is public)
 * 
 * Option D: User-Set Pairing Code (Balance of security and UX)
 *    - User sets permanent PIN on ESP32 via physical button sequence
 *    - PIN stored in NVS, required for ALL connections (not just session)
 *    - App stores device name + PIN hash
 *    - Challenge-response during connection (prevent replay attacks)
 *    - Pros: User control, good security with proper crypto
 *    - Cons: User must remember PIN, setup UX complexity
 * 
 * IMMEDIATE TODO (Before Production):
 * [ ] Decide on device identity strategy (recommend Option A or B)
 * [ ] Implement device pairing flow (user explicitly binds to ONE device)
 * [ ] Add device fingerprint display on ESP32 OLED
 * [ ] Store paired device identity in app (SharedPreferences or secure storage)
 * [ ] Add "unpair device" feature in app settings
 * [ ] Implement device verification on every connection
 * [ ] Add visual indicators for "trusted device" vs "unknown device"
 * [ ] Log all pairing/unpairing events in audit log
 * 
 * ENCRYPTION TODO (Step 8 in implementation plan):
 * [ ] Implement ECDH key exchange for session encryption
 * [ ] Encrypt all credential data in transit (AES-GCM)
 * [ ] Add message authentication (HMAC) to prevent tampering
 * [ ] Implement perfect forward secrecy (new session key per connection)
 * 
 * ============================================================================
 */

