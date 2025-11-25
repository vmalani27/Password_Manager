# ESP32 Password Manager - Command Reference

This document provides a complete reference for all BLE commands supported by the ESP32 Password Manager.

## Connection Flow

```
1. BLE Connect → 2. ECDH Handshake → 3. Token Auth → 4. Commands
```

### Step 1: BLE Connection
Connect to the device named `ESP32-PWD-Manager` using BLE. If prompted for a PIN during pairing, enter `123456`.

### Step 2: ECDH Handshake
Read and write to the ECDH characteristic to establish secure device pairing:
- Read ESP32's public key (64 bytes)
- Compute shared secret locally
- Write client's public key (64 bytes)
- Wait for response: `ECDH_OK`, `ECDH_OK_PAIRED`, or `ECDH_ALREADY_PAIRED`

### Step 3: Token Authentication
Request a session token and authenticate:
```
→ request_token
← TOKEN:1A2B3C4D
→ auth 1A2B3C4D
← AUTH OK
```

### Step 4: Send Commands
Once authenticated, you can send credential management commands.

---

## Command Categories

### Public Commands (No Authentication Required)

#### request_token
Request a session token for authentication.

**Syntax:** `request_token`

**Response:** `TOKEN:<8-char-hex>`

**Example:**
```
→ request_token
← TOKEN:A1B2C3D4
```

---

#### auth
Authenticate using a session token.

**Syntax:** `auth <token>`

**Parameters:**
- `<token>` - 8-character hexadecimal token from `request_token`

**Responses:**
- `AUTH OK` - Authentication successful
- `AUTH FAIL` - Invalid token
- `LOCKED` - Too many failed attempts (wait 60 seconds)

**Example:**
```
→ auth A1B2C3D4
← AUTH OK
```

---

#### status
Check current connection and authentication status.

**Syntax:** `status`

**Responses:**
- `STATUS CONNECTED AUTHORIZED` - Connected and authenticated
- `STATUS CONNECTED UNAUTHORIZED` - Connected but not authenticated
- `STATUS NOT_CONNECTED` - No active connection

**Example:**
```
→ status
← STATUS CONNECTED AUTHORIZED
```

---

#### check_pairing
Check if device has pairing data (enables optimized reconnection).

**Syntax:** `check_pairing`

**Responses:**
- `PAIRED:<128-char-hex>` - Device is paired, returns stored client public key
- `UNPAIRED` - Device is not paired

**Example:**
```
→ check_pairing
← PAIRED:04A1B2C3...
```

**Note:** This command allows fast reconnection by skipping the ECDH handshake if keys match.

---

#### force_disconnect
Force a clean disconnect and reset session.

**Syntax:** `force_disconnect`

**Response:** `DISCONNECTING`

**Example:**
```
→ force_disconnect
← DISCONNECTING
```

**Note:** Use this when the app is closing or going to background to ensure ESP32 cleans up properly.

---

#### unpair
Unpair the device (clears all pairing data). NO AUTHENTICATION REQUIRED for emergency recovery.

**Syntax:** `unpair`

**Responses:**
- `UNPAIRED` - Device successfully unpaired
- `UNPAIR_FAIL` - Unpair operation failed

**Example:**
```
→ unpair
← UNPAIRED
```

**Warning:** This will:
- Clear all ECDH keys from NVS
- Remove all BLE bonds
- Generate new ESP32 ECDH keys
- Disconnect any active connection
- Require fresh pairing for next connection

---

#### db_reset
Emergency database reset. Deletes and recreates the credentials database. NO AUTHENTICATION REQUIRED.

**Syntax:** `db_reset`

**Responses:**
- `DB_RESET_OK` - Database successfully reset
- `DB_RESET_FAIL` - Failed to recreate database

**Example:**
```
→ db_reset
← DB_RESET_OK
```

**WARNING:** This command will PERMANENTLY DELETE ALL STORED CREDENTIALS. Use only if database is corrupted and cannot be recovered.

---

### Authorized Commands (Authentication Required)

All commands below require a valid session. Send `request_token` followed by `auth <token>` first.

#### add
Add a new credential to the database.

**Syntax:** `add <service> <identifier> <password>`

**Parameters:**
- `<service>` - Service name (e.g., github.com, email)
- `<identifier>` - Username, email, or account identifier
- `<password>` - Password to store (encrypted on device)

**Responses:**
- `Added` - Credential successfully added
- `ADD FAIL` - Failed to add (already exists or storage full)
- `NOT AUTHORIZED` - Session not authenticated

**Example:**
```
→ add github.com user@email.com MySecretPass123
← Added
```

**Quote Support:** For service names with spaces, use quotes:
```
→ add "Google Mail" user@gmail.com MyPass123
← Added
```

---

#### get
Retrieve a password for a specific service and identifier.

**Syntax:** `get <service> <identifier>`

**Parameters:**
- `<service>` - Service name
- `<identifier>` - Username or identifier

**Responses:**
- `Password: <plaintext>` - Password retrieved successfully
- `NOT FOUND` - Credential does not exist
- `NOT AUTHORIZED` - Session not authenticated

**Example:**
```
→ get github.com user@email.com
← Password: MySecretPass123
```

**Security Note:** Password is currently transmitted in plaintext over BLE (protected by BLE link encryption). Upgrading to encrypted responses is planned (US-002).

---

#### update
Update the password for an existing credential.

**Syntax:** `update <service> <identifier> <new_password>`

**Parameters:**
- `<service>` - Service name
- `<identifier>` - Username or identifier
- `<new_password>` - New password to store

**Responses:**
- `Updated` - Password successfully updated
- `UPDATE FAIL` - Failed to update (credential not found)
- `NOT AUTHORIZED` - Session not authenticated

**Example:**
```
→ update github.com user@email.com NewPassword456
← Updated
```

---

#### delete
Delete a credential from the database.

**Syntax:** `delete <service> <identifier>`

**Parameters:**
- `<service>` - Service name
- `<identifier>` - Username or identifier

**Responses:**
- `Deleted` - Credential successfully deleted
- `DELETE FAIL` - Failed to delete (credential not found)
- `NOT AUTHORIZED` - Session not authenticated

**Example:**
```
→ delete github.com user@email.com
← Deleted
```

---

#### list
List all stored credentials (service and identifier only, no passwords).

**Syntax:** `list`

**Response:** `LIST:\n<service> <identifier>\n...`

**Example:**
```
→ list
← LIST:
github.com user@email.com
google.com admin@example.com
```

**Response for empty database:**
```
→ list
← LIST:
(none)
```

---

#### logout
Clear the current session token (stays connected).

**Syntax:** `logout`

**Response:** `LOGOUT`

**Example:**
```
→ logout
← LOGOUT
```

**Note:** Connection remains active, but you'll need to re-authenticate with `request_token` → `auth` to send more commands.

---

## Error Responses

| Response | Meaning | Solution |
|----------|---------|----------|
| `NOT AUTHORIZED` | No valid session | Send `request_token` then `auth <token>` |
| `AUTH FAIL` | Invalid token | Request new token (max 5 attempts) |
| `LOCKED` | Too many failed auth attempts | Wait 60 seconds before retry |
| `ECDH_ALREADY_PAIRED` | Device paired to another phone | Unpair first (requires physical access or other paired phone) |
| `NOT FOUND` | Credential doesn't exist | Check service/identifier spelling |
| `ADD FAIL` | Failed to add credential | Entry may already exist or storage full |
| `UPDATE FAIL` | Failed to update credential | Verify entry exists first |
| `DELETE FAIL` | Failed to delete credential | Verify entry exists first |
| `INVALID` | Unknown command or wrong args | Check command syntax and parameter count |
| `SESSION_TIMEOUT` | Session expired (3 min idle) | Re-authenticate with token flow |

---

## Best Practices

### Service and Identifier Naming

**Use clear, consistent naming:**
```
Good: github.com, google.com, work-email
Bad: GH, goog, Email
```

**For accounts without domains:**
```
Examples: wifi-home, vpn-office, server-ssh
```

**For multiple accounts:**
```
Examples: github.com-personal, github.com-work
Or: github.com john@personal.com
    github.com john@work.com
```

### Multi-Word Service Names

Use quotes for service names containing spaces:
```
→ add "Google Mail" user@gmail.com password123
← Added

→ get "Google Mail" user@gmail.com
← Password: password123
```

### Security Tips

1. **Always logout when done:** Use `logout` before closing app
2. **Use force_disconnect on background:** Prevents stale sessions
3. **Unique passwords:** Don't reuse passwords across services
4. **Regular updates:** Update passwords periodically using `update`
5. **Audit regularly:** Use `list` to review stored credentials

### Emergency Recovery

If you lose access to the app or session is corrupted:

1. **Use unpair command:** Clears pairing without auth
2. **Physical button:** Hold BOOT button on ESP32 for 3 seconds
3. **Database reset:** Use `db_reset` if database corrupted (DELETES ALL DATA)

---

## Session Timeout Behavior

### 3-Minute Idle Timeout
- Session is cleared after 3 minutes of inactivity
- Connection remains active
- ESP32 sends `SESSION_TIMEOUT` notification
- Client must re-authenticate (request_token → auth)

### 5-Minute Connection Timeout
- Connection is forcibly closed after 5 minutes idle
- ESP32 restarts advertising
- Client must reconnect fully

---

## ECDH Responses

These responses come from the ECDH characteristic (not RX/TX):

| Response | Meaning | Next Action |
|----------|---------|-------------|
| `ECDH_OK` | Recognized paired device, reconnection successful | Proceed to token auth |
| `ECDH_OK_PAIRED` | New device pairing completed | Save pairing data, proceed to token auth |
| `ECDH_ALREADY_PAIRED` | Device is paired to another phone | Show error, cannot connect |
| `ECDH_FAIL` | ECDH computation failed | Retry connection |
| `ECDH_INVALID` | Invalid public key length | Check key format (must be 64 bytes) |

---

## Complete Example Session

```
# Connect via BLE
→ <BLE Connect>
← <Connected>

# ECDH Handshake (via ECDH characteristic)
→ <Read ECDH characteristic>
← <ESP32 public key, 64 bytes>
→ <Write ECDH characteristic: client public key, 64 bytes>
← ECDH_OK

# Token Authentication
→ request_token
← TOKEN:1A2B3C4D

→ auth 1A2B3C4D
← AUTH OK

# Add a credential
→ add github.com john@example.com MySecretPass123
← Added

# List credentials
→ list
← LIST:
github.com john@example.com

# Retrieve password
→ get github.com john@example.com
← Password: MySecretPass123

# Update password
→ update github.com john@example.com NewPassword456
← Updated

# Delete credential
→ delete github.com john@example.com
← Deleted

# Logout
→ logout
← LOGOUT
```

---

## Technical Details

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

### Command Parsing

- **Case insensitive:** `add`, `ADD`, and `Add` are equivalent
- **Space delimited:** Commands use space as separator
- **Token format:** 8 hexadecimal characters (4 bytes from esp_random())
- **Quote support:** Wrap parameters in double quotes for multi-word values

### Security Layers

1. **ECDH Layer:** Device-level pairing (persistent across reboots)
2. **Token Layer:** Session-level authorization (cleared on disconnect)
3. **Database Encryption:** AES-256-CBC with hardware-derived key
4. **BLE Link Encryption:** 128-bit (OS-managed)

---

## Troubleshooting

### "NOT AUTHORIZED" after ECDH
**Cause:** Skipped token authentication  
**Solution:** Send `request_token` → `auth <token>` before commands

### "ECDH_ALREADY_PAIRED" error
**Cause:** Device paired to another phone  
**Solution:** Unpair from other phone, or use physical unpair button on ESP32

### Commands timeout
**Cause:** Session expired or connection lost  
**Solution:** Check connection status with `status`, re-authenticate if needed

### "NOT FOUND" when credential exists
**Cause:** Service or identifier mismatch  
**Solution:** Use `list` to see exact spelling, check for trailing spaces

### Database corruption
**Cause:** SD card issue or power loss during write  
**Solution:** Use `db_reset` (WARNING: deletes all credentials)

---

## Version Information

- **ESP32 Firmware:** v2.0 (Database Worker Queue)
- **Protocol Version:** v1.0
- **Last Updated:** November 25, 2025
