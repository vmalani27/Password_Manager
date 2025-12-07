# Product Roadmap

Strategic development plan for ESP32 Password Manager. Organized by phases with clear goals and deliverables.

## Document Purpose

This roadmap defines long-term product vision and feature delivery timeline. For current sprint work, see TODO.md. For technical implementation details, see ARCHITECTURE.md.

## Version History

- v0.1.0 (Nov 1, 2025) - Initial prototype with basic encryption
- v0.2.0 (Nov 22, 2025) - ECDH authentication and device binding
- v0.3.0 (Target: Dec 15, 2025) - Database worker queue and AES-GCM
- v1.0.0 (Target: Q1 2026) - Production-ready firmware

---

## Phase 0: Foundation (COMPLETED)

**Timeline:** 2 weeks  
**Status:** DONE

**Goals:**
- Establish development environment
- Create stable codebase foundation
- Set up project documentation

**Deliverables:**
- [x] PlatformIO project structure
- [x] Synchronous key derivation (removed async task manager)
- [x] Documentation framework (ROADMAP, TODO, ARCHITECTURE, CHANGELOG)
- [x] Clean compilation and upload

---


## Phase 1: Protocol Hardening (IN PROGRESS)

**Timeline:** 4-6 weeks  
**Status:** Sprint 3 of 4  
**Target Completion:** December 2025

**Goals:**
- Implement cryptographic authentication (**DONE**)
- Secure communication protocol (**DONE**)
- Prevent common attack vectors (**IN PROGRESS**)
- Finalize software architecture (**IN PROGRESS**)

### Phase 1.1: Cryptographic Security (**COMPLETE**)
**All deliverables complete.**

### Phase 1.2: Authentication Security (**COMPLETE on ESP32, IN PROGRESS on Flutter**)
- ECDH key exchange, HKDF session key, challenge-response, device binding, and unauthorized device rejection are all implemented on ESP32.
- Flutter ECDH client implementation: **Guide complete, code in progress**

### Phase 1.3: Data Integrity (**PARTIAL**)
**Target:** Sprint 3 (Dec 1-15, 2025)
- Database worker queue: **DONE**
- Atomic database operations: **PLANNED**
- AES-GCM migration (authenticated encryption): **PLANNED**
- Session token strengthening (128-bit): **IN PROGRESS**

### Phase 1.4: Command Security (**PLANNED**)
**Target:** Sprint 4 (Dec 15-31, 2025)
- Encrypt commands with session key: **CTR mode implemented, GCM planned**
- Remove plaintext password transmission: **PLANNED**
- Dynamic PIN generation and storage: **PLANNED**
- Audit all security-sensitive logging: **PLANNED**

**Phase 1 Success Criteria:**
- Zero known authentication bypasses (**IN PROGRESS**)
- All credentials encrypted at rest and in transit (**CTR mode, GCM planned**)
- Complete cryptographic device binding (**DONE**)
- Documented protocol specification (**IN PROGRESS**)

---

## Phase 2: Hardware Hardening (PLANNED)

**Timeline:** 8-12 weeks  
**Status:** Not Started  
**Target Start:** Q1 2026

**Goals:**
- Move secrets to secure element
- Enable hardware security features
- Replace SD card with internal flash
- Achieve certification-grade security

### Phase 2.1: Secure Element Integration

**Deliverables:**
- [ ] Select secure element (SE050/ATECC608/STSAFE)
- [ ] Migrate root key to SE
- [ ] Perform ECDH operations in SE
- [ ] Wrap database keys via SE

### Phase 2.2: ESP32 Security Features

**Deliverables:**
- [ ] Enable Secure Boot
- [ ] Enable Flash Encryption
- [ ] Enable NVS encryption
- [ ] Lock JTAG interface

### Phase 2.3: Storage Migration

**Deliverables:**
- [ ] Replace SD card with QSPI flash or eMMC
- [ ] Migrate SQLite to internal storage
- [ ] Implement wear leveling
- [ ] Add backup/recovery mechanism

**Phase 2 Success Criteria:**
- Private keys never exposed to software
- Physical attack resistance
- Certified secure boot chain
- Tamper detection mechanisms

---

## Phase 3: User Experience (PLANNED)

**Timeline:** 4-6 weeks  
**Status:** Not Started  
**Target Start:** Q2 2026

**Goals:**
- Improve usability and reliability
- Add convenience features
- Implement backup/restore
- OTA firmware updates

**Deliverables:**
- [ ] USB HID keyboard emulation (type passwords)
- [ ] OTA firmware update system
- [ ] Encrypted backup export/import
- [ ] Multiple device pairing support
- [ ] Web-based configuration interface

**Phase 3 Success Criteria:**
- No USB cable needed for normal operation
- Backup/restore working reliably
- Firmware updates without re-pairing
- User documentation complete

---

## Phase 4: Production Readiness (PLANNED)

**Timeline:** 6-8 weeks  
**Status:** Not Started  
**Target Start:** Q3 2026

**Goals:**
- Production-quality code and testing
- Hardware design finalization
- Manufacturing preparation
- Certification groundwork

**Deliverables:**
- [ ] Comprehensive unit test suite
- [ ] Integration test framework
- [ ] PCB design (custom board)
- [ ] Enclosure design
- [ ] Manufacturing documentation
- [ ] Compliance testing (FCC, CE)

**Phase 4 Success Criteria:**
- 95%+ test coverage
- Zero critical bugs
- Manufacturing-ready design
- Certification path identified

---

## Phase 5: Optional Enhancements (FUTURE)

**Timeline:** Ongoing  
**Status:** Backlog

**Potential Features:**
- Multi-factor authentication
- Biometric unlock (fingerprint sensor)
- Geographic restrictions (GPS-based)
- Cloud synchronization (encrypted)
- Browser extension integration
- Hardware security key mode (FIDO2/U2F)

---

## Risk Management

**Technical Risks:**
- ECDH implementation vulnerabilities → Mitigation: Code review, penetration testing
- SD card corruption → Mitigation: Worker queue, journaling
- eFuse key extraction → Mitigation: Migrate to secure element

**Timeline Risks:**
- Phase 2 secure element integration → 3-4 weeks buffer built in
- Flutter client development → Parallel track, not blocking

**Dependency Risks:**
- mbedTLS API changes → Pin to stable version
- ESP-IDF breaking changes → Test before upgrades
- Component availability → Identify alternate parts early

---

## Decision Log

**Nov 1, 2025:** Chose PlatformIO over Arduino IDE (better dependency management)  
**Nov 8, 2025:** Adopted ECDH over static shared secret (forward secrecy required)  
**Nov 15, 2025:** Implemented device binding at application layer (stronger than BLE bonding)  
**Nov 22, 2025:** Deferred BLE bonding whitelist to Phase 5 (application layer sufficient)

---

## Success Metrics

**Security Metrics:**
- Zero successful authentication bypasses in penetration testing
- All stored credentials require device + PIN + ECDH key
- Session keys provide forward secrecy

**Quality Metrics:**
- No data corruption in 1000-cycle stress test
- Clean compilation with zero warnings
- All public APIs documented

**Usability Metrics:**
- Pairing process under 30 seconds
- Password retrieval under 2 seconds
- Zero user-facing error messages in normal operation

---

## Document Maintenance

**Update Frequency:** Reviewed every sprint (2 weeks)  
**Owner:** Project Lead  
**Last Review:** November 22, 2025

**Review Triggers:**
- Phase completion
- Major architecture change
- New security requirement identified
- Timeline adjustment needed
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
