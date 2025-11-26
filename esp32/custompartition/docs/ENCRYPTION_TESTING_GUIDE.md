# BLE Encryption - Quick Testing Guide

## Serial Monitor - What to Look For

### ✅ SUCCESS INDICATORS

**After ECDH Handshake:**
```
ECDH: Session key derived successfully
Session encryption initialized
```

**When Sending Encrypted Command:**
```
BLE: Command decrypted successfully
Processing: get instagram vmalanixx
BLE: Password retrieved for instagram/vmalanixx
```

**When Sending Encrypted Response:**
```
BLE: Encrypted notification sent (58 bytes)
```

### ❌ ERROR INDICATORS

**Encryption Failures:**
```
ERROR: Session encryption failed
ERROR: Base64 encoding failed
Session encryption: AES key setup failed
```

**Decryption Failures:**
```
ERROR: Command decryption failed
ERROR: Base64 decode failed
Session decryption: AES key setup failed
```

**Security Violations:**
```
Password: vansh  ← Should NEVER appear in logs!
```

---

## Expected Behavior by Command Type

### Public Commands (Before ECDH)
**Sent**: Plaintext  
**Received**: Plaintext  
**Examples**:
- `request_token` → `TOKEN:A3F7B2C1`
- `status` → `STATUS CONNECTED NOT_AUTHORIZED`

### ECDH Handshake
**Sent**: Raw bytes (64-byte public key)  
**Received**: Plaintext  
**Response**: `ECDH_OK` or `ECDH_OK_PAIRED`

### Authorized Commands (After ECDH + Auth)
**Sent**: `ENC:<base64>`  
**Received**: `ENC:<base64>`  
**Examples**:
- `get instagram vmalanixx` → Encrypted blob
- Response: `ENC:yH2nK8mT...` (encrypted "Password: vansh")

---

## Wireshark Packet Capture

### Filter
```
bluetooth.uuid == <your_characteristic_uuid>
```

### What to Verify

✅ **BEFORE encryption enabled** (for comparison):
```
Frame 1: "get instagram vmalanixx" (plaintext visible)
Frame 2: "Password: vansh" (plaintext visible)
```

✅ **AFTER encryption enabled**:
```
Frame 1: "ENC:xK9mPzL3nF8qR4vT..." (no plaintext)
Frame 2: "ENC:yH2nK8mT1vB6cF9q..." (no plaintext)
```

**Red Flag**: If you see "Password:" in plaintext → ENCRYPTION FAILED

---

## Flutter App Testing

### 1. Pre-Flight Check
```dart
// Before sending command
print('Session key exists: ${sessionCrypto != null}');
print('Command to encrypt: $command');
```

### 2. Encryption Verification
```dart
final encrypted = sessionCrypto.encryptCommand(command);
print('Encrypted command length: ${encrypted.length}');
print('Starts with ENC: ${encrypted.startsWith("ENC:")}');
// Should see: "ENC:..." with base64 characters only
```

### 3. Response Handling
```dart
void onNotificationReceived(List<int> value) {
  String response = utf8.decode(value);
  print('Raw response: ${response.substring(0, 20)}...');  // First 20 chars
  
  if (response.startsWith('ENC:')) {
    print('✅ Response is encrypted');
    final decrypted = sessionCrypto.decryptResponse(response);
    print('✅ Decryption successful');
    // NEVER log: print('Decrypted: $decrypted');
  }
}
```

### 4. Password Retrieval Test
```dart
// Send encrypted GET command
await sendCommand('get instagram vmalanixx');

// Wait for response
final response = await waitForNotification();

// Verify encrypted
assert(response.startsWith('ENC:'));

// Decrypt
final decrypted = sessionCrypto.decryptResponse(response);

// Verify format (but don't log password!)
assert(decrypted.startsWith('Password:'));
print('✅ Password retrieved successfully');

// Use in UI only
_passwordController.text = decrypted.substring(10);
```

---

## Common Issues & Solutions

### Issue: "Base64 decode failed"
**Cause**: Invalid base64 string received  
**Solution**: Check Flutter is properly encoding before sending

### Issue: "Session decryption failed"
**Cause**: Session key mismatch between ESP32 and Flutter  
**Solution**: Verify HKDF derivation matches on both sides

### Issue: "Invalid ciphertext length"
**Cause**: Corrupted BLE packet or wrong nonce extraction  
**Solution**: Check nonce is first 16 bytes, ciphertext is rest

### Issue: Plaintext passwords visible in Wireshark
**Cause**: Encryption not applied  
**Solution**: 
1. Verify `crypto.isEcdhReady()` returns true
2. Check session key is not null/zero
3. Confirm command starts with "ENC:" prefix

---

## Test Sequence Checklist

- [ ] 1. Upload ESP32 firmware
- [ ] 2. Monitor serial output
- [ ] 3. Pair Flutter app (BLE PIN: 123456)
- [ ] 4. Verify ECDH completes: `Session key derived successfully`
- [ ] 5. Request token: `request_token` (plaintext OK)
- [ ] 6. Authenticate: `auth <token>` (should be encrypted)
- [ ] 7. Add credential: `add test user pass123` (encrypted)
- [ ] 8. Verify serial shows: `BLE: Encrypted notification sent`
- [ ] 9. Get credential: `get test user` (encrypted)
- [ ] 10. Verify response starts with `ENC:` in Wireshark
- [ ] 11. Verify Flutter decrypts and shows password in UI
- [ ] 12. Verify NO plaintext "pass123" in Wireshark
- [ ] 13. Verify NO plaintext "pass123" in Serial Monitor
- [ ] 14. Verify NO plaintext "pass123" in Flutter logs

---

## Security Self-Audit

### Check 1: Encryption Active
```bash
# In Serial Monitor after ECDH
grep "Encrypted notification sent" output.log
# Should see multiple occurrences
```

### Check 2: No Password Leakage
```bash
# In Serial Monitor
grep -i "password:" output.log | grep -v "retrieved"
# Should return NOTHING
```

### Check 3: Encryption Format
```bash
# In Wireshark export
grep "ENC:" ble_capture.txt
# Should see multiple "ENC:<base64>" entries
```

### Check 4: Flutter Logs Clean
```bash
# In Flutter debug output
grep -i "password:" flutter.log | grep -v "retrieved"
# Should return NOTHING
```

---

## Performance Benchmarks

**Expected Latency**:
- Command encryption: <1ms
- Response decryption: <1ms
- BLE round-trip: 20-50ms (unchanged)

**Memory Usage**:
- Stack: +1KB during encryption
- Heap: 0 (all buffers stack-allocated)

**Packet Size Increase**:
- Average command: 24 bytes → 58 bytes (2.4x)
- Average response: 15 bytes → 48 bytes (3.2x)
- Still well under BLE MTU (185-512 bytes)

---

## Rollback Plan

If encryption causes issues:

1. Comment out encryption in `sendNotification()`:
   ```cpp
   // if (crypto.isEcdhReady()) { ... }
   // Temporarily send plaintext:
   pCharacteristic->setValue(data.c_str());
   pCharacteristic->notify();
   ```

2. Comment out decryption in `handleCommand()`:
   ```cpp
   // if (crypto.isEcdhReady() && cmdLine.startsWith("ENC:")) { ... }
   // Process commands as-is
   ```

3. Re-upload firmware

**WARNING**: Only use plaintext mode for debugging. NEVER in production.

---

## Contact

**Security Issues**: Immediately halt testing if:
- Plaintext passwords appear in packet captures
- Encryption consistently fails
- Session key derivation fails

**Documentation**: See `BLE_ENCRYPTION_IMPLEMENTATION.md` for full details

**Flutter Guide**: See `FLUTTER_ENCRYPTION_GUIDE.md` for client implementation
