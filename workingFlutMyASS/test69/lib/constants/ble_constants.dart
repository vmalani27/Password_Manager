/// BLE UUIDs and constants for ESP32 Password Manager
class BleConstants {
  BleConstants._();
  
  /// Expected device name prefix - devices will be "ESP32-PWD-Manager-XXXX"
  /// where XXXX is last 4 chars of device's Bluetooth MAC address
  static const String deviceNamePrefix = 'ESP32-PWD-Manager';
  
  /// Service UUID - This is advertised by the ESP32
  /// NOTE: This is currently the same across all devices (development only)
  /// TODO Phase 1: Make this device-unique for production
  static const String serviceUuid = '6E400001-B5A3-F393-E0A9-E50E24DCCA9E';
  
  /// Write characteristic UUID (for sending commands to ESP32)
  /// Commands: request_token, auth <token>, add, get, update, delete, list, logout
  static const String rxCharacteristicUuid = '6E400002-B5A3-F393-E0A9-E50E24DCCA9E';
  
  /// Notification characteristic UUID (for receiving responses from ESP32)
  /// ESP32 sends responses like "TOKEN <hex>", "AUTH OK", "Password: xyz", etc.
  static const String notificationCharacteristicUuid = '6E400003-B5A3-F393-E0A9-E50E24DCCA9E';
  
  /// Device Identity characteristic UUID (read-only)
  /// ESP32 exposes device fingerprint for pairing verification
  /// Format: MAC address or device-specific UUID
  static const String deviceIdentityCharacteristicUuid = '6E400004-B5A3-F393-E0A9-E50E24DCCA9E';
  
  /// ECDH characteristic UUID (for key exchange)
  /// Used for reading ESP32's public key and writing client's public key
  static const String ecdhCharacteristicUuid = '6E400005-B5A3-F393-E0A9-E50E24DCCA9E';
  
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
  
  /// Security constants
  static const int minTokenLength = 32; // 128 bits in hex = 32 chars
  static const Duration sessionTimeout = Duration(minutes: 5);
  static const int maxAuthAttempts = 5;
  static const Duration lockoutDuration = Duration(minutes: 1);
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
  
  // New commands for device identity
  static const String getDeviceIdentity = 'get_identity';
  
  // Device pairing commands (ECDH layer only handles pairing, not session auth)
  static const String unpair = 'unpair';
  
  // Expected response prefixes
  static const String tokenPrefix = 'TOKEN:';
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
  static const String logoutResponse = 'LOGOUT';
  static const String unpairFail = 'UNPAIR_FAIL';
  
  // ECDH pairing response prefixes (device binding only, not session auth)
  static const String ecdhOk = 'ECDH_OK';
  static const String ecdhOkPaired = 'ECDH_OK_PAIRED';
  static const String ecdhAlreadyPaired = 'ECDH_ALREADY_PAIRED';
  static const String ecdhFail = 'ECDH_FAIL';
  static const String ecdhInvalid = 'ECDH_INVALID';
  static const String unpaired = 'UNPAIRED';
  
  // Security responses
  static const String passwordReady = 'PW_READY'; // Future: instead of plaintext password
  static const String deviceIdentityPrefix = 'DEVICE_ID: ';
}
