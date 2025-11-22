# TODO

## TIER S - CRITICAL (DO FIRST)

### Completed ✅
[x] Remove async key manager - Replaced with synchronous deriveRuntimeKey()
[x] Implement ECDH key exchange - Full secp256r1 implementation with HKDF session key derivation
[x] Implement challenge-response auth - HMAC-SHA256 based authentication using ECDH session key
[x] Device binding with NVS persistence - ESP32 saves/loads paired device keys, verifies client identity
[x] Fix idle timeout bug - Removed timeout logic entirely, session only cleared on disconnect

### Remaining 🔴
[ ] **Create DB worker queue** - User sends "get password" → command queued → worker task safely executes SQLite query → response sent. Prevents corruption from concurrent BLE callbacks accessing database.

[ ] **Remove plaintext password over BLE** - User gets password → ESP32 encrypts with AES-GCM using session_key → Flutter decrypts with same key. End-to-end encryption, no plaintext transmission.

[ ] **Strengthen session token** - Legacy token auth still uses 32-bit random(). Generate 128-bit token using esp_random(), convert to hex string. Harder to brute force during deprecation period.

## TIER A - HIGH SECURITY

### Completed ✅
[x] ECDH device binding - Persistent pairing, unauthorized device rejection

### Remaining 🔴
[ ] **Migrate AES-CBC to AES-GCM** - User saves password → ESP32 encrypts with AES-GCM → stores {nonce||tag||ciphertext} in SQLite. Authenticated encryption prevents tampering, integrity verification on decrypt.

[ ] **Remove static passkey (123456)** - First boot → ESP32 generates random 6-digit PIN → saves to NVS → displays on OLED once → user enters in Flutter app. Unique per device, no default PIN vulnerability.

[ ] **Remove sensitive data logging** - Replace `Serial.println("Token: " + token)` with `Serial.println("AUTH_EVENT")`. No passwords/tokens/keys in serial output, only event IDs for debugging.

[ ] **Enable NVS flash encryption** - Add `CONFIG_NVS_ENCRYPTION=y` to sdkconfig → private keys stored encrypted in flash. Physical flash extraction cannot read pairing keys without device boot.

## TIER B - REFACTORING

[ ] **Split codebase into modules** - Break 1177-line main.cpp into ble_manager.cpp (advertising, callbacks), db_manager.cpp (SQLite wrapper), crypto_manager.cpp (ECDH, encryption), ui_manager.cpp (display). Easier maintenance and testing.

[ ] **Simplify BLE initialization** - Extract 50 lines of `esp_ble_gap_set_security_param()` calls into `configureBleSecurity()` function. Single function call in setup(), cleaner code.

[ ] **Normalize logging system** - Create `logInfo(msg)`, `logError(msg)`, `logDebug(msg)` functions → OLED shows brief status, Serial shows full details with timestamps. Consistent logging across codebase.

## TIER C - CLEANUP & POLISH

[ ] **Move hardcoded values to config.h** - Extract `CONNECTION_TIMEOUT_MS=30000`, `MAX_FAILED_ATTEMPTS=5`, `LOCKOUT_DURATION_MS=60000`, `SD_CS=5`, `SPI_FREQ=8000000` to config.h. Single file for tuning parameters.

[ ] **Add watchdog timer** - Setup → `esp_task_wdt_init(10)` → main loop feeds watchdog every cycle. If code hangs (deadlock, infinite loop), ESP32 auto-reboots after 10 seconds.

[ ] **Add version info to advertising** - Include firmware version in BLE manufacturer data → Flutter can check compatibility before connecting. "App v1.2 requires ESP32 firmware v1.1+".

[ ] **Implement OTA updates** - User taps "Update Firmware" in Flutter → ESP32 downloads binary from server → verifies signature → flashes to app1 partition → reboots. Remote updates without USB cable.

## TIER D - FUTURE ENHANCEMENTS (Phase 2+)

[ ] **BLE bonded-device whitelist (Phase 5)** - After BLE pairing → save MAC address to NVS whitelist → on new connection check if MAC is whitelisted → auto-disconnect if not. Defense-in-depth: blocks unauthorized devices before ECDH handshake, reduces DOS attack surface. *NOTE: Lower priority since ECDH pairing already provides cryptographic device binding.*

[ ] **Secure element integration** - Move eFuse key to SE050/ATECC608 → all crypto ops (ECDH, AES) happen in tamper-resistant chip. Physical protection, certified security.

[ ] **USB HID keyboard emulation** - User selects "Type password" → ESP32 acts as USB keyboard → types password into computer. No BLE required, works with any OS.

[ ] **Backup/recovery system** - User exports encrypted backup → stored in Flutter secure storage → on new device import backup → decrypt with master password → restore all credentials. Device replacement protection.
