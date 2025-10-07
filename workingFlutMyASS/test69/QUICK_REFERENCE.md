# Flutter Password Manager - Quick Reference

## 🎯 AppStateProvider - Your One-Stop Shop

```dart
// In any widget:
final appState = ref.watch(appStateProvider);
final appStateNotifier = ref.read(appStateProvider.notifier);

// Check if ready to use
if (appState.isReady) {
  // Can perform credential operations
}

// Get current state
print(appState.connectionState.displayText);  // "Connected & Ready"
print(appState.credentials.length);            // Number of credentials
print(appState.errorMessage);                  // Any error
```

## 🔌 Complete Usage Flow

### 1. Scan for Devices
```dart
final devices = await appStateNotifier.scanForDevices(
  timeout: Duration(seconds: 10),
);

// devices is List<DiscoveredDevice> from flutter_reactive_ble
for (final device in devices) {
  print('${device.name}: ${device.id}');
}
```

### 2. Connect and Authenticate
```dart
try {
  await appStateNotifier.connectAndAuthenticate(
    deviceId: selectedDevice.id,
    deviceName: selectedDevice.name,
    pin: '123456',  // Currently not used by ESP32, for future use
  );
  
  // State automatically updated to: ConnectionState.authenticated
  // OS pairing happens automatically if first time
  
} catch (e) {
  // Error shown in appState.errorMessage
  print('Connection failed: $e');
}
```

### 3. Manage Credentials

#### Add Credential
```dart
await appStateNotifier.addCredential(
  site: 'github.com',
  username: 'john@example.com',
  password: 'SecurePass123!',
);
// Automatically refreshes credential list
```

#### Get Password
```dart
final password = await appStateNotifier.getPassword(
  site: 'github.com',
  username: 'john@example.com',
);
print('Password: $password');
```

#### Update Password
```dart
await appStateNotifier.updateCredential(
  site: 'github.com',
  username: 'john@example.com',
  newPassword: 'NewSecurePass456!',
);
// Automatically refreshes credential list
```

#### Delete Credential
```dart
await appStateNotifier.deleteCredential(
  site: 'github.com',
  username: 'john@example.com',
);
// Automatically refreshes credential list
```

#### Refresh List Manually
```dart
await appStateNotifier.refreshCredentials();
// Updates appState.credentials
```

### 4. Disconnect
```dart
await appStateNotifier.disconnect();
// Resets state to ConnectionState.disconnected
// Clears all credentials and session info
```

### 5. Clear Errors
```dart
appStateNotifier.clearError();
// Removes appState.errorMessage
```

## 📊 State Reference

### ConnectionState Enum
```
disconnected     → Not connected
scanning         → Looking for devices
connecting       → Establishing BLE connection (OS may show pairing)
connected        → BLE connected, starting auth
authenticating   → Performing session token auth
authenticated    → ✅ Ready to use!
error            → Something went wrong
```

### AuthState Enum
```
unauthenticated  → No session token
requestingToken  → Asking ESP32 for token
authenticating   → Sending auth command
authenticated    → ✅ Session authorized!
failed           → Auth failed
lockedOut        → Too many failed attempts
```

### AppState Properties
```dart
appState.connectionState        // Current connection status
appState.authState              // Current auth status
appState.connectedDeviceId      // "XX:XX:XX:XX:XX:XX" or null
appState.connectedDeviceName    // "ESP32-GATT-Manager" or null
appState.errorMessage           // Error string or null
appState.credentials            // List<Credential>
appState.isLoading              // true during operations
appState.isReady                // true when authenticated
```

## 🔐 Security Flow Visual

```
User Action               Flutter                    ESP32                    OS
───────────────────────────────────────────────────────────────────────────────

[Tap Connect]
                    connect(deviceId) ─────────────────────→ [Accept connection]
                                                                      │
                                                                      ├→ Security req
                                      ←────────────────────────────────
                    [Trigger pairing] ────────→ [Show PIN dialog]
[Enter PIN 123456]  ────────────────→ [Verify PIN] ─────────→ [Validate PIN]
                                                               [Store bond]
                    ←──────────────────────────────────────── [Bonded!] ✓
                                                                      │
                    subscribeToNotifications() ─────────────→ [Subscribe]
                                                                      │
                    sendCommand("request_token") ───────────→ [Generate token]
                    ←──────────────────────────────────────── "TOKEN A1B2C3D4"
                                                                      │
                    sendCommand("auth A1B2C3D4") ───────────→ [Validate token]
                    ←──────────────────────────────────────── "AUTH OK" ✓
                                                                      │
[Ready to use!] ✓                                            [Session active]

───────────────────────────────────────────────────────────────────────────────
Subsequent connections: OS auto-reconnects (no pairing dialog)
```

## 🛠️ Error Handling

### Common Errors and Solutions

#### "Connection timeout"
- ESP32 not powered on or out of range
- Check ESP32 serial monitor for errors

#### "LOCKED"
- Too many failed auth attempts (5+)
- Wait 1 minute for lockout to expire

#### "NOT AUTHORIZED"
- Session expired or invalid token
- Disconnect and reconnect

#### "Bluetooth permission denied"
- Request permissions in UI
- User must enable Bluetooth

#### "ADD FAIL" / "UPDATE FAIL" / "DELETE FAIL"
- Database error on ESP32
- Check ESP32 serial monitor for details
- Verify SD card is working

## 🧪 Testing Commands (Serial Monitor)

You can test ESP32 directly via Serial Monitor:

```
request_token          → Returns "TOKEN XXXXXXXX"
auth XXXXXXXX          → Returns "AUTH OK"
add site user pass     → Returns "Added"
get site user          → Returns "Password: pass"
update site user newp  → Returns "Updated"
delete site user       → Returns "Deleted"
list                   → Returns "LIST:\nSite: ... | User: ..."
logout                 → Returns "LOGOUT"
```

## 📱 Minimal UI Example

```dart
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'providers/app_state_provider.dart';

void main() {
  runApp(ProviderScope(child: MyApp()));
}

class MyApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Password Manager',
      home: HomeScreen(),
    );
  }
}

class HomeScreen extends ConsumerWidget {
  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final appState = ref.watch(appStateProvider);
    final notifier = ref.read(appStateProvider.notifier);

    return Scaffold(
      appBar: AppBar(
        title: Text('Password Manager'),
        subtitle: Text(appState.connectionState.displayText),
      ),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            // Connection button
            if (!appState.isReady)
              ElevatedButton(
                onPressed: appState.isLoading
                    ? null
                    : () async {
                        final devices = await notifier.scanForDevices();
                        if (devices.isNotEmpty) {
                          await notifier.connectAndAuthenticate(
                            deviceId: devices.first.id,
                            deviceName: devices.first.name,
                            pin: '123456',
                          );
                        }
                      },
                child: Text(appState.isLoading ? 'Connecting...' : 'Connect to ESP32'),
              ),

            // Credential count
            if (appState.isReady)
              Text('${appState.credentials.length} credentials stored'),

            // Error display
            if (appState.errorMessage != null)
              Padding(
                padding: EdgeInsets.all(16),
                child: Text(
                  appState.errorMessage!,
                  style: TextStyle(color: Colors.red),
                ),
              ),

            // Disconnect button
            if (appState.isReady)
              ElevatedButton(
                onPressed: () => notifier.disconnect(),
                child: Text('Disconnect'),
              ),
          ],
        ),
      ),
    );
  }
}
```

## 🚀 Ready to Build!

All the backend is done. Now you just need to:

1. Create beautiful UI screens
2. Handle permissions properly
3. Add user-friendly error messages
4. Test with real ESP32 device

The heavy lifting is done! 🎉
