# Complete Application Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                         APP LAUNCH                                  │
│                         main.dart                                   │
│                    ProviderScope + HomeScreen                       │
└────────────────────────────┬────────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                       HOME SCREEN                                   │
│                                                                     │
│  [Icon: Bluetooth Disabled]                                         │
│  "Disconnected"                                                     │
│                                                                     │
│  ┌────────────────────────────────────────────┐                    │
│  │  [🔍 Bluetooth] Connect to ESP32           │                    │
│  └────────────────────────────────────────────┘                    │
│                                                                     │
└────────────────────────────┬────────────────────────────────────────┘
                             │ User taps "Connect"
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                   DEVICE SCAN SCREEN                                │
│                                                                     │
│  ━━━━━━━━━━━━━━━━━━━━━━━  (Scanning...)                            │
│                                                                     │
│  Scanning for ESP32 devices...                                     │
│                                                                     │
│  ┌──────────────────────────────────────────────────┐              │
│  │ [📡] ESP32-GATT-Manager                          │              │
│  │      XX:XX:XX:XX:XX:XX                           │              │
│  │                            [Connect] ────────────┼──────────┐   │
│  └──────────────────────────────────────────────────┘          │   │
│                                                                 │   │
└─────────────────────────────────────────────────────────────────┼───┘
                                                                  │
                             User taps "Connect"                 │
                                                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│               OS PAIRING DIALOG (First Time Only)                   │
│                                                                     │
│  ┌───────────────────────────────────────────────────┐             │
│  │  Bluetooth Pairing Request                         │             │
│  │                                                    │             │
│  │  Enter PIN for ESP32-GATT-Manager                 │             │
│  │                                                    │             │
│  │  [______] (User enters: 123456)                   │             │
│  │                                                    │             │
│  │  [Cancel]                            [Pair] ──────┼──────┐      │
│  └───────────────────────────────────────────────────┘      │      │
│                                                              │      │
└──────────────────────────────────────────────────────────────┼──────┘
                                                               │
                             OS completes bonding             │
                             ESP32 stores bond key            │
                                                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                  SESSION AUTHENTICATION                             │
│                                                                     │
│  ⏳ Connecting to ESP32...                                          │
│  Please enter PIN 123456 if prompted                               │
│                                                                     │
│  Flutter → ESP32: "request_token"                                  │
│  ESP32 → Flutter: "TOKEN A1B2C3D4"                                 │
│  Flutter → ESP32: "auth A1B2C3D4"                                  │
│  ESP32 → Flutter: "AUTH OK" ✓                                      │
│                                                                     │
└────────────────────────────┬────────────────────────────────────────┘
                             │ Navigate back
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                       HOME SCREEN                                   │
│                      (Connected State)                              │
│                                                                     │
│  [Icon: Bluetooth Connected] ✓                                      │
│  "Connected & Ready"                                                │
│  ESP32-GATT-Manager                                                 │
│  X credentials stored                                               │
│                                                                     │
│  ┌────────────────────────────────────────────┐                    │
│  │  [🔑 Key] Manage Credentials ──────────────┼────────┐           │
│  └────────────────────────────────────────────┘        │           │
│                                                         │           │
│  ┌────────────────────────────────────────────┐        │           │
│  │  [🚪 Logout] Disconnect                    │        │           │
│  └────────────────────────────────────────────┘        │           │
│                                                         │           │
└─────────────────────────────────────────────────────────┼───────────┘
                                                          │
                             User taps "Manage"          │
                                                          ▼
┌─────────────────────────────────────────────────────────────────────┐
│                 CREDENTIAL MANAGER SCREEN                           │
│                                                                     │
│  [↩] Credentials                                        [↻]         │
│  ━━━━━━━━━━━━━━━━━━━━━━━  (Loading...)                             │
│                                                                     │
│  3 credential(s) stored                                             │
│                                                                     │
│  ┌──────────────────────────────────────────────────┐              │
│  │ [🔑] github.com                            [⋮] ───┼──┐          │
│  │      john@example.com                            │  │          │
│  └──────────────────────────────────────────────────┘  │          │
│  ┌──────────────────────────────────────────────────┐  │          │
│  │ [🔑] google.com                            [⋮]   │  │          │
│  │      jane@gmail.com                              │  │          │
│  └──────────────────────────────────────────────────┘  │          │
│  ┌──────────────────────────────────────────────────┐  │          │
│  │ [🔑] facebook.com                          [⋮]   │  │          │
│  │      bob@fb.com                                  │  │          │
│  └──────────────────────────────────────────────────┘  │          │
│                                                         │          │
│                                              [+] ───────┼──────┐   │
│                                                         │      │   │
└─────────────────────────────────────────────────────────┼──────┼───┘
                                                          │      │
                    User taps menu (⋮)                   │      │
                                                          ▼      │
┌─────────────────────────────────────────────────────────────┐ │
│  CREDENTIAL ACTIONS                                         │ │
│  ┌────────────────────────────────────────────────────────┐ │ │
│  │ [👁] View Password                                     │ │ │
│  │ [✏] Update Password                                    │ │ │
│  │ [🗑] Delete                                            │ │ │
│  └────────────────────────────────────────────────────────┘ │ │
└─────────────────────────────────────────────────────────────┘ │
                                                                │
                    User taps "View Password"                  │
                                                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    VIEW PASSWORD DIALOG                             │
│                                                                     │
│  github.com                                                         │
│                                                                     │
│  Username: john@example.com                                         │
│  Password: SecurePass123!                                           │
│                                                                     │
│  ┌────────────────────────────────────────────┐                    │
│  │  [📋 Copy] Copy Password                   │                    │
│  └────────────────────────────────────────────┘                    │
│                                                                     │
│                                   [Close]                           │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘

                    User taps + button                              │
                                                                     │
                                                                     ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    ADD CREDENTIAL DIALOG                            │
│                                                                     │
│  Add Credential                                                     │
│                                                                     │
│  Site:     [example.com________________]                            │
│  Username: [user@example.com___________]                            │
│  Password: [••••••••••••••••••••••••••]                            │
│                                                                     │
│  [Cancel]                                        [Add] ─────────┐   │
│                                                                 │   │
└─────────────────────────────────────────────────────────────────┼───┘
                                                                  │
                             User taps "Add"                      │
                                                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│           FLUTTER → ESP32 COMMUNICATION                             │
│                                                                     │
│  Flutter: "add example.com user@example.com SecurePass123!"        │
│  ESP32:   Executes prepared SQL INSERT statement                   │
│  ESP32:   "Added" ✓                                                │
│  Flutter: Auto-refreshes credential list                           │
│  Flutter: Shows snackbar "Credential added successfully"           │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## State Flow Diagram

```
Application State Machine:

┌──────────────┐
│ Disconnected │ ◄─────────────────────────┐
└──────┬───────┘                           │
       │ scan()                            │
       ▼                                   │
┌──────────────┐                           │
│   Scanning   │                           │
└──────┬───────┘                           │
       │ devices found                     │
       ▼                                   │
┌──────────────┐                           │
│   Devices    │                           │
│  Available   │                           │
└──────┬───────┘                           │
       │ connect(deviceId)                 │
       ▼                                   │
┌──────────────┐                           │
│  Connecting  │ (OS pairing happens)      │
└──────┬───────┘                           │
       │ BLE connected                     │
       ▼                                   │
┌──────────────┐                           │
│   Connected  │                           │
└──────┬───────┘                           │
       │ performAuthentication()           │
       ▼                                   │
┌──────────────┐                           │
│Authenticating│                           │
└──────┬───────┘                           │
       │ session authorized                │
       ▼                                   │
┌──────────────┐                           │
│Authenticated │ ◄──────────────┐          │
│   (Ready)    │                │          │
└──────┬───────┘                │          │
       │                        │          │
       │ ┌──────────────────────┘          │
       │ │ CRUD Operations:                │
       │ │ • addCredential()               │
       │ │ • getPassword()                 │
       │ │ • updateCredential()            │
       │ │ • deleteCredential()            │
       │ │ • refreshCredentials()          │
       │ └──────────────────────┐          │
       │                        │          │
       │ disconnect()           │          │
       └────────────────────────┴──────────┘
```

---

## Error Handling Flow

```
Any Operation:
├─ Try
│  ├─ Execute operation
│  ├─ Success → Update state
│  └─ Show success snackbar
└─ Catch
   ├─ Log error
   ├─ Update state.errorMessage
   ├─ Show error UI
   └─ Allow retry

Connection Errors:
├─ Timeout → "Connection timeout. Check ESP32 power."
├─ Pairing Failed → "Pairing failed. Verify PIN 123456."
├─ Auth Failed → "Authentication failed. Wrong token."
└─ Locked Out → "Too many attempts. Wait 1 minute."

CRUD Errors:
├─ Add Failed → "Failed to add credential: <reason>"
├─ Get Failed → "Failed to retrieve password: <reason>"
├─ Update Failed → "Failed to update password: <reason>"
└─ Delete Failed → "Failed to delete credential: <reason>"

All errors displayed in:
├─ Red error card (persistent)
└─ Red snackbar (temporary)
```

---

## Complete Tech Stack

```
┌─────────────────────────────────────────────┐
│              Flutter UI Layer               │
│  • Material Design 3                        │
│  • StatelessWidget / StatefulWidget         │
│  • ConsumerWidget (Riverpod)                │
│  • Dialogs, Snackbars, Navigation           │
└──────────────────┬──────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────┐
│         State Management Layer              │
│  • flutter_riverpod 2.5.1                   │
│  • AppStateProvider                         │
│  • Reactive state updates                   │
└──────────────────┬──────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────┐
│            Service Layer                    │
│  • BleConnectionService                     │
│  • AuthService                              │
│  • CredentialService                        │
│  • CommandService                           │
│  • SessionManager                           │
└──────────────────┬──────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────┐
│            BLE Layer                        │
│  • flutter_reactive_ble 5.3.1               │
│  • Scan, Connect, Read, Write, Notify       │
│  • MTU negotiation                          │
└──────────────────┬──────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────┐
│       Operating System BLE Stack            │
│  • Android: BlueZ                           │
│  • iOS: Core Bluetooth                      │
│  • Handles pairing/bonding                  │
└──────────────────┬──────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────┐
│            ESP32 Hardware                   │
│  • Bluedroid BLE Stack                      │
│  • SQLite Database                          │
│  • OLED Display                             │
│  • SD Card Storage                          │
└─────────────────────────────────────────────┘
```

---

## Ready to Deploy! 🚀

All components implemented with **pure functionality**:
- ✅ No library UI copying
- ✅ Clean architecture
- ✅ Proper error handling
- ✅ User feedback everywhere
- ✅ 0 compilation errors
- ✅ Ready for hardware testing
