import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
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

  final _discoveredDevicesController =
      StreamController<List<DiscoveredDevice>>.broadcast();
  final _connectionStateController =
      StreamController<models.ConnectionState>.broadcast();

  final List<DiscoveredDevice> _discoveredDevices = [];
  String? _connectedDeviceId;
  models.ConnectionState _currentState = models.ConnectionState.disconnected;

  // Used so we only do the �stale session check� once per app run
  bool _hasCheckedForStaleConnections = false;

  /// Stream of discovered devices during scan
  Stream<List<DiscoveredDevice>> get discoveredDevicesStream =>
      _discoveredDevicesController.stream;

  /// Stream of connection state changes
  Stream<models.ConnectionState> get connectionStateStream =>
      _connectionStateController.stream;

  /// Current connection state
  models.ConnectionState get connectionState => _currentState;

  /// Currently connected device ID
  String? get connectedDeviceId => _connectedDeviceId;

  /// Check if currently scanning
  bool get isScanning => _scanSubscription != null;

  /// In the future you can add MAC filtering here.
  bool _isDeviceMatching(DiscoveredDevice device) {
    // For now accept any device with the correct name prefix.
    return true;
  }

  /// Start scanning for ESP32 devices.
  ///
  /// ESP32 advertises with name "ESP32-PWD-Manager-XXXX".
  Future<void> startScan() async {
    if (_scanSubscription != null) {
      debugPrint('[BleConnection] Already scanning');
      return;
    }

    _discoveredDevices.clear();
    _updateConnectionState(models.ConnectionState.scanning);

    debugPrint(
        '[BleConnection] Starting scan for devices with prefix: ${BleConstants.deviceNamePrefix}');

    _scanSubscription = _ble
        .scanForDevices(
          withServices: const [], // Scan all devices, filter by name
          scanMode: ScanMode.lowLatency,
        )
        .listen(
      (device) {
        if (device.name.startsWith(BleConstants.deviceNamePrefix)) {
          final isMatching = _isDeviceMatching(device);

          if (!_discoveredDevices.any((d) => d.id == device.id)) {
            debugPrint(
                '[BleConnection] Found device: ${device.name} (${device.id}) ${isMatching ? "MATCH" : ""}');
            _discoveredDevices.add(device);
            _discoveredDevicesController.add(List.from(_discoveredDevices));

            // Auto-stop scan immediately when a first matching device is found
            stopScan();
          }
        }
      },
      onError: (error) {
        debugPrint('[BleConnection] Scan error: $error');
        stopScan();
      },
    );

    // Auto-stop scan after duration if nothing found
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

    debugPrint(
        '[BleConnection] Scan stopped. Found ${_discoveredDevices.length} device(s)');
  }

  /// Scan for devices and return list (instant return on discovery)
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

      final completer = Completer<List<DiscoveredDevice>>();
      late StreamSubscription<List<DiscoveredDevice>> subscription;
      bool deviceFound = false;

      subscription = discoveredDevicesStream.listen((devices) {
        if (devices.isNotEmpty && !deviceFound) {
          deviceFound = true;
          completer.complete(List.from(devices));
          subscription.cancel();
          stopScan();
        }
      });

      // Fallback timeout: complete with whatever is found after timeout
      Future.delayed(timeout, () {
        if (!completer.isCompleted) {
          completer.complete(List.from(_discoveredDevices));
          subscription.cancel();
          stopScan();
        }
      });

      final result = await completer.future;
      return result;
    } catch (e) {
      debugPrint('[BleConnection] Scan error: $e');
      stopScan(); // Ensure scan is stopped on error
      rethrow;
    }
  }

  /// Optional: one-time �stale session� cleanup before first main connection.
  ///
  /// Connects briefly, asks ESP32 for STATUS, then disconnects so that
  /// any stale AUTHORIZED session is cleared on the device side.
  Future<void> _checkAndCleanStaleSessionIfNeeded(String deviceId) async {
    if (_hasCheckedForStaleConnections) return;

    debugPrint('[BleConnection] ========================================');
    debugPrint('[BleConnection] CHECKING FOR STALE ESP32 SESSION');
    debugPrint('[BleConnection] (App may have been hot reloaded/restarted)');
    debugPrint('[BleConnection] ========================================');

    _hasCheckedForStaleConnections = true;

    StreamSubscription<ConnectionStateUpdate>? tempSubscription;

    try {
      final connectionStream = _ble.connectToDevice(
        id: deviceId,
        connectionTimeout: const Duration(seconds: 5),
      );

      final completer = Completer<void>();
      bool isConnected = false;

      tempSubscription = connectionStream.listen(
        (update) {
          debugPrint(
              '[BleConnection] Status check connection state: ${update.connectionState}');
          if (update.connectionState == DeviceConnectionState.connected) {
            isConnected = true;
            if (!completer.isCompleted) {
              completer.complete();
            }
          } else if (update.connectionState ==
              DeviceConnectionState.disconnected) {
            if (!completer.isCompleted) {
              completer.completeError(
                  Exception('Status check connection failed'));
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
        const Duration(seconds: 5),
        onTimeout: () {
          debugPrint(
              '[BleConnection] Status check connection timeout - assuming ESP32 is clean');
          throw TimeoutException('Status check connection timeout');
        },
      );

      if (isConnected) {
        debugPrint('[BleConnection] Connected for status check');
        await Future.delayed(const Duration(milliseconds: 500));

        debugPrint('[BleConnection] Setting up notifications for status check');
        await _commandService.subscribeToNotifications(deviceId);
        await Future.delayed(const Duration(milliseconds: 300));

        try {
          debugPrint('[BleConnection] Sending status command...');
          final status = await _commandService.getStatus().timeout(
                const Duration(seconds: 3),
                onTimeout: () {
                  debugPrint(
                      '[BleConnection] Status command timeout - assuming clean session');
                  return 'TIMEOUT';
                },
              );

          debugPrint('[BleConnection] ESP32 status: $status');

          if (status.contains('AUTHORIZED')) {
            debugPrint(
                '[BleConnection] STALE SESSION DETECTED - disconnecting to clear');
            // Just disconnect; ESP32 onDisconnect() will clear session.
          } else {
            debugPrint(
                '[BleConnection] Session is clean or ESP32 not responding');
          }
        } catch (e) {
          debugPrint('[BleConnection] Status check failed: $e');
        }

        await _commandService.cleanup();
      }
    } catch (e) {
      debugPrint('[BleConnection] Status check error: $e');
    } finally {
      await tempSubscription?.cancel();
      debugPrint('[BleConnection] Status check connection closed');
      debugPrint(
          '[BleConnection] Waiting 2s for ESP32 to fully reset after check...');
      await Future.delayed(const Duration(seconds: 2));
      debugPrint('[BleConnection] Cleanup complete - ready for fresh connection');
      debugPrint('[BleConnection] ========================================');
    }
  }

  /// Connect to a device by ID.
  ///
  /// This will also request MTU and subscribe CommandService to notifications.
  Future<void> connect(
    String deviceId, {
    String? deviceName,
    int retryCount = 0,
  }) async {
    debugPrint('[BleConnection] ========================================');
    debugPrint('[BleConnection] CONNECT() called (attempt ${retryCount + 1})');
    debugPrint('[BleConnection] Device ID: $deviceId');
    debugPrint('[BleConnection] Device Name: ${deviceName ?? "unknown"}');
    debugPrint('[BleConnection] ========================================');

    // Always run stale session cleanup before every connection
    await _checkAndCleanStaleSessionIfNeeded(deviceId);

    try {
      _updateConnectionState(models.ConnectionState.connecting);

      bool isConnecting = true;
      final completer = Completer<void>();

      // Ensure any previous connection subscription is cancelled
      await _connectionSubscription?.cancel();
      _connectionSubscription = null;

      final connectionStream = _ble.connectToDevice(
        id: deviceId,
        connectionTimeout: BleConstants.connectionTimeout,
      );

      _connectionSubscription = connectionStream.listen(
        (update) {
          debugPrint(
              '[BleConnection] *** Connection state update: ${update.connectionState} ***');

          if (update.connectionState == DeviceConnectionState.connecting) {
            debugPrint(
                '[BleConnection] CONNECTING state - Android may be performing bonding/pairing');
          } else if (update.connectionState ==
              DeviceConnectionState.connected) {
            debugPrint('[BleConnection] CONNECTED state received');
            _connectedDeviceId = deviceId;
            _updateConnectionState(models.ConnectionState.connected);

            if (!completer.isCompleted && isConnecting) {
              debugPrint('[BleConnection] Completing connection future...');
              completer.complete();
            }

            debugPrint(
                '[BleConnection] Connection established to $deviceId');
          } else if (update.connectionState ==
              DeviceConnectionState.disconnected) {
            debugPrint('[BleConnection] DISCONNECTED state received');
            debugPrint(
                '[BleConnection] isConnecting: $isConnecting, completer.isCompleted: ${completer.isCompleted}');

            if (isConnecting && !completer.isCompleted) {
              debugPrint(
                  '[BleConnection] ERROR: Disconnected before connection established');
              completer.completeError(
                Exception('Disconnected before connection established'),
              );
            } else {
              debugPrint(
                  '[BleConnection] Normal disconnect (connection was established)');
              _handleDisconnection();
            }
          } else {
            debugPrint(
                '[BleConnection] Other connection state: ${update.connectionState}');
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
            debugPrint(
                '[BleConnection] Future already completed, error ignored');
          }
        },
        cancelOnError: false,
      );

      debugPrint('[BleConnection] Connection stream created, waiting for result...');

      try {
        await completer.future;
        debugPrint('[BleConnection] completer.future completed successfully');
      } catch (e) {
        debugPrint('[BleConnection] completer.future failed: $e');
        rethrow;
      }

      isConnecting = false;
      debugPrint('[BleConnection] isConnecting set to false');

      // Short stabilization + parallel MTU + notification subscription
      debugPrint(
          '[BleConnection] Waiting 300ms for connection stabilization...');
      await Future.delayed(const Duration(milliseconds: 300));

      debugPrint(
          '[BleConnection] Starting MTU negotiation and notification subscription in parallel...');
      await Future.wait([
        _requestMtu(deviceId),
        _commandService.subscribeToNotifications(deviceId),
      ]);

      debugPrint(
          '[BleConnection] MTU negotiation and notification subscription complete');
      debugPrint('[BleConnection] ========================================');
      debugPrint('[BleConnection] CONNECT() SUCCESSFUL');
      debugPrint('[BleConnection] ========================================');
    } catch (e, stackTrace) {
      debugPrint('[BleConnection] ========================================');
      debugPrint(
          '[BleConnection] CONNECT() FAILED (attempt ${retryCount + 1})');
      debugPrint('[BleConnection] Error: $e');
      debugPrint('[BleConnection] ========================================');

      // Retry once on the classic Android �disconnect before connect� issue
      if (retryCount == 0 &&
          e.toString().contains('Disconnected before connection established')) {
        debugPrint(
            '[BleConnection] First connection failed - common with Android bonding');
        debugPrint('[BleConnection] Retrying connection in 700ms...');

        await _connectionSubscription?.cancel();
        _connectionSubscription = null;
        _connectedDeviceId = null;

        await Future.delayed(const Duration(milliseconds: 700));
        return connect(deviceId, deviceName: deviceName, retryCount: 1);
      }

      debugPrint('[BleConnection] No retry - final failure');
      debugPrint('[BleConnection] Stack trace: $stackTrace');
      _updateConnectionState(models.ConnectionState.error);
      _connectedDeviceId = null;

      await _connectionSubscription?.cancel();
      _connectionSubscription = null;

      rethrow;
    }
  }

  /// Request MTU size negotiation
  Future<void> _requestMtu(String deviceId) async {
    try {
      final mtu = await _ble.requestMtu(
        deviceId: deviceId,
        mtu: BleConstants.preferredMtu,
      );
      debugPrint('[BleConnection] MTU negotiated: $mtu bytes');
    } catch (e) {
      debugPrint(
          '[BleConnection] MTU negotiation failed: $e (continuing anyway)');
    }
  }

  /// Disconnect from current device
  Future<void> disconnect([String? deviceId]) async {
    debugPrint('[BleConnection] Disconnecting...');

    final currentDeviceId = _connectedDeviceId ?? deviceId;
    debugPrint('[BleConnection] Device ID: $currentDeviceId');

    await _connectionSubscription?.cancel();
    _connectionSubscription = null;

    _handleDisconnection();

    debugPrint(
        '[BleConnection] Disconnected - ESP32 will clear session in onDisconnect()');
  }

  void _handleDisconnection() {
    _connectedDeviceId = null;
    _updateConnectionState(models.ConnectionState.disconnected);
    debugPrint('[BleConnection] Disconnected');
  }

  void _updateConnectionState(models.ConnectionState newState) {
    if (_currentState != newState) {
      _currentState = newState;
      _connectionStateController.add(newState);
      debugPrint('[BleConnection] State: ${newState.displayText}');
    }
  }

  /// Very simple �connected?� helper for now
  Future<bool> isDeviceConnected(String deviceId) async {
    return _currentState == models.ConnectionState.connected ||
        _currentState == models.ConnectionState.authenticated;
  }

  /// Subscribe to notifications from ESP32, with bonding dialog handling
  Future<void> subscribeToNotifications([String? deviceId]) async {
    final id = deviceId ?? _connectedDeviceId;
    if (id == null) {
      throw Exception('No deviceId provided and no device is currently connected');
    }
    int attempt = 1;
    int maxAttempts = 8;
    int delayMs = 300;
    while (attempt <= maxAttempts) {
      try {
        await _commandService.subscribeToNotifications(id);
        debugPrint('[BleConnection] Subscribed to ESP32 notifications');
        return;
      } catch (e) {
        if (e is PlatformException && e.code == 'service_discovery_failure' &&
            e.message != null && e.message!.contains('Bonding is in progress')) {
          debugPrint('[BleConnection] Bonding in progress, waiting before retry (attempt $attempt/$maxAttempts)...');
          await Future.delayed(Duration(milliseconds: delayMs));
          attempt++;
          delayMs *= 2;
        } else {
          debugPrint('[BleConnection] Notification subscription failed: $e');
          rethrow;
        }
      }
    }
    throw Exception('Bonding did not complete after $maxAttempts attempts. Please complete the pairing dialog and try again.');
  }

  /// Dispose of all resources
  void dispose() {
    stopScan();
    // fire and forget
    // ignore: discarded_futures
    disconnect();
    _discoveredDevicesController.close();
    _connectionStateController.close();
  }
}
