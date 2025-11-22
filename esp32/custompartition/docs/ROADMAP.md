# ROADMAP

Industry-standard progression for security hardware development.

---

## PHASE 0 - Cleanup & Stabilization (1-2 weeks)

Goal: Make firmware stable enough that future improvements won't crumble.

### 0.1 - Repository & Development Hygiene

- Migrate fully to PlatformIO
- Add .gitignore
- Create repo structure: /firmware, /docs, /hardware
- Add mandatory docs: ROADMAP.md, TODO.md, CHANGELOG.md, PROTOCOL.md, THREAT_MODEL.md

### 0.2 - Codebase Clean Boot

- Remove startKeyManagerTask()
- Make key derivation synchronous
- Get main.cpp to compile + run under PlatformIO
- Remove leftover Arduino-IDE garbage

### 0.3 - Stability Fixes

- Move all DB ops to a worker queue
- Fix connection timeout logic
- Fix BLE callback crashes
- Stop sending decrypted passwords over BLE
- Remove plaintext logs

Output: A repo that compiles cleanly, runs reliably, and has documented structure.

---

## PHASE 1 - Protocol Hardening (3-5 weeks)

Goal: Finalize software architecture so that future hardware can implement it safely.

### 1.1 - Cryptographic Security

- Replace AES-CBC with AES-GCM
- Add integrity (tag) verification
- Zero all sensitive buffers
- Increase session token to 128-256 bits

### 1.2 - Authentication Security

- Enforce BLE bonded-device whitelist
- Replace static PIN with runtime PIN stored in NVS
- Implement ECDH key exchange (software-only for now)
- Implement challenge-response using ECDH-derived session key
- Remove token-only auth once ECDH works

### 1.3 - Credential Lifecycle

- Redesign insert/update/get/delete to use worker queue
- Add DB journaling + atomic backups
- Create randomness test routine for IVs and session keys

### 1.4 - UX & Flow

- Define exact flow: Pair -> Approve -> Authorize -> Command
- Document all command formats in PROTOCOL.md

Output: A mature, secure prototype protocol ready for integration into secure hardware.

---

## PHASE 2 - Hardware Hardening (6-12 weeks)

Goal: Remove security risks of ESP32 + SD by migrating secrets to a secure element.

### 2.1 - Secure Element Selection

Pick one:
- NXP SE050 (best for security certifications)
- Microchip ATECC608B (cheaper, easier)
- STSAFE-A110 (balanced, good docs)

### 2.2 - Key Storage Migration

- Device root key moves into SE
- ECDH private operations done inside SE
- DB master key wrapped/unwrapped via SE
- BLE pairing public keys stored in SE

### 2.3 - Secure Boot & Flash Encryption

If staying on ESP32:
- Enable ESP-IDF Secure Boot
- Enable Flash Encryption
- Lock JTAG
- Move critical config to NVS with encryption enabled

### 2.4 - External Flash / Internal Flash Migration

Replace SD card with:
- QSPI flash (Winbond) OR
- eMMC (industrial grade)

SQLite DB sits on encrypted flash, not removable SD.

Output: A hardened hardware platform where secrets cannot be extracted by attackers.

---

## PHASE 3 - Productization (3-6 months)

Goal: Turn hardened prototype into a manufacturable device.

### 3.1 - PCB + Hardware Engineering

- Design custom PCB
- ESP32 or secure MCU + SE
- USB-C port for HID
- Buttons / biometric (optional)
- Secure mounting + tamper mesh (optional)

### 3.2 - Manufacturing Pipeline

- JLCPCB / PCBWay prototypes
- Factory provisioning:
  - Device key injection
  - Firmware signing
  - Secure boot keys

### 3.3 - HID Autofill

- USB HID implementation
- HID-only password output
- Zero plaintext to phone
- Mobile app becomes "approval" device, not password receiver

### 3.4 - Backup & Recovery

Choose between:
- Encrypted backup file
- Shamir split keys
- Recovery phrase (like hardware wallets)

Output: A fully manufacturable device with tamper-resistant security.

---

## PHASE 4 - Certification & Launch (3-9 months)

Goal: Meet real-world enterprise privacy/security requirements.

### 4.1 - Security Testing

- Fuzz BLE
- Fuzz DB
- Side-channel checks
- Penetration testing

### 4.2 - Compliance

Optional but valuable:
- Common Criteria (EAL5+ for SE)
- FIPS 140-3 Level 2/3
- FCC/CE hardware certification
- GDPR/DPDPB compliance docs

### 4.3 - Alpha Launch

- Distribute 20-100 units
- Gather telemetry (non-sensitive)
- Fix UX issues
- Iterate

Output: Market-ready launchable product.

---

## THE NO-OVERWHELM RULE

At any point in time, work on ONE SUB-MILESTONE only.

No jumping. No rethinking architecture daily. No spiraling.

---

## IMMEDIATE NEXT 7 DAYS PLAN

### Day 1-2: Repo cleanup & PlatformIO stabilization
- Get main.cpp compiling
- Fix directory structure
- Add docs

### Day 3-4: Remove key manager task & add DB worker
- Make everything synchronous
- Add queue-based DB execution

### Day 5-6: AES-GCM migration
- Replace crypto functions
- Update DB schema

### Day 7: Document PROTOCOL.md
- Current behavior only
- No future ideas

After this week: stable foundation achieved.
