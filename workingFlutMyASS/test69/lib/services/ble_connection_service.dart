import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import '../constants/ble_constants.dart';
import '../models/connection_state.dart' as models;
import '../models/paired_device.dart';
import 'command_service.dart';
import 'package:shared_preferences/shared_preferences.dart';

/// Handles BLE device scanning, connection, and connection lifecycle
/// Maps to ESP32 advertising and connection handling
class BleConnectionService {
  final FlutterReactiveBle _ble = FlutterReactiveBle();
  final CommandService _commandService;
  
  BleConnectionService(this._commandService);
  
  StreamSubscription<DiscoveredDevice>? _scanSubscription;
  StreamSubscription<ConnectionStateUpdate>? _connectionSubscription;
  
  final _discoveredDevicesController = StreamController<List<DiscoveredDevice>>.broadcast();
  final _connectionStateController = StreamController<models.ConnectionState>.broadcast();
  
  final List<DiscoveredDevice> _discoveredDevices = [];
  String? _connectedDeviceId;
  models.ConnectionState _currentState = models.ConnectionState.disconnected;
  
  PairedDevice? _pairedDevice;
  
  /// Stream of discovered devices during scan
  Stream<List<DiscoveredDevice>> get discoveredDevicesStream => _discoveredDevicesController.stream;
  
  /// Stream of connection state changes
  Stream<models.ConnectionState> get connectionStateStream => _connectionStateController.stream;
  
  /// Current connection state
  models.ConnectionState get connectionState => _currentState;
  
  /// Currently connected device ID
  String? get connectedDeviceId => _connectedDeviceId;
  
  /// Currently paired device
  PairedDevice? get pairedDevice => _pairedDevice;
  
  /// Check if currently scanning
  bool get isScanning => _scanSubscription != null;
  
  /// Load paired device from storage
  Future<void> loadPairedDevice() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final deviceJson = prefs.getString('paired_device');
      
      if (deviceJson != null) {
        _pairedDevice = PairedDevice.fromJson(deviceJson as Map<String, dynamic>);
        debugPrint('[BleConnection] Loaded paired device: ${_pairedDevice?.deviceName}');
      }
    } catch (e) {
      debugPrint('[BleConnection] Error loading paired device: $e');
    }
  }
  
  /// Save paired device to storage
  Future<void> _savePairedDevice(PairedDevice device) async {
    try {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString('paired_device', device.toJson() as String);
      _pairedDevice = device;
      debugPrint('[BleConnection] Saved paired device: ${device.deviceName}');
    } catch (e) {
      debugPrint('[BleConnection] Error saving paired device: $e');
    }
  }
  
  /// Clear paired device
  Future<void> unpairDevice() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      await prefs.remove('paired_device');
      _pairedDevice = null;
      debugPrint('[BleConnection] Unpaired device');
      
      // Disconnect if currently connected
      if (_connectedDeviceId != null) {
        await disconnect();
      }
    } catch (e) {
      debugPrint('[BleConnection] Error unpairing device: $e');
    }
  }
  
  /// Check if a discovered device matches the paired device
  bool _isDeviceMatching(DiscoveredDevice device) {
    if (_pairedDevice == null) return true; // No pairing restriction yet
    
    // Match by Bluetooth address (MAC)
    return _pairedDevice!.matches(device.name, device.id);
  }
  
  /// Verify device identity by reading device identity characteristic
  /// ESP32 exposes device MAC via characteristic
  Future<String?> _readDeviceIdentity(String deviceId) async {
    try {
      final characteristic = QualifiedCharacteristic(
        serviceId: Uuid.parse(BleConstants.serviceUuid),
        characteristicId: Uuid.parse(BleConstants.deviceIdentityCharacteristicUuid),
        deviceId: deviceId,
      );
      
      final response = await _ble.readCharacteristic(characteristic);
      final identity = String.fromCharCodes(response);
      
      debugPrint('[BleConnection] Device identity: $identity');
      return identity;
      
    } catch (e) {
      debugPrint('[BleConnection] Failed to read device identity: $e');
      return null;
    }
  }
  
  /// Start scanning for ESP32 devices
  /// ESP32 advertises with name "ESP32-PWD-Manager-XXXX" and service UUID
  /// ESP32 code: BLEDevice::init(deviceName.c_str());
  Future<void> startScan() async {
    if (_scanSubscription != null) {
      debugPrint('[BleConnection] Already scanning');
      return;
    }
    
    _discoveredDevices.clear();
    _updateConnectionState(models.ConnectionState.scanning);
    
    debugPrint('[BleConnection] Starting scan for devices with prefix: ${BleConstants.deviceNamePrefix}...');
    
    _scanSubscription = _ble.scanForDevices(
      withServices: [], // Scan all devices, filter by name
      scanMode: ScanMode.lowLatency,
    ).listen(
      (device) {
        // Filter by device name prefix (matches ESP32: "ESP32-PWD-Manager-XXXX")
        if (device.name.startsWith(BleConstants.deviceNamePrefix)) {
          // Check if device matches paired device (if any)
          final isMatching = _isDeviceMatching(device);
          
          // Add only if not already in list
          if (!_discoveredDevices.any((d) => d.id == device.id)) {
            debugPrint('[BleConnection] Found device: ${device.name} (${device.id}) ${isMatching ? "PAIRED" : ""}');
            _discoveredDevices.add(device);
            _discoveredDevicesController.add(List.from(_discoveredDevices));
          }
        }
      },
      onError: (error) {
        debugPrint('[BleConnection] Scan error: $error');
        stopScan();
      },
    );
    
    // Auto-stop scan after duration
    Future.delayed(BleConstants.scanDuration, () {
      if (_scanSubscription != null) {
        debugPrint('[BleConnection] Scan timeout, stopping...');
        stopScan();
      }
    });
  }
  
  /// Stop scanning for devices
  void stopScan() {
    _scanSubscription?.cancel();
    _scanSubscription = null;
    
    if (_currentState == models.ConnectionState.scanning) {
      _updateConnectionState(models.ConnectionState.disconnected);
    }
    
    debugPrint('[BleConnection] Scan stopped. Found ${_discoveredDevices.length} device(s)');
  }
  
  /// Scan for devices and return list (convenience method for AppStateProvider)
  /// Returns list of discovered devices after timeout
  Future<List<DiscoveredDevice>> scanForDevices({Duration timeout = const Duration(seconds: 10)}) async {
    try {
      _discoveredDevices.clear();
      
      // Check BLE status first
      final bleStatus = await _ble.statusStream.first;
      debugPrint('[BleConnection] BLE Status: $bleStatus');
      
      if (bleStatus != BleStatus.ready) {
        throw Exception('Bluetooth is not ready. Status: $bleStatus. Please enable Bluetooth.');
      }
      
      await startScan();
      
      // Wait for scan to complete
      await Future.delayed(timeout);
      
      stopScan();
      
      return List.from(_discoveredDevices);
    } catch (e) {
      debugPrint('[BleConnection] Scan error: $e');
      stopScan(); // Ensure scan is stopped on error
      rethrow;
    }
  }
  
  /// Subscribe to notifications from ESP32
  /// ESP32 sends responses via: pCharacteristic->notify()
  Future<void> subscribeToNotifications(String deviceId) async {
    await _commandService.subscribeToNotifications(deviceId);
    debugPrint('[BleConnection] Subscribed to ESP32 notifications');
  }
  
  /// Connect to a discovered device
  /// ESP32 accepts connection and triggers: ServerCallbacks::onConnect()
  Future<void> connect(String deviceId, {String? deviceName, bool pairDevice = false}) async {
    if (_connectedDeviceId == deviceId && _currentState.isConnected) {
      debugPrint('[BleConnection] Already connected to $deviceId');
      return;
    }
    
    // Check if connecting to a different device than paired
    if (_pairedDevice != null && !_pairedDevice!.matches(deviceName ?? '', deviceId)) {
      debugPrint('[BleConnection] WARNING: Attempting to connect to different device than paired!');
      debugPrint('[BleConnection] Paired: ${_pairedDevice!.deviceName} (${_pairedDevice!.bluetoothAddress})');
      debugPrint('[BleConnection] Connecting: $deviceName ($deviceId)');
      
      // Optionally reject connection
      // throw Exception('Cannot connect to unpaired device. Unpair current device first.');
    }
    
    // Disconnect from any existing connection
    if (_connectedDeviceId != null) {
      await disconnect();
    }
    
    _updateConnectionState(models.ConnectionState.connecting);
    debugPrint('[BleConnection] Connecting to $deviceId...');
    
    try {
      // Cancel any existing connection subscription
      await _connectionSubscription?.cancel();
      
      final completer = Completer<void>();
      bool isConnecting = true;
      
      _connectionSubscription = _ble.connectToDevice(
        id: deviceId,
        connectionTimeout: BleConstants.connectionTimeout,
      ).listen(
        (update) {
          debugPrint('[BleConnection] Connection state: ${update.connectionState}');
          
          if (update.connectionState == DeviceConnectionState.connected) {
            _connectedDeviceId = deviceId;
            _updateConnectionState(models.ConnectionState.connected);
            
            if (!completer.isCompleted && isConnecting) {
              completer.complete();
            }
            
            debugPrint('[BleConnection] Connected to $deviceId');
            
          } else if (update.connectionState == DeviceConnectionState.disconnected) {
            debugPrint('[BleConnection] Disconnected from $deviceId');
            
            // Only handle as error if we were still connecting
            if (isConnecting && !completer.isCompleted) {
              completer.completeError(Exception('Disconnected before connection established'));
            } else {
              // Normal disconnect after connection was established
              _handleDisconnection();
            }
          }
        },
        onError: (error) {
          debugPrint('[BleConnection] Connection error: $error');
          _updateConnectionState(models.ConnectionState.error);
          
          if (!completer.isCompleted) {
            completer.completeError(error);
          }
        },
        cancelOnError: false, // Keep stream alive even on errors
      );
      
      // Wait for connection to complete
      await completer.future;
      isConnecting = false; // Mark that initial connection is done
      
      // Add delay to let connection stabilize before MTU negotiation
      await Future.delayed(const Duration(milliseconds: 1000));
      
      // Now request MTU after connection is established
      // ESP32 code: BLEDevice::setMTU(256);
      debugPrint('[BleConnection] Connection established, negotiating MTU...');
      try {
        await _requestMtu(deviceId);
        debugPrint('[BleConnection] MTU negotiation complete');
      } catch (e) {
        debugPrint('[BleConnection] MTU negotiation failed (non-fatal): $e');
        // Continue anyway - MTU negotiation failure shouldn't kill connection
      }
      
      // Read device identity for verification
      String? deviceIdentity;
      try {
        deviceIdentity = await _readDeviceIdentity(deviceId);
      } catch (e) {
        debugPrint('[BleConnection] Failed to read device identity (non-fatal): $e');
        deviceIdentity = null;
      }
      
      // If pairing this device for first time
      if (pairDevice && deviceName != null) {
        final pairedDevice = PairedDevice(
          deviceName: deviceName,
          bluetoothAddress: deviceId,
          deviceFingerprint: deviceIdentity,
          pairedAt: DateTime.now(),
          lastConnectedAt: DateTime.now(),
        );
        await _savePairedDevice(pairedDevice);
        debugPrint('[BleConnection] Device paired: $deviceName');
      }
      
      // Update last connected time for existing paired device
      if (_pairedDevice != null && _pairedDevice!.matches(deviceName ?? '', deviceId)) {
        final updatedDevice = PairedDevice(
          deviceName: _pairedDevice!.deviceName,
          bluetoothAddress: _pairedDevice!.bluetoothAddress,
          deviceFingerprint: _pairedDevice!.deviceFingerprint,
          pairedAt: _pairedDevice!.pairedAt,
          lastConnectedAt: DateTime.now(),
        );
        await _savePairedDevice(updatedDevice);
      }
      
    } catch (e) {
      debugPrint('[BleConnection] Connection failed: $e');
      _updateConnectionState(models.ConnectionState.error);
      _connectedDeviceId = null;
      
      // Clean up connection subscription on error
      await _connectionSubscription?.cancel();
      _connectionSubscription = null;
      
      rethrow;
    }
  }
  
  /// Request MTU size negotiation
  /// ESP32 sets MTU: BLEDevice::setMTU(256);
  Future<void> _requestMtu(String deviceId) async {
    try {
      final mtu = await _ble.requestMtu(
        deviceId: deviceId,
        mtu: BleConstants.preferredMtu,
      );
      debugPrint('[BleConnection] MTU negotiated: $mtu bytes');
    } catch (e) {
      debugPrint('[BleConnection] MTU negotiation failed: $e (continuing anyway)');
    }
  }
  
  /// Disconnect from current device
  /// ESP32 triggers: ServerCallbacks::onDisconnect()
  Future<void> disconnect([String? deviceId]) async {
    debugPrint('[BleConnection] Disconnecting...');
    
    await _connectionSubscription?.cancel();
    _connectionSubscription = null;
    
    _handleDisconnection();
  }
  
  /// Handle disconnection cleanup
  void _handleDisconnection() {
    _connectedDeviceId = null;
    _updateConnectionState(models.ConnectionState.disconnected);
    debugPrint('[BleConnection] Disconnected');
  }
  
  /// Update connection state and notify listeners
  void _updateConnectionState(models.ConnectionState newState) {
    if (_currentState != newState) {
      _currentState = newState;
      _connectionStateController.add(newState);
      debugPrint('[BleConnection] State: ${newState.displayText}');
    }
  }
  
  /// Dispose of all resources
  void dispose() {
    stopScan();
    disconnect();
    _discoveredDevicesController.close();
    _connectionStateController.close();
  }
}