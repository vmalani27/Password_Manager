Nice — below is a single, self-contained **step-by-step engineering plan + ready-to-use `.md`** you can drop into Copilot (or follow manually). It applies **incremental, testable changes** to your original ESP32 firmware so you don’t end up with one big messy merge. Each step includes:

* Rationale
* Exact code snippets / file patches (Arduino-style C++ for ESP32)
* How to test locally (serial + BLE client)
* What to verify before moving to the next step

Follow the steps in order. Do not skip tests — each step is small and verifiable.

---

# ESP32 Password Manager — Incremental Security Hardening Plan (for Copilot / Developer)

> Target: ESP32-WROOM-32 dev kit (4MB flash).
> Build system: Arduino Core for ESP32 (or PlatformIO).
> Existing libs: sqlite3 (embedded), mbedTLS available via ESP-IDF/Arduino, Adafruit_SSD1306, BLE libs.

---

## Table of Contents

1. Step 0 — Prep: repo structure & small helpers
2. Step 1 — Enforce BLE Secure Connections + bonding (pairing)
3. Step 2 — Replace string SQL with parameterized queries (sqlite3_prepare_v2 + bind)
4. Step 3 — Move DB access to an async worker (FreeRTOS task + queue)
5. Step 4 — Rate limiting & lockout logic (per-session)
6. Step 5 — Add audit logging table + writes
7. Step 6 — Basic session authorization token (post-pairing)
8. Step 7 — Chunking & MTU-aware notifications/read/write
9. Step 8 — ECDH session key + AES-GCM for payload integrity/confidentiality (mbedTLS)
10. Step 9 — Testing checklist & notes for Flutter client compatibility
11. Appendix — Useful snippets & compile hints

---

## Step 0 — Prep: repo structure & small helpers

Create these files/folders (if not present):

```
src/
  main.ino                <-- your current code (rename .cpp/.ino)
  ble_security.h
  db_worker.h
  db_worker.cpp
  crypto_session.h
  ratelimit.h
  audit.h
  helpers.h
docs/
  implement_steps.md      <-- this file
```

Add a small helper for safe `updateOutput()` and logging to serial + display — keep as is, but move into `helpers.h`.

**helpers.h**

```cpp
#pragma once
#include <Adafruit_SSD1306.h>
extern Adafruit_SSD1306 display;

inline void updateOutput(const String &msg) {
  Serial.println(msg);
  if (&display) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println(msg);
    display.display();
  }
}
```

Test: compile and run to ensure `updateOutput` works.

---

## Step 1 — Enforce BLE Secure Connections + bonding (pairing)

**Goal:** Require BLE bonding and Secure Connections so only paired devices can connect and BLE link-layer encryption is used by default.

**Rationale:** Prevent random clients from connecting. Use the Arduino/ESP32 BLEServer callbacks to request pairing/bonding.

**Patch:** Replace BLEDevice::init(...) call with enabling security and request bonding on connection. Create `ble_security.h`.

**ble_security.h**

```cpp
#pragma once
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// Call this once in setup after BLEDevice::init(...)
inline void enableBleSecurity() {
  // require secure connection (LE SC) and bonding
  BLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM);
  // Set IO capability to display or keyboard as needed. Use DisplayYesNo for numeric compare.
  BLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO);
  // Require encryption and bonding
  BLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT); // ensure encryption
}
```

**main.ino (changes in setup)** — after `BLEDevice::init("ESP32-GATT-Manager");` add:

```cpp
enableBleSecurity();
```

**ServerCallbacks modification** — check pairing state:

```cpp
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    updateOutput("Client connected (connectedCount="+String(pServer->getConnectedCount())+")");
  }
  void onDisconnect(BLEServer* pServer) {
    updateOutput("Client disconnected.");
    pServer->getAdvertising()->start();
  }
  void onPassKeyNotify(uint32_t pass_key) {
    updateOutput("Passkey: " + String(pass_key));
  }
};
```

**Test:**

1. Flash firmware.
2. From phone app (flutter_reactive_ble) try to connect. The OS should prompt pairing (numeric/passkey) depending on IO capability.
3. Verify device shows bonding in phone Bluetooth settings.
4. Attempt connecting with unpaired phone → should not allow sensitive operations (we'll gate commands later).

**Verify before next step:** device only accepts connections from paired clients (bond exists). Use Serial logs to confirm.

---

## Step 2 — Parameterized Queries (prevent SQL injection)

**Goal:** Replace all string concatenated SQL with prepared statements + sqlite3_bind_*.

**Rationale:** Prevent SQL injection (malicious BLE payload containing SQL).

**Patch:** Implement safe functions in `db_worker.h` to insert/select/update/delete using prepared statements. We'll later move to worker, but code should use prepared statements now.

**db_helper snippet (add to main or db_worker.cpp)**

Example `safeInsert`:

```cpp
bool safeInsertCredential(sqlite3 *db, const String &site, const String &username, const String &password) {
  const char *sql = "INSERT INTO credentials (site, username, password) VALUES (?, ?, ?);";
  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    updateOutput("Prepare failed: " + String(sqlite3_errmsg(db)));
    return false;
  }
  sqlite3_bind_text(stmt, 1, site.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, password.c_str(), -1, SQLITE_TRANSIENT);
  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

String safeGetPassword(sqlite3 *db, const String &site, const String &username) {
  const char *sql = "SELECT password FROM credentials WHERE site=? AND username=?;";
  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    return "ERR_PREPARE:" + String(sqlite3_errmsg(db));
  }
  sqlite3_bind_text(stmt, 1, site.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
  String out = "Entry not found";
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char *pw = sqlite3_column_text(stmt, 0);
    if (pw) out = String((const char*)pw);
  }
  sqlite3_finalize(stmt);
  return out;
}
```

**Change handleCommand**: replace executeSQL concatenation with calls to safeInsert/safeGetPassword.

**Test:**

1. Use BLE client to send `add site user pass` and `get site user`.
2. Try malicious input: `add site user "'); DROP TABLE credentials; --` — should be stored as text not cause SQL error.

**Verify before next step:** SQL injection attempts do not delete/alter tables and malicious payloads are stored as data.

---

## Step 3 — Move DB access to an async worker (FreeRTOS task + queue)

**Goal:** Prevent BLE callbacks from blocking while DB operations run. Offload DB queries to a worker task and queue.

**Rationale:** BLE stacks time out if operations block. Also improves responsiveness.

**Patch:** Create `db_worker.h` + `db_worker.cpp` with a FreeRTOS queue of command structs.

**db_worker.h**

```cpp
#pragma once
#include <Arduino.h>
#include <queue>
#include "sqlite3.h"

enum DbCmdType { DB_ADD, DB_GET, DB_UPDATE, DB_DELETE, DB_LIST };

struct DbCommand {
  DbCmdType type;
  String arg1; // site
  String arg2; // username
  String arg3; // password
  String replyCharacteristicId; // optional
  // additional fields as needed
};

void dbWorkerInit(sqlite3 *db);
void postDbCommand(const DbCommand &cmd);
```

**db_worker.cpp** (simplified)

```cpp
#include "db_worker.h"
#include "helpers.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

static QueueHandle_t dbQueue;
static sqlite3 *g_db;

static void dbTask(void *pvParameters) {
  DbCommand cmd;
  while (true) {
    if (xQueueReceive(dbQueue, &cmd, portMAX_DELAY) == pdTRUE) {
      // Process commands synchronously here but outside BLE callbacks
      if (cmd.type == DB_ADD) {
        bool ok = safeInsertCredential(g_db, cmd.arg1, cmd.arg2, cmd.arg3);
        updateOutput(ok ? "DB: Added" : "DB: Add failed");
        // send notification via BLE (post to main thread or use global pCharacteristic pointer)
      }
      else if (cmd.type == DB_GET) {
        String pw = safeGetPassword(g_db, cmd.arg1, cmd.arg2);
        updateOutput("DB: Get -> " + pw);
        // send notification
      }
      // ... handle other commands
    }
  }
}

void dbWorkerInit(sqlite3 *db) {
  g_db = db;
  dbQueue = xQueueCreate(10, sizeof(DbCommand));
  xTaskCreatePinnedToCore(dbTask, "DBTask", 8192, NULL, 1, NULL, 1);
}

void postDbCommand(const DbCommand &cmd) {
  if (dbQueue) xQueueSend(dbQueue, &cmd, 0);
}
```

**CommandCallback change:** instead of calling handleCommand() directly, parse & push a DbCommand to queue and ACK.

**Test:**

1. Send `add` and `get` commands while monitoring Serial. Ensure BLE write callback returns quickly and DBTask processes later.
2. Check BLE responsiveness (device doesn't stall).

**Verify:** BLE stays responsive; DB operations logged on serial.

---

## Step 4 — Rate limiting & lockout logic

**Goal:** Prevent brute-force by adding rate limiting & lockout counters per session / device.

**Rationale:** Reduce brute-force and rapid DoS attempts.

**ratelimit.h**

```cpp
#pragma once
#include <Arduino.h>

struct RateLimiter {
  unsigned long windowMs;
  int maxRequests;
  std::vector<unsigned long> timestamps;
  int failedAttempts;
  unsigned long lockUntil;

  RateLimiter(unsigned long w=60000, int m=20): windowMs(w), maxRequests(m), failedAttempts(0), lockUntil(0){}
  bool allow() {
    unsigned long now = millis();
    if (now < lockUntil) return false;
    // remove old
    timestamps.erase(std::remove_if(timestamps.begin(), timestamps.end(), [&](unsigned long t){ return (now - t) > windowMs; }), timestamps.end());
    if ((int)timestamps.size() < maxRequests) {
      timestamps.push_back(now);
      return true;
    }
    return false;
  }
  void onFailedAuth() {
    failedAttempts++;
    if (failedAttempts >= 5) {
      lockUntil = millis() + 5*60*1000; // lock 5 min
      failedAttempts = 0;
    }
  }
  void onSuccessAuth() { failedAttempts = 0; }
};
```

Integrate a `RateLimiter` instance per connected session (store in memory). When a command arrives, check `allow()`. For auth commands, call `onFailedAuth()` when appropriate.

**Test:**

1. Rapidly send many commands — device should throttle/block after threshold.
2. Simulate failed auth attempts (if implemented) and ensure lockout occurs.

**Verify:** rate limit enforced and lockout displayed on OLED.

---

## Step 5 — Audit logging table + writes

**Goal:** Keep an append-only audit log of events: pairing, connect, disconnect, commands, failed attempts.

**Rationale:** Accountability & forensics.

**SQL schema (add at DB init):**

```sql
CREATE TABLE IF NOT EXISTS audit_log (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  ts INTEGER,
  event TEXT,
  client_id TEXT,
  details TEXT
);
```

**Function to write audit (db_worker)**

```cpp
void auditLog(sqlite3 *db, const String &event, const String &clientId, const String &details) {
  const char *sql = "INSERT INTO audit_log (ts, event, client_id, details) VALUES (?, ?, ?, ?);";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)time(NULL));
    sqlite3_bind_text(stmt, 2, event.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, clientId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, details.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  } else {
    updateOutput("Audit prepare failed");
  }
}
```

Call `auditLog` at major events.

**Test:**

1. After operations, query `audit_log` via DB worker and verify entries exist.

**Verify:** audit entries are logged for adds, gets, failures.

---

## Step 6 — Minimal per-session authorization token

**Goal:** Only allow commands after a session handshake (post-bond) that issues a session token (in-memory) and expires.

**Rationale:** Prevent raw paired client misuse (e.g., if paired but not unlocked on phone).

**Design:** After pairing, the mobile app sends an `AUTH` command containing proof (e.g., HMAC over a challenge using the app-held key). For MVP we can implement a simpler flow: after bonding, app sends `AUTH <passphrase>` or `AUTH <signed-challenge>`; ESP32 verifies and sets `sessionAuthorized=true` for e.g. 5 minutes.

**Simpler example for now (replace later with ECDH + signature):**

* On first connect, ESP32 sends random `nonce` to client (`CHALLENGE:<nonce>`).
* App computes `HMAC-SHA256(nonce, secret)` where secret is derived from provisioning; sends back `AUTH <hmac>`.
* ESP32 validates HMAC using the shared secret derived from provisioning or a stored device key.

**Implementation (simplified):**

```cpp
String currentNonce;
bool sessionAuthorized = false;
unsigned long sessionExpires = 0;

String genNonce() {
  uint32_t r = esp_random();
  return String(r, HEX);
}

String computeHmac(const String &key, const String &msg) {
  // use mbedtls or tiny-hmac implementation. For brevity pseudocode:
  // return hex(hmac_sha256(key, msg))
}

// On connect:
currentNonce = genNonce();
sendNotification("CHALLENGE:" + currentNonce);

// On AUTH handler:
if (cmd == "AUTH" && tokens.size()==2) {
  String clientHmac = tokens[1];
  String expected = computeHmac(deviceSecret, currentNonce);
  if (clientHmac == expected) {
    sessionAuthorized = true;
    sessionExpires = millis() + 5*60*1000; // 5 min
    updateOutput("Authorized");
  } else {
    updateOutput("Auth failed");
    rateLimiter.onFailedAuth();
  }
}
```

**Test:**

1. Use the app to perform `AUTH` handshake — then issue `get`/`add`.
2. Without `AUTH`, commands should be refused.

**Verify:** sessionAuthorized gating enforced.

---

## Step 7 — Chunking & MTU-aware notifications/read/write

**Goal:** Ensure large payloads are split into MTU-sized chunks and reassembled on client to support iOS.

**Rationale:** iOS often only supports default small MTU; avoid packet loss.

**Strategy:**

* Determine negotiated MTU on connect and set chunk size (min(20, MTU-3) for safety).
* Prefix each chunk with a small header: 1 byte flags (START=0x01, CONT=0x02, END=0x04), 2 bytes seq (optional).
* Reassemble on client.

**ESP32 helper:**

```cpp
void notifyLarge(BLECharacteristic *c, const std::string &payload) {
  size_t mtu = BLEDevice::getMTU(); // not exact in Arduino port; keep conservative 20
  size_t chunkSize = 20;
  const uint8_t START=0x01, CONT=0x02, END=0x04;
  size_t off=0;
  while (off < payload.size()) {
    size_t rem = payload.size() - off;
    size_t sendLen = min(chunkSize-1, rem); // 1 byte header
    uint8_t buf[21];
    uint8_t hdr = (off==0) ? START : (rem<=sendLen ? END : CONT);
    buf[0] = hdr;
    memcpy(buf+1, payload.data()+off, sendLen);
    c->setValue(buf, sendLen+1);
    c->notify();
    off += sendLen;
    delay(5); // small delay to allow BLE stack
  }
}
```

**Client side:** Assemble chunks by reading header flags until END.

**Test:**

1. Send `list` for many credentials that exceed 20 bytes response.
2. Verify client reassembles full text.

**Verify:** No truncation on iOS.

---

## Step 8 — ECDH session key + AES-GCM (secure transport & integrity)

**Goal:** Establish an ephemeral session key via ECDH and encrypt BLE payloads with AES-GCM.

**Rationale:** Even if BLE link layer is compromised, application-layer AEAD ensures confidentiality and integrity and protects against MITM when combined with pairing/bonding + authenticating public keys.

**Notes:** This step is heavier: uses mbedTLS APIs (available in Arduino/ESP-IDF). Keep code careful with entropy and ensure unique nonces. Use HKDF-SHA256 to derive keys.

**crypto_session.h** — simplified example outline (not full production code):

```cpp
#pragma once
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <string>

struct CryptoSession {
  mbedtls_ecp_group grp;
  mbedtls_ecp_keypair keypair;
  unsigned char sharedSecret[64]; // raw shared
  unsigned char sessionKey[32]; // derived via HKDF
  // init, generate keypair, compute shared secret, derive session key, encrypt/decrypt functions
  bool init();
  bool genKeyPair();
  bool computeShared(const unsigned char *peerPub, size_t len);
  bool deriveSessionKey();
  int encrypt(const uint8_t *in, size_t inLen, uint8_t *out, size_t &outLen, uint8_t *iv, size_t ivLen, uint8_t *tag, size_t tagLen);
  int decrypt(...);
};
```

**High-level flow:**

1. Both sides generate ECC P-256 keys.
2. Exchange public keys (during secure pairing or via a characteristic).
3. Compute ECDH shared secret; derive `sessionKey` via HKDF-SHA256.
4. Use AES-256-GCM with `sessionKey` for payload encryption. Use a per-message counter (unique nonce).
5. Include sequence number as AAD for AEAD to prevent replay.

**Important security notes:**

* Ensure **unique nonces** per key (use 96-bit nonce: 32-bit session ID + 64-bit counter).
* Prefer using device stored keys + authenticating public keys (fingerprint / bonding) to prevent MITM.

**Test:**

1. Implement minimal ECDH exchange with your Flutter app (app must support ECDH; Flutter side can use `package:pointycastle` or platform crypto).
2. After deriving sessionKey, encrypt a small message on app, decrypt on ESP32 and vice versa.
3. Validate tag verification fails on tampered payload.

**Verify:** encrypted messages decrypt correctly; tampered messages rejected.

---

## Step 9 — Testing checklist & Flutter client compatibility

**At each step** run these tests:

1. **BLE Security & Pairing (Step 1)**

   * Pair phone with device.
   * Try connecting with unpaired device — should be rejected.

2. **Parameterized SQL (Step 2)**

   * Add normal credential via BLE.
   * Try SQL-injection payload — database intact.

3. **Async DB worker (Step 3)**

   * Ensure BLE response returns quickly and DB operations processed in background.

4. **Rate Limiting (Step 4)**

   * Send > rate limit commands; device should block and display lockout.

5. **Audit Logging (Step 5)**

   * Verify `audit_log` entries after operations.

6. **Session Auth (Step 6)**

   * Ensure unauthorized sessions cannot run CRUD. After auth, allowed.

7. **Chunking (Step 7)**

   * Large `list` response reconstructs on client (both iOS & Android).

8. **ECDH + AES-GCM (Step 8)**

   * Encrypted roundtrip works; tampered payloads rejected.

**Flutter-specific notes:**

* Use `flutter_reactive_ble` and implement chunk reassembling.
* For ECDH & AES-GCM, use `cryptography` or `pointycastle` packages (or platform channels using platform crypto).
* For pairing, rely on OS-level BLE pairing; for ECDH exchange use characteristic reads/writes (make public keys readable during pairing).

---

## Step 10 — Final integration considerations (post-steps)

* **DB encryption at rest (DEK + KEK):** after session security is solid, move to encrypt DB file using AES-GCM and DEK wrapping via device key or app key as described earlier. This is heavier and may require SQLCipher or file-level encrypt/decrypt on open/close.
* **OTA:** plan secure OTA (signed firmware).
* **Backup:** encrypted export feature (user-controlled).
* **Key storage:** consider ATECC secure element for production.

---

## Appendix — Useful Code Snippets (ready to copy)

### A. Replace `executeSQL` with safe worker post (example)

```cpp
// inside CommandCallback.onWrite
void onWrite(BLECharacteristic *pChar) override {
  String val = String(pChar->getValue().c_str());
  // parse tokens minimally
  std::vector<String> tokens;
  char *token = strtok((char*)val.c_str(), " ");
  while (token) { tokens.push_back(String(token)); token = strtok(NULL, " "); }
  if (tokens.size() == 0) return;
  DbCommand cmd;
  if (tokens[0] == "add" && tokens.size() == 4) {
    cmd.type = DB_ADD; cmd.arg1 = tokens[1]; cmd.arg2 = tokens[2]; cmd.arg3 = tokens[3];
    postDbCommand(cmd);
    sendNotification("ACK: queued add");
  } else if (tokens[0] == "auth" && tokens.size() == 2) {
    // handle auth synchronously or via worker
  } else {
    sendNotification("ERR: invalid cmd");
  }
}
```

### B. Parameterized SELECT example (reusable function)

(see Step 2 snippet earlier — copy into db_worker)

### C. Chunk notify function

(see Step 7 snippet)

---

## How to use this `.md` with Copilot

1. Create separate commits/branches for each step (`step-01-ble-secure`, `step-02-sqlite-prepare`, ...).
2. For each branch, ask Copilot: *"Apply the changes from Step X in docs/implement_steps.md: create file Y and patch file Z with the snippet. Ensure it compiles."*
3. Run local builds & tests (Serial monitor + Flutter test client).
4. Merge into `develop` only after passing tests.

---

## Final notes & priorities

* **Do Step 1 and Step 2 first.** They are minimal and remove huge risks (open connections & SQLi).
* **Do Step 3 before Step 8.** You must not do heavy crypto inside BLE callbacks.
* **Keep test harness**: a small Flutter test client that can send scripted commands (AUTH, ADD, GET, LIST). Use it to run automated smoke tests after each step.
* **Document everything** in `docs/` so your legal team / SSIP can follow.

---

If you want, I can now:

* Generate each file's full code in separate messages (start with `ble_security.h` and updated `main.ino` for Step 1), compile-ready, and a tiny Flutter test script to run the pairing/auth flow for verification.
* Or I can produce the Flutter-side test code for ECDH & chunk reassembly.

Which should I produce first?
