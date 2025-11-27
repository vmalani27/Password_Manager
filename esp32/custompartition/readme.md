# ESP32 Password Manager

Hardware-based password vault with cryptographic authentication using ESP32, BLE, and Flutter.

## Project Overview

**Status:** Sprint 3 In Progress - Database Worker Queue  
**Version:** 0.4.0-alpha  
**Last Updated:** November 25, 2025

### What This Project Does

Secure credential storage on ESP32 hardware, accessed wirelessly via Flutter mobile app. Credentials are encrypted with hardware-backed keys and protected by ECDH cryptographic authentication.

### Key Features

**Implemented (Sprint 3 - Nov 25, 2025):**
- **Modular Architecture** - Refactored from 1241 lines monolithic to 5 manager modules
- **AES-256-CBC Encryption** - eFuse-derived hardware keys for database encryption
- **ECDH Key Exchange** - secp256r1 (P-256) curve with device binding
- **Two-Layer Authentication** - ECDH device pairing + token-based session authorization
- **Device Binding** - NVS persistent pairing, one phone ↔ one ESP32
- **SQLite Encrypted Storage** - Credentials encrypted at rest on SD card
- **BLE Security Protocol** - SMP pairing + ECDH characteristic + command/response model
- **Session Management** - Fast unlock flow with check_pairing optimization (~500ms)
- **Timeout Management** - 3-min session timeout, 5-min connection timeout with auto-clear
- **OS-Level Unpair Detection** - Auto-recovery when user forgets device in Bluetooth settings
- **Physical Unpair Button** - Hold BOOT button (GPIO0) for 3 seconds to unpair
- **Optimized Reconnection** - Skip ECDH if pairing verified (4-6x faster)
- **Partition Viewer** - Boot-time flash partition and memory diagnostics
- **Database Worker Queue** - FreeRTOS task prevents SQLite corruption from concurrent BLE callbacks
- **Quote-Aware Parser** - Support for multi-word service names: `add "Google Mail" user pass`
- **Emergency Recovery** - `db_reset` command for corrupted database recovery
- **Secure Token Generation** - Uses `esp_random()` hardware RNG (32-bit tokens, upgrading to 128-bit)

**In Progress:**
- Flutter client implementation (reference code provided in flutreadme.md)
- App resynchronization testing (ghost connections, crash recovery, OS unpair scenarios)
- Token size upgrade (32-bit → 128-bit for enhanced security)

**Planned (Sprint 3 Remaining - Dec 2025):**
- Remove plaintext passwords over BLE (encrypt with session key)
- AES-GCM migration (authenticated encryption with integrity checks)
- Dynamic PIN generation (per-pairing session displayed on OLED)

## Quick Start

### Prerequisites

- PlatformIO Core 6.1+
- ESP32 DevKit (4MB flash)
- SD card module + microSD card
- SSD1306 OLED display (128x32)

### Hardware Connections

```
ESP32          Component
GPIO5     -->  SD Card CS
GPIO18    -->  SD Card SCK
GPIO19    -->  SD Card MISO
GPIO23    -->  SD Card MOSI
SDA       -->  OLED SDA
SCL       -->  OLED SCL
```

### Build and Flash

```bash
cd esp32/custompartition
pio run                  # Build firmware
pio run --target upload  # Flash to ESP32
pio device monitor       # View serial output
```



### First Boot

1. ESP32 generates encryption keys from eFuse
2. Initializes SD card and SQLite database
3. Generates ECDH key pair
4. Starts BLE advertising as "ESP32-PWD-Manager"
5. Displays "Device UNPAIRED" on OLED
6. **Note:** Hold BOOT button (GPIO0) for 3 seconds at any time to unpair device

### Pairing with Flutter App

1. Open Flutter app, scan for devices
2. Select "ESP32-PWD-Manager"
3. Enter PIN: `123456` (default, will be randomized in future)
4. App performs ECDH handshake
5. Device bound - only this phone can access passwords

## Project Structure

### Refactored Modular Architecture (Nov 2025)

```
esp32/custompartition/
├── include/
│   ├── config.h              (58 lines - All constants, UUIDs, pin definitions)
│   ├── crypto_manager.h      (80 lines - ECDH, NVS device binding)
│   ├── ble_manager.h         (120 lines - BLE server, callbacks)
│   ├── db_manager.h          (96 lines - SQLite + FreeRTOS worker queue)
│   └── ui_manager.h          (40 lines - OLED display)
├── src/
│   ├── main.cpp              (169 lines - Setup + partition diagnostics + worker task)
│   ├── crypto_manager.cpp    (380 lines - ECDH implementation)
│   ├── ble_manager.cpp       (820 lines - BLE + command handling + quote parser + db_reset)
│   ├── db_manager.cpp        (527 lines - Database operations + worker queue)
│   └── ui_manager.cpp        (80 lines - Display management)
├── lib/
│   └── secure_sd/            (Legacy encryption library)
│       ├── secure_sd.h
│       └── secure_sd.cpp
├── docs/
│   ├── ARCHITECTURE.md       (Technical design)
│   ├── ROADMAP.md            (Long-term plan)
│   ├── TODO.md               (Current sprint backlog)
│   └── ECDH_FLUTTER_GUIDE.md (Mobile app integration)
├── flutreadme.md             (Flutter client protocol reference)
├── platformio.ini            (Build config: -Iinclude, lib_ldf_mode=deep+)
└── partitions.csv            (Custom flash layout)
└── partitions.csv            (Flash layout)
```

## Documentation

### For Users
- **README.md** (this file) - Quick start and overview

### For Developers
- **[ARCHITECTURE.md](docs/ARCHITECTURE.md)** - System design, security model, component details
- **[ROADMAP.md](docs/ROADMAP.md)** - Product roadmap, phased development plan
- **[TODO.md](docs/TODO.md)** - Current sprint tasks, prioritized backlog
- **[REFACTORING_GUIDE.md](docs/REFACTORING_GUIDE.md)** - Module architecture and migration guide

### For Integration
- **[ECDH_FLUTTER_GUIDE.md](docs/ECDH_FLUTTER_GUIDE.md)** - Flutter client implementation guide
- **[flutreadme.md](flutreadme.md)** - Complete ESP32 command protocol reference with source code examples

## Development Workflow

### Sprint Structure (2-week sprints)

**Sprint Planning:**
1. Review TODO.md for prioritized tasks
2. Select tasks from current tier (S/A/B/C)
3. Break down into sub-tasks if needed

**Daily Development:**
1. Pick highest priority unstarted task
2. Implement and test locally
3. Update TODO.md status
4. Commit with descriptive message

**Sprint Review:**
1. Test all implemented features
2. Update ARCHITECTURE.md with changes
3. Move completed items to "Completed" section
4. Plan next sprint

### Task Priority Levels

- **TIER S (Critical)** - Must complete before adding new features
- **TIER A (High)** - Security improvements, no workarounds acceptable
- **TIER B (Medium)** - Code quality, maintainability improvements
- **TIER C (Low)** - Nice-to-have, polish items
- **TIER D (Future)** - Long-term enhancements, Phase 2+

## Current Sprint Goals

**Sprint 2 (Nov 15 - Nov 29, 2025) - COMPLETED**

Completed:
- ECDH key exchange implementation
- Token-based session authentication
- Device binding with NVS persistence
- Code refactoring into modular architecture (1241 → 166 lines main.cpp, 92.7% reduction)
- Successfully compiled refactored code (Flash: 41.7%, RAM: 14.3%)
- Fixed BLE manager const pointer issues
- Added ECDH session clearing on reconnection
- Implemented partition diagnostics at boot
- Documented complete command protocol in flutreadme.md

**Sprint 3 (Nov 30 - Dec 13, 2025) - IN PROGRESS**

Completed (Nov 25):
- ✅ Database worker queue (US-001, 5 points)
  - FreeRTOS task with command queue (10 capacity)
  - Thread-safe public API with task notifications
  - Prevents SQLite corruption from concurrent BLE callbacks
  - Queue depth: 10 commands, worker priority: 5, stack: 8KB
- ✅ Quote-aware command parser
  - Supports: `add "Google Mail" user@example.com password`
  - Backward compatible with non-quoted strings
- ✅ Fixed LIST output format (removed pipe separator)
  - Old: `instagram | vmalani27.github.io\n`
  - New: `instagram vmalani27.github.io\n`
- ✅ Emergency recovery command: `db_reset`
  - Deletes corrupted database files
  - Recreates fresh database
  - No auth required (emergency use only)
- ✅ Updated COMMAND_REFERENCE.md with best practices

In Progress:
- App resynchronization testing (ghost connections, crash recovery scenarios)
- Flutter client integration testing

Remaining:
- US-002: Remove plaintext passwords over BLE (3 points)
- US-003: Strengthen session tokens (1 point)
- AES-GCM migration (5 points)
- Dynamic PIN generation (3 points)

See [TODO.md](docs/TODO.md) for detailed task list.

## Security Architecture

### Multi-Layer Defense Model

**Layer 1 - Hardware Root of Trust**
- eFuse BLOCK3 (256-bit device-unique key burned at manufacturing)
- Read-only protection (cannot be changed after burning)
- Accessible only by secure ROM bootloader
- Used as master key derivation input

**Layer 2 - Runtime Key Derivation**
- HMAC-SHA256(eFuse_key, "ESP32-PWD-RUNTIME-KEY") → 256-bit runtime key
- Key derived on every boot, never stored in flash or RAM permanently
- Used exclusively for credential encryption/decryption
- Automatically cleared from memory after crypto operations

**Layer 3 - BLE Transport Security**
- BLE Security Manager Protocol (SMP) with Secure Connections (LE SC)
- Pairing with static PIN: `123456` (will be dynamic in Phase 1.4)
- MITM protection enabled (man-in-the-middle attack prevention)
- Bonding required (persistent encrypted link layer)
- 128-bit AES-CCM encryption at link layer (automatic)

**Layer 4 - ECDH Device Binding**
- **Curve**: secp256r1 (NIST P-256, 256-bit security level)
- **Key Generation**: mbedTLS `mbedtls_ecdh_gen_public()` on first boot
- **Shared Secret**: ECDH(ESP32_private, Client_public) → 256-bit shared secret
- **Session Key Derivation**: 
  ```
  PRK = HMAC-SHA256(salt="BLE_PASSWORD_MGR", shared_secret)
  session_key = HMAC-SHA256(PRK, "SESSION" || 0x01)
  ```
- **Device Binding**: Client public key stored in NVS after first pairing
- **Pairing Persistence**: Keys survive ESP32 reboot, client can reconnect without re-pairing
- **Unpair Mechanisms**: 
  1. `unpair` command (from authorized client)
  2. Physical button (hold GPIO0 for 3 seconds)
  3. Auto-recovery (OS Bluetooth settings "Forget Device")

**Layer 5 - Session Authorization**
- Token-based authentication (32-bit random hex token)
- Token valid for single auth attempt
- Session timeout: 3 minutes of inactivity → auto-lock (sends SESSION_TIMEOUT)
- Connection timeout: 5 minutes of inactivity → force disconnect
- Failed auth lockout: 3 failures → 1 minute lockout
- `sessionAuthorized` flag gates all CRUD operations

**Layer 6 - Database Encryption (At Rest)**
- Algorithm: AES-256-CBC (PKCS#7 padding)
- Key: Runtime key (derived from eFuse, Layer 2)
- IV: 16 bytes random per password entry (stored alongside ciphertext)
- Storage format: SQLite BLOB columns {encrypted_password, iv}
- Metadata: Service name and username in plaintext (for LIST command)

### Security Properties

**Confidentiality:**
- Passwords encrypted at rest (AES-256-CBC, eFuse-derived key)
- Passwords transmitted in plaintext over BLE encrypted link (future: add AEAD layer)
- ECDH prevents passive eavesdropping (perfect forward secrecy)

**Integrity:**
- Currently: No integrity checks on stored passwords (future: AES-GCM with auth tags)
- BLE link layer provides automatic integrity via AES-CCM

**Authentication:**
- Device binding via ECDH (only paired client can connect)
- Session authorization via token challenge-response
- BLE pairing via PIN (MITM protection)

**Availability:**
- Unpair recovery via physical button
- Session timeout prevents indefinite lockout
- Auto-recovery from OS-level unpair

**Threat Model:**
- Protected: Casual attacker, stolen device (powered off), BLE eavesdropping
- Protected: Malware on phone (requires ECDH key pair to authenticate)
- Partial Protection: Physical access to powered device (eFuse readable via JTAG/flash dump)
- Not Protected: Nation-state attacks, hardware reverse engineering, secure element bypass

### Cryptographic Protocols Used

**1. BLE Security Manager Protocol (SMP)**
- **Standard**: Bluetooth Core Spec 5.0, Vol 3, Part H
- **Implementation**: ESP-IDF BLE stack (NimBLE or Bluedroid)
- **Features Used**:
  - LE Secure Connections (ECDH P-256 for pairing)
  - Passkey Entry (static PIN 123456)
  - MITM protection (authenticated pairing)
  - Bonding (persistent keys across connections)
  - AES-CCM link layer encryption (automatic post-pairing)

**2. ECDH Key Exchange (Elliptic Curve Diffie-Hellman)**
- **Standard**: NIST FIPS 186-4, SEC 2 v2.0
- **Curve**: secp256r1 (aka NIST P-256, prime256v1)
  - Prime field: p = 2^256 - 2^224 + 2^192 + 2^96 - 1
  - Generator point G with order n ≈ 2^256
  - Cofactor: 1 (prime order subgroup)
- **Implementation**: mbedTLS 2.x (`mbedtls_ecdh_context`)
- **Key Derivation**:
  ```
  ESP32:  d_esp (private), Q_esp = d_esp * G (public)
  Client: d_cli (private), Q_cli = d_cli * G (public)
  Shared: Z = d_esp * Q_cli = d_cli * Q_esp (32 bytes)
  ```
- **Security Level**: ~128-bit (due to best known attacks on P-256)

**3. HMAC-Based Key Derivation (HKDF-like)**
- **Standard**: Simplified HKDF (RFC 5869)
- **Hash Function**: HMAC-SHA256 (RFC 2104 + FIPS 180-4)
- **Extract Phase**: `PRK = HMAC-SHA256(salt="BLE_PASSWORD_MGR", IKM=shared_secret)`
- **Expand Phase**: `OKM = HMAC-SHA256(PRK, info="SESSION" || 0x01)`
- **Output**: 32-byte session key (256 bits)

**4. AES-256-CBC (Credential Encryption)**
- **Standard**: NIST FIPS 197 (AES) + NIST SP 800-38A (CBC mode)
- **Implementation**: mbedTLS `mbedtls_aes_context`
- **Parameters**:
  - Key size: 256 bits (32 bytes from eFuse-derived runtime key)
  - Block size: 128 bits (16 bytes)
  - IV: 128 bits random per encryption (16 bytes)
  - Padding: PKCS#7 (RFC 2315)
- **Encrypt**: `C = AES-256-CBC_Encrypt(K, IV, Pad(P))`
- **Decrypt**: `P = Unpad(AES-256-CBC_Decrypt(K, IV, C))`

**5. HMAC-SHA256 (Key Derivation & Future Auth)**
- **Standard**: RFC 2104 (HMAC), FIPS 180-4 (SHA-256)
- **Implementation**: mbedTLS `mbedtls_md_hmac()`
- **Usage**:
  - Runtime key derivation (eFuse → encryption key)
  - ECDH shared secret → session key (HKDF extract/expand)
  - Future: Command authentication tags

**6. NVS (Non-Volatile Storage) Security**
- **Standard**: ESP-IDF NVS API (proprietary flash wear-leveling)
- **Encryption**: Optional NVS encryption (not enabled in current build)
- **Data Stored**:
  - `paired` flag (1 byte): Pairing state
  - `esp_priv` (32 bytes): ESP32 ECDH private key
  - `esp_pub` (64 bytes): ESP32 ECDH public key (X||Y coordinates)
  - `client_pub` (64 bytes): Paired client's ECDH public key
- **Security Note**: Keys stored in plaintext in NVS partition (future: enable NVS encryption)

---

## Protocol Implementation Summary

### Complete Authentication Flow

**First-Time Pairing:**
```
1. Phone discovers "ESP32-PWD-Manager" via BLE advertising
2. Phone initiates BLE pairing (enter PIN: 123456)
3. BLE SMP negotiates encrypted link (AES-CCM at link layer)
4. Phone reads ECDH characteristic → gets ESP32 public key (64 bytes)
5. Phone generates its own ECDH key pair (secp256r1 curve)
6. Phone computes shared_secret = ECDH(phone_private, ESP32_public)
7. Phone writes its public key to ECDH characteristic
8. ESP32 receives phone's public key, computes same shared_secret
9. ESP32 derives session_key = HKDF(shared_secret, salt, info)
10. ESP32 saves pairing to NVS: {paired=1, esp_priv, esp_pub, client_pub}
11. ESP32 responds: "ECDH_OK_PAIRED"
12. Phone saves pairing locally: {ESP32_pub, phone_priv, phone_pub}
13. Phone sends: "request_token" → ESP32 responds: "TOKEN:ABC12345"
14. Phone sends: "auth ABC12345" → ESP32 responds: "AUTH OK"
15. Session authorized → CRUD commands now work
```

**Optimized Reconnection (Paired Device):**
```
1. Phone connects to ESP32 via BLE (reuses existing bond, ~100ms)
2. Phone sends: "check_pairing"
3. ESP32 responds: "PAIRED:<128-hex-chars>" (stored phone public key)
4. Phone compares received key with local key:
     - Match: Skip ECDH handshake ✓
     - Mismatch: Do full ECDH (key changed)
5. Phone sends: "request_token" → "TOKEN:DEF67890"
6. Phone sends: "auth DEF67890" → "AUTH OK"
7. Total time: ~300-500ms (vs ~2-3s for full ECDH)
```

**Session Timeout & Unlock:**
```
1. User idle for 3 minutes → ESP32 sends "SESSION_TIMEOUT"
2. Phone shows "Device Locked" screen with "Unlock Device" button
3. User taps unlock:
     - check_pairing (verify still paired) ~50ms
     - request_token ~50ms
     - auth <token> ~100ms
4. Total unlock time: ~200-300ms
5. ESP32 responds: "AUTH OK" → session active again
```

### Command Categories

**Public Commands (No Auth Required):**
- `request_token` - Get session token for authentication
- `status` - Query connection/authorization/pairing state
- `check_pairing` - Verify pairing and get stored client key
- `force_disconnect` - Clean disconnect with state reset
- `unpair` - Delete all pairing data, generate new keys

**Authorized Commands (Session Required):**
- `auth <token>` - Authenticate using token (sets sessionAuthorized = true)
- `add <service> <username> <password>` - Store encrypted credential
- `get <service> <username>` - Retrieve and decrypt password
- `update <service> <username> <new_password>` - Update credential
- `delete <service> <username>` - Remove credential
- `list` - List all services and usernames (no passwords)
- `logout` - Clear session authorization (stay connected)

See [COMMAND_REFERENCE.md](COMMAND_REFERENCE.md) for complete command specification with examples and responses.

---

## Performance & Resource Usage

**Build Statistics (Nov 24, 2025):**
- Compilation Time: 31.08 seconds
- Flash Usage: 1,641,537 bytes (41.7% of 3.93MB)
- RAM Usage: 46,944 bytes (14.3% of 320KB)
- Code Size Reduction: 92.7% (1241 lines → 166 lines main.cpp via modularization)

**Runtime Performance:**
- First-Time Pairing: ~2-3 seconds (ECDH handshake + key derivation)
- Reconnection (Optimized): ~500ms (check_pairing + token auth)
- Session Unlock: ~300ms (check_pairing + auth)
- Password Encryption: ~50ms (AES-256-CBC + IV generation)
- Password Decryption: ~40ms (AES-256-CBC + padding removal)
- Password Retrieval: ~100ms (query + decrypt + send)
- Database Insert: ~150ms (encrypt + SQLite write + commit)

**Power Consumption (Estimated):**
- Active (BLE Connected): ~150mA @ 3.3V
- Idle (BLE Advertising): ~80mA @ 3.3V
- Deep Sleep: ~10µA @ 3.3V (not currently implemented)

---

## Known Limitations & Future Work

### Current Limitations

**Security:**
- eFuse keys readable via flash dump (attackers with physical access + tools)
- Commands transmitted in plaintext over BLE encrypted link (no AEAD layer yet)
- Static PIN for BLE pairing (123456, same for all devices)
- No secure element (keys in NVS plaintext)
- No tamper detection or secure boot

**Functionality:**
- No password generation service
- No TOTP/2FA code generation
- No cloud backup or sync
- No multi-device pairing (one phone per ESP32)
- No credential categories/folders

**Reliability:**
- Database operations run in BLE callback (blocking, can cause corruption if interrupted)
- No transaction rollback on power loss
- No wear-leveling for SD card

### Planned Improvements (Phase 1.3 - Sprint 3, Dec 2025)

1. **Database Worker Queue** - Move SQLite operations to separate task, prevent corruption
2. **AES-GCM Migration** - Replace AES-CBC with authenticated encryption
3. **Command Encryption** - Add AEAD layer using ECDH session key
4. **Dynamic PIN** - Generate per-pairing PIN, display on OLED
5. **Secure Deletion** - Zero credential memory before SQLite DELETE

### Long-Term Roadmap (Phase 2+)

- Secure element integration (ATECC608A) for tamper-resistant key storage
- Hardware v2.0 PCB with fingerprint sensor
- USB HID autofill for browser integration
- TOTP generator for 2FA codes
- Encrypted backup/restore via QR codes
- Multi-device management (pair multiple phones)

See [docs/ROADMAP.md](docs/ROADMAP.md) for complete development timeline.

---

## Contributing & Development

### Repository Structure

- **`/src`** - Modular C++ implementation (5 managers: BLE, Crypto, DB, UI, Command)
- **`/include`** - Header files with interface definitions
- **`/lib`** - Legacy encryption library (being phased out)
- **`/docs`** - Architecture, roadmap, sprint planning, guides
- **`flutreadme.md`** - Flutter integration reference with code examples
- **`COMMAND_REFERENCE.md`** - Complete protocol specification
- **`SECURITY_REQUIREMENTS.txt`** - Security model questionnaire and design decisions

### Development Workflow

1. Check [docs/TODO.md](docs/TODO.md) for current sprint tasks
2. Pick highest priority unassigned task
3. Implement following [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) design
4. Test locally with serial monitor
5. Update documentation if protocol changes
6. Commit with descriptive message

### Testing

**Manual Testing:**
```bash
pio run --target upload  # Flash firmware
pio device monitor       # Watch serial output
# Use Serial Bluetooth Terminal app to send commands
```

**Expected Boot Sequence:**
```
BLEManager: Initializing BLE...
BLE security configured (PIN: 123456)
CryptoManager: Initializing...
ECDH: Device is PAIRED (or UNPAIRED)
DBManager: Initializing SD card...
Database ready
BLE advertising started
Ready for pairing
```

---

## License & Disclaimer

**License:** MIT (see LICENSE file)

**Disclaimer:** This is alpha software for educational/personal use. Not audited for production use. Do not store critical credentials (banking, crypto wallets, master passwords) until Phase 2 security hardening is complete. Use at your own risk.

---

## Support & Contact

**Issues:** Report bugs via GitHub Issues  
**Documentation:** See `/docs` folder for detailed specifications  
**Flutter Integration:** See `flutreadme.md` for mobile app protocol reference  
**Architecture:** See `docs/architecture.md` for system design details

**Last Updated:** November 24, 2025  
**Version:** 0.3.0-alpha  
**Sprint:** Sprint 2 Complete (Protocol Hardening Phase)
**Layer 2 - Cryptographic:** ECDH device binding (application layer)  
**Layer 3 - Transport:** BLE pairing with PIN  
**Layer 4 - Storage:** AES-256 encrypted database  
**Layer 5 - Session:** Challenge-response authentication

Note: Application-layer security (Layer 2) is primary defense. BLE security (Layer 3) provides convenience but has known vulnerabilities.

### Threat Model

Protected Against:
- Unauthorized device access (ECDH binding)
- Credential extraction from SD card (AES-256)
- Session replay attacks (challenge-response)
- Brute force attacks (rate limiting)

Vulnerabilities:
- Static BLE PIN (123456) - planned fix in Sprint 3
- Plaintext passwords over BLE - planned fix in Sprint 2
- No secure element - planned for Phase 2

See [ARCHITECTURE.md](docs/ARCHITECTURE.md) for detailed security analysis.

## Testing

### Unit Tests
Not yet implemented - planned for Sprint 4.

### Integration Testing
Manual testing protocol:
1. Flash firmware to ESP32
2. Verify boot sequence in serial monitor
3. Connect with Flutter app
4. Test credential CRUD operations
5. Test unpair and re-pair flow
6. Verify unauthorized device rejection
7. **Test session resynchronization:**
   - Send `status` command to check state
   - Test session timeout (3 minutes idle)
   - Test connection timeout (5 minutes idle)
   - Test session resume with session ID
   - Test force_disconnect command
8. **Test OS-level unpair recovery:**
   - Forget device in phone Bluetooth settings
   - Reconnect from Flutter app
   - Verify automatic pairing reset
9. **Test physical unpair button:**
   - Hold BOOT button for 3 seconds
   - Verify device unpairs and restarts advertising
   - Pair new device successfully

## Contributing

### Code Style
- 2-space indentation
- Descriptive variable names
- Comment complex crypto operations
- No sensitive data in logs

### Commit Messages
```
feat: Add database worker queue
fix: Correct HMAC verification logic
refactor: Split main.cpp into modules
docs: Update ECDH implementation guide
```

## License

[To be determined]

## Contact

[Project maintainer contact info]

## Acknowledgments

- mbedTLS for cryptographic primitives
- PlatformIO for build system
- ESP-IDF for ESP32 framework
