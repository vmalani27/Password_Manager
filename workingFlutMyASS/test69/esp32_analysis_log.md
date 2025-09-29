### 7. Code Snippets for Solutions

**1) Parameterized Queries (sqlite3_prepare_v2 + sqlite3_bind_text)**
```cpp
// Safe INSERT
const char* sql = "INSERT INTO credentials (site, username, password) VALUES (?, ?, ?);";
sqlite3_stmt* stmt;
if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
  sqlite3_bind_text(stmt, 1, site.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, password.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

// Safe SELECT
const char* sql = "SELECT password FROM credentials WHERE site=? AND username=?;";
sqlite3_stmt* stmt;
if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
  sqlite3_bind_text(stmt, 1, site.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    // handle result
  }
  sqlite3_finalize(stmt);
}
```

**2) ECDH Session Key Establishment (mbedTLS pseudo-code)**
```cpp
// ESP32 generates ECC keypair
mbedtls_ecp_group grp;
mbedtls_ecp_group_init(&grp);
mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
mbedtls_ecp_keypair keypair;
mbedtls_ecp_keypair_init(&keypair);
mbedtls_ecp_gen_keypair(&grp, &keypair.d, &keypair.Q, mbedtls_ctr_drbg_random, &ctr_drbg);

// Exchange public keys with app, then:
mbedtls_ecdh_context ctx;
mbedtls_ecdh_init(&ctx);
mbedtls_ecdh_get_params(&ctx, &keypair, MBEDTLS_ECDH_OURS);
// Load peer public key into ctx
mbedtls_ecdh_compute_shared(&grp, &session_key, &peer_pub, &keypair.d, mbedtls_ctr_drbg_random, &ctr_drbg);
```

**3) AES-GCM Encryption/Decryption (mbedTLS pseudo-code)**
```cpp
mbedtls_gcm_context gcm;
mbedtls_gcm_init(&gcm);
mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, session_key, 256);
mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, len, iv, iv_len, aad, aad_len, input, output, tag_len, tag);
// For decryption: mbedtls_gcm_auth_decrypt(...)
mbedtls_gcm_free(&gcm);
```

**4) Rate Limiting and Lockout (C++ logic)**
```cpp
int failedAttempts = 0;
const int MAX_ATTEMPTS = 5;
unsigned long lockoutUntil = 0;

bool allowCommand() {
  if (millis() < lockoutUntil) return false;
  if (failedAttempts >= MAX_ATTEMPTS) {
    lockoutUntil = millis() + 60000; // 1 min lockout
    failedAttempts = 0;
    return false;
  }
  return true;
}

void onFailedAuth() {
  failedAttempts++;
  if (failedAttempts >= MAX_ATTEMPTS) {
    lockoutUntil = millis() + 60000;
  }
}
```

**5) Minimal Per-Session Authorization (C++ logic)**
```cpp
String sessionToken;
bool isAuthorized(String token) {
  return token == sessionToken;
}
// Generate sessionToken after successful pairing/biometric unlock
```

**6) Audit Logging (C++/SQLite)**
```cpp
const char* sql = "INSERT INTO audit_log (timestamp, event, user) VALUES (?, ?, ?);";
sqlite3_stmt* stmt;
if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
  sqlite3_bind_int64(stmt, 1, (sqlite3_int64)time(NULL));
  sqlite3_bind_text(stmt, 2, event.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, user.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}
```

**7) User Feedback for Security Events (OLED + BLE)**
```cpp
void notifySecurityEvent(const String& msg) {
  updateOutput(msg); // OLED
  sendNotification(msg); // BLE
}
```
# ESP32 Password Manager - Security, Power, and Feasibility Log

## Date: 2025-09-28

### 1. Security Concerns
- **No BLE Pairing/Authentication:** BLE GATT server accepts any connection, no authentication or encryption is enforced. This exposes all commands and data to anyone in BLE range.
- **Plaintext Data Transfer:** Credentials (site, username, password) are sent and received in plaintext over BLE. No encryption is used at the application layer.
- **SQL Injection Risk:** User input is directly concatenated into SQL queries. This allows for potential SQL injection attacks via BLE commands.
- **No User Authorization:** Any connected BLE client can execute all commands (add, get, update, delete, list) without restriction.
- **No Rate Limiting/Lockout:** Unlimited attempts for commands, which could allow brute-force or denial-of-service attacks.
- **No Data Integrity Checks:** No checksums, signatures, or validation of data sent/received.

### 2. Power Consumption Concerns
- **BLE Always On:** BLE advertising and server are always running, which increases power draw.
- **OLED Display Always On:** The OLED display is updated for every command, which may not be necessary and increases power usage.
- **No Deep Sleep/Low Power Modes:** The ESP32 does not enter any sleep or low-power state when idle.
- **No Connection Timeout:** BLE connections are not timed out or disconnected after inactivity.

### 3. Commercialization/Market Concerns
- **No Secure Onboarding:** No secure pairing or provisioning process for first-time setup.
- **No Firmware Update Mechanism:** No OTA or secure update process for bug fixes or security patches.
- **No Audit Logging:** No logging of access or command history for accountability.
- **No User Feedback for Security Events:** No alerts for failed access, repeated attempts, or suspicious activity.

### 4. Questions for Further Analysis
- Should BLE require pairing/bonding with passkey or numeric comparison?
- Should credentials be encrypted at rest in SQLite?
- What is the expected battery life and power source for the device?
- Is there a need for multi-user support or access control?
- What is the process for device reset or recovery if lost/compromised?
- Should the device support firmware updates in the field?
- What is the maximum number of credentials to be supported?
- Should the device support backup/restore of the database?

### 5. Solution Ideas / Implementation Notes

- **Secure Onboarding & First-Time Authentication:**  
  Consider implementing BLE Secure Connections pairing with passkey entry or numeric comparison for first-time setup. Optionally, use an Out-of-Band (OOB) method such as displaying a one-time passkey or QR code on the device, which the user enters or scans in the app to establish a trusted bond and shared secret. This will ensure only authorized users can pair and access the device, especially during initial setup.

- [Add further solution ideas here as we discuss them.]

---

**Problems Fixed by Implementing BLE Secure Connections:**
- **No BLE Pairing/Authentication:** Enforces authentication and encryption, only allowing trusted devices to connect.
- **Plaintext Data Transfer:** Encrypts all BLE traffic, protecting credentials in transit.
- **No User Authorization:** (Partially) Only paired devices can connect, reducing unauthorized access risk.
- **No Secure Onboarding:** Provides a secure, user-verified pairing process for first-time setup.

*Note: Other issues such as SQL injection, rate limiting, data integrity, audit logging, and power management require additional solutions.*

---

**Database Encryption Design (ESP32 + Commercial Use Case):**

- **Encrypt SQLite Database:**  
  Use AES-256-GCM (via mbedTLS) to encrypt the database or sensitive fields.

- **Data Encryption Key (DEK):**  
  Generate a 32-byte random DEK for database encryption. Never store the DEK in plaintext.

- **Key Wrapping:**  
  - ESP32 holds an ECC keypair (P-256).
  - During provisioning, the app and ESP32 exchange public keys and perform ECDH to derive a Key Encryption Key (KEK) using HKDF.
  - The KEK is used to wrap (encrypt) the DEK with AES-GCM.
  - Store only the wrapped DEK on the SD card.

- **Unlock Session:**  
  - The app unlocks its private key (e.g., via biometric).
  - App and ESP32 perform ECDH again to derive a session key.
  - The session key is used to unwrap the DEK or to encrypt BLE messages.

- **KDFs:**  
  - If a passphrase fallback is needed, use PBKDF2-HMAC-SHA256 (50,000 iterations) with a unique salt per device.

- **BLE Layer:**  
  - Use short AES-GCM encrypted packets for BLE communication (ESP32 can handle 1–2 KB/s).
  - Do not rely solely on BLE’s built-in encryption; always wrap sensitive data in your own encryption layer.

*This approach ensures that even if the SD card is removed or BLE is compromised, credentials remain protected. Only the legitimate app and device can access the decrypted data.*

---


---

### 6. High-Priority Solutions & Implementation Notes

**1) SQL Injection Risk — Fix with Parameterized Queries**
- Problem: Building SQL by concatenating strings from BLE commands is exploitable.
- Solution: Use prepared statements + binding (sqlite3_prepare_v2 + sqlite3_bind_text).
- Why: Prevents SQL injection and is more efficient.

**C++ Example: Safe INSERT**
```cpp
const char* sql = "INSERT INTO credentials (site, username, password) VALUES (?, ?, ?);";
sqlite3_stmt* stmt;
if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
  sqlite3_bind_text(stmt, 1, tokens[1].c_str(), -1, SQLITE_TRANSIENT); // site
  sqlite3_bind_text(stmt, 2, tokens[2].c_str(), -1, SQLITE_TRANSIENT); // username
  sqlite3_bind_text(stmt, 3, tokens[3].c_str(), -1, SQLITE_TRANSIENT); // password
  if (sqlite3_step(stmt) == SQLITE_DONE) {
    updateOutput("Added successfully");
    sendNotification("Added successfully");
  } else {
    updateOutput("Error: Insert failed");
    sendNotification("Error: Insert failed");
  }
  sqlite3_finalize(stmt);
} else {
  updateOutput("Error: Prepare failed");
  sendNotification("Error: Prepare failed");
}
```

**C++ Example: Safe SELECT**
```cpp
const char* sql = "SELECT password FROM credentials WHERE site=? AND username=?;";
sqlite3_stmt* stmt;
if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
  sqlite3_bind_text(stmt, 1, tokens[1].c_str(), -1, SQLITE_TRANSIENT); // site
  sqlite3_bind_text(stmt, 2, tokens[2].c_str(), -1, SQLITE_TRANSIENT); // username
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    String response = "Password: " + String((const char*)sqlite3_column_text(stmt, 0));
    updateOutput(response);
    sendNotification(response);
  } else {
    updateOutput("Entry not found");
    sendNotification("Entry not found");
  }
  sqlite3_finalize(stmt);
} else {
  updateOutput("Error: Prepare failed");
  sendNotification("Error: Prepare failed");
}
```

**2) Protect Communication & Integrity**
- Establish a session key (ECDH) and use AEAD (AES-GCM) for BLE payloads.
- Provides confidentiality and integrity beyond BLE's built-in encryption.

**3) Add Rate Limiting & Lockout**
- Limit command attempts and failed authentication tries per session.
- Lock out after repeated failures to prevent brute-force/DoS.

**4) Minimal Per-Session Authorization**
- Use a session token tied to BLE pairing and biometric unlock on the app.
- Only allow commands from authorized sessions.

**5) Audit Logging**
- Log access and commands in an append-only table (encrypt logs if possible).

**6) User Feedback for Security Events**
- Notify user via OLED and BLE notifications for failed access, lockouts, or suspicious activity.

---

**Next Steps:**
- Discuss which security and power features are required for MVP vs. commercial release.
- Prioritize fixes and enhancements based on risk and feasibility.
