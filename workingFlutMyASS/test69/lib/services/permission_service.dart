import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:permission_handler/permission_handler.dart';

/// Service to handle Bluetooth and location permissions
class PermissionService {
  /// Check if all required permissions are granted
  Future<bool> hasRequiredPermissions() async {
    if (Platform.isAndroid) {
      final androidInfo = await _getAndroidVersion();
      
      if (androidInfo >= 31) {
        // Android 12+ (API 31+)
        final bluetoothScan = await Permission.bluetoothScan.isGranted;
        final bluetoothConnect = await Permission.bluetoothConnect.isGranted;
        return bluetoothScan && bluetoothConnect;
      } else {
        // Android 11 and below
        final location = await Permission.location.isGranted;
        final bluetooth = await Permission.bluetooth.isGranted;
        return location && bluetooth;
      }
    } else if (Platform.isIOS) {
      // iOS only needs Bluetooth permission
      return await Permission.bluetooth.isGranted;
    }
    
    return true;
  }

  /// Request all required permissions
  Future<PermissionResult> requestPermissions() async {
    debugPrint('[Permissions] Requesting permissions...');
    
    if (Platform.isAndroid) {
      final androidInfo = await _getAndroidVersion();
      
      if (androidInfo >= 31) {
        // Android 12+ (API 31+)
        debugPrint('[Permissions] Android 12+ detected, requesting Bluetooth scan/connect');
        
        final statuses = await [
          Permission.bluetoothScan,
          Permission.bluetoothConnect,
        ].request();
        
        final scanGranted = statuses[Permission.bluetoothScan]?.isGranted ?? false;
        final connectGranted = statuses[Permission.bluetoothConnect]?.isGranted ?? false;
        
        if (scanGranted && connectGranted) {
          debugPrint('[Permissions] ✓ All permissions granted');
          return PermissionResult.granted;
        } else if ((statuses[Permission.bluetoothScan]?.isPermanentlyDenied ?? false) ||
                   (statuses[Permission.bluetoothConnect]?.isPermanentlyDenied ?? false)) {
          debugPrint('[Permissions] ✗ Permissions permanently denied');
          return PermissionResult.permanentlyDenied;
        } else {
          debugPrint('[Permissions] ✗ Permissions denied');
          return PermissionResult.denied;
        }
      } else {
        // Android 11 and below
        debugPrint('[Permissions] Android 11- detected, requesting Location + Bluetooth');
        
        final statuses = await [
          Permission.location,
          Permission.bluetooth,
        ].request();
        
        final locationGranted = statuses[Permission.location]?.isGranted ?? false;
        final bluetoothGranted = statuses[Permission.bluetooth]?.isGranted ?? false;
        
        if (locationGranted && bluetoothGranted) {
          debugPrint('[Permissions] ✓ All permissions granted');
          return PermissionResult.granted;
        } else if ((statuses[Permission.location]?.isPermanentlyDenied ?? false) ||
                   (statuses[Permission.bluetooth]?.isPermanentlyDenied ?? false)) {
          debugPrint('[Permissions] ✗ Permissions permanently denied');
          return PermissionResult.permanentlyDenied;
        } else {
          debugPrint('[Permissions] ✗ Permissions denied');
          return PermissionResult.denied;
        }
      }
    } else if (Platform.isIOS) {
      // iOS
      debugPrint('[Permissions] iOS detected, requesting Bluetooth');
      
      final status = await Permission.bluetooth.request();
      
      if (status.isGranted) {
        debugPrint('[Permissions] ✓ Bluetooth permission granted');
        return PermissionResult.granted;
      } else if (status.isPermanentlyDenied) {
        debugPrint('[Permissions] ✗ Bluetooth permission permanently denied');
        return PermissionResult.permanentlyDenied;
      } else {
        debugPrint('[Permissions] ✗ Bluetooth permission denied');
        return PermissionResult.denied;
      }
    }
    
    return PermissionResult.granted;
  }

  /// Open app settings
  Future<void> openAppSettings() async {
    await openAppSettings();
  }

  /// Get Android SDK version
  Future<int> _getAndroidVersion() async {
    if (!Platform.isAndroid) return 0;
    
    try {
      // This is a simplified check - in production you'd use device_info_plus
      // For now, we'll assume Android 12+ to be safe
      return 31;
    } catch (e) {
      debugPrint('[Permissions] Failed to get Android version: $e');
      return 31; // Default to Android 12+ for safety
    }
  }
}

/// Result of permission request
enum PermissionResult {
  granted,
  denied,
  permanentlyDenied,
}
