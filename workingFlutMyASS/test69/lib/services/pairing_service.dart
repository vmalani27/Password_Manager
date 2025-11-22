import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../models/pairing_device.dart';

class PairingService {
  static const String _keyPairedDevice = 'paired_device';
  
  /// Check if a device is paired
  Future<bool> isPaired() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.containsKey(_keyPairedDevice);
  }
  
  /// Get paired device info
  Future<PairedDevice?> getPairedDevice() async {
    final prefs = await SharedPreferences.getInstance();
    final jsonStr = prefs.getString(_keyPairedDevice);
    
    if (jsonStr == null) return null;
    
    try {
      final json = jsonDecode(jsonStr);
      return PairedDevice.fromJson(json);
    } catch (e) {
      debugPrint('[Pairing] Failed to parse paired device: $e');
      return null;
    }
  }
  
  /// Save pairing after successful first handshake
  Future<void> savePairing(PairedDevice device) async {
    final prefs = await SharedPreferences.getInstance();
    final jsonStr = jsonEncode(device.toJson());
    await prefs.setString(_keyPairedDevice, jsonStr);
    debugPrint('[Pairing] Device paired: ${device.deviceName}');
  }
  
  /// Remove pairing (unpair)
  Future<void> removePairing() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove(_keyPairedDevice);
    debugPrint('[Pairing] Device unpaired');
  }
  
  /// Check if this device ID matches paired device
  Future<bool> isDeviceIdPaired(String deviceId) async {
    final paired = await getPairedDevice();
    return paired?.deviceId == deviceId;
  }
}
