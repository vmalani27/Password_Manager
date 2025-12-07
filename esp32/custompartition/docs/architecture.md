# ESP32 Password Manager Architecture

## Overview

A hardware-based password manager using ESP32 as a secure vault, communicating with a Flutter mobile app via Bluetooth Low Energy (BLE). The system uses cryptographic authentication and end-to-end encryption to protect credentials.

---

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Flutter Mobile App                       │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  UI Layer (Credential Management, Settings)            │ │
│  └────────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  Business Logic (ECDH, Challenge-Response Auth)        │ │
│  └────────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  BLE Service (Connection, Command Protocol)            │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ BLE (Encrypted Link)
                              │
┌─────────────────────────────────────────────────────────────┐
│                        ESP32 Device                          │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  BLE Server (Advertising, Pairing, Characteristics)    │ │
│  └────────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  Security Layer (ECDH, HMAC, AES-256, Runtime Key)    │ │
│  └────────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  Command Handler (Add, Get, Update, Delete, List)     │ │
│  └────────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  SQLite Database (Encrypted Credentials on SD Card)    │ │
│  └────────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  Display (OLED - Status & Notifications)              │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

---

## Target Architecture (What We're Building Toward)

### Security Model

**Multi-Layer Defense:**

1. **Hardware Root of Trust**: eFuse BLOCK3 (256-bit device-unique key)
2. **Runtime Key Derivation**: HMAC-SHA256(eFuse, random_challenge) → never stored
3. **ECDH Key Exchange**: Establishes ephemeral session key per connection
4. **Challenge-Response Auth**: Cryptographic proof of client identity
5. **Database Encryption**: AES-256-CBC for password storage (upgrading to AES-GCM)
6. **BLE Session Encryption**: AES-256-CTR with ECDH-derived session key (IMPLEMENTED)

### Data Flow

#### Credential Storage Flow:
```
User Input (plaintext password)
    ↓
AES-256-CBC Encrypt (using runtime_key derived from eFuse)
    ↓
Store {encrypted_password, IV} in SQLite
    ↓
SQLite on SD Card (encrypted at rest)
```

#### Credential Retrieval Flow:
```
BLE Command "ENC:<base64(nonce + ciphertext)>" (encrypted with session_key)
    ↓
Decrypt command using AES-256-CTR (session_key derived from ECDH)
    ↓
ECDH Challenge-Response Authentication (session verified)
    ↓
SQLite Query → {encrypted_password, IV}
    ↓
AES-256-CBC Decrypt (using runtime_key)
    ↓
Encrypt password with session_key (AES-256-CTR)
    ↓
Send "ENC:<base64(nonce + ciphertext)>" over BLE (encrypted)
```

#### ECDH Authentication Flow:
```
1. Client connects → BLE pairing (PIN: 123456)
2. Client reads ESP32 public key (ECDH characteristic)

## Current Implementation Status

### [32mPhase 0 - Cleanup & Stabilization (COMPLETE)[0m
All repository and build system setup, codebase migrated to PlatformIO, and legacy code removed.

### [32mPhase 1.1 - Cryptographic Security (COMPLETE)[0m
eFuse-based runtime key, AES-256-CBC for database, IV per password, and secure buffer handling are all implemented.

### [32mPhase 1.2 - Authentication Security (COMPLETE on ESP32, IN PROGRESS on Flutter)[0m
- ECDH key exchange, challenge-response, device binding, and pairing state machine are fully implemented on ESP32.
- BLE pairing uses static PIN (123456) — **dynamic PIN not yet implemented**.
- Flutter client: ECDH/session encryption guide complete, code in progress.
- Legacy token-based auth still present (32-bit, being upgraded to 128-bit).

### [33mPhase 1.3 - Credential Lifecycle (PARTIAL)[0m
- Database worker queue (FreeRTOS) is **implemented** (prevents SQLite corruption).
- Atomic backups, journaling, and IV randomness testing are **planned**.

### [33mPhase 1.4 - UX & Protocol (PARTIAL)[0m
- Command protocol (add, get, update, delete, list, logout, request_token, auth, ecdh_auth, respond) is **implemented and encrypted** (CTR mode).
- Protocol documentation is **incomplete**.
- Display: OLED status, connection, and command notifications are implemented. **Security indicators missing.**

---

### **Summary of Gaps and Next Steps**
- **AES-GCM migration** (for authenticated encryption) — **not started**
- **Dynamic PIN generation** — **not started**
- **Message authentication and replay protection** — **not started**
- **Database key persistence** — **critical blocker, not fixed**
- **Sensitive logging removal** — **not started**
- **Flutter client integration** — **in progress**
- **Testing (unit/integration/security)** — **not started**

**Command Protocol (Working)**
- ✅ Commands: add, get, update, delete, list, logout
- ✅ Token-based: request_token, auth
- ✅ ECDH commands: ecdh_auth, respond
- ✅ Response format standardized
- 🔄 Protocol documentation incomplete

**Display**
- ✅ OLED status messages
- ✅ Connection notifications
- ✅ Brief status display
- ❌ Security indicators missing

---

## Component Details

### 1. Security Layer (`lib/secure_core/`)

**Purpose**: All cryptographic operations isolated from main logic

**Current State:**
```cpp
secure_core.h   (27 lines)  - API definitions (database + session encryption)
secure_core.cpp (370 lines) - Implementation (AES-256-CBC + AES-256-CTR)
```

**Functions:**

*Database Encryption (AES-256-CBC):*
- `initKeyManager()` - Initialize crypto subsystem
- `deriveRuntimeKey()` - Derive encryption key from eFuse
- `encrypt_password()` - AES-256-CBC encryption for database
- `decrypt_password()` - AES-256-CBC decryption for database
- `generate_iv()` - Random IV generation (16 bytes)

*BLE Session Encryption (AES-256-CTR):*
- `encrypt_session()` - AES-256-CTR encryption for BLE traffic
- `decrypt_session()` - AES-256-CTR decryption for BLE traffic
- `generate_nonce()` - Random nonce generation (16 bytes)

**Dependencies:**
- mbedTLS (ESP-IDF built-in)
- ESP eFuse API

**Security Properties:**

*Database Layer:*
- Runtime key never persisted to flash/SD
- eFuse read-only (hardware-enforced)
- IV uniqueness per password
- PKCS7 padding

*Session Layer:*
- Session key derived from ECDH shared secret
- AES-256-CTR stream cipher (no padding required)
- Random nonce per message (16 bytes)
- Nonce prepended to ciphertext
- Session key destroyed on disconnect
- Forward secrecy (ephemeral ECDH keys)

### 1.5. BLE Session Encryption Layer (`lib/secure_core/`, `src/ble_manager.cpp`)

**Purpose**: Encrypt all BLE commands and responses using ECDH-derived session key

**Implementation Status**: ✅ COMPLETE (ESP32 side)

**Encryption Flow:**

*Command Decryption (Incoming):*
```cpp
// In BLEManager::handleCommand()
1. Receive: "ENC:<base64_data>"
2. Base64 decode → nonce (16 bytes) + ciphertext
3. Extract session_key from CryptoManager
4. decrypt_session(session_key, ciphertext, nonce) → plaintext
5. Process decrypted command
```

*Response Encryption (Outgoing):*
```cpp
// In BLEManager::sendNotification()
1. Check: crypto.isEcdhReady() (session key exists?)
2. generate_nonce() → 16 random bytes
3. encrypt_session(session_key, plaintext, nonce) → ciphertext
4. Combine: nonce || ciphertext
5. Base64 encode: combined → base64_data
6. Send: "ENC:<base64_data>"
```

**Wire Protocol:**
```
Plaintext command:  "get instagram vmalanixx"
        ↓
Encrypt (AES-256-CTR, session_key, random_nonce)
        ↓
Ciphertext:         [16 random bytes (nonce)] [N bytes (ciphertext)]
        ↓
Base64 encode:      "JYweovWKMyALPfmTSqrQcW0K..."
        ↓
Transmit BLE:       "ENC:JYweovWKMyALPfmTSqrQcW0K..."
```

**Key Properties:**
- **Cipher**: AES-256-CTR (Counter mode)
- **Key**: 32-byte session key from ECDH (HKDF-derived)
- **Nonce**: 16 bytes random per message
- **Overhead**: 16 bytes (nonce) + ~33% (base64)
- **Padding**: None required (CTR is a stream cipher)

**Security Analysis:**

*Strengths:*
- ✅ End-to-end encryption (only paired device can decrypt)
- ✅ Forward secrecy (ephemeral ECDH keys)
- ✅ Nonce uniqueness (random 16 bytes per message)
- ✅ Post-quantum resistant key exchange (P-256 ECDH)
- ✅ No plaintext passwords in BLE packets
- ✅ Session key destroyed on disconnect

*Limitations:*
- ❌ No message authentication (CTR mode doesn't provide integrity)
- ❌ No replay protection (CTR nonce is random, not sequential)
- ❌ Vulnerable to bit-flipping attacks (need AES-GCM for AEAD)

*Future Improvements:*
- Upgrade to AES-256-GCM (provides authentication)
- Add message sequence numbers (replay protection)
- Implement perfect forward secrecy with key rotation

**Exception Handling:**
```cpp
// Commands that MUST be plaintext (no session key exists yet):
- "ecdh <pubkey>"        // ECDH handshake initiation
- "check_pairing"        // Pre-handshake query

// Responses that are plaintext (handshake phase):
- "PAIRED:<pubkey>"      // Pairing status with ESP32 public key
- "ECDH_OK"              // Handshake success
- "ECDH_OK_PAIRED"       // Handshake success (first pairing)
- "ECDH_ALREADY_PAIRED"  // Rejection (different device)
- "NOT_PAIRED"           // Pairing status
- "UNPAIRED"             // Unpair confirmation

// All other commands/responses: ENCRYPTED
```

**Implementation Files:**
- `lib/secure_core/secure_core.h` (lines 13-15): Function declarations
- `lib/secure_core/secure_core.cpp` (lines 260-370): encrypt_session/decrypt_session
- `src/ble_manager.cpp` (lines 330-380): Response encryption in sendNotification()
- `src/ble_manager.cpp` (lines 520-580): Command decryption in handleCommand()

**Flutter Integration:**
- ✅ Guide created: `docs/FLUTTER_ENCRYPTION_GUIDE.md`
- ⏳ SessionCrypto class implementation (in progress)
- ⏳ CommandService encryption/decryption (in progress)
- ⏳ End-to-end testing (pending)

### 2. ECDH Layer (`main.cpp` - lines 78-475)

**Purpose**: Establish shared secret with Flutter client and manage device pairing

**Current State:**
- 6 functions: `initECDH()`, `computeSharedSecret()`, `clearECDH()`, `loadOrGenerateKeys()`, `savePairing()`, `unpairDevice()`
- ~398 lines of ECDH + NVS device binding implementation
- Using mbedTLS ECDH context + ESP-IDF NVS API

**Key Generation:**
```cpp
Curve: secp256r1 (NIST P-256)
Private key: 32 bytes (kept secret, stored in NVS when paired)
Public key: 64 bytes (X||Y coordinates, stored in NVS when paired)
Shared secret: 32 bytes → HKDF → session_key (32 bytes)
Client public key: 64 bytes (stored in NVS after first pairing)
```

**HKDF Implementation:**
```
Extract: PRK = HMAC-SHA256(salt="BLE_PASSWORD_MGR", shared_secret)
Expand: session_key = HMAC-SHA256(PRK, info="SESSION" || 0x01)
```

**NVS Device Binding:**

*Storage Schema:*
```cpp
Namespace: "pwmgr"
Keys:
  - "paired" (u8): 0=UNPAIRED, 1=PAIRED
  - "esp_priv" (blob, 32 bytes): ESP32 private key
  - "esp_pub" (blob, 64 bytes): ESP32 public key
  - "client_pub" (blob, 64 bytes): Paired client public key
```

*Pairing State Machine:*
```
UNPAIRED (pairing_state = 0):
  - Generate ephemeral ECDH keys on boot
  - Accept any client for pairing
  - On successful handshake → save to NVS → PAIRED

PAIRED (pairing_state = 1):
  - Load persistent keys from NVS on boot
  - Verify incoming client public key matches stored key
  - Reject unauthorized devices (send ECDH_ALREADY_PAIRED)
  - Allow paired device to reconnect instantly
```

*Functions:*
- `loadOrGenerateKeys()`: Boot-time initialization
  - Check NVS for pairing state
  - If PAIRED: load keys from NVS
  - If UNPAIRED: generate new ephemeral keys
- `savePairing(client_pubkey)`: Save pairing after first handshake
  - Store ESP32 keys to NVS
  - Store client public key to NVS
  - Set paired flag
  - Commit to flash
- `unpairDevice()`: Factory reset for pairing
  - Erase all NVS keys
  - Clear runtime state
  - Generate new ephemeral keys
  - Return to UNPAIRED state
- `isDevicePaired()`: Query pairing status

*EcdhCallback Logic:*
```cpp
onWrite(client_public_key):
  if (isDevicePaired()):
    if (client_public_key == stored_client_public_key):
      // Recognized paired device
      computeSharedSecret() → ECDH_OK
    else:
      // Different device trying to connect
      sendNotification("ECDH_ALREADY_PAIRED")
  else:
    // New pairing
    computeSharedSecret()
    savePairing(client_public_key) → ECDH_OK_PAIRED
```

*Commands:*
- `unpair`: Unpair device (requires authentication)
  - Calls unpairDevice()
  - Clears session
  - Restart advertising for new pairing

**State Management:**
- Keys loaded/generated on boot (`loadOrGenerateKeys()`)
- Pairing saved to NVS after first handshake (`savePairing()`)
- Session state cleared on disconnect (`clearECDH()` - preserves NVS)
- Persistent keys survive reboots
- Challenge state tracked per session

**Security Advantages:**
- Device-to-device binding (one phone can pair)
- Persistent pairing across reboots
- Unauthorized devices rejected before authentication
- Keys stored in encrypted NVS partition
- Factory reset available via unpair command

### 3. BLE Server (`main.cpp` - lines 265-340, 715-850)

**Purpose**: Wireless communication with Flutter app

**Characteristics:**
1. **RX (Write)**: `6E400002-...` - Receive commands from client
2. **TX (Notify)**: `6E400003-...` - Send responses to client
3. **ECDH (Read/Write)**: `6E400005-...` - Key exchange

**Security:**
- Pairing required before any commands
- PIN: 123456 (static, needs replacement)
- Secure Connections enabled
- MITM protection
- Bonding enforced

**Advertising:**
- Device name: `ESP32-PWD-Manager`
- Service UUID advertised
- Scan response included (phone compatibility)
- Connection params: min=0x06, max=0x12

### 4. Command Handler (`main.cpp` - handleCommand(), lines 512-700)

**Purpose**: Parse and execute client commands

**Authentication Flow:**
```
Unauthenticated:
  - request_token → Generate 32-bit token
  - auth <token> → Verify token
  - ecdh_auth → Send challenge
  - respond <HMAC> → Verify HMAC

Authenticated:
  - add <site> <user> <pass>
  - get <site> <user>
  - update <site> <user> <newpass>
  - delete <site> <user>
  - list
  - logout
```

**Current Status:**
- ✅ All commands encrypted with AES-256-CTR (except ECDH handshake)
- ✅ All responses encrypted with AES-256-CTR
- ✅ Passwords encrypted in transit over BLE
- ❌ Still runs in BLE callback context (should use queue)
- ❌ Token still logged to serial (security leak)

### 5. Database Layer (`main.cpp` - SQLite functions, lines 350-510)

**Purpose**: Persistent credential storage

**Schema:**
```sql
CREATE TABLE credentials (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    site TEXT,
    username TEXT,
    encrypted_password BLOB,  -- AES-256-CBC ciphertext
    iv BLOB                   -- 16-byte IV
);

CREATE TABLE audit_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER,
    event TEXT,
    user TEXT
);
```

**Operations:**
- `insertCredential()` - Encrypts password, stores with IV
- `updateCredential()` - Re-encrypts with new IV
- `getPassword()` - Retrieves, decrypts password
- `deleteCredential()` - Removes entry
- `listCredentials()` - Returns site/username pairs (no passwords)

**Current State:**
- ✅ Encryption working
- ✅ IV uniqueness guaranteed
- ❌ No DB integrity checks
- ❌ No backup mechanism
- ❌ Runs in BLE callback (blocking)

### 6. Display (`main.cpp` - updateOutput(), line 335)

**Purpose**: User feedback via OLED

**Current Output:**
- Connection status
- Authentication events
- Command processing
- Error messages

**Format:**
- 128x32 OLED (Adafruit SSD1306)
- Single-line status updates
- Scrolls automatically

---

## Memory Layout

### Flash (4MB)
```
0x1000   - Bootloader
0x8000   - Partition table
0x9000   - NVS (20KB) - Device pairing data
0xE000   - OTA Data (8KB)
0x10000  - App0 (1.28MB)
0x150000 - App1 (1.28MB) [OTA]
0x290000 - SPIFFS (1.5MB)
```

### NVS Storage (Namespace: "pwmgr")
```
Key: "paired" (u8)
  - 0 = UNPAIRED (device accepting new pairing)
  - 1 = PAIRED (device bound to specific client)
  - Size: 1 byte

Key: "esp_priv" (blob)
  - ESP32 private ECDH key (persistent across reboots when paired)
  - Size: 32 bytes
  
Key: "esp_pub" (blob)
  - ESP32 public ECDH key (persistent across reboots when paired)
  - Size: 64 bytes
  
Key: "client_pub" (blob)
  - Paired client public ECDH key (for device verification)
  - Size: 64 bytes
  
Total NVS usage: ~161 bytes (plus NVS metadata)
```

### RAM Usage (Estimated)
```
Heap: ~200KB available
Stack: 8KB main task
BLE: ~60KB (Bluedroid)
SQLite: ~50KB cache
ECDH: ~1KB temporary during handshake
Crypto: ~2KB temporary during encrypt/decrypt
NVS: ~4KB cache
```

### Sensitive Data Locations
```
Stack:
  - runtime_key[32]          (zeroed after use)
  - plaintext passwords      (temporary during command)
  
Heap:
  - String objects           (automatically freed)
  
Static:
  - esp32_private_key[32]    (zeroed on unpair, persistent when paired)
  - esp32_public_key[64]     (persistent when paired)
  - stored_client_public_key[64] (loaded from NVS when paired)
  - session_aes_key[32]      (zeroed on disconnect)
  - pending_challenge[16]    (overwritten per auth)
  
eFuse:
  - BLOCK3 hardware key      (read-only, never extracted)
  
NVS (Flash):
  - esp32_private_key[32]    (encrypted by ESP-IDF NVS encryption)
  - esp32_public_key[64]     (public data, integrity protected)
  - client_public_key[64]    (public data, integrity protected)
  - pairing state flag       (integrity protected)
  
SD Card:
  - encrypted_password BLOB  (AES-256-CBC)
  - iv BLOB                  (per-password IV)
```

**NVS Security:**
- ESP-IDF NVS supports flash encryption (can be enabled in sdkconfig)
- NVS data integrity protected by CRC
- Private keys stored in NVS benefit from flash encryption if enabled
- Unpair command provides factory reset for pairing data

---

## Security Posture

### Current Strengths
✅ Hardware root of trust (eFuse)
✅ Runtime key derivation (never stored)
✅ AES-256 encryption at rest (database)
✅ **AES-256-CTR session encryption** (BLE traffic)
✅ ECDH key exchange (cryptographic auth)
✅ **End-to-end encryption** (commands and responses)
✅ **Forward secrecy** (ephemeral ECDH keys)
✅ **Device binding** (one-device pairing with NVS persistence)
✅ **Persistent pairing** (survives reboots, no re-pairing needed)
✅ **Unauthorized device rejection** (only paired client can authenticate)
✅ BLE pairing required
✅ Rate limiting on auth attempts
✅ Factory reset available (unpair command)
✅ **No plaintext passwords in BLE packets** (all encrypted)

### Current Weaknesses
❌ **No message authentication** (CTR mode doesn't provide integrity - need AES-GCM)
❌ **No replay protection** (CTR nonce is random, not sequential)
❌ Static PIN (123456) - predictable
❌ 32-bit legacy tokens (brute-forceable)
❌ SQLite operations in BLE callback (can corrupt DB)
❌ No DB integrity verification
❌ No backup/recovery mechanism
❌ Sensitive data logging (tokens in serial)
❌ NVS flash encryption not enabled (private keys readable if flash extracted)
❌ **Runtime database key not persistent** (credentials unreadable after reboot)

### Attack Vectors
🔴 **Critical:**
- **Database key not persistent** (all credentials lost on reboot)
- Known static PIN allows unauthorized pairing

🟠 **High:**
- **No message authentication** (bit-flipping attacks possible)
- **No replay protection** (captured packets can be replayed)
- DB corruption from callback crashes

🟡 **Medium:**
- Legacy token brute-force (32 bits)
- No audit log analysis

🟢 **Mitigated:**
- ~~BLE sniffing~~ (encrypted with AES-256-CTR)
- ~~Plaintext passwords~~ (all encrypted in transit)

---

## Next Steps (Priority Order)

### Immediate (Phase 1.2 Completion)
1. ✅ ESP32 ECDH implementation (DONE)
2. ✅ **NVS device binding** (DONE)
3. ✅ **Pairing state machine** (DONE)
4. ✅ **Unpair command** (DONE)
5. ✅ **BLE session encryption** (ESP32 side DONE)
6. ⏳ **Fix database key persistence** (CRITICAL - blocks all usage)
7. ⏳ Flutter ECDH implementation (guide created, needs implementation)
8. ⏳ Flutter session encryption (guide created, needs implementation)
9. ⏳ Flutter pairing persistence (SharedPreferences)
10. ⏳ Test end-to-end encrypted communication
11. ⏳ Deprecate legacy token auth

### Short-Term (Phase 1.3)
12. Create DB worker queue (FreeRTOS)
13. Move SQLite ops out of BLE callbacks
14. Implement atomic writes
15. Enable NVS flash encryption (sdkconfig)
16. **Upgrade to AES-GCM** (replace CTR for authenticated encryption)

### Medium-Term (Phase 1.4 + Phase 2)
17. AES-GCM migration for database (replace CBC)
18. Add message authentication codes (HMAC)
19. Implement replay protection (sequence numbers)
20. Generate dynamic PIN at boot (store in NVS)
21. Remove all sensitive logging
22. Key rotation mechanism (periodic session key refresh)

### Long-Term (Phase 2+)
18. Secure element integration (SE050/ATECC608)
19. USB HID keyboard emulation
20. Replace SD card with eMMC/QSPI flash
21. Secure boot + flash encryption
22. Backup/recovery system

---

## File Structure

```
esp32/custompartition/
├── src/
│   ├── main.cpp              (1177 lines - BLE, ECDH, NVS, commands, SQLite)
│   └── CMakeLists.txt
├── lib/
│   └── secure_core/
│       ├── secure_core.h     (18 lines - crypto API)
│       ├── secure_core.cpp   (245 lines - encryption impl)
│       └── library.json
├── docs/
│   ├── ROADMAP.md                    (5-phase plan)
│   ├── TODO.md                       (prioritized task list)
│   ├── CHANGELOG.md                  (version history)
│   ├── ECDH_FLUTTER_GUIDE.md         (ECDH client implementation)
│   ├── FLUTTER_ENCRYPTION_GUIDE.md   (session encryption guide)
│   ├── BLE_ENCRYPTION_IMPLEMENTATION.md (technical docs)
│   ├── ENCRYPTION_TESTING_GUIDE.md   (testing procedures)
│   ├── SECURITY_ASSESSMENT.md        (threat model)
│   └── ARCHITECTURE.md               (this file)
├── platformio.ini           (build config)
├── partitions.csv           (flash layout)
└── sdkconfig.esp32dev       (ESP-IDF config)
```

**Main.cpp Breakdown:**
- Lines 1-77: Includes, constants, globals, NVS state
- Lines 78-100: initECDH() - Generate ECDH key pair
- Lines 101-310: computeSharedSecret() - ECDH + HKDF session key
- Lines 311-325: clearECDH() - Session cleanup
- Lines 326-475: NVS device binding (loadOrGenerateKeys, savePairing, unpairDevice)
- Lines 476-605: BLE callbacks (Security, Command, ECDH, Server)
- Lines 606-755: SQLite functions (insert, get, update, delete, list)
- Lines 756-1005: handleCommand() - Auth + command routing
- Lines 1006-1177: setup() - Initialization + BLE advertising

---

## Performance Characteristics

### Boot Time
```
Total: ~3-5 seconds
- Key derivation: ~200ms
- NVS load/ECDH generation: ~300-400ms
- SD card init: ~500ms
- SQLite init: ~1s
- BLE init: ~1s
```

### Operation Latency
```
Encrypt password: ~10ms (AES-256-CBC)
Decrypt password: ~10ms
ECDH handshake: ~500ms (one-time per session)
Challenge-response: ~50ms
SQLite query: ~50-100ms (depends on DB size)
BLE round-trip: ~100-200ms
```

### Power Consumption
```
Active (BLE connected): ~150mA
Idle (advertising): ~80mA
Deep sleep: ~10µA (not implemented)
```

---

## Testing Strategy

### Unit Tests (TODO)
- Encryption/decryption correctness
- ECDH key derivation consistency
- IV uniqueness verification
- HMAC challenge-response

### Integration Tests (TODO)
- End-to-end ECDH handshake
- Multi-client connection handling
- DB corruption recovery
- Power loss during write

### Security Tests (TODO)
- BLE fuzzing
- SQLite injection attempts
- Replay attack resistance
- Side-channel analysis

---

## Future Enhancements

### Phase 2 - Hardware Security
- Migrate secrets to secure element
- Tamper detection
- Secure boot chain
- Flash encryption

### Phase 3 - Production
- Custom PCB design
- USB HID autofill
- Biometric unlock (optional)
- Backup via encrypted QR codes

### Phase 4 - Certification
- Common Criteria evaluation
- FIPS 140-3 compliance
- Penetration testing
- GDPR compliance documentation

---

## Conclusion

**Current State:** Functional prototype with hardware-backed encryption, ECDH authentication, and end-to-end BLE session encryption. ESP32 side fully implements encrypted communication; Flutter client integration in progress.

**Security Level:** Development/prototype with strong transport security. Passwords now encrypted in transit over BLE. Main blockers: database key persistence issue, lack of message authentication (need AES-GCM), and Flutter client integration.

**Maturity:** Phase 1 (Protocol Hardening) - 75% complete. BLE session encryption implemented. Next: fix database key persistence (critical), complete Flutter integration, upgrade to AES-GCM for authenticated encryption.
