import 'dart:convert';
import 'dart:typed_data';

/// Represents a paired ESP32 device with persistent pairing data
class PairedDevice {
  final String deviceId;
  final String deviceName;
  final Uint8List clientPrivateKey;  // Our private key (32 bytes)
  final Uint8List clientPublicKey;   // Our public key (64 bytes)
  final Uint8List esp32PublicKey;    // ESP32's public key (64 bytes)
  final DateTime pairedAt;
  
  PairedDevice({
    required this.deviceId,
    required this.deviceName,
    required this.clientPrivateKey,
    required this.clientPublicKey,
    required this.esp32PublicKey,
    required this.pairedAt,
  });
  
  /// Convert to JSON for SharedPreferences storage
  Map<String, dynamic> toJson() {
    return {
      'deviceId': deviceId,
      'deviceName': deviceName,
      'clientPrivateKey': base64Encode(clientPrivateKey),
      'clientPublicKey': base64Encode(clientPublicKey),
      'esp32PublicKey': base64Encode(esp32PublicKey),
      'pairedAt': pairedAt.toIso8601String(),
    };
  }
  
  /// Create from JSON
  factory PairedDevice.fromJson(Map<String, dynamic> json) {
    return PairedDevice(
      deviceId: json['deviceId'],
      deviceName: json['deviceName'],
      clientPrivateKey: base64Decode(json['clientPrivateKey']),
      clientPublicKey: base64Decode(json['clientPublicKey']),
      esp32PublicKey: base64Decode(json['esp32PublicKey']),
      pairedAt: DateTime.parse(json['pairedAt']),
    );
  }
}
