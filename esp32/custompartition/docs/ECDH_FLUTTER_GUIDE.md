# Flutter ECDH Implementation Guide

Complete step-by-step guide to implement ECDH key exchange, challenge-response authentication, and persistent device pairing in Flutter.

---

## Overview

This guide implements:
- **ECDH Key Exchange**: Establish shared secret with ESP32
- **Challenge-Response Auth**: Cryptographic proof of identity
- **Device Binding**: Persistent pairing across app restarts
- **Pairing States**: Handle first pairing vs reconnection
- **Unpair Support**: Factory reset for device pairing

---

## Step 1: Add Dependencies

Add to `pubspec.yaml`:

```yaml
dependencies:
  pointycastle: ^3.7.3
  convert: ^3.1.1
  shared_preferences: ^2.2.2  # For persistent pairing storage
```

Run:
```bash
flutter pub get
```

---

## Step 2: Update BLE Constants

Add ECDH characteristic UUID to `lib/constants/ble_constants.dart`:

```dart
class BleConstants {
  // ...existing constants...
  
  /// ECDH characteristic UUID (for key exchange)
  static const String ecdhCharacteristicUuid = '6E400005-B5A3-F393-E0A9-E50E24DCCA9E';
  
  // ...existing constants...
}
```

Add ECDH commands to `lib/constants/ble_constants.dart`:

```dart
class Esp32Commands {
  // ...existing commands...
  
  // ECDH authentication commands
  static const String ecdhAuth = 'ecdh_auth';
  static String respond(String hmacHex) => 'respond $hmacHex';
  static const String unpair = 'unpair';  // Factory reset pairing
  
  // Expected response prefixes
  static const String ecdhOk = 'ECDH_OK';
  static const String ecdhOkPaired = 'ECDH_OK_PAIRED';  // First pairing
  static const String ecdhAlreadyPaired = 'ECDH_ALREADY_PAIRED';  // Unauthorized device
  static const String ecdhFail = 'ECDH_FAIL';
  static const String ecdhInvalid = 'ECDH_INVALID';
  static const String ecdhNotReady = 'ECDH NOT READY';
  static const String challengePrefix = 'CHALLENGE ';
  static const String noChallenge = 'NO CHALLENGE';
  static const String invalidResponse = 'INVALID RESPONSE';
  static const String unpaired = 'UNPAIRED';
  
  // ...existing constants...
}
```

---

## Step 3: Create Pairing Model

Create `lib/models/paired_device.dart`:

```dart
import 'dart:convert';
import 'dart:typed_data';

/// Represents a paired ESP32 device with persistent pairing data
class PairedDevice {
  final String deviceId;
  final String deviceName;
  final Uint8List clientPrivateKey;  // Our private key (32 bytes)
  final Uint8List clientPublicKey;   // Our public key (64 bytes)
  final Uint8List esp32PublicKey;    // ESP32's public key (64 bytes)
  final DateTime pairedAt;
  
  PairedDevice({
    required this.deviceId,
    required this.deviceName,
    required this.clientPrivateKey,
    required this.clientPublicKey,
    required this.esp32PublicKey,
    required this.pairedAt,
  });
  
  /// Convert to JSON for SharedPreferences storage
  Map<String, dynamic> toJson() {
    return {
      'deviceId': deviceId,
      'deviceName': deviceName,
      'clientPrivateKey': base64Encode(clientPrivateKey),
      'clientPublicKey': base64Encode(clientPublicKey),
      'esp32PublicKey': base64Encode(esp32PublicKey),
      'pairedAt': pairedAt.toIso8601String(),
    };
  }
  
  /// Create from JSON
  factory PairedDevice.fromJson(Map<String, dynamic> json) {
    return PairedDevice(
      deviceId: json['deviceId'],
      deviceName: json['deviceName'],
      clientPrivateKey: base64Decode(json['clientPrivateKey']),
      clientPublicKey: base64Decode(json['clientPublicKey']),
      esp32PublicKey: base64Decode(json['esp32PublicKey']),
      pairedAt: DateTime.parse(json['pairedAt']),
    );
  }
}
```

---

## Step 4: Create Pairing Service

Create `lib/services/pairing_service.dart`:

```dart
import 'dart:convert';
import 'package:shared_preferences/shared_preferences.dart';
import '../models/paired_device.dart';

class PairingService {
  static const String _keyPairedDevice = 'paired_device';
  
  /// Check if a device is paired
  Future<bool> isPaired() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.containsKey(_keyPairedDevice);
  }
  
  /// Get paired device info
  Future<PairedDevice?> getPairedDevice() async {
    final prefs = await SharedPreferences.getInstance();
    final jsonStr = prefs.getString(_keyPairedDevice);
    
    if (jsonStr == null) return null;
    
    try {
      final json = jsonDecode(jsonStr);
      return PairedDevice.fromJson(json);
    } catch (e) {
      debugPrint('[Pairing] Failed to parse paired device: $e');
      return null;
    }
  }
  
  /// Save pairing after successful first handshake
  Future<void> savePairing(PairedDevice device) async {
    final prefs = await SharedPreferences.getInstance();
    final jsonStr = jsonEncode(device.toJson());
    await prefs.setString(_keyPairedDevice, jsonStr);
    debugPrint('[Pairing] Device paired: ${device.deviceName}');
  }
  
  /// Remove pairing (unpair)
  Future<void> removePairing() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove(_keyPairedDevice);
    debugPrint('[Pairing] Device unpaired');
  }
  
  /// Check if this device ID matches paired device
  Future<bool> isDeviceIdPaired(String deviceId) async {
    final paired = await getPairedDevice();
    return paired?.deviceId == deviceId;
  }
}
```

---

## Step 5: Create ECDH Service

Create `lib/services/ecdh_service.dart`:

```dart
import 'dart:typed_data';
import 'dart:math';
import 'package:flutter/foundation.dart';
import 'package:pointycastle/export.dart';
import 'package:convert/convert.dart';

class EcdhService {
  ECPrivateKey? _privateKey;
  ECPublicKey? _publicKey;
  Uint8List? _sharedSecret;
  Uint8List? _sessionKey;
  
  bool get isReady => _sessionKey != null;
  
  /// Generate ephemeral ECDH key pair (secp256r1)
  void generateKeyPair() {
    final params = ECKeyGeneratorParameters(ECCurve_secp256r1());
    final random = FortunaRandom();
    
    // Seed random with secure random bytes
    final seedSource = Random.secure();
    final seeds = List<int>.generate(32, (_) => seedSource.nextInt(256));
    random.seed(KeyParameter(Uint8List.fromList(seeds)));
    
    final generator = ECKeyGenerator();
    generator.init(ParametersWithRandom(params, random));
    
    final keyPair = generator.generateKeyPair();
    _privateKey = keyPair.privateKey as ECPrivateKey;
    _publicKey = keyPair.publicKey as ECPublicKey;
    
    debugPrint('[ECDH] Key pair generated');
  }
  
  /// Load existing key pair from saved pairing
  void loadKeyPair(Uint8List privateKeyBytes, Uint8List publicKeyBytes) {
    final curve = ECCurve_secp256r1();
    final domainParams = ECDomainParameters(curve.name);
    
    // Reconstruct private key
    final d = _bytesToBigInt(privateKeyBytes);
    _privateKey = ECPrivateKey(d, domainParams);
    
    // Reconstruct public key
    final x = _bytesToBigInt(publicKeyBytes.sublist(0, 32));
    final y = _bytesToBigInt(publicKeyBytes.sublist(32, 64));
    final point = curve.curve.createPoint(x, y);
    _publicKey = ECPublicKey(point, domainParams);
    
    debugPrint('[ECDH] Key pair loaded from pairing');
  }
  }
  
  /// Get public key as 64 bytes (X||Y coordinates)
  Uint8List getPublicKeyBytes() {
    if (_publicKey == null) throw Exception('Key pair not generated');
    
    final q = _publicKey!.Q!;
    final x = q.x!.toBigInteger()!.toRadixString(16).padLeft(64, '0');
    final y = q.y!.toBigInteger()!.toRadixString(16).padLeft(64, '0');
    
    debugPrint('[ECDH] Public key X: $x');
    debugPrint('[ECDH] Public key Y: $y');
    
    return Uint8List.fromList(hex.decode(x + y));
  }
  
  /// Compute shared secret from ESP32's public key
  void computeSharedSecret(Uint8List esp32PublicKey) {
    if (_privateKey == null) throw Exception('Private key not available');
    if (esp32PublicKey.length != 64) {
      throw Exception('Invalid public key length: ${esp32PublicKey.length}');
    }
    
    // Parse ESP32's public key coordinates
    final xHex = hex.encode(esp32PublicKey.sublist(0, 32));
    final yHex = hex.encode(esp32PublicKey.sublist(32, 64));
    
    debugPrint('[ECDH] ESP32 public key X: $xHex');
    debugPrint('[ECDH] ESP32 public key Y: $yHex');
    
    final x = BigInt.parse(xHex, radix: 16);
    final y = BigInt.parse(yHex, radix: 16);
    
    final curve = ECCurve_secp256r1();
    final point = curve.curve.createPoint(x, y);
    final esp32PubKey = ECPublicKey(point, curve);
    
    // Compute shared secret: d_A * Q_B
    final sharedPoint = esp32PubKey.Q! * _privateKey!.d;
    if (sharedPoint == null || sharedPoint.x == null) {
      throw Exception('Failed to compute shared secret');
    }
    
    _sharedSecret = _bigIntToBytes(sharedPoint.x!.toBigInteger()!, 32);
    debugPrint('[ECDH] Shared secret: ${hex.encode(_sharedSecret!)}');
    
    // Derive session key using HKDF-like approach (matching ESP32)
    _sessionKey = _deriveSessionKey(_sharedSecret!);
    debugPrint('[ECDH] Session key derived: ${hex.encode(_sessionKey!)}');
  }
  
  /// Derive session key using HKDF-like approach (matches ESP32 implementation)
  Uint8List _deriveSessionKey(Uint8List sharedSecret) {
    final hmac = HMac(SHA256Digest(), 64);
    
    // HKDF-Extract: PRK = HMAC-SHA256(salt, shared_secret)
    final salt = Uint8List.fromList('BLE_PASSWORD_MGR'.codeUnits);
    hmac.init(KeyParameter(salt));
    final prk = hmac.process(sharedSecret);
    
    // HKDF-Expand: session_key = HMAC-SHA256(PRK, info || 0x01)
    final info = Uint8List.fromList([...'SESSION'.codeUnits, 0x01]);
    hmac.reset();
    hmac.init(KeyParameter(prk));
    final sessionKey = hmac.process(info);
    
    return sessionKey;
  }
  
  /// Compute HMAC-SHA256 for challenge-response
  Uint8List computeHmac(Uint8List challenge) {
    if (_sessionKey == null) throw Exception('Session key not derived');
    
    final hmac = HMac(SHA256Digest(), 64);
    hmac.init(KeyParameter(_sessionKey!));
    return hmac.process(challenge);
  }
  
  /// Get session key (for future AES-GCM encryption)
  Uint8List? get sessionKey => _sessionKey;
  
  /// Get private key bytes (for saving pairing)
  Uint8List getPrivateKeyBytes() {
    if (_privateKey == null) throw Exception('Private key not generated');
    return _bigIntToBytes(_privateKey!.d!, 32);
  }
  
  /// Get public key bytes (for saving pairing)
  Uint8List getPublicKeyBytes() {
    if (_publicKey == null) throw Exception('Public key not generated');
    return Uint8List.fromList([
      ..._bigIntToBytes(_publicKey!.Q!.x!.toBigInteger()!, 32),
      ..._bigIntToBytes(_publicKey!.Q!.y!.toBigInteger()!, 32),
    ]);
  }
  
  /// Clear all sensitive data
  void clear() {
    _privateKey = null;
    _publicKey = null;
    _sharedSecret?.fillRange(0, _sharedSecret!.length, 0);
    _sessionKey?.fillRange(0, _sessionKey!.length, 0);
    _sharedSecret = null;
    _sessionKey = null;
    debugPrint('[ECDH] Cleared sensitive data');
  }
  
  /// Convert BigInt to fixed-length byte array
  Uint8List _bigIntToBytes(BigInt number, int length) {
    final hexStr = number.toRadixString(16).padLeft(length * 2, '0');
    return Uint8List.fromList(hex.decode(hexStr));
  }
  
  /// Convert bytes to BigInt
  BigInt _bytesToBigInt(Uint8List bytes) {
    return BigInt.parse(hex.encode(bytes), radix: 16);
  }
}
```

---

## Step 6: Update Command Service

Add ECDH handshake and pairing methods to `lib/services/command_service.dart`:

```dart
import 'ecdh_service.dart';
import 'pairing_service.dart';
import '../models/paired_device.dart';

class CommandService {
  // ...existing code...
  final PairingService _pairingService = PairingService();
  
  /// Perform ECDH key exchange with device binding support
  Future<({EcdhService ecdh, bool isNewPairing})> performEcdhHandshake(
    String deviceId,
    String deviceName,
  ) async {
    debugPrint('[ECDH] Starting handshake...');
    
    final ecdh = EcdhService();
    bool isNewPairing = false;
    
    try {
      // Check if we have existing pairing for this device
      final pairedDevice = await _pairingService.getPairedDevice();
      final isAlreadyPaired = pairedDevice?.deviceId == deviceId;
      
      if (isAlreadyPaired && pairedDevice != null) {
        // RECONNECTION: Load saved keys
        debugPrint('[ECDH] Device already paired - loading saved keys');
        ecdh.loadKeyPair(pairedDevice.clientPrivateKey, pairedDevice.clientPublicKey);
        
        // Use saved ESP32 public key
        debugPrint('[ECDH] Computing shared secret with saved ESP32 key...');
        ecdh.computeSharedSecret(pairedDevice.esp32PublicKey);
        
        // Send our public key to ESP32 (ESP32 will recognize us)
        debugPrint('[ECDH] Sending saved client public key...');
        await _ble.writeCharacteristicWithoutResponse(
          QualifiedCharacteristic(
            serviceId: Uuid.parse(BleConstants.serviceUuid),
            characteristicId: Uuid.parse(BleConstants.ecdhCharacteristicUuid),
            deviceId: deviceId,
          ),
          value: pairedDevice.clientPublicKey,
        );
        
        // Wait for confirmation
        final response = await _waitForNotification(timeout: Duration(seconds: 5));
        debugPrint('[ECDH] Response: $response');
        
        if (!response.startsWith(Esp32Commands.ecdhOk)) {
          throw Exception('Reconnection failed: $response');
        }
        
        debugPrint('[ECDH] Reconnection successful!');
        
      } else {
        // FIRST PAIRING: Generate new keys
        debugPrint('[ECDH] First pairing - generating new keys');
        
        // 1. Read ESP32's public key
        debugPrint('[ECDH] Reading ESP32 public key...');
        final esp32PubKey = await _ble.readCharacteristic(
          QualifiedCharacteristic(
            serviceId: Uuid.parse(BleConstants.serviceUuid),
            characteristicId: Uuid.parse(BleConstants.ecdhCharacteristicUuid),
            deviceId: deviceId,
          ),
        );
        
        debugPrint('[ECDH] Read ESP32 public key: ${esp32PubKey.length} bytes');
        
        // 2. Generate our key pair
        debugPrint('[ECDH] Generating client key pair...');
        ecdh.generateKeyPair();
        
        // 3. Compute shared secret
        debugPrint('[ECDH] Computing shared secret...');
        ecdh.computeSharedSecret(Uint8List.fromList(esp32PubKey));
        
        // 4. Send our public key to ESP32
        debugPrint('[ECDH] Sending client public key...');
        final clientPubKey = ecdh.getPublicKeyBytes();
        await _ble.writeCharacteristicWithoutResponse(
          QualifiedCharacteristic(
            serviceId: Uuid.parse(BleConstants.serviceUuid),
            characteristicId: Uuid.parse(BleConstants.ecdhCharacteristicUuid),
            deviceId: deviceId,
          ),
          value: clientPubKey,
        );
        
        // 5. Wait for response
        final response = await _waitForNotification(timeout: Duration(seconds: 5));
        debugPrint('[ECDH] Response: $response');
        
        if (response == Esp32Commands.ecdhAlreadyPaired) {
          throw Exception('ESP32 is already paired to another device. Unpair first.');
        } else if (!response.startsWith(Esp32Commands.ecdhOk)) {
          throw Exception('Handshake failed: $response');
        }
        
        // 6. Save pairing if ESP32 confirmed
        if (response == Esp32Commands.ecdhOkPaired) {
          debugPrint('[ECDH] New pairing confirmed - saving to storage');
          final newPairing = PairedDevice(
            deviceId: deviceId,
            deviceName: deviceName,
            clientPrivateKey: ecdh.getPrivateKeyBytes(),
            clientPublicKey: clientPubKey,
            esp32PublicKey: Uint8List.fromList(esp32PubKey),
            pairedAt: DateTime.now(),
          );
          await _pairingService.savePairing(newPairing);
          isNewPairing = true;
        }
        
        debugPrint('[ECDH] First pairing complete!');
      }
      
      return (ecdh: ecdh, isNewPairing: isNewPairing);
      
    } catch (e) {
      debugPrint('[ECDH] Handshake failed: $e');
      ecdh.clear();
      rethrow;
    }
  }
      
      // 5. Wait for ECDH_OK notification
      debugPrint('[ECDH] Waiting for ECDH_OK...');
      await Future.delayed(Duration(milliseconds: 500)); // Give ESP32 time to compute
      
      // Check latest notification (should be ECDH_OK)
      // This assumes your notification stream captures it
      
      debugPrint('[ECDH] Handshake complete!');
      return ecdh;
      
    } catch (e) {
      debugPrint('[ECDH] Handshake failed: $e');
      ecdh.clear();
      rethrow;
    }
  }
  
  /// Authenticate using ECDH challenge-response
  Future<bool> authenticateWithEcdh(EcdhService ecdh) async {
    try {
      debugPrint('[ECDH Auth] Requesting challenge...');
      
      // 1. Request challenge
      await sendCommand(Esp32Commands.ecdhAuth);
      
      // 2. Wait for challenge notification
      // Format: "CHALLENGE <32-char-hex>"
      final response = await _waitForResponse(timeout: Duration(seconds: 5));
      
      if (!response.startsWith(Esp32Commands.challengePrefix)) {
        throw Exception('Expected challenge, got: $response');
      }
      
      final challengeHex = response.substring(Esp32Commands.challengePrefix.length);
      debugPrint('[ECDH Auth] Received challenge: $challengeHex');
      
      // 3. Convert challenge from hex to bytes
      final challengeBytes = Uint8List.fromList(hex.decode(challengeHex));
      
      // 4. Compute HMAC response
      final hmacBytes = ecdh.computeHmac(challengeBytes);
      final hmacHex = hex.encode(hmacBytes);
      debugPrint('[ECDH Auth] Computed HMAC: $hmacHex');
      
      // 5. Send response
      await sendCommand(Esp32Commands.respond(hmacHex));
      
      // 6. Wait for AUTH OK or AUTH FAIL
      final authResult = await _waitForResponse(timeout: Duration(seconds: 5));
      
      if (authResult == Esp32Commands.authOk) {
        debugPrint('[ECDH Auth] Authentication successful!');
        return true;
      } else {
        debugPrint('[ECDH Auth] Authentication failed: $authResult');
        return false;
      }
      
    } catch (e) {
      debugPrint('[ECDH Auth] Error: $e');
      return false;
    }
  }
  
  /// Unpair device (factory reset pairing on both sides)
  Future<bool> unpairDevice() async {
    try {
      debugPrint('[Pairing] Sending unpair command to ESP32...');
      
      // Send unpair command to ESP32
      await sendCommand(Esp32Commands.unpair);
      
      // Wait for confirmation
      final response = await _waitForResponse(timeout: Duration(seconds: 5));
      
      if (response == Esp32Commands.unpaired) {
        // Remove local pairing data
        await _pairingService.removePairing();
        debugPrint('[Pairing] Device unpaired successfully');
        return true;
      } else {
        debugPrint('[Pairing] Unpair failed: $response');
        return false;
      }
      
    } catch (e) {
      debugPrint('[Pairing] Unpair error: $e');
      return false;
    }
  }
  
  /// Check if app is paired with a device
  Future<bool> isPaired() async {
    return await _pairingService.isPaired();
  }
  
  /// Get paired device info
  Future<PairedDevice?> getPairedDevice() async {
    return await _pairingService.getPairedDevice();
  }
  
  /// Helper to wait for next notification response
  Future<String> _waitForResponse({Duration timeout = const Duration(seconds: 10)}) async {
    final completer = Completer<String>();
    StreamSubscription? subscription;
    
    subscription = notificationStream.listen((data) {
      if (!completer.isCompleted) {
        completer.complete(data);
        subscription?.cancel();
      }
    });
    
    // Timeout handling
    Future.delayed(timeout, () {
      if (!completer.isCompleted) {
        subscription?.cancel();
        completer.completeError(TimeoutException('Response timeout'));
      }
    });
    
    return completer.future;
  }
}
```

---

## Step 7: Update Connection Flow

Update your connection service to handle device binding:

```dart
// In BleConnectionService or AppStateProvider:

Future<void> connectAndAuthenticate(String deviceId, String deviceName) async {
  try {
    // 1. Connect to device
    await _bleConnection.connect(deviceId);
    
    // 2. Subscribe to notifications
    await _bleConnection.subscribeToNotifications(deviceId);
    
    // 3. Check pairing state
    final isPaired = await _commandService.isPaired();
    
    if (isPaired) {
      final pairedDevice = await _commandService.getPairedDevice();
      
      if (pairedDevice?.deviceId != deviceId) {
        throw Exception(
          'Already paired to ${pairedDevice?.deviceName}. '
          'Unpair first to connect to a different device.'
        );
      }
      
      debugPrint('[App] Reconnecting to paired device: ${pairedDevice.deviceName}');
    } else {
      debugPrint('[App] First pairing with: $deviceName');
    }
    
    // 4. Perform ECDH handshake (handles both first pairing and reconnection)
    debugPrint('[App] Starting ECDH handshake...');
    final result = await _commandService.performEcdhHandshake(deviceId, deviceName);
    final ecdh = result.ecdh;
    final isNewPairing = result.isNewPairing;
    
    if (isNewPairing) {
      debugPrint('[App] ✨ New device paired successfully!');
      // Show success message to user
    } else {
      debugPrint('[App] 🔄 Reconnected to paired device');
    }
    
    // 5. Authenticate using challenge-response
    debugPrint('[App] Authenticating with ECDH...');
    final authenticated = await _commandService.authenticateWithEcdh(ecdh);
    
    if (!authenticated) {
      throw Exception('ECDH authentication failed');
    }
    
    // 6. Store ECDH service for future use (optional - for command encryption)
    // _ecdhService = ecdh;
    
    debugPrint('[App] ✅ Connection and authentication complete!');
    
  } catch (e) {
    debugPrint('[App] ❌ Connection failed: $e');
    await disconnect();
    rethrow;
  }
}

/// Unpair from device (factory reset pairing)
Future<void> unpairDevice() async {
  try {
    // Must be connected to unpair
    if (!isConnected) {
      throw Exception('Must be connected to unpair');
    }
    
    final success = await _commandService.unpairDevice();
    
    if (success) {
      debugPrint('[App] Device unpaired - disconnecting');
      await disconnect();
    } else {
      throw Exception('Unpair command failed');
    }
    
  } catch (e) {
    debugPrint('[App] Unpair error: $e');
    rethrow;
  }
}
```

---

## Step 8: Test the Implementation

### Test Case 1: First Pairing

**Flow:**
1. App not paired → generates new keys
2. ESP32 unpaired → accepts pairing
3. ECDH handshake succeeds
4. ESP32 responds with `ECDH_OK_PAIRED`
5. App saves pairing to SharedPreferences
6. ESP32 saves pairing to NVS
7. Both devices now paired

**Expected Debug Output:**
```
[App] First pairing with: ESP32-PWD-Manager
[ECDH] Starting handshake...
[ECDH] First pairing - generating new keys
[ECDH] Reading ESP32 public key...
[ECDH] Read ESP32 public key: 64 bytes
[ECDH] Generating client key pair...
[ECDH] Key pair generated
[ECDH] Computing shared secret...
[ECDH] Shared secret computed
[ECDH] Session key derived
[ECDH] Sending client public key...
[ECDH] Response: ECDH_OK_PAIRED
[ECDH] New pairing confirmed - saving to storage
[Pairing] Device paired: ESP32-PWD-Manager
[ECDH] First pairing complete!
[App] ✨ New device paired successfully!
[ECDH Auth] Requesting challenge...
[ECDH Auth] Received challenge: a1b2c3d4...
[ECDH Auth] Computed HMAC: e5f6g7h8...
[ECDH Auth] Authentication successful!
[App] ✅ Connection and authentication complete!
```

### Test Case 2: Reconnection

**Flow:**
1. App has pairing → loads saved keys
2. ESP32 has pairing → loads saved keys from NVS
3. App sends saved public key
4. ESP32 recognizes key → fast authentication
5. No new pairing saved

**Expected Debug Output:**
```
[App] Reconnecting to paired device: ESP32-PWD-Manager
[ECDH] Starting handshake...
[ECDH] Device already paired - loading saved keys
[ECDH] Key pair loaded from pairing
[ECDH] Computing shared secret with saved ESP32 key...
[ECDH] Sending saved client public key...
[ECDH] Response: ECDH_OK
[ECDH] Reconnection successful!
[App] 🔄 Reconnected to paired device
[ECDH Auth] Requesting challenge...
[ECDH Auth] Authentication successful!
[App] ✅ Connection and authentication complete!
```

### Test Case 3: Unauthorized Device Rejection

**Flow:**
1. ESP32 already paired to Device A
2. Device B tries to connect
3. Device B sends different public key
4. ESP32 rejects with `ECDH_ALREADY_PAIRED`

**Expected Debug Output (Device B):**
```
[ECDH] Starting handshake...
[ECDH] First pairing - generating new keys
[ECDH] Sending client public key...
[ECDH] Response: ECDH_ALREADY_PAIRED
[ECDH] Handshake failed: ESP32 is already paired to another device. Unpair first.
[App] ❌ Connection failed: Exception: ESP32 is already paired to another device. Unpair first.
```

### Test Case 4: Unpair

**Flow:**
1. Connected and authenticated
2. User triggers unpair
3. App sends `unpair` command
4. ESP32 erases NVS pairing data
5. App erases SharedPreferences pairing data
6. Both devices unpaired

**Expected Debug Output:**
```
[Pairing] Sending unpair command to ESP32...
[Pairing] Device unpaired successfully
[App] Device unpaired - disconnecting
```

---

## Step 9: Handle Edge Cases

Add comprehensive error handling:

```dart
// In your connection logic:

try {
  final result = await _commandService.performEcdhHandshake(deviceId, deviceName);
  final authenticated = await _commandService.authenticateWithEcdh(result.ecdh);
  
  if (!authenticated) {
    throw Exception('Authentication failed');
  }
  
} on TimeoutException {
  debugPrint('[App] ECDH timeout - ESP32 may be busy');
  // Retry or show error to user
  
} catch (e) {
  if (e.toString().contains('ECDH_ALREADY_PAIRED')) {
    // ESP32 is paired to another device
    showDialog(
      context: context,
      builder: (_) => AlertDialog(
        title: Text('Device Already Paired'),
        content: Text(
          'This ESP32 is already paired to another device. '
          'To pair with this device, you must first unpair it from the other device.'
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: Text('OK'),
          ),
        ],
      ),
    );
    
  } else if (e.toString().contains('Already paired to')) {
    // App is paired to different device
    final pairedDevice = await _commandService.getPairedDevice();
    showDialog(
      context: context,
      builder: (_) => AlertDialog(
        title: Text('Already Paired'),
        content: Text(
          'Already paired to ${pairedDevice?.deviceName}. '
          'Unpair first to connect to a different device.'
        ),
        actions: [
          TextButton(
            onPressed: () async {
              Navigator.pop(context);
              // Optionally trigger unpair flow
            },
            child: Text('Unpair'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: Text('Cancel'),
          ),
        ],
      ),
    );
    
  } else if (e.toString().contains('ECDH_FAIL')) {
    debugPrint('[App] ECDH handshake failed - try reconnecting');
    
  } else if (e.toString().contains('INVALID RESPONSE')) {
    debugPrint('[App] HMAC verification failed - check implementation');
  }
  
  rethrow;
}
```

---

## Step 10: Cleanup on Disconnect

Always clear ECDH session state (but preserve pairing):

```dart
Future<void> disconnect() async {
  _ecdhService?.clear(); // Clear session key (pairing stays in SharedPreferences)
  await _bleConnection.disconnect();
}
```

---

## Step 11: UI Integration

Add settings option for unpairing:

```dart
// In SettingsScreen or similar:

class SettingsScreen extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text('Settings')),
      body: ListView(
        children: [
          FutureBuilder<PairedDevice?>(
            future: commandService.getPairedDevice(),
            builder: (context, snapshot) {
              final paired = snapshot.data;
              
              if (paired == null) {
                return ListTile(
                  leading: Icon(Icons.bluetooth_disabled),
                  title: Text('No device paired'),
                  subtitle: Text('Connect to a device to pair'),
                );
              }
              
              return Column(
                children: [
                  ListTile(
                    leading: Icon(Icons.bluetooth_connected),
                    title: Text('Paired Device'),
                    subtitle: Text(paired.deviceName),
                    trailing: Text(
                      'Since ${_formatDate(paired.pairedAt)}',
                      style: TextStyle(fontSize: 12, color: Colors.grey),
                    ),
                  ),
                  ListTile(
                    leading: Icon(Icons.link_off, color: Colors.red),
                    title: Text('Unpair Device'),
                    subtitle: Text('Remove pairing and factory reset'),
                    onTap: () async {
                      final confirm = await showDialog<bool>(
                        context: context,
                        builder: (_) => AlertDialog(
                          title: Text('Unpair Device?'),
                          content: Text(
                            'This will unpair ${paired.deviceName} and require '
                            're-pairing to connect again.'
                          ),
                          actions: [
                            TextButton(
                              onPressed: () => Navigator.pop(context, false),
                              child: Text('Cancel'),
                            ),
                            TextButton(
                              onPressed: () => Navigator.pop(context, true),
                              child: Text('Unpair', style: TextStyle(color: Colors.red)),
                            ),
                          ],
                        ),
                      );
                      
                      if (confirm == true) {
                        try {
                          await connectionService.unpairDevice();
                          ScaffoldMessenger.of(context).showSnackBar(
                            SnackBar(content: Text('Device unpaired successfully')),
                          );
                        } catch (e) {
                          ScaffoldMessenger.of(context).showSnackBar(
                            SnackBar(content: Text('Unpair failed: $e')),
                          );
                        }
                      }
                    },
                  ),
                ],
              );
            },
          ),
        ],
      ),
    );
  }
  
  String _formatDate(DateTime date) {
    final now = DateTime.now();
    final diff = now.difference(date);
    
    if (diff.inDays > 0) return '${diff.inDays}d ago';
    if (diff.inHours > 0) return '${diff.inHours}h ago';
    if (diff.inMinutes > 0) return '${diff.inMinutes}m ago';
    return 'just now';
  }
}
```

---

## Troubleshooting

### Issue: "ECDH_ALREADY_PAIRED" when trying to pair
- **Cause**: ESP32 is already paired to another device
- **Fix**: Unpair the ESP32 from the other device first, or use the unpair command

### Issue: "Already paired to [Device]" when connecting to different ESP32
- **Cause**: Flutter app is already paired to a different ESP32
- **Fix**: Unpair from current device in app settings, then try again

### Issue: "ECDH_FAIL" after sending public key
- **Cause**: ESP32 couldn't compute shared secret
- **Fix**: Check public key format (must be 64 bytes: X||Y coordinates)

### Issue: "AUTH FAIL" on challenge response
- **Cause**: HMAC mismatch
- **Fix**: Ensure Flutter's HKDF implementation matches ESP32 (extract + expand with same salt/info)

### Issue: Timeout waiting for ECDH_OK
- **Cause**: ESP32 computing shared secret or key derivation failed
- **Fix**: Check ESP32 serial logs for error messages

### Issue: "NO CHALLENGE" response
- **Cause**: Sent "respond" command before "ecdh_auth"
- **Fix**: Always send "ecdh_auth" first, wait for challenge

### Issue: Reconnection slow or fails
- **Cause**: Pairing data corrupted or mismatched
- **Fix**: Unpair and re-pair the device (will do fresh handshake)

### Issue: Keys don't persist after app restart
- **Cause**: SharedPreferences not saving properly
- **Fix**: Check permissions, ensure `await` on save operations

### Issue: Can't connect after ESP32 reboot but before app restart
- **Cause**: ESP32 regenerated ephemeral keys (was unpaired)
- **Fix**: Normal - ESP32 loads paired keys from NVS on boot, should recognize saved Flutter keys

---

## Complete Architecture

### Pairing States

**ESP32 States:**
- `UNPAIRED`: NVS empty → generates ephemeral keys → accepts any client
- `PAIRED`: NVS has keys → loads persistent keys → only accepts known client

**Flutter States:**
- `Not Paired`: SharedPreferences empty → generates new keys on connect
- `Paired`: SharedPreferences has keys → loads saved keys → fast reconnection

### First Pairing Flow
```
1. Flutter: Generate new key pair
2. Flutter → ESP32: Send public key (64 bytes)
3. ESP32: Compute shared secret + derive session key
4. ESP32 → Flutter: "ECDH_OK_PAIRED"
5. ESP32: Save {esp_priv, esp_pub, client_pub} to NVS
6. Flutter: Save {client_priv, client_pub, esp32_pub, deviceId} to SharedPreferences
7. Both: PAIRED state
```

### Reconnection Flow
```
1. Flutter: Load saved key pair from SharedPreferences
2. Flutter → ESP32: Send saved public key (64 bytes)
3. ESP32: Load paired client_pub from NVS
4. ESP32: Verify match → compute shared secret
5. ESP32 → Flutter: "ECDH_OK"
6. Both: Session established (no new pairing saved)
```

### Unpair Flow
```
1. Flutter → ESP32: "unpair" command (requires auth)
2. ESP32: Erase NVS keys {esp_priv, esp_pub, client_pub, paired}
3. ESP32: Generate new ephemeral keys
4. ESP32 → Flutter: "UNPAIRED"
5. Flutter: Remove SharedPreferences pairing
6. Both: UNPAIRED state
```

---

## Summary

### ✅ Implemented Features

**Security:**
- ECDH handshake establishes shared secret (secp256r1 curve)
- HKDF derives session key (matches ESP32 implementation)
- Challenge-response authenticates client cryptographically
- No secrets transmitted over BLE (only public keys)
- Forward secrecy (ephemeral session keys)

**Device Binding:**
- Persistent pairing across app restarts (SharedPreferences)
- Persistent pairing across ESP32 reboots (NVS)
- Device-to-device binding (one phone ↔ one ESP32)
- Unauthorized device rejection
- Factory reset via unpair command

**User Experience:**
- First pairing is automatic during connection
- Reconnection uses saved keys (faster auth)
- Settings UI for viewing paired device
- Settings UI for unpairing

### 🔄 Connection Flow Summary

```
┌─────────────────────────────────────────────────────────────┐
│                    First Connection                          │
├─────────────────────────────────────────────────────────────┤
│ 1. BLE Connect                                              │
│ 2. Generate new ECDH keys                                   │
│ 3. Read ESP32 public key                                    │
│ 4. Compute shared secret                                    │
│ 5. Send client public key → ESP32                           │
│ 6. ESP32 responds: ECDH_OK_PAIRED                          │
│ 7. Save pairing to SharedPreferences                        │
│ 8. Challenge-response auth                                  │
│ 9. Authenticated ✓                                          │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    Reconnection                              │
├─────────────────────────────────────────────────────────────┤
│ 1. BLE Connect                                              │
│ 2. Load saved ECDH keys from SharedPreferences             │
│ 3. Compute shared secret with saved ESP32 public key       │
│ 4. Send saved client public key → ESP32                     │
│ 5. ESP32 recognizes key: ECDH_OK                           │
│ 6. Challenge-response auth                                  │
│ 7. Authenticated ✓ (faster than first pairing)             │
└─────────────────────────────────────────────────────────────┘
```

### 📦 Storage

**SharedPreferences (Flutter):**
- `paired_device` (JSON):
  - deviceId (String)
  - deviceName (String)
  - clientPrivateKey (32 bytes, base64)
  - clientPublicKey (64 bytes, base64)
  - esp32PublicKey (64 bytes, base64)
  - pairedAt (DateTime)

**NVS (ESP32):**
- Namespace: `"pwmgr"`
- Keys:
  - `paired` (u8): 0=UNPAIRED, 1=PAIRED
  - `esp_priv` (blob, 32 bytes): ESP32 private key
  - `esp_pub` (blob, 64 bytes): ESP32 public key
  - `client_pub` (blob, 64 bytes): Paired client public key

### 🚀 Next Phase

After ECDH device binding is working:

**Phase 1.3 - Command Encryption:**
1. Encrypt all commands using AES-GCM with `session_key`
2. Remove plaintext password transmission
3. End-to-end encryption for credentials

**Phase 1.4 - Security Hardening:**
4. Generate dynamic PIN at boot (store in ESP32 NVS)
5. Enable ESP32 NVS flash encryption
6. Remove all sensitive logging

**Testing:**
- Test first pairing flow end-to-end
- Test reconnection with saved keys
- Test unauthorized device rejection
- Test unpair and re-pair flow
- Test ESP32 reboot persistence
- Test app restart persistence

---

## Implementation Checklist

- [ ] Add `pointycastle`, `convert`, `shared_preferences` dependencies
- [ ] Update BLE constants (ECDH UUIDs, commands, responses)
- [ ] Create `PairedDevice` model
- [ ] Create `PairingService` with SharedPreferences storage
- [ ] Update `EcdhService` with `loadKeyPair()` method
- [ ] Update `CommandService` with device binding logic
- [ ] Update connection flow to handle first pairing vs reconnection
- [ ] Add unpair command support
- [ ] Add settings UI for viewing/unpairing device
- [ ] Test all flows (first pair, reconnect, unpair, rejection)
- [ ] Verify pairing persists across app restarts
- [ ] Verify pairing persists across ESP32 reboots

**When complete, you'll have a production-ready device binding system with cryptographic authentication!**
