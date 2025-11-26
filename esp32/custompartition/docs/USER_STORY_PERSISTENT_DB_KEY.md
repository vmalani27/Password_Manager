# User Story: Persistent Database Encryption Key

**Priority**: 🔴 CRITICAL  
**Status**: Open  
**Component**: Security - Database Encryption  
**Severity**: Blocker  

---

## User Story

**As a** password manager user  
**I want** my stored passwords to remain accessible after ESP32 reboots  
**So that** I don't lose access to my credentials every time the device restarts

---

## Current Behavior (Bug)

**What happens now**:
1. User adds credential: `add instagram vmalanixx mypassword123`
2. ESP32 encrypts password with runtime key (Key_A) and stores to database
3. Credential saved successfully ✓
4. ESP32 reboots (power cycle, firmware update, or crash)
5. User requests credential: `get instagram vmalanixx`
6. ESP32 tries to decrypt with NEW runtime key (Key_B) ≠ Key_A
7. **Decryption fails**: "Invalid padding value"
8. **Result**: Password permanently lost ❌

**Error logs**:
```
BLE: Command decrypted successfully
Processing: get ig vm
Invalid padding value
DBManager: Decryption failed for ig/vm - data may be corrupted
Entry not found
BLE: Encrypted notification sent (36 bytes)
```

---

## Expected Behavior

**What should happen**:
1. User adds credential: `add instagram vmalanixx mypassword123`
2. ESP32 encrypts password with **persistent** runtime key (derived from NVS master key)
3. Credential saved successfully ✓
4. ESP32 reboots (power cycle, firmware update, or crash)
5. ESP32 derives **same runtime key** from NVS master key
6. User requests credential: `get instagram vmalanixx`
7. ESP32 decrypts successfully using persistent runtime key
8. **Result**: Password retrieved successfully ✓

---

## Root Cause

**File**: `lib/secure_core/secure_core.cpp`  
**Function**: `deriveRuntimeKey()`

**Problem**: Runtime encryption key is generated **randomly** on every boot instead of being derived **deterministically** from persistent storage.

```cpp
// CURRENT (WRONG):
bool deriveRuntimeKey() {
    esp_fill_random(g_runtimeKey, 32);  // ← Random every boot!
    g_keyReady = true;
    return true;
}
```

**Why this breaks**:
- Boot 1: Random key = `0x1A2B3C...` → Encrypt password with this key
- Boot 2: Random key = `0x9F8E7D...` → Try to decrypt with different key → FAIL

---

## Impact Assessment

### Security Impact
- **Severity**: CRITICAL
- **Data Loss**: All credentials become unreadable after reboot
- **User Experience**: Catastrophic - users must re-enter all passwords after every reboot
- **Trust**: Users cannot rely on the device to store passwords securely

### Business Impact
- **Product Viability**: Product is unusable in current state
- **Data Integrity**: Database appears corrupted but is actually encrypted with wrong key
- **Recovery**: No way to recover passwords encrypted with previous boot's key

### Affected Users
- **100% of users** who:
  - Add credentials
  - Reboot device (power cycle, firmware update, crash recovery)
  - Try to retrieve previously stored credentials

---

## Acceptance Criteria

### ✅ Success Criteria

1. **Persistent Key Derivation**
   - Runtime key MUST be derived from persistent master key stored in NVS
   - Same master key → same runtime key across all boots
   - Master key generated once on first boot, then reused forever

2. **Boot-to-Boot Consistency**
   - Add credential on Boot 1
   - Reboot device (power cycle)
   - Retrieve same credential on Boot 2
   - **Result**: Password decrypts successfully

3. **Multiple Reboot Test**
   - Add 5 credentials
   - Reboot 10 times
   - All 5 credentials remain accessible after every reboot

4. **Firmware Update Test**
   - Add credentials with old firmware
   - Flash new firmware (OTA or USB)
   - All credentials remain accessible with new firmware

5. **Factory Reset Support**
   - Provide mechanism to erase master key from NVS
   - After factory reset, all encrypted data becomes unrecoverable (expected)
   - New master key generated on next boot

### ❌ Failure Criteria

- Any credential becomes unreadable after reboot
- "Invalid padding value" error on valid credentials
- Different decryption results for same credential across boots

---

## Technical Implementation

### Solution: Persistent Key Derivation with NVS

**Files to modify**:
1. `lib/secure_core/secure_core.cpp` - Fix `deriveRuntimeKey()`
2. `lib/secure_core/secure_core.h` - Add `getMasterKey()` declaration

**Architecture**:
```
┌──────────────────────────────────────────────────┐
│ NVS Flash Storage (Persistent)                   │
│ Key: "secure_store/master_key"                   │
│ Value: 32-byte random master key (generated once)│
└──────────────────────────────────────────────────┘
                        ↓
                  getMasterKey()
                        ↓
┌──────────────────────────────────────────────────┐
│ HKDF Key Derivation (Deterministic)              │
│ Input: Master Key (32 bytes)                     │
│ Salt: "ESP32_DB_ENCRYPTION"                      │
│ Info: "RUNTIME_KEY_V1"                           │
│ Output: Runtime Key (32 bytes)                   │
└──────────────────────────────────────────────────┘
                        ↓
                g_runtimeKey (in RAM)
                        ↓
┌──────────────────────────────────────────────────┐
│ Database Encryption/Decryption                   │
│ AES-256-CBC with persistent runtime key          │
│ Same key every boot → consistent encryption      │
└──────────────────────────────────────────────────┘
```

**Key properties**:
- **Persistent**: Master key stored in NVS (survives reboots)
- **Deterministic**: HKDF ensures same input → same output
- **Secure**: Master key never leaves ESP32, stored in encrypted NVS
- **Recoverable**: Factory reset erases master key → data unrecoverable (by design)

---

## Implementation Steps

### Phase 1: Add Master Key Storage (1-2 hours)

**Task 1.1**: Add NVS master key management
```cpp
// In secure_core.cpp
bool getMasterKey(uint8_t* outKey, size_t keySize) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("secure_store", NVS_READONLY, &nvs);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // First boot - generate and save master key
        Serial.println("First boot: Generating master key...");
        esp_fill_random(outKey, keySize);
        
        // Save to NVS
        err = nvs_open("secure_store", NVS_READWRITE, &nvs);
        if (err != ESP_OK) return false;
        
        nvs_set_blob(nvs, "master_key", outKey, keySize);
        nvs_commit(nvs);
        nvs_close(nvs);
        
        Serial.println("Master key saved to NVS");
        return true;
    }
    
    // Load existing master key
    size_t required_size = keySize;
    err = nvs_get_blob(nvs, "master_key", outKey, &required_size);
    nvs_close(nvs);
    
    Serial.println("Master key loaded from NVS");
    return (err == ESP_OK && required_size == keySize);
}
```

**Task 1.2**: Fix `deriveRuntimeKey()` to use persistent master key
```cpp
bool deriveRuntimeKey() {
    // Get persistent master key
    uint8_t masterKey[32];
    if (!getMasterKey(masterKey, sizeof(masterKey))) {
        Serial.println("ERROR: Failed to get master key");
        return false;
    }
    
    // Derive runtime key using HKDF (deterministic)
    const char* salt = "ESP32_DB_ENCRYPTION";
    const char* info = "RUNTIME_KEY_V1";
    
    // HKDF derivation (same master key → same runtime key)
    if (!hkdf_sha256(masterKey, 32, salt, strlen(salt), 
                     info, strlen(info), g_runtimeKey, 32)) {
        memset(masterKey, 0, sizeof(masterKey));
        return false;
    }
    
    memset(masterKey, 0, sizeof(masterKey));
    g_keyReady = true;
    
    Serial.println("Runtime key derived from persistent master key");
    return true;
}
```

### Phase 2: Testing (30 minutes)

**Test 2.1**: Basic reboot test
```
1. Erase NVS: `pio run --target erase`
2. Upload firmware
3. Add credential: `add test user pass123`
4. Power cycle ESP32
5. Get credential: `get test user`
6. EXPECTED: Returns "pass123"
7. ACTUAL: (verify against expected)
```

**Test 2.2**: Multiple credential test
```
1. Add 5 credentials
2. Reboot 3 times
3. Verify all 5 credentials accessible after each reboot
```

**Test 2.3**: Firmware update test
```
1. Add credentials with current firmware
2. Make trivial code change (e.g., update version string)
3. Upload new firmware
4. Verify all credentials still accessible
```

### Phase 3: Factory Reset Support (30 minutes)

**Task 3.1**: Add factory reset command
```cpp
// In ble_manager.cpp handleCommand()
if (command == "factory_reset") {
    nvs_handle_t nvs;
    nvs_open("secure_store", NVS_READWRITE, &nvs);
    nvs_erase_key(nvs, "master_key");
    nvs_commit(nvs);
    nvs_close(nvs);
    
    sendNotification("FACTORY_RESET_OK - All data will be lost on next boot");
    ESP.restart();
}
```

---

## Testing Plan

### Unit Tests

**Test Case 1**: Master key persistence
- **Setup**: First boot (no NVS data)
- **Action**: Call `getMasterKey()`
- **Expected**: Returns newly generated 32-byte key
- **Verify**: Key saved to NVS with namespace "secure_store", key "master_key"

**Test Case 2**: Master key consistency
- **Setup**: Second boot (NVS data exists)
- **Action**: Call `getMasterKey()` twice
- **Expected**: Both calls return identical key
- **Verify**: memcmp(key1, key2, 32) == 0

**Test Case 3**: Runtime key derivation
- **Setup**: Known master key = `0x01020304...` (32 bytes)
- **Action**: Call `deriveRuntimeKey()`
- **Expected**: Runtime key = deterministic HKDF output
- **Verify**: Same master key always produces same runtime key

### Integration Tests

**Test Case 4**: Cross-boot decryption
- **Setup**: Boot 1
- **Action**: Add credential with password "test123"
- **Reboot**: Power cycle
- **Action**: Retrieve same credential
- **Expected**: Password returns "test123"
- **Verify**: No "Invalid padding value" error

**Test Case 5**: Multiple credentials persistence
- **Setup**: Add 10 credentials with different passwords
- **Action**: Reboot 5 times
- **Expected**: All 10 credentials accessible after each reboot
- **Verify**: All passwords decrypt correctly

### Stress Tests

**Test Case 6**: Rapid reboot test
- **Action**: Reboot every 10 seconds for 100 reboots
- **Expected**: Credentials remain accessible throughout
- **Verify**: No data corruption or key derivation failures

**Test Case 7**: Firmware update test
- **Setup**: Credentials added with version 1.0
- **Action**: OTA update to version 1.1
- **Expected**: All credentials accessible in new version
- **Verify**: Runtime key derivation compatible across versions

---

## Risks & Mitigations

### Risk 1: Master Key Loss
**Risk**: If master key is lost (NVS corruption), all data becomes unrecoverable  
**Likelihood**: Low (NVS is wear-leveled and redundant)  
**Impact**: High (all passwords lost)  
**Mitigation**: 
- Implement master key backup to SD card (encrypted with device PIN)
- Add NVS health monitoring and alerts
- Document recovery procedures for users

### Risk 2: Key Derivation Changes
**Risk**: Changing HKDF parameters breaks existing encrypted data  
**Likelihood**: Medium (during future updates)  
**Impact**: High (all passwords lost)  
**Mitigation**:
- Version the key derivation scheme ("RUNTIME_KEY_V1")
- Support migration from old to new schemes
- Never change derivation parameters without migration path

### Risk 3: NVS Wear
**Risk**: Frequent writes to NVS could cause wear (but master key written only once)  
**Likelihood**: Very Low (single write on first boot)  
**Impact**: Low (can regenerate master key if needed)  
**Mitigation**: Master key written only once, no wear concerns

---

## Definition of Done

- [x] User story documented
- [ ] Technical implementation complete
- [ ] Unit tests passing (100% coverage for key derivation)
- [ ] Integration tests passing (cross-boot decryption)
- [ ] Code reviewed and approved
- [ ] Firmware uploaded to test device
- [ ] Manual testing completed (10+ reboot cycles)
- [ ] Documentation updated (security architecture doc)
- [ ] Deployment to production approved

---

## Related Issues

- **Blocks**: All database functionality (passwords unusable after reboot)
- **Related to**: BLE Session Encryption (different key hierarchy)
- **Dependencies**: NVS partition must be available and writable

---

## Notes

### Why This Wasn't Caught Earlier
- Initial development focused on getting encryption working at all
- Testing was done within single boot session
- Reboot testing was not part of acceptance criteria
- Database encryption and BLE session encryption use different keys (BLE is ephemeral by design)

### Why This Is CRITICAL
- **Data Loss**: Users lose all passwords on every reboot
- **Unusable Product**: Core functionality (persistent storage) is broken
- **Security Theater**: Strong encryption is meaningless if data is inaccessible
- **User Trust**: Users cannot trust device to store passwords

### Timeline
- **Discovery**: November 26, 2025 (during BLE encryption testing)
- **Priority**: CRITICAL (blocks all production use)
- **Target Fix**: Within 24 hours
- **Testing**: 48 hours (multiple reboot cycles required)

---

**Created by**: GitHub Copilot  
**Created on**: November 26, 2025  
**Last updated**: November 26, 2025  
**Assigned to**: [To be assigned]  
**Sprint**: Current Sprint (Unplanned Work)
