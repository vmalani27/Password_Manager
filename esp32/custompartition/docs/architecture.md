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
6. **Transport Security**: BLE pairing + ECDH session encryption (future)

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
BLE Command "get site username"
    ↓
ECDH Challenge-Response Authentication
    ↓
SQLite Query → {encrypted_password, IV}
    ↓
AES-256-CBC Decrypt (using runtime_key)
    ↓
Send plaintext password over BLE (TODO: encrypt with session_key)
```

#### ECDH Authentication Flow:
```
1. Client connects → BLE pairing (PIN: 123456)
2. Client reads ESP32 public key (ECDH characteristic)
3. Client generates ephemeral key pair
4. Client sends public key → ESP32
5. Both compute shared_secret = ECDH(private_own, public_other)
6. Both derive session_key = HKDF(shared_secret, salt, info)
7. Client sends "ecdh_auth"
8. ESP32 sends random challenge (16 bytes)
9. Client computes HMAC(session_key, challenge)
10. ESP32 verifies HMAC → AUTH OK
11. Session authorized for commands
```

---

## Current Implementation Status

### ✅ Phase 0 - Cleanup & Stabilization (COMPLETE)

**0.1 - Repository Structure**
- ✅ Migrated from Arduino IDE to PlatformIO
- ✅ Created docs: ROADMAP.md, TODO.md, CHANGELOG.md, PROTOCOL.md
- ✅ Organized: /src, /lib, /docs, /test

**0.2 - Codebase Clean Boot**
- ✅ Removed async key manager (startKeyManagerTask)
- ✅ Made key derivation synchronous (deriveRuntimeKey)
- ✅ Clean compilation under PlatformIO
- ✅ Removed Arduino-IDE legacy code

**0.3 - Stability Fixes**
- ✅ Removed buggy connection timeout logic (waiting for ECDH)
- ✅ Fixed BLE callback crashes
- ✅ Removed plaintext token logging (partial - still logs token value)

### ✅ Phase 1.1 - Cryptographic Security (COMPLETE)

**Runtime Key System**
- ✅ eFuse BLOCK3 reader (256-bit hardware key)
- ✅ HMAC-SHA256 key derivation with random challenge
- ✅ Synchronous initialization (no race conditions)
- ✅ Zero sensitive buffers on use
- ✅ Runtime key used for password encryption/decryption

**Database Encryption**
- ✅ AES-256-CBC encryption for passwords
- ✅ Random IV generation per password
- ✅ Encrypted storage: {encrypted_password BLOB, iv BLOB}
- ✅ PKCS7 padding implementation

### 🔶 Phase 1.2 - Authentication Security (MOSTLY COMPLETE)

**ECDH Implementation (ESP32 Side - COMPLETE)**
- ✅ secp256r1 (NIST P-256) elliptic curve
- ✅ Ephemeral key pair generation using mbedTLS
- ✅ Public key exposure via BLE characteristic
- ✅ Client public key reception
- ✅ Shared secret computation
- ✅ HKDF-like session key derivation (manual HMAC-based)
- ✅ Challenge-response authentication
- ✅ ECDH state cleanup on disconnect
- ✅ **NVS device binding** (persistent pairing across reboots)
- ✅ **Pairing state machine** (UNPAIRED → PAIRED)
- ✅ **Device verification** (reject unauthorized clients when paired)
- ✅ **Unpair command** (factory reset for pairing)

**ECDH Implementation (Flutter Side - NOT STARTED)**
- ❌ PointyCastle ECDH integration
- ❌ Key pair generation
- ❌ Handshake flow
- ❌ Challenge-response client
- ❌ Session key storage
- ❌ Pairing persistence (SharedPreferences)

**BLE Security**
- ✅ BLE pairing with static PIN (123456) - needs replacement
- ✅ Secure connections (SC) enabled
- ✅ MITM protection
- ✅ Bonding required
- ✅ Device binding (ECDH key pairing in NVS)
- ❌ Dynamic passkey generation (TODO)

**Legacy Auth (Still Active)**
- ✅ Token-based authentication (32-bit, weak)
- ✅ Rate limiting (5 attempts → 1 minute lockout)
- ✅ Session tokens
- 🔄 Will be deprecated after ECDH is fully working

### ❌ Phase 1.3 - Credential Lifecycle (NOT STARTED)

- ❌ DB worker queue (move SQLite ops out of BLE callbacks)
- ❌ Atomic backups
- ❌ Journaling
- ❌ Randomness testing for IVs

### ❌ Phase 1.4 - UX & Protocol (PARTIAL)

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
secure_core.h  (18 lines)  - API definitions
secure_core.cpp (245 lines) - Implementation
```

**Functions:**
- `initKeyManager()` - Initialize crypto subsystem
- `deriveRuntimeKey()` - Derive encryption key from eFuse
- `encrypt_password()` - AES-256-CBC encryption
- `decrypt_password()` - AES-256-CBC decryption
- `generate_iv()` - Random IV generation

**Dependencies:**
- mbedTLS (ESP-IDF built-in)
- ESP eFuse API

**Security Properties:**
- Runtime key never persisted to flash/SD
- eFuse read-only (hardware-enforced)
- IV uniqueness per password
- PKCS7 padding

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

**Current Issues:**
- Runs in BLE callback context (should use queue)
- Sends plaintext passwords over BLE (needs encryption)
- Token still logged to serial (security leak)

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
✅ AES-256 encryption at rest
✅ ECDH key exchange (cryptographic auth)
✅ **Device binding** (one-device pairing with NVS persistence)
✅ **Persistent pairing** (survives reboots, no re-pairing needed)
✅ **Unauthorized device rejection** (only paired client can authenticate)
✅ BLE pairing required
✅ Rate limiting on auth attempts
✅ Factory reset available (unpair command)

### Current Weaknesses
❌ Plaintext passwords over BLE (session encryption not implemented)
❌ Static PIN (123456) - predictable
❌ 32-bit legacy tokens (brute-forceable)
❌ SQLite operations in BLE callback (can corrupt DB)
❌ No DB integrity verification
❌ No backup/recovery mechanism
❌ Sensitive data logging (tokens, passwords in serial)
❌ NVS flash encryption not enabled (private keys readable if flash extracted)

### Attack Vectors
🔴 **Critical:**
- BLE sniffing can capture plaintext passwords (after pairing)
- Known static PIN allows unauthorized pairing

🟠 **High:**
- No device binding (any paired phone can access)
- DB corruption from callback crashes

🟡 **Medium:**
- Legacy token brute-force (32 bits)
- No audit log analysis

---

## Next Steps (Priority Order)

### Immediate (Phase 1.2 Completion)
1. ✅ ESP32 ECDH implementation (DONE)
2. ✅ **NVS device binding** (DONE)
3. ✅ **Pairing state machine** (DONE)
4. ✅ **Unpair command** (DONE)
5. ⏳ Flutter ECDH implementation (guide created, needs implementation)
6. ⏳ Flutter pairing persistence (SharedPreferences)
7. ⏳ Test end-to-end ECDH auth with device binding
8. ⏳ Deprecate legacy token auth

### Short-Term (Phase 1.3)
9. Create DB worker queue (FreeRTOS)
10. Move SQLite ops out of BLE callbacks
11. Implement atomic writes
12. Enable NVS flash encryption (sdkconfig)

### Medium-Term (Phase 1.4 + Phase 2)
13. AES-GCM migration (replace CBC)
14. Command encryption using session_key
15. Remove plaintext password transmission
16. Generate dynamic PIN at boot (store in NVS)
17. Remove all sensitive logging

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
│   ├── ROADMAP.md           (5-phase plan)
│   ├── TODO.md              (prioritized task list)
│   ├── CHANGELOG.md         (version history)
│   ├── ECDH_FLUTTER_GUIDE.md (client implementation)
│   └── ARCHITECTURE.md      (this file)
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

**Current State:** Functional prototype with hardware-backed encryption and ECDH authentication framework in place. ESP32 side is ready for cryptographic authentication; Flutter client implementation is next.

**Security Level:** Development/prototype. Not suitable for production use yet due to plaintext password transmission and missing defense-in-depth layers.

**Maturity:** Phase 1 (Protocol Hardening) - 60% complete. On track for Phase 2 (Hardware Hardening) after ECDH client implementation and DB worker queue.
