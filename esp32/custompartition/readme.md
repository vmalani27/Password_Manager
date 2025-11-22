# ESP32 Password Manager

Hardware-based password vault with cryptographic authentication using ESP32, BLE, and Flutter.

## Project Overview

**Status:** Sprint 2 - Protocol Hardening (Phase 1.2)  
**Version:** 0.2.0-alpha  
**Last Updated:** November 22, 2025

### What This Project Does

Secure credential storage on ESP32 hardware, accessed wirelessly via Flutter mobile app. Credentials are encrypted with hardware-backed keys and protected by ECDH cryptographic authentication.

### Key Features

**Implemented:**
- AES-256-CBC encryption with eFuse-derived keys
- ECDH key exchange (secp256r1 curve)
- Challenge-response authentication (HMAC-SHA256)
- Device binding with NVS persistence
- SQLite encrypted credential storage
- BLE secure connections with pairing

**In Progress:**
- Database worker queue (prevent corruption)
- AES-GCM migration (authenticated encryption)
- Command encryption over BLE

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

### Pairing with Flutter App

1. Open Flutter app, scan for devices
2. Select "ESP32-PWD-Manager"
3. Enter PIN: `123456` (default, will be randomized in future)
4. App performs ECDH handshake
5. Device bound - only this phone can access passwords

## Project Structure

```
esp32/custompartition/
├── src/
│   └── main.cpp              (1177 lines - needs refactoring)
├── lib/
│   └── secure_core/          (Encryption library)
├── include/                  (Headers - to be created)
├── docs/
│   ├── ARCHITECTURE.md       (Technical design)
│   ├── ROADMAP.md            (Long-term plan)
│   ├── TODO.md               (Current sprint backlog)
│   └── ECDH_FLUTTER_GUIDE.md (Mobile app integration)
├── platformio.ini            (Build configuration)
└── partitions.csv            (Flash layout)
```

## Documentation

### For Users
- **README.md** (this file) - Quick start and overview

### For Developers
- **[ARCHITECTURE.md](docs/ARCHITECTURE.md)** - System design, security model, component details
- **[ROADMAP.md](docs/ROADMAP.md)** - Product roadmap, phased development plan
- **[TODO.md](docs/TODO.md)** - Current sprint tasks, prioritized backlog

### For Integration
- **[ECDH_FLUTTER_GUIDE.md](docs/ECDH_FLUTTER_GUIDE.md)** - Flutter client implementation guide

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

**Sprint 2 (Nov 15 - Nov 29, 2025)**

Completed:
- [x] ECDH key exchange implementation
- [x] Challenge-response authentication
- [x] Device binding with NVS persistence

In Progress:
- [ ] Database worker queue
- [ ] Remove plaintext passwords over BLE
- [ ] Strengthen session tokens

See [TODO.md](docs/TODO.md) for detailed task list.

## Security Architecture

### Defense Layers

**Layer 1 - Physical:** eFuse hardware root of trust  
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
