# Command Timeout Debugging Guide

## Current Issue
- ✅ Bonding completes successfully
- ✅ Service discovery succeeds
- ✅ Notification subscription succeeds
- ❌ Commands sent to ESP32 → **No response** (timeout)

## Possible Causes

### 1. ESP32 Not Receiving Commands
**Symptom**: ESP32 Serial Monitor shows no "Processing:" messages  
**Cause**: Commands not being written to correct characteristic  
**Check**:
- Flutter writes to: `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` (RX)
- ESP32 `CommandCallback::onWrite()` is registered on this UUID
- MTU negotiation completed before sending commands

**ESP32 Serial Monitor Should Show**:
```
Processing: request_token
Generated token: 12AB34CD
Processing: auth 12AB34CD
AUTH OK
```

### 2. ESP32 Sending Notifications But App Not Receiving
**Symptom**: ESP32 shows "sendNotification()" but Flutter timeout  
**Cause**: Notification characteristic not properly subscribed or listening  
**Check**:
- Flutter subscribed to: `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` (Notify)
- ESP32 `pCharacteristic->notify()` is being called
- BLE connection still active during command

**ESP32 Serial Monitor Should Show**:
```
Generated token: 12AB34CD
[sendNotification called]
Client connected count: 1
```

### 3. Notification Stream Consumed by "Ready" Message
**Symptom**: First command times out, subsequent might work  
**Cause**: ESP32 sends initial "Ready" message that consumes the stream  
**ESP32 Code**:
```cpp
pCharacteristic->setValue("Ready");
```

**Fix**: Clear initial message or wait for it before sending commands

### 4. Timing Issue - Commands Sent Too Early
**Symptom**: Intermittent failures, works on retry  
**Cause**: Commands sent before ESP32 finishes bonding internally  
**Current Fix**: 500ms delay after subscription (may need more)

### 5. MTU Negotiation Failure
**Symptom**: Write succeeds but no data transmitted  
**Cause**: MTU mismatch between Flutter (requested 256) and ESP32 (set 256)  
**Check Android logs** for:
```
D/BluetoothGatt: onMtuChanged() - device=XX:XX status=0 mtu=256
```

## Debug Checklist

### Flutter Side (Check Logs):
```
[ ] [BleConnection] MTU negotiation complete
[ ] [CommandService] ✅ Service discovery successful!
[ ] [CommandService] Writing to characteristic 6E400002...
[ ] [CommandService] ✓ Write successful, waiting for response...
[ ] [CommandService] ← Received: TOKEN 12AB34CD
```

### ESP32 Side (Check Serial Monitor):
```
[ ] Client connected.
[ ] BLE: Bonded successfully!
[ ] Processing: request_token
[ ] Generated token: <HEX>
[ ] Processing: auth <token>
[ ] AUTH OK
```

### Android BLE Logs (Logcat):
```
[ ] D/BluetoothGatt: onConnectionStateChange() status=0 newState=2
[ ] D/BluetoothGatt: onMtuChanged() status=0 mtu=256
[ ] D/BluetoothGatt: onServicesDiscovered() status=0
[ ] D/BluetoothGatt: onCharacteristicWrite() status=0
[ ] D/BluetoothGatt: onCharacteristicChanged() characteristic=6E400003
```

## Debugging Steps

### Step 1: Verify ESP32 is Receiving Commands
1. Open ESP32 Serial Monitor (115200 baud)
2. Connect from Flutter app
3. Watch for "Processing: request_token"
4. **If NOT seen**: Issue is with write characteristic or MTU
5. **If seen**: ESP32 is receiving, issue is with notifications

### Step 2: Verify ESP32 is Sending Notifications
1. Add debug to ESP32 `sendNotification()`:
```cpp
void sendNotification(const String &data) {
  Serial.println("DEBUG: sendNotification called with: " + data);
  Serial.println("DEBUG: Connected count: " + String(pServer->getConnectedCount()));
  if (pServer->getConnectedCount() > 0) {
    pCharacteristic->setValue(data.c_str());
    pCharacteristic->notify();
    Serial.println("DEBUG: notify() called");
  }
}
```

2. Check if `notify()` is actually being called
3. **If NOT called**: Server thinks client disconnected
4. **If called**: Notification sent, issue is on Flutter side

### Step 3: Check Notification Subscription
1. Add debug to Flutter notification listener:
```dart
_notificationSubscription = _ble.subscribeToCharacteristic(characteristic).listen(
  (data) {
    debugPrint('[CommandService] RAW DATA: $data');
    final response = String.fromCharCodes(data).trim();
    debugPrint('[CommandService] ← Received: $response');
    _responseController.add(response);
  },
```

2. Check if RAW DATA is received
3. **If received**: Parsing issue
4. **If NOT received**: BLE stack issue

### Step 4: Test with Simple Read
Instead of waiting for notification, try reading the characteristic directly:

```dart
Future<String> readInitialValue() async {
  final characteristic = QualifiedCharacteristic(
    serviceId: Uuid.parse(BleConstants.serviceUuid),
    characteristicId: Uuid.parse(BleConstants.notificationCharacteristicUuid),
    deviceId: _connectedDeviceId!,
  );
  
  final value = await _ble.readCharacteristic(characteristic);
  final response = String.fromCharCodes(value);
  debugPrint('[CommandService] Read initial value: $response');
  return response;
}
```

Call this after subscription to verify characteristic is readable.

## Quick Fixes to Try

### Fix 1: Increase Delay After Subscription
```dart
await bleService.subscribeToNotifications(deviceId);
await Future.delayed(const Duration(seconds: 2)); // Increase from 500ms
```

### Fix 2: Clear Initial "Ready" Message
```dart
// After subscription, wait for and discard "Ready" message
try {
  final initial = await responseStream.first.timeout(Duration(seconds: 1));
  debugPrint('[CommandService] Initial message: $initial');
} catch (e) {
  debugPrint('[CommandService] No initial message (OK)');
}
```

### Fix 3: Use writeCharacteristicWithoutResponse
```dart
await _ble.writeCharacteristicWithoutResponse(
  writeCharacteristic,
  value: command.codeUnits,
);
```

This might work better for some BLE stacks.

### Fix 4: Add Service Discovery Delay
After bonding, ESP32 might need time to initialize services:
```dart
debugPrint('[AppState] Bonding complete, waiting for services...');
await Future.delayed(const Duration(seconds: 1));
```

## Expected Working Flow

```
1. App: Connect to ESP32
2. Android: Show pairing dialog
3. User: Enter PIN 123456
4. Android: Bonding complete
5. App: Wait 800ms
6. App: Retry service discovery (succeeds after bonding)
7. App: Subscribe to notifications ✅
8. App: Wait 500ms for ESP32 ready
9. App: Write "request_token" to RX characteristic
10. ESP32: Receive "request_token" via CommandCallback::onWrite()
11. ESP32: Generate token, call sendNotification("TOKEN 12AB34CD")
12. ESP32: pCharacteristic->notify() sends notification
13. Android: onCharacteristicChanged() receives notification
14. Flutter: Notification stream receives data
15. App: Parse "TOKEN 12AB34CD", extract token
16. App: Write "auth 12AB34CD" to RX characteristic
17. ESP32: Validate token, sendNotification("AUTH OK")
18. App: Receive "AUTH OK", mark authenticated
19. App: Ready for credential operations
```

## Next Steps

1. **Run the app with Flutter attached** - Check all debug logs
2. **Open ESP32 Serial Monitor** - Check if commands received
3. **Compare logs** against the expected flow above
4. **Identify where the flow breaks** - ESP32 not receiving? Not sending? App not listening?
5. **Apply appropriate fix** from above

---

**Status**: Waiting for ESP32 Serial Monitor output to diagnose
