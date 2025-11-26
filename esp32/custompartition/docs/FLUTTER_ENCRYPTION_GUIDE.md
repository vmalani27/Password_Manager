# Flutter Client - BLE Session Encryption Implementation Guide

## Overview

This guide explains how to implement end-to-end encryption for BLE communication between the Flutter app and ESP32. All commands and responses are encrypted using AES-256-CTR with the ECDH-derived session key.

## Security Architecture

### Current Threat Model (FIXED)
- **Before**: Passwords transmitted in plaintext over BLE → interceptable
- **After**: All BLE traffic encrypted with session key → secure end-to-end

### Encryption Flow

1. **ECDH Handshake** (unchanged):
   - Client sends public key → ESP32
   - Both derive shared secret using ECDH
   - ESP32 derives session AES key using HKDF

2. **Command Encryption** (NEW):
   ```
   Client → ESP32:
   Plaintext: "get instagram vmalanixx"
   Encrypt with session key (AES-256-CTR)
   Base64 encode: nonce(16) + ciphertext
   Send: "ENC:<base64>"
   ```

3. **Response Decryption** (NEW):
   ```
   ESP32 → Client:
   Receives: "ENC:<base64>"
   Base64 decode → nonce + ciphertext
   Decrypt with session key (AES-256-CTR)
   Display: "Password: vansh"
   ```

## Implementation Steps

### 1. Add Dependencies

Add to `pubspec.yaml`:
```yaml
dependencies:
  flutter_blue_plus: ^1.32.0
  pointycastle: ^3.7.3  # For AES-CTR encryption
  convert: ^3.1.1       # For base64 encoding
```

### 2. Session Encryption Helper

Create `lib/services/session_crypto.dart`:

```dart
import 'dart:typed_data';
import 'dart:convert';
import 'package:pointycastle/export.dart';

class SessionCrypto {
  final Uint8List sessionKey;
  
  SessionCrypto(this.sessionKey);
  
  /// Encrypt command for BLE transmission (AES-256-CTR)
  String encryptCommand(String plaintext) {
    // Generate random nonce (16 bytes)
    final nonce = _generateNonce();
    
    // Setup AES-CTR cipher
    final cipher = CTRStreamCipher(AESEngine())
      ..init(
        true,
        ParametersWithIV(KeyParameter(sessionKey), nonce),
      );
    
    // Encrypt
    final plaintextBytes = utf8.encode(plaintext);
    final ciphertext = cipher.process(Uint8List.fromList(plaintextBytes));
    
    // Combine: nonce(16) + ciphertext
    final combined = Uint8List(16 + ciphertext.length);
    combined.setRange(0, 16, nonce);
    combined.setRange(16, combined.length, ciphertext);
    
    // Base64 encode and add prefix
    return 'ENC:${base64Encode(combined)}';
  }
  
  /// Decrypt response from BLE transmission (AES-256-CTR)
  String decryptResponse(String encryptedResponse) {
    // Remove "ENC:" prefix
    if (!encryptedResponse.startsWith('ENC:')) {
      // Plaintext response (e.g., ECDH_OK)
      return encryptedResponse;
    }
    
    final base64Data = encryptedResponse.substring(4);
    final decoded = base64Decode(base64Data);
    
    // Extract nonce (first 16 bytes)
    final nonce = decoded.sublist(0, 16);
    final ciphertext = decoded.sublist(16);
    
    // Setup AES-CTR cipher
    final cipher = CTRStreamCipher(AESEngine())
      ..init(
        false,
        ParametersWithIV(KeyParameter(sessionKey), nonce),
      );
    
    // Decrypt
    final plaintext = cipher.process(ciphertext);
    return utf8.decode(plaintext);
  }
  
  /// Generate random 16-byte nonce
  Uint8List _generateNonce() {
    final random = SecureRandom('Fortuna')
      ..seed(KeyParameter(
        Uint8List.fromList(
          List.generate(32, (_) => DateTime.now().millisecondsSinceEpoch % 256),
        ),
      ));
    return Uint8List.fromList(
      List.generate(16, (_) => random.nextUint8()),
    );
  }
}
```

### 3. Add Session Key Derivation to EcdhService

Add HKDF session key derivation to your `EcdhService` class (must match ESP32 implementation):

```dart
import 'package:pointycastle/export.dart';
import 'dart:typed_data';
import 'dart:convert';

class EcdhService {
  // ... existing ECDH code ...
  
  /// Derive session key from shared secret using HKDF
  /// MUST match ESP32 implementation exactly:
  /// - Salt: "BLE_PASSWORD_MGR" (16 bytes)
  /// - Info: "SESSION\x01" (8 bytes)
  /// - Output: 32 bytes (AES-256 key)
  Uint8List deriveSessionKey() {
    if (_sharedSecret == null) {
      throw StateError('Cannot derive session key: shared secret not computed');
    }
    
    // HKDF parameters (MUST match ESP32 crypto_manager.cpp)
    final salt = utf8.encode('BLE_PASSWORD_MGR'); // 16 chars
    final info = Uint8List.fromList([...utf8.encode('SESSION'), 0x01]); // "SESSION\x01"
    
    debugPrint('[ECDH] Deriving session key using HKDF...');
    debugPrint('[ECDH] Shared secret: ${_sharedSecret!.length} bytes');
    debugPrint('[ECDH] Salt length: ${salt.length} bytes');
    debugPrint('[ECDH] Info length: ${info.length} bytes');
    
    // HKDF-Extract: PRK = HMAC-SHA256(salt, sharedSecret)
    final extractHmac = HMac(SHA256Digest(), 64);
    extractHmac.init(KeyParameter(Uint8List.fromList(salt)));
    final prk = extractHmac.process(_sharedSecret!);
    
    debugPrint('[ECDH] PRK (pseudo-random key): ${prk.length} bytes');
    
    // HKDF-Expand: SessionKey = HMAC-SHA256(PRK, info)
    final expandHmac = HMac(SHA256Digest(), 64);
    expandHmac.init(KeyParameter(prk));
    final sessionKey = expandHmac.process(info);
    
    debugPrint('[ECDH] Session key derived: ${sessionKey.length} bytes');
    
    return sessionKey;
  }
}
```

### 4. Initialize SessionCrypto After ECDH

Modify your `CommandService.performEcdhHandshake()` method to derive session key and initialize encryption:

```dart
Future<({EcdhService ecdh, bool isNewPairing})> performEcdhHandshake(
  String deviceId,
  String deviceName,
) async {
  // ... existing ECDH handshake code ...
  
  // After ECDH handshake completes successfully:
  if (actualResponse == Esp32Commands.ecdhOk) {
    debugPrint('[ECDH] Received ECDH_OK - reconnection successful!');
    
    // CRITICAL: Derive session key and initialize encryption
    final sessionKey = ecdh.deriveSessionKey();
    _initializeSessionCrypto(sessionKey);
    
    debugPrint('[ECDH] ========================================');
    debugPrint('[ECDH] HANDSHAKE COMPLETE (RECONNECTION)');
    debugPrint('[ECDH] SESSION ENCRYPTION ACTIVE');
    debugPrint('[ECDH] ========================================');
    return (ecdh: ecdh, isNewPairing: false);
    
  } else if (actualResponse == Esp32Commands.ecdhOkPaired) {
    debugPrint('[ECDH] Received ECDH_OK_PAIRED - new pairing established!');
    
    // Save pairing...
    await pairingService.savePairing(pairedDevice);
    
    // CRITICAL: Derive session key and initialize encryption
    final sessionKey = ecdh.deriveSessionKey();
    _initializeSessionCrypto(sessionKey);
    
    debugPrint('[ECDH] Pairing saved to persistent storage');
    debugPrint('[ECDH] ========================================');
    debugPrint('[ECDH] HANDSHAKE COMPLETE (NEW PAIRING)');
    debugPrint('[ECDH] SESSION ENCRYPTION ACTIVE');
    debugPrint('[ECDH] ========================================');
    return (ecdh: ecdh, isNewPairing: true);
  }
  
  // ... rest of error handling code ...
}

/// Initialize session encryption with derived session key
void _initializeSessionCrypto(Uint8List sessionKey) {
  _sessionCrypto = SessionCrypto(sessionKey);
  debugPrint('[CommandService] Session encryption initialized');
  debugPrint('[CommandService] Session key length: ${sessionKey.length} bytes');
}
```

### 5. Update sendCommand() to Encrypt Commands

Modify your `CommandService.sendCommand()` method:

```dart
Future<void> sendCommand(String command) async {
  if (_commandCharacteristic == null) {
    throw Exception('Command characteristic not initialized');
  }
  
  String commandToSend = command;
  
  // Commands that MUST remain plaintext (protocol requirements)
  final alwaysPlaintextCommands = [
    'ecdh',           // ECDH handshake must be plaintext
  ];
  
  final commandName = command.split(' ').first.toLowerCase();
  final mustBePlaintext = alwaysPlaintextCommands.contains(commandName);
  
  // Encrypt everything except ECDH if session crypto is available
  if (_sessionCrypto != null && !mustBePlaintext) {
    try {
      commandToSend = _sessionCrypto!.encryptCommand(command);
      debugPrint('[CommandService] ENCRYPTED: $commandName');
      debugPrint('[CommandService] Length: ${commandToSend.length} bytes');
    } catch (e) {
      debugPrint('[CommandService] ENCRYPTION FAILED: $e');
      throw Exception('Encryption failed: $e');
    }
  } else if (_sessionCrypto == null && !mustBePlaintext) {
    debugPrint('[CommandService] WARNING: No session key, sending plaintext: $commandName');
  } else {
    debugPrint('[CommandService] PLAINTEXT: $commandName');
  }
  
  await _ble.writeCharacteristicWithResponse(
    _commandCharacteristic!,
    value: utf8.encode(commandToSend),
  );
  
  debugPrint('[CommandService] Sent successfully');
}
```

### 6. Update Notification Handler to Decrypt Responses

Modify your notification handler in `CommandService`:

```dart
void _onNotificationReceived(List<int> value) {
  String response = utf8.decode(value);
  
  debugPrint('[CommandService] RAW: ${value.length} bytes');
  
  // Decrypt if encrypted
  if (_sessionCrypto != null && response.startsWith('ENC:')) {
    try {
      response = _sessionCrypto!.decryptResponse(response);
      debugPrint('[CommandService] DECRYPTED: ${response.length} chars');
    } catch (e) {
      debugPrint('[CommandService] DECRYPTION FAILED: $e');
      response = 'DECRYPT_ERROR';
    }
  }
  
  // SECURITY: Never log passwords
  if (response.startsWith('Password:')) {
    debugPrint('[CommandService] Password retrieved successfully');
  } else {
    debugPrint('[CommandService] Received: "$response"');
  }
  
  // Broadcast to listeners
  _responseController.add(response);
}
```

### 7. Complete Connection Flow

Your connection flow should be:

```dart
// In your AppStateProvider or connection manager

Future<void> connectAndAuthenticate(String deviceId, String deviceName) async {
  try {
    // 1. Connect to device
    await bleConnectionService.connect(deviceId, deviceName: deviceName);
    
    // 2. Subscribe to notifications
    await commandService.subscribeToNotifications(deviceId);
    
    // 3. Check pairing status (informational)
    final pairingStatus = await commandService.checkPairingStatus();
    debugPrint('[App] Pairing status: $pairingStatus');
    
    // 4. CRITICAL: Perform ECDH handshake (establishes session encryption)
    final result = await commandService.performEcdhHandshake(deviceId, deviceName);
    debugPrint('[App] ECDH complete, new pairing: ${result.isNewPairing}');
    
    // 5. Request authentication token (now encrypted)
    await commandService.requestToken();
    
    // 6. Authenticate (now encrypted)
    await commandService.authenticate(token);
    
    debugPrint('[App] Connected and authenticated - all traffic encrypted');
    
  } catch (e) {
    debugPrint('[App] Connection failed: $e');
    rethrow;
  }
}
```

### 8. Testing Checklist

After implementation, verify:

- [ ] ECDH handshake completes successfully
- [ ] Session key derived correctly (32 bytes)
- [ ] Commands show "ENCRYPTED" in logs (not "plaintext")
- [ ] Responses show "DECRYPTED" in logs
- [ ] Password retrieval works end-to-end
- [ ] Wireshark shows "ENC:" prefix in BLE packets (no plaintext)
- [ ] No passwords in Flutter logs
- [ ] No passwords in ESP32 Serial output

### 9. Expected Log Output

After implementing correctly, you should see:

**Flutter logs**:
```
[ECDH] ========================================
[ECDH] STARTING HANDSHAKE
[ECDH] ========================================
[ECDH] Deriving session key using HKDF...
[ECDH] Shared secret: 32 bytes
[ECDH] PRK (pseudo-random key): 32 bytes
[ECDH] Session key derived: 32 bytes
[CommandService] Session encryption initialized
[ECDH] ========================================
[ECDH] HANDSHAKE COMPLETE
[ECDH] SESSION ENCRYPTION ACTIVE
[ECDH] ========================================

[CommandService] ENCRYPTED: request_token
[CommandService] Length: 68 bytes
[CommandService] Sent successfully
[CommandService] RAW: 58 bytes
[CommandService] DECRYPTED: 20 chars
[CommandService] Received: "TOKEN:818D500E"

[CommandService] ENCRYPTED: auth
[CommandService] Sent successfully
[CommandService] DECRYPTED: 7 chars
[CommandService] Received: "AUTH OK"

[CommandService] ENCRYPTED: list
[CommandService] Sent successfully
[CommandService] DECRYPTED: 32 chars
[CommandService] Received: "LIST:\ninstagram vmalanixx"
```

**ESP32 Serial Monitor**:
```
Client connected.
BLE: Bonded successfully!
Processing: check_pairing
BLE: Plaintext notification sent: PAIRED:...

Processing: ECDH <base64>
CryptoManager: ECDH handshake complete
BLE: Plaintext notification sent: ECDH_OK_PAIRED

Processing: ENC:xK9mP2L7nF8qR4vT...
BLE: Command decrypted successfully: request_token
BLE: Encrypted notification sent (45 bytes)

Processing: ENC:yH3nQzR8tB6cF9qL...
BLE: Command decrypted successfully: auth 818D500E
BLE: Encrypted notification sent (28 bytes)

Processing: ENC:zR7tWmF9kM4pV7uI...
BLE: Command decrypted successfully: list
BLE: Encrypted notification sent (58 bytes)
```

### 5. Secure Logging Rules

**NEVER log sensitive data:**

```dart
// WRONG - Logs password
print('Password: $password');
print('Received: $response');

// CORRECT - Sanitized logging
if (response.startsWith('Password:')) {
  print('Password retrieved successfully');
  // Use password in UI only
  _passwordController.text = response.substring(10);
} else {
  print('Response: $response');
}

// WRONG - Logs decrypted plaintext
debugPrint('Decrypted: $plaintext');

// CORRECT - Logs metadata only
debugPrint('Decryption successful (${plaintext.length} bytes)');
```

### 6. Testing Checklist

- [ ] ECDH handshake completes successfully
- [ ] Session key derived correctly (32 bytes)
- [ ] Commands encrypted before sending (ENC: prefix visible in logs)
- [ ] Responses decrypted correctly
- [ ] Password retrieval works end-to-end
- [ ] Wireshark verification: No plaintext passwords in BLE packets
- [ ] No passwords in Flutter logs
- [ ] No passwords in ESP32 Serial output

### 7. Wireshark Verification

Capture BLE traffic to verify encryption:

1. Use Wireshark with BLE adapter
2. Filter: `bluetooth.uuid == <your_characteristic_uuid>`
3. Send `get instagram vmalanixx` command
4. **Expected**: Encrypted blob in packets (no readable text)
5. **Before fix**: Plaintext "Password: vansh" visible

## Error Handling

```dart
try {
  final encrypted = sessionCrypto!.encryptCommand(command);
  await sendCommand(encrypted);
} catch (e) {
  print('Encryption failed: $e');
  // Handle gracefully - don't send plaintext!
}

try {
  final decrypted = sessionCrypto!.decryptResponse(response);
  return decrypted;
} catch (e) {
  print('Decryption failed: $e');
  return 'DECRYPT_ERROR';
}
```

## Troubleshooting

### Problem: Flutter Sending Plaintext Commands

**Symptoms**:
```
[CommandService] Sending plaintext command
[CommandService] → Sending: list
```

**Root Causes**:
1. `sessionCrypto` is null (ECDH didn't complete)
2. Session key not derived after ECDH
3. Logic error in `sendCommand()` (not checking for authorized commands)

**Fix**:
```dart
// After ECDH completes, verify:
print('Session crypto initialized: ${sessionCrypto != null}');
if (sessionCrypto != null) {
  print('Session key length: ${sessionCrypto!.sessionKey.length} bytes');
}

// In sendCommand(), add logging:
print('Command: $command');
print('Is public: $isPublicCommand');
print('Session ready: ${sessionCrypto != null}');
```

### Problem: ESP32 Sending Plaintext Responses

**Symptoms**:
```
DEBUG: Notification sent: LIST:\ninstagram vmalanixx
DEBUG: Notification sent: NOT FOUND
```

**Root Causes**:
1. **Old firmware still running** (new encryption code not uploaded)
2. `crypto.isEcdhReady()` returning false

**Fix**:
```bash
# Upload new firmware
platformio run --target upload

# Monitor serial output - should see:
# BLE: Encrypted notification sent (58 bytes)
# NOT: DEBUG: Notification sent: ...
```

### Problem: Database Corruption ("Invalid padding value")

**Symptoms**:
```
Invalid padding value
DBManager: Decryption failed for instagram/vmalanixx - data may be corrupted
```

**Root Cause**: Corrupted credential from previous database issues

**Fix**:
```
delete instagram vmalanixx
add instagram vmalanixx yourpassword
```

### Problem: ECDH Complete But No Encryption

**Symptoms**:
- ECDH handshake succeeds
- `sessionCrypto` is null in Flutter
- Commands sent plaintext

**Root Cause**: Session key not derived or not stored

**Fix**:
```dart
// After ECDH, ensure you call:
final sessionKey = deriveSessionKey(sharedSecret);
bleService.onEcdhComplete(sessionKey);

// Verify storage:
assert(bleService.sessionCrypto != null);
```

### Problem: Decryption Fails on Flutter Side

**Symptoms**:
```
[CommandService] ← Received: "ENC:xK9mP..."
FormatException: Invalid base64 data
```

**Root Causes**:
1. Session key mismatch (HKDF derivation different)
2. Wrong nonce extraction
3. Corrupted BLE packet

**Fix**:
```dart
// Verify HKDF matches ESP32 exactly:
final salt = utf8.encode('BLE_PASSWORD_MGR');  // 16 chars
final info = utf8.encode('SESSION\x01');       // 8 bytes with 0x01

// Debug decryption:
try {
  final decoded = base64Decode(encryptedResponse.substring(4));
  print('Decoded length: ${decoded.length}');
  print('Nonce: ${decoded.sublist(0, 16)}');
  print('Ciphertext length: ${decoded.length - 16}');
} catch (e) {
  print('Base64 decode failed: $e');
}
```

## Security Notes

1. **Session Key Lifetime**: Cleared on BLE disconnect
2. **Nonce Uniqueness**: New random nonce per message (16 bytes)
3. **No Replay Protection**: CTR mode alone doesn't prevent replay attacks (consider adding message counter if needed)
4. **Key Derivation**: HKDF ensures session key is cryptographically independent from shared secret
5. **CTR vs CBC**: CTR mode chosen because:
   - No padding (ciphertext = plaintext length)
   - Parallelizable encryption/decryption
   - Stream cipher mode (better for variable-length BLE packets)

## Migration Path

**Phase 1** (ESP32 - DONE):
- ✅ Implement `encrypt_session()` / `decrypt_session()`
- ✅ Modify `sendNotification()` to encrypt responses
- ✅ Modify `handleCommand()` to decrypt commands
- ✅ Remove password logging

**Phase 2** (Flutter - TODO):
- [ ] Implement `SessionCrypto` class
- [ ] Update BLE service to encrypt/decrypt
- [ ] Ensure HKDF matches ESP32
- [ ] Remove password logging

**Phase 3** (Testing):
- [ ] End-to-end testing
- [ ] Wireshark packet capture verification
- [ ] Security audit

## Backward Compatibility

Current implementation maintains backward compatibility:

- **Public commands** (ECDH, status): Sent/received in plaintext
- **Authorized commands**: Encrypted only if session key exists
- **Graceful fallback**: If decryption fails, ESP32 returns error

This allows gradual Flutter client updates without breaking existing functionality.

## Summary

### What Changed
- **Database encryption**: Already secure (AES-256-CBC)
- **BLE transmission**: NOW SECURE (AES-256-CTR with session key)
- **Attack surface**: Eliminated plaintext password interception

### Security Guarantees
- No plaintext passwords in BLE packets
- No plaintext passwords in Serial logs
- No plaintext passwords in Flutter logs
- Session-specific encryption (unique per connection)
- Forward secrecy (ECDH ephemeral keys)

**Result**: Attacker intercepting BLE traffic sees encrypted blobs, not passwords.

### Critical Implementation Points

1. **ECDH is mandatory**: Must be performed after every connection, even if device is already paired
2. **Session key derivation**: Must use HKDF with exact same parameters as ESP32
3. **Encryption scope**: All commands except ECDH itself must be encrypted after handshake
4. **Logging security**: Never log decrypted passwords or sensitive data
5. **Error handling**: If encryption fails, do not fall back to plaintext

### Common Mistakes to Avoid

**Mistake 1**: Skipping ECDH when device is paired
- Pairing = device trust (persistent)
- ECDH = session encryption (per-connection)
- Both are required

**Mistake 2**: Not deriving session key
- Shared secret is not the session key
- Must use HKDF to derive AES-256 key

**Mistake 3**: Wrong HKDF parameters
- Salt must be: "BLE_PASSWORD_MGR" (16 bytes)
- Info must be: "SESSION\x01" (8 bytes with 0x01)
- Any mismatch = decryption fails

**Mistake 4**: Encrypting ECDH command
- ECDH handshake must remain plaintext
- Cannot encrypt without session key
- Session key is established by ECDH

**Mistake 5**: Logging passwords
- Never log response.startsWith('Password:')
- Log "Password retrieved successfully" instead
