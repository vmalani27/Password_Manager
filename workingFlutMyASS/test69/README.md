# ESP32 Password Manager

A secure Bluetooth Low Energy (BLE) password manager using Flutter and ESP32 with ECDH key exchange and device pairing.

## Security Features

### ✅ Implemented (Current State)

**Phase 1 - ECDH Key Exchange & Device Pairing:**
- ✅ ECDH key exchange using secp256r1 curve
- ✅ HKDF-based session key derivation (salt: "BLE_PASSWORD_MGR")
- ✅ Challenge-response authentication with HMAC-SHA256
- ✅ Persistent device pairing (one phone ↔ one ESP32)
- ✅ Saved keys in SharedPreferences (Flutter) and NVS (ESP32)
- ✅ First pairing vs reconnection logic
- ✅ Unpair functionality from both sides
- ✅ Protection against connecting to different device while paired
- ✅ ESP32 rejects connections from non-paired devices (ECDH_ALREADY_PAIRED)

**BLE Protocol:**
- Connection → ECDH handshake → Challenge-response → Authenticated session
- Responses: `ECDH_OK` (reconnection), `ECDH_OK_PAIRED` (first pairing), `ECDH_ALREADY_PAIRED` (rejection)
- Commands: `ECDH_AUTH`, `RESPOND:<hmac>`, `UNPAIR`

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
2. Client generates ECDH key pair
3. Client reads ESP32's public key
4. Client computes shared secret and session key
5. Client sends public key to ESP32
6. ESP32 responds with `ECDH_OK_PAIRED`
7. Both sides save pairing data (keys + device info)
8. Challenge-response authentication proceeds

### Reconnection
1. Client checks if device is paired
2. Client loads saved private/public keys
3. Client reads ESP32's public key (verify it matches)
4. Client computes shared secret and session key
5. Client sends saved public key to ESP32
6. ESP32 recognizes client and responds `ECDH_OK`
7. Challenge-response authentication proceeds

### Unpair
- From Flutter: Settings → Unpair Device → Sends `UNPAIR` command → Removes local data
- From ESP32: Button press → Clears NVS → Rejects connections with `ECDH_ALREADY_PAIRED`

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

## Dependencies

- `flutter_reactive_ble: ^5.3.1` - BLE communication
- `pointycastle: ^3.7.3` - ECDH cryptography
- `convert: ^3.1.1` - Hex encoding
- `shared_preferences: ^2.2.3` - Persistent storage
- `permission_handler: ^11.3.1` - Android/iOS permissions
- `flutter_riverpod: ^2.5.1` - State management