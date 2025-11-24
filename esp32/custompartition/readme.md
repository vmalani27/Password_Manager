# ESP32 Password Manager

Hardware-based password vault with cryptographic authentication using ESP32, BLE, and Flutter.

## Project Overview

**Status:** Sprint 2 Complete - Refactored Architecture  
**Version:** 0.3.0-alpha  
**Last Updated:** November 24, 2025

### What This Project Does

Secure credential storage on ESP32 hardware, accessed wirelessly via Flutter mobile app. Credentials are encrypted with hardware-backed keys and protected by ECDH cryptographic authentication.

### Key Features

**Implemented:**
- ✅ **Modular Architecture** - Refactored from 1241 lines monolithic to 5 manager modules
- ✅ **AES-256-CBC Encryption** - eFuse-derived hardware keys for database encryption
- ✅ **ECDH Key Exchange** - secp256r1 curve device binding
- ✅ **Two-Layer Authentication** - ECDH device pairing + token-based session authorization
- ✅ **Device Binding** - NVS persistence, one phone ↔ one ESP32
- ✅ **SQLite Encrypted Storage** - Credentials encrypted at rest on SD card
- ✅ **BLE Protocol** - Command/response with TX/RX characteristics
- ✅ **Session Management** - Token auth with lockout protection
- ✅ **Partition Viewer** - Boot-time flash partition and memory diagnostics

**In Progress:**
- 🔄 Flutter client integration (ECDH + token auth flow)
- 🔄 Reconnection stability improvements

**Planned (Phase 2):**
- Database worker queue (prevent corruption)
- AES-GCM migration (authenticated encryption)
- Command encryption over BLE
- Dynamic PIN generation

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

**Build Stats (Nov 24, 2025):**
- Compilation time: 31.08 seconds
- Flash usage: 1,641,537 bytes (41.7% of 3.93MB)
- RAM usage: 46,944 bytes (14.3% of 320KB)

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

### Refactored Modular Architecture (Nov 2025)

```
esp32/custompartition/
├── include/
│   ├── config.h              (58 lines - All constants, UUIDs, pin definitions)
│   ├── crypto_manager.h      (80 lines - ECDH, NVS device binding)
│   ├── ble_manager.h         (120 lines - BLE server, callbacks)
│   ├── db_manager.h          (50 lines - SQLite operations)
│   └── ui_manager.h          (40 lines - OLED display)
├── src/
│   ├── main.cpp              (166 lines - Setup + partition diagnostics)
│   ├── crypto_manager.cpp    (380 lines - ECDH implementation)
│   ├── ble_manager.cpp       (474 lines - BLE + command handling)
│   ├── db_manager.cpp        (230 lines - Database operations)
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

**Sprint 2 (Nov 15 - Nov 29, 2025) -COMPLETED**

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

**Sprint 3 (Nov 30 - Dec 13, 2025) - PLANNING**

Focus Areas:
- Flutter client stability (reconnection handling)
- Database worker queue (prevent corruption)
- Remove plaintext passwords over BLE (AES-GCM)
- Dynamic PIN generation (replace static 123456)

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
