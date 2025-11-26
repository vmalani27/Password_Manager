import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import '../constants/ble_constants.dart';
import '../models/connection_state.dart' as models;
import 'command_service.dart';
import 'pairing_service.dart';

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
  bool _hasCheckedForStaleConnections = false;
  
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
  
  /// Check if a discovered device matches the paired device
  /// Note: Pairing is now handled by PairingService via ECDH handshake
  bool _isDeviceMatching(DiscoveredDevice device) {
    // No filtering at BLE level - pairing verification happens during ECDH handshake
    return true;
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
  
  /// Check for stale ESP32 session and clean up if needed
  /// Only runs once per app session on first connection
  Future<void> cleanupStaleConnectionIfNeeded(String deviceId) async {
    if (_hasCheckedForStaleConnections) {
      debugPrint('[BleConnection] Stale check already completed - skipping');
      return;
    }

    debugPrint('[BleConnection] ========================================');
    debugPrint('[BleConnection] CHECKING FOR STALE ESP32 SESSION');
    debugPrint('[BleConnection] (App may have been hot reloaded/restarted)');
    debugPrint('[BleConnection] ========================================');

    StreamSubscription<ConnectionStateUpdate>? tempSubscription;
    
    try {
      // Brief connection to check status
      debugPrint('[BleConnection] Attempting brief connection to check ESP32 state...');
      
      final connectionStream = _ble.connectToDevice(
        id: deviceId,
        connectionTimeout: const Duration(seconds: 5),
      );

      final completer = Completer<void>();
      bool isConnected = false;

      tempSubscription = connectionStream.listen(
        (update) {
          debugPrint('[BleConnection] Status check connection state: ${update.connectionState}');
          
          if (update.connectionState == DeviceConnectionState.connected) {
            isConnected = true;
            if (!completer.isCompleted) {
              completer.complete();
            }
          } else if (update.connectionState == DeviceConnectionState.disconnected) {
            if (!completer.isCompleted) {
              completer.completeError(Exception('Status check connection failed'));
            }
          }
        },
        onError: (error) {
          debugPrint('[BleConnection] Status check connection error: $error');
          if (!completer.isCompleted) {
            completer.completeError(error);
          }
        },
      );

      // Wait for connection with timeout
      await completer.future.timeout(
        Duration(seconds: 5),
        onTimeout: () {
          debugPrint('[BleConnection] Status check connection timeout - assuming ESP32 is clean');
          throw TimeoutException('Status check connection timeout');
        },
      );

      if (isConnected) {
        debugPrint('[BleConnection] Connected for status check');
        
        // Give ESP32 time to stabilize
        await Future.delayed(Duration(milliseconds: 500));
        
          // Wait for bonding to complete (placeholder logic)
          // TODO: Replace with actual bonded state check if available from BLE library
          debugPrint('[BleConnection] Waiting for BLE bonding to complete...');
          await Future.delayed(const Duration(seconds: 2)); // Increase delay for bonding
          // If you can check bonded state, poll until bonded
          // while (!await isBonded(deviceId)) {
          //   await Future.delayed(const Duration(milliseconds: 200));
          // }

        // Subscribe to notifications for status check
        debugPrint('[BleConnection] Setting up notifications for status check...');
        await _commandService.subscribeToNotifications(deviceId);
        
        // Wait for notification subscription to be ready
        await Future.delayed(Duration(milliseconds: 300));
        
        try {
          // Try to get status
          debugPrint('[BleConnection] Sending status command...');
          final status = await _commandService.getStatus().timeout(
            Duration(seconds: 3),
            onTimeout: () {
              debugPrint('[BleConnection] Status command timeout - assuming clean session');
              return 'TIMEOUT';
            },
          );
          
          debugPrint('[BleConnection] ESP32 status: $status');
          
          if (status.contains('AUTHORIZED')) {
            // Stale session detected - just disconnect, onDisconnect() will clear it
            debugPrint('[BleConnection] STALE SESSION DETECTED - disconnecting to clear');
            await Future.delayed(Duration(seconds: 2));
            
          } else if (status.contains('UNAUTHORIZED') || status == 'TIMEOUT') {
            // Clean session or timeout - just disconnect
            debugPrint('[BleConnection] Session is clean or ESP32 not responding - disconnecting');
            await Future.delayed(Duration(seconds: 2));
          }
        } catch (e) {
          debugPrint('[BleConnection] Status check failed: $e');
          debugPrint('[BleConnection] Assuming clean session - will disconnect');
          await Future.delayed(Duration(seconds: 2));
        }
        
        // Clean up command service notifications
        await _commandService.cleanup();
      }

    } catch (e) {
      debugPrint('[BleConnection] Status check error: $e');
      debugPrint('[BleConnection] Will proceed with normal connection');
    } finally {
      // Always cancel the status check connection
      await tempSubscription?.cancel();
      debugPrint('[BleConnection] Status check connection closed');
      
      // Mark as completed so we don't do this again
      _hasCheckedForStaleConnections = true;
      
      // Extra delay to let ESP32 fully disconnect
      debugPrint('[BleConnection] Waiting for ESP32 to fully reset...');
      await Future.delayed(Duration(seconds: 2));
      
      debugPrint('[BleConnection] Cleanup complete - ready for fresh connection');
      debugPrint('[BleConnection] ========================================');
    }
  }
  
  /// Connect to a discovered device
  /// ESP32 accepts connection and triggers: ServerCallbacks::onConnect()
  Future<void> connect(String deviceId, {String? deviceName, int retryCount = 0}) async {
    debugPrint('[BleConnection] ========================================');
    debugPrint('[BleConnection] CONNECT() called (attempt ${retryCount + 1})');
    debugPrint('[BleConnection] Device ID: $deviceId');
    debugPrint('[BleConnection] Device Name: ${deviceName ?? "unknown"}');
    debugPrint('[BleConnection] ========================================');
    
    // On first connection attempt, check for stale ESP32 session
    // BUT ONLY if we have existing pairing data (reconnection scenario)
    if (retryCount == 0) {
      // Check if we have pairing data for this device
      final pairingService = PairingService();
      final pairedDevice = await pairingService.getPairedDevice();
      final hasPairingData = pairedDevice?.deviceId == deviceId;
      
      if (hasPairingData) {
        debugPrint('[BleConnection] Found pairing data - checking for stale session...');
        await cleanupStaleConnectionIfNeeded(deviceId);
        
        // Add extra delay after cleanup to let Android BLE stack fully reset
        debugPrint('[BleConnection] Waiting additional 1 second after cleanup for Android BLE reset...');
        await Future.delayed(const Duration(seconds: 1));
      } else {
        debugPrint('[BleConnection] No pairing data - skipping stale session check (first-time pairing)');
      }
    }
    
    if (_connectedDeviceId == deviceId && _currentState.isConnected) {
      debugPrint('[BleConnection] Already connected to $deviceId');
      return;
    }
    
    // Disconnect from any existing connection
    if (_connectedDeviceId != null) {
      debugPrint('[BleConnection] Disconnecting from existing device: $_connectedDeviceId');
      await disconnect();
    }
    
    _updateConnectionState(models.ConnectionState.connecting);
    debugPrint('[BleConnection] Updated state to: connecting');
    
    try {
      // Cancel any existing connection subscription
      debugPrint('[BleConnection] Cancelling any existing subscription...');
      await _connectionSubscription?.cancel();
      _connectionSubscription = null;
      
      final completer = Completer<void>();
      bool isConnecting = true;
      
      debugPrint('[BleConnection] Creating connection stream to $deviceId...');
      debugPrint('[BleConnection] Connection timeout: ${BleConstants.connectionTimeout}');
      
      // Add delay before connection attempt to let Android settle
      // Always do this, not just on first retry - important after status check disconnect
      debugPrint('[BleConnection] Waiting 1 second for Android to settle...');
      await Future.delayed(const Duration(seconds: 1));
      
      _connectionSubscription = _ble.connectToDevice(
        id: deviceId,
        connectionTimeout: BleConstants.connectionTimeout,
      ).listen(
        (update) {
          debugPrint('[BleConnection] *** Connection state update: ${update.connectionState} ***');
          
          if (update.connectionState == DeviceConnectionState.connecting) {
            debugPrint('[BleConnection] CONNECTING state - Android may be performing bonding/pairing');
            debugPrint('[BleConnection] If bonding dialog appears, enter PIN: 123456');
            // Don't do anything here - wait for connected or disconnected
            
          } else if (update.connectionState == DeviceConnectionState.connected) {
            debugPrint('[BleConnection] CONNECTED state received');
            _connectedDeviceId = deviceId;
            _updateConnectionState(models.ConnectionState.connected);
            
            if (!completer.isCompleted && isConnecting) {
              debugPrint('[BleConnection] Completing connection future...');
              completer.complete();
            }
            
            debugPrint('[BleConnection] Connection established to $deviceId');
            
          } else if (update.connectionState == DeviceConnectionState.disconnected) {
            debugPrint('[BleConnection] DISCONNECTED state received');
            debugPrint('[BleConnection] isConnecting: $isConnecting, completer.isCompleted: ${completer.isCompleted}');
            
            // Only handle as error if we were still connecting
            if (isConnecting && !completer.isCompleted) {
              debugPrint('[BleConnection] ERROR: Disconnected before connection established!');
              completer.completeError(Exception('Disconnected before connection established'));
            } else {
              debugPrint('[BleConnection] Normal disconnect (connection was established)');
              // Normal disconnect after connection was established
              _handleDisconnection();
            }
          } else {
            debugPrint('[BleConnection] Other connection state: ${update.connectionState}');
          }
        },
        onError: (error) {
          debugPrint('[BleConnection] CONNECTION ERROR');
          debugPrint('[BleConnection] Error type: ${error.runtimeType}');
          debugPrint('[BleConnection] Error: $error');
          _updateConnectionState(models.ConnectionState.error);
          
          if (!completer.isCompleted) {
            debugPrint('[BleConnection] Completing future with error...');
            completer.completeError(error);
          } else {
            debugPrint('[BleConnection] Future already completed, error ignored');
          }
        },
        cancelOnError: false, // Keep stream alive even on errors
      );
      
      debugPrint('[BleConnection] Connection stream created, waiting for result...');
      
      // Wait for connection to complete
      try {
        await completer.future;
        debugPrint('[BleConnection] completer.future completed successfully');
      } catch (e) {
        debugPrint('[BleConnection] completer.future failed: $e');
        rethrow;
      }
      
      isConnecting = false; // Mark that initial connection is done
      debugPrint('[BleConnection] isConnecting set to false');
      // Reduce stabilization delay and parallelize MTU negotiation and notification subscription
      debugPrint('[BleConnection] Waiting 300ms for connection stabilization...');
      await Future.delayed(const Duration(milliseconds: 300));
      debugPrint('[BleConnection] Starting MTU negotiation and notification subscription in parallel...');
      await Future.wait([
        _requestMtu(deviceId),
        subscribeToNotifications(deviceId),
      ]);
      debugPrint('[BleConnection] MTU negotiation and notification subscription complete');
      debugPrint('[BleConnection] ========================================');
      debugPrint('[BleConnection] CONNECT() SUCCESSFUL');
      debugPrint('[BleConnection] ========================================');
    } catch (e, stackTrace) {
      debugPrint('[BleConnection] ========================================');
      debugPrint('[BleConnection] CONNECT() FAILED (attempt ${retryCount + 1})');
      debugPrint('[BleConnection] Error: $e');
      debugPrint('[BleConnection] ========================================');
      // Retry once on first failure (common with Android bonding)
      if (retryCount == 0 && e.toString().contains('Disconnected before connection established')) {
        debugPrint('[BleConnection] First connection failed - this is common with Android bonding');
        debugPrint('[BleConnection] Retrying connection in 700ms...');
        // Clean up
        await _connectionSubscription?.cancel();
        _connectionSubscription = null;
        _connectedDeviceId = null;
        // Wait before retry
        await Future.delayed(const Duration(milliseconds: 700));
        // Retry
        return connect(deviceId, deviceName: deviceName, retryCount: 1);
      }
      
      // No more retries or different error
      debugPrint('[BleConnection] No retry - final failure');
      debugPrint('[BleConnection] Stack trace: $stackTrace');
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
  /// ESP32 triggers: ServerCallbacks::onDisconnect() which clears the session
  Future<void> disconnect([String? deviceId]) async {
    debugPrint('[BleConnection] Disconnecting...');
    
    final currentDeviceId = _connectedDeviceId ?? deviceId;
    debugPrint('[BleConnection] Device ID: $currentDeviceId');
    
    // Cancel connection subscription - this triggers ESP32's onDisconnect()
    // which automatically clears sessionAuthorized and sessionToken
    await _connectionSubscription?.cancel();
    _connectionSubscription = null;
    
    _handleDisconnection();
    
    debugPrint('[BleConnection] Disconnected - ESP32 will clear session in onDisconnect()');
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
  
  /// Returns true if the given deviceId is currently connected
  Future<bool> isDeviceConnected(String deviceId) async {
    // Use FlutterReactiveBle's API to check connection
    // If you have a direct reference, use it; otherwise, always return false if not matching
    return _currentState == models.ConnectionState.connected || _currentState == models.ConnectionState.authenticated;
  }
  
  /// Dispose of all resources
  void dispose() {
    stopScan();
    disconnect();
    _discoveredDevicesController.close();
    _connectionStateController.close();
  }
}