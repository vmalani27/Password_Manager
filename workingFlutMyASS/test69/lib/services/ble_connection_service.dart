import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import '../constants/ble_constants.dart';
import '../models/connection_state.dart' as models;
import 'command_service.dart';

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
  
  /// Stream of discovered devices during scan
  Stream<List<DiscoveredDevice>> get discoveredDevicesStream => _discoveredDevicesController.stream;
  
  /// Stream of connection state changes
  Stream<models.ConnectionState> get connectionStateStream => _connectionStateController.stream;
  
  /// Current connection state
  models.ConnectionState get connectionState => _currentState;
  
  /// Currently connected device ID
  String? get connectedDeviceId => _connectedDeviceId;
  
  /// Check if currently scanning
  bool get isScanning => _scanSubscription != null;
  
  /// Start scanning for ESP32 devices
  /// ESP32 advertises with name "ESP32-GATT-Manager" and service UUID
  /// ESP32 code: BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  Future<void> startScan() async {
    if (_scanSubscription != null) {
      debugPrint('[BleConnection] Already scanning');
      return;
    }
    
    _discoveredDevices.clear();
    _updateConnectionState(models.ConnectionState.scanning);
    
    debugPrint('[BleConnection] Starting scan for ${BleConstants.expectedDeviceName}...');
    
    _scanSubscription = _ble.scanForDevices(
      withServices: [], // Scan all devices, filter by name
      scanMode: ScanMode.lowLatency,
    ).listen(
      (device) {
        // Filter by device name (matches ESP32: BLEDevice::init("ESP32-GATT-Manager"))
        if (device.name == BleConstants.expectedDeviceName) {
          // Add only if not already in list
          if (!_discoveredDevices.any((d) => d.id == device.id)) {
            debugPrint('[BleConnection] Found device: ${device.name} (${device.id})');
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
  Future<void> connect(String deviceId) async {
    if (_connectedDeviceId == deviceId && _currentState.isConnected) {
      debugPrint('[BleConnection] Already connected to $deviceId');
      return;
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
      
      _connectionSubscription = _ble.connectToDevice(
        id: deviceId,
        connectionTimeout: BleConstants.connectionTimeout,
      ).listen(
        (update) {
          debugPrint('[BleConnection] Connection state: ${update.connectionState}');
          
          if (update.connectionState == DeviceConnectionState.connected) {
            _connectedDeviceId = deviceId;
            _updateConnectionState(models.ConnectionState.connected);
            
            if (!completer.isCompleted) {
              completer.complete();
            }
            
            debugPrint('[BleConnection] ✓ Connected to $deviceId');
            
          } else if (update.connectionState == DeviceConnectionState.disconnected) {
            debugPrint('[BleConnection] Disconnected from $deviceId');
            _handleDisconnection();
            
            if (!completer.isCompleted) {
              completer.completeError(Exception('Disconnected before connection established'));
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
      );
      
      // Wait for connection to complete
      await completer.future;
      
      // Now request MTU after connection is established
      // ESP32 code: BLEDevice::setMTU(256);
      debugPrint('[BleConnection] Connection established, negotiating MTU...');
      await _requestMtu(deviceId);
      debugPrint('[BleConnection] MTU negotiation complete');
      
    } catch (e) {
      debugPrint('[BleConnection] ✗ Connection failed: $e');
      _updateConnectionState(models.ConnectionState.error);
      _connectedDeviceId = null;
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
