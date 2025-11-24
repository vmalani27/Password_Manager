    # ESP32 Password Manager

    A secure Bluetooth Low Energy (BLE) password manager using Flutter and ESP32 with ECDH key exchange and device pairing.

    ## Security Features

    ### ✅ Implemented (Current State)

    **Two-Layer Authentication System:**

    **Layer 1 - ECDH Device Pairing (One-time):**
    - ✅ ECDH key exchange using secp256r1 curve
    - ✅ HKDF-based session key derivation (salt: "BLE_PASSWORD_MGR")
    - ✅ Persistent device pairing (one phone ↔ one ESP32)
    - ✅ Saved keys in SharedPreferences (Flutter) and NVS (ESP32)
    - ✅ First pairing vs reconnection logic
    - ✅ Unpair functionality from both sides
    - ✅ Protection against connecting to different device while paired
    - ✅ ESP32 rejects connections from non-paired devices (ECDH_ALREADY_PAIRED)

    **Layer 2 - Token-Based Session Authorization:**
    - ✅ Standard `request_token` → `auth <token>` flow
    - ✅ Session tokens for command authorization
    - ✅ Token verification before allowing add/get/list/delete commands
    - ✅ Session timeout and re-authentication support

    **BLE Protocol:**
    - Connection → ECDH handshake (pairing) → Token auth (session) → Commands
    - ECDH responses: `ECDH_OK` (reconnection), `ECDH_OK_PAIRED` (first pairing), `ECDH_ALREADY_PAIRED` (rejection)
    - Auth responses: `TOKEN <hex>`, `AUTH OK`, `AUTH FAIL`, `NOT AUTHORIZED`
    - Commands: `add`, `get`, `list`, `update`, `delete`, `unpair`

    ### 🚧 Remaining Security Improvements

    **Phase 2 - End-to-End Encryption (AES-256-GCM):**
    - [ ] Encrypt all commands using session key
    - [ ] Encrypt all responses using session key
    - [ ] Implement message authentication with GCM tags
    - [ ] Add message counters to prevent replay attacks

    **Phase 3 - Secure Password Transmission:**
    - [ ] Remove plaintext password transmission over BLE
    - [ ] Implement USB HID keyboard emulation on ESP32
    - [ ] OR: Encrypt passwords with AES-256-GCM before transmission

    **Phase 4 - Additional Hardening:**
    - [ ] Session timeout implementation
    - [ ] Rate limiting for failed authentication attempts
    - [ ] Factory reset protection (require button press to unpair)

    ## Architecture

    ```
    Flutter App (Client)                 ESP32 (Server)
    ├── PairingService                   ├── NVS Storage
    │   └── SharedPreferences            │   └── Paired device keys
    ├── EcdhService                      ├── ECDH (mbedtls)
    │   ├── Key generation               │   ├── Key generation
    │   ├── Shared secret                │   ├── Shared secret
    │   └── HKDF + HMAC                  │   └── HKDF + HMAC
    ├── CommandService                   ├── BLE Server
    │   ├── Handshake logic              │   ├── Characteristics
    │   └── Challenge-response           │   └── Notifications
    └── AppStateProvider                 └── Password Storage
    ```

    ## Pairing Flow

    ### First Pairing
    1. User selects ESP32 from scan list
    2. **ECDH Device Binding:**
    - Client generates ECDH key pair
    - Client reads ESP32's public key
    - Client computes shared secret and session key
    - Client sends public key to ESP32
    - ESP32 responds with `ECDH_OK_PAIRED`
    - Both sides save pairing data (keys + device info)
    3. **Session Authentication:**
    - Client sends `request_token`
    - ESP32 responds with `TOKEN <hex>`
    - Client sends `auth <token>`
    - ESP32 responds with `AUTH OK`
    4. Client can now send commands (add, get, list, etc.)

    ### Reconnection
    1. Client checks if device is paired
    2. **ECDH Device Verification:**
    - Client loads saved private/public keys
    - Client reads ESP32's public key (verify it matches)
    - Client computes shared secret and session key
    - Client sends saved public key to ESP32
    - ESP32 recognizes client and responds `ECDH_OK`
    3. **Session Authentication:**
    - Client sends `request_token`
    - ESP32 responds with `TOKEN <hex>`
    - Client sends `auth <token>`
    - ESP32 responds with `AUTH OK`
    4. Client can now send commands

    ### Key Concepts
    - **ECDH Layer**: Device-level pairing (one phone ↔ one ESP32), persistent across reboots
    - **Token Layer**: Session-level authorization, required for each connection, expires on disconnect
    - **Why Both?**: ECDH prevents unauthorized devices from connecting, tokens prevent unauthorized commands even if connected

    ### Unpair Flow

    **From Flutter (authenticated user):**
    1. Settings → "Unpair Device" button
    2. Confirmation dialog
    3. Client sends `unpair` command (lowercase, must be authenticated)
    4. ESP32 erases NVS pairing data
    5. ESP32 responds with `UNPAIRED`
    6. Client deletes local pairing data (SharedPreferences)
    7. ESP32 generates new ephemeral ECDH keys
    8. Device returns to UNPAIRED state
    9. New pairing can now be initiated

    **From ESP32 (physical button - not yet implemented):**
    1. User presses hardware reset button on ESP32
    2. ESP32 erases NVS pairing data
    3. ESP32 generates new ephemeral keys
    4. Device returns to UNPAIRED state
    5. Any connection from old phone gets `ECDH_ALREADY_PAIRED`

    ### Unauthorized Device Rejection
    1. Phone B tries to pair while Phone A is already paired
    2. Phone B sends its public key to ESP32
    3. ESP32 checks NVS: `stored_client_key ≠ phone_b_key`
    4. ESP32 responds with `ECDH_ALREADY_PAIRED`
    5. Phone B shows error: "Device is paired to another phone"
    6. Phone B cannot authenticate - must unpair first

    ## Key Files

    ### Services
    - `lib/services/pairing_service.dart` - Persistent pairing storage
    - `lib/services/ecdh_service.dart` - Cryptographic operations
    - `lib/services/command_service.dart` - BLE protocol implementation
    - `lib/services/ble_connection_service.dart` - BLE connection management

    ### Models
    - `lib/models/pairing_device.dart` - Paired device data structure

    ### Constants
    - `lib/constants/ble_constants.dart` - UUIDs, commands, responses

    ## Setup

    1. Flash ESP32 with firmware from `eso32_codes/esp32code/`
    2. Install Flutter dependencies:
    ```bash
    flutter pub get
    ```
    3. Run the app:
    ```bash
    flutter run
    ```

    ## Security Model

    ### Key Hierarchy

    ```
    Hardware Layer (ESP32):
    └─ eFuse BLOCK3 (256-bit hardware key, one-time programmable)
        └─ Runtime Key (HMAC-SHA256 derived, ephemeral)
                └─ Database Encryption (AES-256-CBC)
                    └─ Encrypted passwords on SD card

    Application Layer (Device Binding):
    └─ ECDH Key Exchange
        ├─ ESP32 Private/Public Key (NVS persistent when paired)
        ├─ Client Private/Public Key (Flutter SharedPreferences)
        ├─ Shared Secret (ephemeral, computed per session)
        └─ Session Key (HKDF derived, cleared on disconnect)
                └─ Future: Encrypt BLE commands (not implemented)

    Transport Layer (BLE):
    └─ BLE Pairing (PIN: 123456, weak - needs replacement)
        └─ BLE Link Encryption (128-bit, vulnerable to KNOB attack)

    Session Layer (Command Authorization):
    └─ Token-Based Auth (32-bit random token)
        └─ Required for all commands after ECDH handshake
        └─ Cleared on disconnect
    ```

    ### What Each Key Protects

    **Runtime Key (from eFuse):**
    - Encrypts/decrypts passwords in SQLite database
    - Never leaves ESP32 RAM
    - Cleared after each encryption/decryption operation
    - **NOT used for BLE communication or ECDH**
    - Only used by `secure_core.cpp`: `encrypt_password()` / `decrypt_password()`

    **ECDH Keys:**
    - Proves device identity (public key binding)
    - Establishes shared secret without transmitting it
    - Session key derived for future command encryption
    - **Currently NOT used for password transmission (TODO Phase 2)**
    - Prevents unauthorized devices from connecting

    **Session Token (32-bit random):**
    - Temporary authorization after ECDH handshake
    - Generated on `request_token` command
    - Cleared on disconnect or `logout`
    - **Legacy mechanism - will be replaced by ECDH challenge-response**

    ### Current Security Gaps

    1. **Passwords transmitted in plaintext over BLE**
    - BLE link encryption is weak (128-bit, KNOB attack vulnerable)
    - Session key is derived but not yet used for encryption
    - **Risk:** BLE sniffer can capture passwords in transit
    - **Fix (Phase 2):** Encrypt all commands with `session_key` using AES-256-GCM

    2. **Static BLE PIN (123456)**
    - Predictable, anyone can BLE-pair during initial connection
    - ECDH layer still prevents unauthorized access after pairing
    - **Risk:** Man-in-the-middle during first pairing
    - **Fix (Phase 3):** Generate dynamic PIN on boot, display on OLED

    3. **Legacy token authentication still active**
    - 32-bit tokens are brute-forceable (~4 billion attempts)
    - Rate limiting helps but not cryptographically secure
    - **Risk:** Token prediction or replay attacks
    - **Fix (Phase 2):** Replace with ECDH challenge-response (256-bit HMAC)

    4. **NVS flash encryption not enabled**
    - Private keys readable if ESP32 flash physically extracted
    - **Risk:** Device compromise if stolen
    - **Fix (Phase 4):** Enable ESP32 flash encryption in sdkconfig

    5. **No message authentication**
    - Commands can be modified in transit (though BLE has integrity checks)
    - **Risk:** Command injection or modification
    - **Fix (Phase 2):** Use AES-GCM with authentication tags

## Command Protocol

### BLE Characteristics

```
Service UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E

Characteristics:
├─ RX (Write): 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
│  └─ Client sends commands to ESP32
├─ TX (Notify): 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
│  └─ ESP32 sends responses to client
└─ ECDH (Read/Write): 6E400005-B5A3-F393-E0A9-E50E24DCCA9E
├─ Read: Get ESP32's public key (64 bytes)
└─ Write: Send client's public key (64 bytes)
```

### Complete ESP32 Command Reference

**From ESP32 Codebase (`src/ble_manager.cpp`)**

#### ECDH Key Exchange Responses (ECDH Characteristic)
```
ECDH_OK                 - Shared secret computed (reconnecting paired device)
ECDH_OK_PAIRED         - New device paired and saved to NVS
ECDH_ALREADY_PAIRED    - Rejected: ESP32 already paired to different device
ECDH_FAIL              - Shared secret computation failed
ECDH_INVALID           - Invalid public key length (expected 64 bytes hex)
```

#### Authentication Commands (RX Characteristic - Public Access)
| Command | Input Format | Response | Notes |
|---------|--------------|----------|-------|
| `request_token` | `request_token` | `TOKEN:<8-char-hex>` | Example: `TOKEN:1A2B3C4D` |
| `auth` | `auth <token>` | `AUTH OK` / `AUTH FAIL` | Example: `auth 1A2B3C4D` |

#### Credential Management (RX Characteristic - Requires Authorization)
| Command | Input Format | Success | Failure | Notes |
|---------|--------------|---------|---------|-------|
| `add` | `add <site> <user> <pass>` | `Added` | `ADD FAIL` | All 3 parameters required |
| `get` | `get <site> <user>` | `Password: <plaintext>` | `NOT FOUND` | Returns actual password |
| `update` | `update <site> <user> <newpass>` | `Updated` | `UPDATE FAIL` | Site+user must exist |
| `delete` | `delete <site> <user>` | `Deleted` | `DELETE FAIL` | Removes from database |
| `list` | `list` | `LIST:\n<site> <user>\n...` | *(none)* | All credentials |
| `unpair` | `unpair` | `UNPAIRED` | `UNPAIR_FAIL` | Erases NVS pairing |
| `logout` | `logout` | `LOGOUT` | *(none)* | Clears session token |

#### Global Error Responses (All Commands)
```
NOT AUTHORIZED    - No valid session (must complete token auth first)
LOCKED           - Too many failed auth attempts (wait 30 seconds)
INVALID          - Unknown command or wrong number of arguments
```

#### Implementation Details from ESP32 Source
- **Case Sensitivity**: Commands use `.equalsIgnoreCase()` - case-insensitive
- **Tokenization**: Space-delimited parsing (`cmd.split(' ')`)
- **Token Format**: 8 hex characters (4 bytes from `esp_random()`)
- **ECDH Key**: 64-byte hex string (32-byte P-256 public key)
- **Lockout Policy**: 3 failed auth attempts → 30 second lock
- **Session Lifetime**: Token cleared on disconnect or `logout`
- **Pairing Persistence**: ECDH keys survive ESP32 reboot (NVS storage)    ### Authentication Sequence

    ```
    Client                                ESP32
    |                                     |
    |-- Connect via BLE ----------------->|
    |<-- Connected ----------------------|
    |                                     |
    |-- Read ECDH characteristic -------->|
    |<-- ESP32 public key (64 bytes) ----|
    |                                     |
    | [Compute shared_secret & session_key] |
    |                                     |
    |-- Write ECDH characteristic ------->|
    |   (Client public key 64 bytes)      |
    |                                     | [Verify device pairing]
    |<-- "ECDH_OK" (reconnect) or -------|
    |    "ECDH_OK_PAIRED" (first) or ----|
    |    "ECDH_ALREADY_PAIRED" (reject) --|
    |                                     |
    |-- "request_token" ----------------->|
    |<-- "TOKEN:12345678" ---------------|
    |                                     |
    |-- "auth 12345678" ----------------->|
    |<-- "AUTH OK" ----------------------|
    |                                     |
    [Session authorized - can send commands]
    ```

    ### ECDH Responses

    | Response | Meaning | Next Action |
    |----------|---------|-------------|
    | `ECDH_OK` | Recognized paired device, reconnection successful | Proceed to token auth |
    | `ECDH_OK_PAIRED` | New device pairing completed | Save pairing data, proceed to token auth |
    | `ECDH_ALREADY_PAIRED` | Device is paired to another phone | Show error, cannot connect |
    | `ECDH_FAIL` | ECDH computation failed | Retry connection |
    | `ECDH_INVALID` | Invalid public key length | Check key format (must be 64 bytes) |

    ### Token Authentication Responses

    | Response | Meaning | Next Action |
    |----------|---------|-------------|
    | `TOKEN:XXXXXXXX` | Session token generated (8 hex chars) | Send `auth <token>` |
    | `AUTH OK` | Authentication successful | Can now send commands |
    | `AUTH FAIL` | Invalid token | Request new token (max 5 attempts) |
    | `NOT AUTHORIZED` | No session token | Send `request_token` first |
    | `LOCKED` | Too many failed attempts | Wait 60 seconds before retry |

### Credential Commands

All commands require `AUTH OK` session.

#### Command Examples with Actual ESP32 Responses

```bash
# Request session token
→ request_token
← TOKEN:A1B2C3D4

# Authenticate with token
→ auth A1B2C3D4
← AUTH OK

# List all credentials
→ list
← LIST:
github.com user@email.com
google.com admin

# Add new credential
→ add github.com john@email.com MySecretPass123
← Added

# Retrieve password
→ get github.com john@email.com
← Password: MySecretPass123

# Update password
→ update github.com john@email.com NewPassword456
← Updated

# Delete credential
→ delete github.com john@email.com
← Deleted

# Logout (stays connected)
→ logout
← LOGOUT

# Unpair device
→ unpair
← UNPAIRED
```

#### Command Syntax from ESP32 Source Code

**Public Commands (no auth required):**
```cpp
// From ble_manager.cpp handleCommand()
if (cmd.equalsIgnoreCase("request_token")) {
    // Generate 4-byte random token
    uint32_t token = esp_random();
    // Send as 8 hex characters
    sendNotification("TOKEN:" + String(token, HEX));
}

if (cmd.equalsIgnoreCase("auth")) {
    // Expects: auth <token>
    if (tokens[1] == sessionToken) {
        sendNotification("AUTH OK");
    } else {
        sendNotification("AUTH FAIL");
    }
}
```

**Authorized Commands (requires AUTH OK):**
```cpp
// Authorization check for all commands below
if (!isSessionAuthorized) {
    sendNotification("NOT AUTHORIZED");
    return;
}

// Add credential: add <site> <username> <password>
if (cmd.equalsIgnoreCase("add")) {
    if (db.insertCredential(tokens[1], tokens[2], tokens[3])) {
        sendNotification("Added");
    } else {
        sendNotification("ADD FAIL");
    }
}

// Get password: get <site> <username>
if (cmd.equalsIgnoreCase("get")) {
    String password = db.getPassword(tokens[1], tokens[2]);
    if (password.length() > 0) {
        sendNotification("Password: " + password);
    } else {
        sendNotification("NOT FOUND");
    }
}

// Update password: update <site> <username> <new_password>
if (cmd.equalsIgnoreCase("update")) {
    if (db.updateCredential(tokens[1], tokens[2], tokens[3])) {
        sendNotification("Updated");
    } else {
        sendNotification("UPDATE FAIL");
    }
}

// Delete credential: delete <site> <username>
if (cmd.equalsIgnoreCase("delete")) {
    if (db.deleteCredential(tokens[1], tokens[2])) {
        sendNotification("Deleted");
    } else {
        sendNotification("DELETE FAIL");
    }
}

// List all credentials
if (cmd.equalsIgnoreCase("list")) {
    String list = db.listCredentials();
    sendNotification("LIST:\n" + list);
}

// Unpair device
if (cmd.equalsIgnoreCase("unpair")) {
    if (crypto.unpairDevice()) {
        sendNotification("UNPAIRED");
        // Disconnect client after unpair
    } else {
        sendNotification("UNPAIR_FAIL");
    }
}

// Logout (clear session token)
if (cmd.equalsIgnoreCase("logout")) {
    clearSession();
    sendNotification("LOGOUT");
}

// Unknown command
sendNotification("INVALID");
```

**ECDH Callback Responses:**
```cpp
// From EcdhCallback::onWrite() in ble_manager.cpp
if (crypto.isDevicePaired()) {
    if (memcmp(rxValue, storedClientKey, 64) == 0) {
        // Recognized paired device
        sendNotification("ECDH_OK");
    } else {
        // Different device attempting connection
        sendNotification("ECDH_ALREADY_PAIRED");
    }
} else {
    // New pairing
    if (crypto.savePairing(clientKey)) {
        sendNotification("ECDH_OK_PAIRED");
    } else {
        sendNotification("ECDH_OK"); // Handshake OK but save failed
    }
}

// Invalid key length (not 64 bytes)
if (rxValue.length() != 64) {
    sendNotification("ECDH_INVALID");
}

// ECDH computation failed
if (!crypto.computeSharedSecret(clientKey)) {
    sendNotification("ECDH_FAIL");
}
```    ### Error Responses

    | Response | Meaning | Solution |
    |----------|---------|----------|
    | `NOT AUTHORIZED` | Session not authenticated | Send `request_token` → `auth <token>` |
    | `AUTH FAIL` | Invalid token | Request new token (max 5 attempts) |
    | `LOCKED` | Too many failed attempts | Wait 60 seconds |
    | `ECDH_ALREADY_PAIRED` | Paired to another device | Unpair first (requires physical access) |
    | `NOT FOUND` | Credential doesn't exist | Check site/username spelling |
    | `ADD FAIL` | Database insert failed | Check SD card, retry |
    | `UPDATE FAIL` | Credential update failed | Verify entry exists first |
    | `DELETE FAIL` | Credential delete failed | Verify entry exists first |
    | `INVALID` | Unknown command or wrong args | Check command syntax |

    ## Data Flow Examples

    ### Storing a Password (Current Implementation)

    ```
    User enters password in Flutter UI
        ↓
    [Flutter] Plaintext: "MyPassword123"
        ↓
    [Flutter → BLE] Command: "add github.com user@email.com MyPassword123"
        ↓ (Transmitted in plaintext over BLE - SECURITY GAP!)
        ↓
    [ESP32 BLE] Receives command
        ↓
    [ESP32] Parses: site="github.com", user="user@email.com", pass="MyPassword123"
        ↓
    [ESP32] encrypt_password() using Runtime Key (from eFuse)
        ↓
    [ESP32] AES-256-CBC: ciphertext + random IV
        ↓
    [ESP32] SQLite INSERT: {site, username, encrypted_password, iv}
        ↓
    [SD Card] Encrypted blob stored (safe at rest)
    ```

    ### Retrieving a Password (Current Implementation)

    ```
    [Flutter] Command: "get github.com user@email.com"
        ↓
    [ESP32 BLE] Receives command
        ↓
    [ESP32] SQLite SELECT: WHERE site='github.com' AND username='user@email.com'
        ↓
    [ESP32] Returns: {encrypted_password, iv}
        ↓
    [ESP32] decrypt_password() using Runtime Key (from eFuse)
        ↓
    [ESP32] AES-256-CBC decrypt: ciphertext + iv → "MyPassword123"
        ↓
    [ESP32 → BLE] Response: "Password: MyPassword123"
        ↓ (Transmitted in plaintext over BLE - SECURITY GAP!)
        ↓
    [Flutter] Displays password to user
    ```

    ### Future: Encrypted Command Flow (Phase 2)

    ```
    [Flutter] Plaintext: "add site user pass"
        ↓
    [Flutter] AES-256-GCM encrypt with session_key
        ↓
    [Flutter → BLE] Encrypted blob + authentication tag
        ↓
    [ESP32 BLE] Receives encrypted command
        ↓
    [ESP32] AES-256-GCM decrypt with session_key
        ↓
    [ESP32] Verify authentication tag
        ↓
    [ESP32] Process command
        ↓
    [ESP32] AES-256-GCM encrypt response
        ↓
    [ESP32 → BLE] Encrypted response + tag
        ↓
    [Flutter] Decrypt with session_key
        ↓
    [Flutter] Verify tag and display result
    ```

    ## Testing Checklist

    ### First Pairing Test
    - [ ] Scan finds ESP32 device "ESP32-PWD-Manager"
    - [ ] BLE pairing prompts for PIN (123456)
    - [ ] ECDH handshake succeeds → `ECDH_OK_PAIRED`
    - [ ] Token auth succeeds → `AUTH OK`
    - [ ] `add` command stores password
    - [ ] `get` command retrieves password
    - [ ] `list` command shows stored entries
    - [ ] Pairing data saved locally (check SharedPreferences)

    ### Reconnection Test
    - [ ] Close and reopen app (don't unpair)
    - [ ] App auto-connects to paired device
    - [ ] ECDH handshake succeeds → `ECDH_OK` (not PAIRED)
    - [ ] Token auth succeeds → `AUTH OK`
    - [ ] Previous passwords still accessible

    ### Unauthorized Device Test
    - [ ] Phone A pairs successfully
    - [ ] Phone B tries to connect (without unpairing)
    - [ ] Phone B gets `ECDH_ALREADY_PAIRED`
    - [ ] Phone B cannot authenticate
    - [ ] Phone A still works normally

    ### Unpair Test
    - [ ] Unpair from Flutter settings
    - [ ] `unpair` command succeeds → `UNPAIRED`
    - [ ] Local pairing data deleted
    - [ ] Reconnection requires fresh pairing
    - [ ] Old Phone A now gets `ECDH_ALREADY_PAIRED`

    ### Error Handling Test
    - [ ] Send command without auth → `NOT AUTHORIZED`
    - [ ] Wrong token 5 times → `LOCKED` for 60s
    - [ ] `get` non-existent entry → `NOT FOUND`
    - [ ] Disconnect during command → graceful timeout
    - [ ] Invalid command syntax → `INVALID`

    ## Troubleshooting

    ### "ECDH_ALREADY_PAIRED" Error
    **Cause:** Device is paired to another phone  
    **Solution:**
    1. Unpair from the paired phone's settings
    2. OR physically reset ESP32 (button press - not yet implemented)
    3. OR reflash ESP32 firmware (erases NVS)

    ### "NOT AUTHORIZED" After ECDH
    **Cause:** Forgot token authentication step  
    **Solution:**
    1. After `ECDH_OK`, send `request_token`
    2. Wait for `TOKEN:XXXXXXXX`
    3. Send `auth XXXXXXXX`
    4. Wait for `AUTH OK`
    5. Now send commands

    ### Connection Fails Immediately
    **Cause:** BLE pairing not completed  
    **Solution:**
    1. Check device Bluetooth settings
    2. "Forget" ESP32-PWD-Manager if listed
    3. Re-pair with PIN: 123456
    4. Try connection again

    ### Commands Timeout
    **Cause:** Session expired or lost  
    **Solution:**
    1. Check BLE connection status
    2. Re-authenticate with token flow
    3. If still failing, disconnect and reconnect

    ### Passwords Not Saving
    **Cause:** SD card issue or database corruption  
    **Solution:**
    1. Check ESP32 serial monitor for errors
    2. Verify SD card is inserted and formatted (FAT32)
    3. Check SD card connections (GPIO5=CS, GPIO18=SCK, etc.)
    4. Try different SD card

    ## Dependencies

    - `flutter_reactive_ble: ^5.3.1` - BLE communication
    - `pointycastle: ^3.7.3` - ECDH cryptography
    - `convert: ^3.1.1` - Hex encoding
    - `shared_preferences: ^2.2.3` - Persistent pairing storage
    - `permission_handler: ^11.3.1` - Android/iOS BLE permissions
    - `flutter_riverpod: ^2.5.1` - State management