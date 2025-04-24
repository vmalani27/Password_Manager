import 'dart:async';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart' as reactive_ble;
import 'package:flutter/foundation.dart';

class BLEService {
  final _ble = reactive_ble.FlutterReactiveBle();
  String? _connectedDeviceId;
  StreamSubscription? _connectionSubscription;
  StreamSubscription? _notificationSubscription;
  final _responseController = StreamController<String>.broadcast();
  bool _isConnecting = false;
  String _buffer = '';

  String? get connectedDeviceId => _connectedDeviceId;
  Stream<String> get onResponse => _responseController.stream;

  Future<void> connectToDevice(String deviceId) async {
    if (_isConnecting) return;
    _isConnecting = true;

    try {
      if (_connectedDeviceId != null) {
        await disconnect();
      }

      // Cancel any existing connection subscription
      await _connectionSubscription?.cancel();

      final connectionStream = _ble.connectToDevice(
        id: deviceId,
        connectionTimeout: const Duration(seconds: 10),
      );

      final completer = Completer<void>();

      _connectionSubscription = connectionStream.listen(
        (update) async {
          debugPrint('Connection state: ${update.connectionState}');
          if (update.connectionState == reactive_ble.DeviceConnectionState.connected) {
            _connectedDeviceId = deviceId;

            // Request a larger MTU size
            try {
              final mtu = await _ble.requestMtu(deviceId: _connectedDeviceId!, mtu: 512);
              debugPrint('MTU size negotiated: $mtu');
            } catch (e) {
              debugPrint('Failed to negotiate MTU size: $e');
            }

            await Future.delayed(const Duration(seconds: 2)); // Wait for services to be discovered

            _setupNotifications();
            debugPrint('✅ Connected to $deviceId');
            completer.complete(); // Connection successful
          } else if (update.connectionState == reactive_ble.DeviceConnectionState.disconnected) {
            debugPrint('Disconnected from device: $deviceId');
            _connectedDeviceId = null;
            _notificationSubscription?.cancel();
            if (!completer.isCompleted) {
              completer.completeError(Exception('Disconnected before connection could be established'));
            }
          }
        },
        onError: (error) {
          debugPrint('Connection error: $error');
          _connectedDeviceId = null;
          _notificationSubscription?.cancel();
          if (!completer.isCompleted) {
            completer.completeError(error);
          }
        },
      );

      // Await until device is connected
      await completer.future;

    } catch (e) {
      _connectedDeviceId = null;
      debugPrint('❌ Failed to connect: $e');
      throw Exception('Failed to connect: $e');
    } finally {
      _isConnecting = false;
    }
  }

  void _setupNotifications() async {
    if (_connectedDeviceId == null) return;

    try {
      final characteristic = reactive_ble.QualifiedCharacteristic(
        serviceId: reactive_ble.Uuid.parse("6E400001-B5A3-F393-E0A9-E50E24DCCA9E"),
        characteristicId: reactive_ble.Uuid.parse("6E400003-B5A3-F393-E0A9-E50E24DCCA9E"), // Notification UUID
        deviceId: _connectedDeviceId!,
      );

      // Subscribe to notifications
      _notificationSubscription = _ble.subscribeToCharacteristic(characteristic).listen(
        (data) {
          final response = String.fromCharCodes(data);
          debugPrint('Received notification: $response');
          _handleNotification(response);
        },
        onError: (error) {
          debugPrint('Notification error: $error');
        },
      );

      debugPrint('Subscribed to notifications for characteristic: ${characteristic.characteristicId}');

      // Explicitly read the characteristic value
      final value = await _ble.readCharacteristic(characteristic);
      final response = String.fromCharCodes(value);
      debugPrint('Read notification characteristic: $response');
      _handleNotification(response);
    } catch (e) {
      debugPrint('Error setting up notifications: $e');
    }
  }

  void _handleNotification(String data) {
    debugPrint('Notification received: $data');
    _responseController.add(data); // Add the notification data to the stream
  }

  Future<void> disconnect() async {
    try {
      debugPrint('Disconnecting from device...');
      await _connectionSubscription?.cancel();
      _connectionSubscription = null;

      await _notificationSubscription?.cancel();
      _notificationSubscription = null;

      _connectedDeviceId = null;
      debugPrint('Disconnected successfully');
    } catch (e) {
      debugPrint('Error during disconnect: $e');
    }
  }

  Future<String> sendCommand(String command, {int retries = 3}) async {
    if (_connectedDeviceId == null) {
      throw Exception('No device connected');
    }

    for (int attempt = 1; attempt <= retries; attempt++) {
      try {
        final writeCharacteristic = reactive_ble.QualifiedCharacteristic(
          serviceId: reactive_ble.Uuid.parse("6E400001-B5A3-F393-E0A9-E50E24DCCA9E"),
          characteristicId: reactive_ble.Uuid.parse("6E400002-B5A3-F393-E0A9-E50E24DCCA9E"), // Write UUID
          deviceId: _connectedDeviceId!,
        );

        final notificationCharacteristic = reactive_ble.QualifiedCharacteristic(
          serviceId: reactive_ble.Uuid.parse("6E400001-B5A3-F393-E0A9-E50E24DCCA9E"),
          characteristicId: reactive_ble.Uuid.parse("6E400003-B5A3-F393-E0A9-E50E24DCCA9E"), // Notification UUID
          deviceId: _connectedDeviceId!,
        );

        // Write the command
        await _ble.writeCharacteristicWithResponse(
          writeCharacteristic,
          value: command.codeUnits,
        );

        // Explicitly read the notification characteristic
        final value = await _ble.readCharacteristic(notificationCharacteristic);
        final response = String.fromCharCodes(value);
        debugPrint('Read notification characteristic: $response');
        return response;
      } catch (e) {
        if (attempt == retries) {
          debugPrint('Error sending command after $retries attempts: $e');
          throw Exception('Failed to send command: $e');
        }
        debugPrint('Retrying command (attempt $attempt): $e');
        await Future.delayed(const Duration(seconds: 2));
      }
    }

    throw Exception('Failed to send command after $retries attempts');
  }

  void dispose() {
    _connectionSubscription?.cancel();
    _notificationSubscription?.cancel();
    _responseController.close();
  }
}
