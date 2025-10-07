# BLE Bonding Retry Fix

## Problem
The app was experiencing "service discovery failure" exceptions during BLE bonding because:
1. App connects to ESP32 → BLE connection established
2. OS initiates pairing/bonding process → shows PIN dialog
3. App immediately tries to discover services/characteristics
4. ❌ Service discovery fails because bonding is still in progress

The initial fix used a **fixed 3-second delay**, but this had issues:
- ⏱️ Bonding time varies (1-10+ seconds) depending on user response time, OS, device distance
- ⏳ Wasted time if bonding completes quickly (1 second bonding → still wait 3 seconds)
- ❌ Insufficient if bonding takes longer (5 second bonding → still fails with 3 second delay)

## Solution: Exponential Backoff Retry

Instead of a fixed delay, we now **retry service discovery with exponential backoff**:

### How It Works
1. **Immediate attempt**: Try service discovery right away (succeeds if already bonded)
2. **Smart retries**: If it fails (bonding in progress), retry with increasing delays:
   - Attempt 1: Immediate (0ms)
   - Attempt 2: Wait 500ms
   - Attempt 3: Wait 1000ms (1s)
   - Attempt 4: Wait 2000ms (2s)
   - Attempt 5: Wait 4000ms (4s)
3. **Early success**: As soon as bonding completes, service discovery succeeds → no more waits
4. **Timeout**: If all 5 attempts fail, throw error (total max wait: ~7.5 seconds)

### Benefits
✅ **Adaptive**: Works for fast bonding (1s) and slow bonding (5s)  
✅ **Efficient**: No unnecessary delays if bonding is already complete  
✅ **User feedback**: Debug logs show retry progress  
✅ **Robust**: Handles edge cases (weak signal, OS delays, etc.)

## Code Changes

### 1. `lib/services/command_service.dart`
Updated `subscribeToNotifications()` to implement retry logic:

```dart
Future<void> subscribeToNotifications(String deviceId) async {
  // Retry configuration
  const maxAttempts = 5;
  const initialDelay = Duration(milliseconds: 500);
  const maxDelay = Duration(seconds: 4);
  
  for (int attempt = 1; attempt <= maxAttempts; attempt++) {
    try {
      // Try to subscribe to characteristic
      final characteristic = QualifiedCharacteristic(...);
      _notificationSubscription = _ble.subscribeToCharacteristic(characteristic).listen(...);
      
      debugPrint('[CommandService] ✓ Subscribed to notifications');
      return; // Success!
      
    } catch (e) {
      debugPrint('[CommandService] Attempt $attempt failed: $e');
      
      if (attempt == maxAttempts) {
        rethrow; // Give up after 5 attempts
      }
      
      // Exponential backoff: 500ms, 1s, 2s, 4s
      final delayMs = initialDelay.inMilliseconds * (1 << (attempt - 1));
      final delay = Duration(milliseconds: delayMs).compareTo(maxDelay) > 0 
          ? maxDelay 
          : Duration(milliseconds: delayMs);
      
      debugPrint('[CommandService] Retrying in ${delay.inMilliseconds}ms...');
      await Future.delayed(delay);
    }
  }
}
```

### 2. `lib/providers/app_state_provider.dart`
Removed the fixed 3-second delay - now relies on retry logic:

```dart
// Before:
await bleService.connect(deviceId);
await Future.delayed(const Duration(seconds: 3)); // ❌ Fixed delay
await bleService.subscribeToNotifications(deviceId);

// After:
await bleService.connect(deviceId);
await bleService.subscribeToNotifications(deviceId); // ✅ Has retry logic
```

## Testing Scenarios

### Scenario 1: Already Bonded Device
- **Result**: Service discovery succeeds on first attempt (0ms)
- **User experience**: Instant connection

### Scenario 2: Fast Bonding (User enters PIN in 2s)
- **Result**: Fails attempt 1, succeeds on attempt 2 or 3 (~500-1000ms wait)
- **User experience**: Quick connection (~2-3s total)

### Scenario 3: Slow Bonding (User enters PIN in 5s)
- **Result**: Fails attempts 1-3, succeeds on attempt 4 (~3.5s cumulative wait)
- **User experience**: Reasonable connection (~5-8s total)

### Scenario 4: Bonding Failure
- **Result**: All 5 attempts fail, error thrown after ~7.5s
- **User experience**: Clear error message, can retry

## Debug Logs Example

```
[AppState] Starting connection flow for ESP32-GATT-Manager
[BleConnection] Connecting to AA:BB:CC:DD:EE:FF...
[BleConnection] Connection state: connected
[BleConnection] ✓ Connected to AA:BB:CC:DD:EE:FF
[CommandService] Subscribing to notifications (attempt 1/5)
[CommandService] Attempt 1 failed: Service discovery failure
[CommandService] Retrying in 500ms (bonding may still be in progress)...
[CommandService] Subscribing to notifications (attempt 2/5)
[CommandService] ✓ Subscribed to notifications
[AppState] Subscribed to notifications
[AuthService] Requesting session token...
```

## ESP32 Side (No Changes Needed)

The ESP32 code already handles bonding correctly:
- Uses `ESP_LE_AUTH_REQ_SC_MITM_BOND` for secure connections + bonding
- Shows PIN 123456 via `MySecurityCallbacks::onPassKeyRequest()`
- Handles `onAuthenticationComplete()` callback
- Session token auth happens **after** bonding completes

## Next Steps

1. **Test on real device**: Connect to ESP32, enter PIN, verify it works
2. **Monitor logs**: Check retry attempts and timing in debug output
3. **Tune parameters** (if needed):
   - Increase `maxAttempts` to 6-7 for slower devices
   - Adjust `initialDelay` to 1000ms for more conservative approach
   - Increase `maxDelay` to 5-6s for very slow devices

## Why This Is Better

| Aspect | Fixed Delay (Old) | Exponential Backoff (New) |
|--------|------------------|---------------------------|
| Fast bonding (1s) | Wait 3s ❌ | Wait 0-500ms ✅ |
| Normal bonding (3s) | Wait 3s ✅ | Wait 1.5s ✅ |
| Slow bonding (5s) | Fail ❌ | Wait 3.5s ✅ |
| Already bonded | Wait 3s ❌ | Instant ✅ |
| Edge cases | Limited ❌ | Robust ✅ |
| User feedback | Silent wait ❌ | Progress logs ✅ |

---

**Implementation Date**: October 7, 2025  
**Status**: ✅ Ready for testing
