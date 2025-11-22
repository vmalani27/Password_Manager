
/// Paired device information stored locally
class PairedDevice {
  final String deviceName;
  final String bluetoothAddress; // MAC address
  final String? deviceFingerprint; // Optional: device-specific UUID or public key hash
  final DateTime pairedAt;
  final DateTime? lastConnectedAt;
  
  const PairedDevice({
    required this.deviceName,
    required this.bluetoothAddress,
    this.deviceFingerprint,
    required this.pairedAt,
    this.lastConnectedAt,
  });
  
  Map<String, dynamic> toJson() => {
    'deviceName': deviceName,
    'bluetoothAddress': bluetoothAddress,
    'deviceFingerprint': deviceFingerprint,
    'pairedAt': pairedAt.toIso8601String(),
    'lastConnectedAt': lastConnectedAt?.toIso8601String(),
  };
  
  factory PairedDevice.fromJson(Map<String, dynamic> json) => PairedDevice(
    deviceName: json['deviceName'],
    bluetoothAddress: json['bluetoothAddress'],
    deviceFingerprint: json['deviceFingerprint'],
    pairedAt: DateTime.parse(json['pairedAt']),
    lastConnectedAt: json['lastConnectedAt'] != null 
        ? DateTime.parse(json['lastConnectedAt']) 
        : null,
  );
  
  /// Check if this device matches a discovered BLE device
  bool matches(String discoveredName, String discoveredAddress) {
    return bluetoothAddress.toLowerCase() == discoveredAddress.toLowerCase();
  }
}