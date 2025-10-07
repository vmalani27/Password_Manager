# App Crash Fix - Bluetooth Permissions

## Problem
The app was crashing when pressing "Connect to ESP32" with no logs because **Bluetooth permissions were missing** from AndroidManifest.xml.

## Solution Implemented

### 1. Added Bluetooth Permissions to AndroidManifest.xml ✅

**File:** `android/app/src/main/AndroidManifest.xml`

Added the following permissions:
```xml
<!-- Bluetooth permissions for Android 12+ (API 31+) -->
<uses-permission android:name="android.permission.BLUETOOTH_SCAN"
                 android:usesPermissionFlags="neverForLocation" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />

<!-- Bluetooth permissions for older Android versions -->
<uses-permission android:name="android.permission.BLUETOOTH" 
                 android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" 
                 android:maxSdkVersion="30" />

<!-- Location permission required for BLE scanning on Android -->
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" 
                 android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION" 
                 android:maxSdkVersion="30" />

<uses-feature android:name="android.hardware.bluetooth_le" android:required="true" />
```

### 2. Created PermissionService ✅

**File:** `lib/services/permission_service.dart`

Handles runtime permission requests:
- **Android 12+**: Requests `BLUETOOTH_SCAN` and `BLUETOOTH_CONNECT`
- **Android 11-**: Requests `LOCATION` and `BLUETOOTH`
- **iOS**: Requests `BLUETOOTH`

Features:
- Check if permissions are already granted
- Request permissions with proper dialogs
- Handle permanently denied permissions
- Open app settings if needed

### 3. Updated HomeScreen ✅

**File:** `lib/screens/home_screen.dart`

Modified `_navigateToScan()` method to:
1. Check if permissions are granted
2. Show permission rationale dialog
3. Request permissions if needed
4. Handle denied/permanently denied cases
5. Only navigate to scan screen if permissions granted

## User Experience Flow

### First Time (Permissions Not Granted):
```
1. User taps "Connect to ESP32"
2. App shows: "Bluetooth Permission Required" dialog
   - Explains why permission is needed
   - Mentions Android 12+ location requirement
3. User taps "Grant Permission"
4. OS shows: "Allow test69 to find, connect to, and determine location of nearby devices?"
5. User taps "While using the app"
6. ✓ Permissions granted → Navigate to scan screen
```

### If User Denies:
```
1. Snackbar shows: "Bluetooth permission is required to use this app"
2. User can try again
```

### If Permanently Denied:
```
1. App shows: "Permission Required" dialog
2. Option to "Open Settings"
3. User can manually enable in system settings
```

### Subsequent Uses:
```
1. User taps "Connect to ESP32"
2. ✓ Permissions already granted → Directly navigate to scan screen
```

## Why This Fixes the Crash

### Root Cause:
- `flutter_reactive_ble` tries to access Bluetooth hardware
- Without manifest permissions, Android immediately crashes the app
- No error logs because it's a security violation, not a Dart exception

### Fix:
1. **Manifest permissions** allow the app to request Bluetooth access
2. **Runtime permission handling** ensures user grants permission before BLE operations
3. **Graceful fallback** prevents crashes if permissions denied

## Android 12+ Special Note

Android 12 (API 31+) introduced new fine-grained Bluetooth permissions:
- `BLUETOOTH_SCAN` - To discover nearby devices
- `BLUETOOTH_CONNECT` - To connect to paired devices

The flag `android:usesPermissionFlags="neverForLocation"` tells Android that we're not using Bluetooth for location tracking, which allows the app to avoid location permission on Android 12+.

However, on Android 11 and below, location permission is still required by the OS for BLE scanning (even though we don't use location data).

## Testing

1. **Clean install:**
   ```bash
   flutter clean
   flutter pub get
   flutter run
   ```

2. **First launch:**
   - Tap "Connect to ESP32"
   - Should see permission dialog (not crash)
   - Grant permission
   - Should see scan screen

3. **Subsequent launches:**
   - Tap "Connect to ESP32"
   - Should directly open scan screen

4. **Deny permission test:**
   - Uninstall app
   - Reinstall
   - Tap "Connect to ESP32"
   - Deny permission
   - Should see snackbar (not crash)

5. **Permanently deny test:**
   - Deny twice in a row
   - Should see "Open Settings" dialog
   - Can manually enable in settings

## Files Changed

1. ✅ `android/app/src/main/AndroidManifest.xml` - Added Bluetooth permissions
2. ✅ `lib/services/permission_service.dart` - Created (new file)
3. ✅ `lib/screens/home_screen.dart` - Updated `_navigateToScan()` method

## Still TODO (Not Critical)

- iOS Info.plist Bluetooth permission description (for iOS builds)
- Add device_info_plus package for accurate Android version detection
- More detailed permission error messages

## Status

✅ **App should no longer crash when tapping "Connect to ESP32"**
✅ **Proper permission flow implemented**
✅ **User-friendly error messages**
✅ **Handles all permission states**

Ready to test! 🚀
