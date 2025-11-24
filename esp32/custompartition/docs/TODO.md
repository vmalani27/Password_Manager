# Sprint Backlog

Active tasks for current development sprint. Organized by priority tier using Agile methodology.

## Document Purpose

This is the working task list for current sprint (2-week iteration). For long-term planning, see ROADMAP.md. For technical specifications, see ARCHITECTURE.md.

## Current Sprint

**Sprint 2:** November 15 - November 29, 2025  
**Sprint Goal:** Complete ECDH authentication and begin database safety improvements  
**Team Velocity:** ~8 story points per sprint (1 developer)

---

## Backlog Items

### Epic: Authentication & Device Binding

**Status:** 80% Complete

**Completed Stories:**
- [DONE] Implement ECDH key exchange - 3 points
  - Acceptance: secp256r1 curve, HKDF session key derivation working
  
- [DONE] Implement challenge-response auth - 2 points
  - Acceptance: HMAC-SHA256 verification passes for valid clients
  
- [DONE] Add device binding with NVS - 3 points
  - Acceptance: Paired device keys persist across reboots, unauthorized devices rejected

- [DONE] Flutter ECDH client implementation - 5 points
  - Status: Implementation guide complete, awaiting Flutter development
  - Blockers: None (parallel track)

---

## Sprint 2 Committed Work

### TIER S: Critical (Must Complete)

#### US-001: Database Worker Queue
**Priority:** P0 (Critical)  
**Story Points:** 5  
**Status:** Not Started

**User Story:**
As a developer, I need SQLite operations to run in a dedicated worker task so that concurrent BLE callback access doesn't corrupt the database.

**Acceptance Criteria:**
- [ ] Create FreeRTOS task for database operations
- [ ] Implement QueueHandle_t for command passing
- [ ] BLE callbacks enqueue commands instead of direct DB access
- [ ] Worker task processes queue and sends responses
- [ ] Test with rapid concurrent commands (10 requests/second)

**Technical Notes:**
```cpp
// Create queue in setup()
QueueHandle_t dbQueue = xQueueCreate(10, sizeof(DbCommand));

// BLE callback enqueues
DbCommand cmd = {.type = GET_PASSWORD, .site = "github.com"};
xQueueSend(dbQueue, &cmd, 0);

// Worker task processes
xTaskCreate(dbWorkerTask, "DB_Worker", 4096, NULL, 5, NULL);
```

**Definition of Done:**
- Code compiles without warnings
- No database corruption after 100 rapid operations
- Response time under 500ms per operation

---

#### US-002: Remove Plaintext Passwords Over BLE
**Priority:** P0 (Critical)  
**Story Points:** 3  
**Status:** Not Started  
**Dependencies:** Requires session_key from ECDH

**User Story:**
As a security-conscious user, I need passwords encrypted during BLE transmission so that sniffing attacks cannot capture credentials.

**Acceptance Criteria:**
- [ ] Encrypt password with AES-GCM using session_key
- [ ] Include nonce and authentication tag
- [ ] Flutter client decrypts with same session_key
- [ ] Remove all `sendNotification("Password: " + pw)` calls
- [ ] Test with BLE sniffer to verify encryption

**Implementation:**
```cpp
// ESP32 side
String encryptedPw = encryptWithSessionKey(password, session_aes_key);
sendNotification("ENCRYPTED_PW:" + encryptedPw);

// Flutter side
String decrypted = decryptWithSessionKey(encryptedPw, sessionKey);
```

**Definition of Done:**
- Wireshark BLE capture shows no plaintext passwords
- Flutter successfully decrypts all passwords
- Error handling for decryption failures

---

#### US-003: Strengthen Session Tokens
**Priority:** P1 (High)  
**Story Points:** 1  
**Status:** Not Started

**User Story:**
As a security engineer, I need session tokens to be 128-bit minimum so that brute force attacks are infeasible during the deprecation period before legacy auth is removed.

**Acceptance Criteria:**
- [ ] Replace `random()` with `esp_random()`
- [ ] Generate 16 bytes (128 bits) minimum
- [ ] Convert to hex string for transmission
- [ ] Update Flutter client to handle longer tokens
- [ ] Add note that this is temporary (will be removed after ECDH fully deployed)

**Implementation:**
```cpp
String generateSessionToken() {
    uint8_t token[16];
    for (int i = 0; i < 16; i++) {
        token[i] = esp_random() & 0xFF;
    }
    return bytesToHex(token, 16);
}
```

**Definition of Done:**
- Token entropy verified (Shannon entropy > 7.5)
- Legacy auth still works with new tokens
- Documented as deprecated in code comments

---

## Sprint 3 Planned Work (Dec 1-15, 2025)

### TIER A: High Security

#### US-004: Migrate AES-CBC to AES-GCM
**Priority:** P0 (Critical)  
**Story Points:** 5

**User Story:**
As a security engineer, I need authenticated encryption for password storage so that database tampering is detectable.

**Acceptance Criteria:**
- [ ] Replace mbedtls_aes_crypt_cbc with mbedtls_gcm_crypt_and_tag
- [ ] Store {nonce || tag || ciphertext} in database
- [ ] Verify authentication tag on decrypt
- [ ] Migrate existing credentials to new format
- [ ] Update secure_core library API

---

#### US-005: Remove Static BLE PIN
**Priority:** P1 (High)  
**Story Points:** 3

**User Story:**
As a user, I need a unique BLE PIN per device so that the default PIN (123456) vulnerability is eliminated.

**Acceptance Criteria:**
- [ ] Generate random 6-digit PIN on first boot
- [ ] Store PIN in NVS
- [ ] Display PIN on OLED once during pairing
- [ ] Add "show PIN" command for re-display if forgotten
- [ ] Update Flutter app to prompt for PIN input

---

## Backlog (Not Committed)

### TIER B: Refactoring

**US-006:** Split main.cpp into modules - 8 points  
**US-007:** Normalize logging system - 2 points  
**US-008:** Simplify BLE initialization - 1 point

### TIER C: Cleanup

**US-009:** Extract constants to config.h - 1 point  
**US-010:** Add watchdog timer - 2 points  
**US-011:** Add version info to advertising - 1 point

---

## Sprint Burndown

**Total Committed:** 9 story points  
**Completed:** 0 points  
**Remaining:** 9 points

**Days Remaining:** 7 days  
**Target Velocity:** 8 points/sprint  
**On Track:** Yes (buffer of 1 point)

---

## Blockers

None currently.

---

## Sprint Retrospective Template

**What Went Well:**
- ECDH implementation completed ahead of schedule
- Device binding working reliably

**What Needs Improvement:**
- Need to start Flutter client work earlier
- Database worker queue more complex than estimated

**Action Items for Next Sprint:**
- Break down large tasks into smaller subtasks
- Allocate time for Flutter development in parallel

---

## Task States

**Not Started:** Task in backlog, not yet begun  
**In Progress:** Actively being worked on  
**Blocked:** Waiting on dependency or external input  
**In Review:** Code complete, awaiting testing  
**Done:** Acceptance criteria met, merged to main

---

## Definition of Ready (Task Prerequisites)

Before moving task to "In Progress":
- [ ] Acceptance criteria defined
- [ ] Technical approach documented
- [ ] Dependencies identified
- [ ] Story points estimated
- [ ] Assigned to developer

## Definition of Done (Task Completion)

Before marking task "Done":
- [ ] Code implements acceptance criteria
- [ ] Code compiles without warnings
- [ ] Manual testing passed
- [ ] No new security vulnerabilities introduced
- [ ] Documentation updated (if API changed)
- [ ] Committed to version control

---

## Document Maintenance

**Update Frequency:** Daily during active sprint  
**Owner:** Development Team  
**Last Updated:** November 22, 2025

**Update Triggers:**
- Task status changes
- New blockers identified
- Sprint planning completed
- Sprint retrospective held
