import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart' as reactive_ble;
import '../services/ble_uuids.dart';

class BleProvider with ChangeNotifier {
  final reactive_ble.FlutterReactiveBle ble = reactive_ble.FlutterReactiveBle();
  final List<reactive_ble.DiscoveredDevice> _devices = [];
  bool _isScanning = false;
  String? _error;

  // BLE connection state
  reactive_ble.DiscoveredDevice? _connectedDevice;
  bool _isConnecting = false;
  bool _isConnected = false;
  String? _connectionError;
  String? _notification;
  StreamSubscription? _connectionSub;
  StreamSubscription? _notifySub;

  List<reactive_ble.DiscoveredDevice> get devices => List.unmodifiable(_devices);
  bool get isScanning => _isScanning;
  String? get error => _error;
  bool get isConnecting => _isConnecting;
  bool get isConnected => _isConnected;
  String? get connectionError => _connectionError;
  String? get notification => _notification;

  void startScan() {
    _isScanning = true;
    _error = null;
    _devices.clear();
    notifyListeners();
  }

  void setScanning(bool value) {
    _isScanning = value;
    notifyListeners();
  }

  void addDevice(reactive_ble.DiscoveredDevice device) {
    if (!_devices.any((d) => d.id == device.id)) {
      _devices.add(device);
      notifyListeners();
    }
  }

  void setError(String error) {
    _error = error;
    notifyListeners();
  }

  void clear() {
    _devices.clear();
    _error = null;
    _isScanning = false;
    notifyListeners();
  }

  Future<void> connectToDevice(reactive_ble.DiscoveredDevice device) async {
    _isConnecting = true;
    _connectionError = null;
    notifyListeners();

    await _connectionSub?.cancel();
    await _notifySub?.cancel();

    debugPrint('Attempting BLE connection to device: ${device.name} (${device.id})');
    _connectionSub = ble.connectToDevice(
      id: device.id,
      connectionTimeout: const Duration(seconds: 10),
    ).listen((update) {
      debugPrint('BLE connection state: ${update.connectionState}');
      if (update.connectionState == reactive_ble.DeviceConnectionState.connected) {
        debugPrint('BLE connected!');
        _isConnected = true;
        _connectedDevice = device;
        _isConnecting = false;
        notifyListeners();
        _subscribeToNotifications();
      } else if (update.connectionState == reactive_ble.DeviceConnectionState.disconnected) {
        debugPrint('BLE disconnected.');
        _isConnected = false;
        _isConnecting = false;
        _connectedDevice = null;
        notifyListeners();
      } else {
        debugPrint('BLE state: ${update.connectionState}');
      }
    }, onError: (e) {
      debugPrint('BLE connection error: $e');
      _isConnecting = false;
      _isConnected = false;
      _connectionError = e.toString();
      notifyListeners();
    });
  }

  void _subscribeToNotifications() {
    if (_connectedDevice == null) return;
    final characteristic = reactive_ble.QualifiedCharacteristic(
      serviceId: reactive_ble.Uuid.parse(SERVICE_UUID),
      characteristicId: reactive_ble.Uuid.parse(NOTIFICATION_CHARACTERISTIC_UUID),
      deviceId: _connectedDevice!.id,
    );
    _notifySub = ble.subscribeToCharacteristic(characteristic).listen((data) {
      _notification = String.fromCharCodes(data);
      notifyListeners();
    });
  }

  Future<void> sendCommand(String command) async {
    if (_connectedDevice == null) return;
    final characteristic = reactive_ble.QualifiedCharacteristic(
      serviceId: reactive_ble.Uuid.parse(SERVICE_UUID),
      characteristicId: reactive_ble.Uuid.parse(CHARACTERISTIC_UUID),
      deviceId: _connectedDevice!.id,
    );
    await ble.writeCharacteristicWithResponse(
      characteristic,
      value: command.codeUnits,
    );
  }

  Future<void> disconnect() async {
    await _connectionSub?.cancel();
    await _notifySub?.cancel();
    _isConnected = false;
    _connectedDevice = null;
    notifyListeners();
  }
}
