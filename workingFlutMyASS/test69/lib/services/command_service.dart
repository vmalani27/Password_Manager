import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import '../constants/ble_constants.dart';

/// Low-level service for sending commands and receiving responses from ESP32
/// Handles BLE characteristic read/write operations
class CommandService {
  final FlutterReactiveBle _ble = FlutterReactiveBle();
  
  String? _connectedDeviceId;
  StreamSubscription<List<int>>? _notificationSubscription;
  final _responseController = StreamController<String>.broadcast();
  
  /// Stream of all responses received from ESP32 notification characteristic
  Stream<String> get responseStream => _responseController.stream;
  
  /// Check if we have an active connection
  bool get isConnected => _connectedDeviceId != null;
  
  /// Initialize notification listener after connection
  /// Must be called after successful BLE connection
  /// ESP32 sends responses via NOTIFICATION_CHARACTERISTIC_UUID
  /// 
  /// Implements retry logic with exponential backoff for bonding scenarios:
  /// - OS bonding may still be in progress when this is called
  /// - Service discovery will fail if bonding isn't complete
  /// - Retries with increasing delays (500ms, 1s, 2s, 4s)
  /// 
  /// This method will NOT throw exceptions during retries - only after all attempts fail
  Future<void> subscribeToNotifications(String deviceId) async {
    if (_connectedDeviceId == deviceId && _notificationSubscription != null) {
      debugPrint('[CommandService] Already subscribed to notifications');
      return;
    }
    
    _connectedDeviceId = deviceId;
    
    // Retry configuration
    const maxAttempts = 8;
    const initialDelay = Duration(milliseconds: 300);
    const maxDelay = Duration(seconds: 3);
    
    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
      try {
        debugPrint('[CommandService] 🔍 Discovering services (attempt $attempt/$maxAttempts)');
        
        final characteristic = QualifiedCharacteristic(
          serviceId: Uuid.parse(BleConstants.serviceUuid),
          characteristicId: Uuid.parse(BleConstants.notificationCharacteristicUuid),
          deviceId: deviceId,
        );
        
        // Create a completer to track subscription success/failure
        final completer = Completer<void>();
        StreamSubscription<List<int>>? tempSubscription;
        
        // Try to subscribe - this will fail if bonding is still in progress
        // Subscribe to notifications from ESP32
        // ESP32 code: pCharacteristic->setValue(data.c_str()); pCharacteristic->notify();
        tempSubscription = _ble.subscribeToCharacteristic(characteristic).listen(
          (data) {
            debugPrint('[CommandService] RAW: ${data.length} bytes: $data');
            final response = String.fromCharCodes(data).trim();
            debugPrint('[CommandService] ← Received: "$response"');
            _responseController.add(response);
          },
          onError: (error) {
            debugPrint('[CommandService] Stream error: $error');
            if (!completer.isCompleted) {
              completer.completeError(error);
            }
          },
          onDone: () {
            debugPrint('[CommandService] Notification stream closed');
          },
        );
        
        // Give the stream a moment to either succeed or fail
        // If bonding is in progress, the stream will error immediately
        await Future.any([
          completer.future,
          Future.delayed(const Duration(milliseconds: 200)),
        ]);
        
        // If we get here without an error, subscription succeeded
        if (!completer.isCompleted) {
          _notificationSubscription = tempSubscription;
          debugPrint('[CommandService] ✅ Service discovery successful!');
          return; // Success!
        } else {
          // completer completed with error, will be caught below
          await completer.future;
        }
        
      } catch (e) {
        // Log the error but don't propagate it yet (silent retry)
        final errorMsg = e.toString();
        final truncatedMsg = errorMsg.length > 150 
            ? errorMsg.substring(0, 150) + '...' 
            : errorMsg;
        debugPrint('[CommandService] ⚠️  Attempt $attempt failed: $truncatedMsg');
        
        // Cancel any partial subscription
        await _notificationSubscription?.cancel();
        _notificationSubscription = null;
        await _notificationSubscription?.cancel();
        _notificationSubscription = null;
        
        // If this was the last attempt, give up
        if (attempt == maxAttempts) {
          debugPrint('[CommandService] ❌ All $maxAttempts attempts failed. Bonding did not complete in time.');
          throw Exception(
            'Failed to discover BLE services after $maxAttempts attempts. '
            'Bonding may have failed or timed out. Please try again.'
          );
        }
        
        // Calculate exponential backoff delay
        final delayMs = initialDelay.inMilliseconds * (1 << (attempt - 1)); // 500, 1000, 2000, 4000, 8000
        final delay = Duration(milliseconds: delayMs).compareTo(maxDelay) > 0 
            ? maxDelay 
            : Duration(milliseconds: delayMs);
        
        debugPrint('[CommandService] ⏳ Waiting ${delay.inMilliseconds}ms before retry (bonding in progress)...');
        await Future.delayed(delay);
      }
    }
  }
  
  /// Send a command to ESP32 and wait for response
  /// ESP32 receives commands via CHARACTERISTIC_UUID (RX characteristic)
  /// ESP32 responds via NOTIFICATION_CHARACTERISTIC_UUID
  Future<String> sendCommand(
    String command, {
    Duration? timeout,
    bool waitForResponse = true,
  }) async {
    if (_connectedDeviceId == null) {
      throw Exception('No device connected');
    }
    
    debugPrint('[CommandService] → Sending: $command');
    
    try {
      final writeCharacteristic = QualifiedCharacteristic(
        serviceId: Uuid.parse(BleConstants.serviceUuid),
        characteristicId: Uuid.parse(BleConstants.rxCharacteristicUuid),
        deviceId: _connectedDeviceId!,
      );
      
      debugPrint('[CommandService] Writing to characteristic ${BleConstants.rxCharacteristicUuid}');
      
      // Start listening for response BEFORE sending command
      final responseFuture = waitForResponse 
          ? responseStream.first.timeout(
              timeout ?? BleConstants.commandTimeout,
              onTimeout: () {
                debugPrint('[CommandService] ⏱️ Timeout waiting for response to: $command');
                throw TimeoutException('No response from ESP32 for command: $command');
              },
            )
          : Future.value('');
      
      // Send command to ESP32
      // ESP32 code: class CommandCallback : public BLECharacteristicCallbacks { void onWrite(...) }
      await _ble.writeCharacteristicWithResponse(
        writeCharacteristic,
        value: command.codeUnits,
      );
      
      debugPrint('[CommandService] ✓ Write successful, waiting for response...');
      
      if (!waitForResponse) {
        return '';
      }
      
      // Wait for ESP32 response via notification
      // ESP32 code: sendNotification(response);
      final response = await responseFuture;
      
      debugPrint('[CommandService] ✓ Response: $response');
      return response;
      
    } catch (e) {
      debugPrint('[CommandService] ✗ Command failed: $e');
      rethrow;
    }
  }
  
  /// Send command without waiting for response (fire and forget)
  Future<void> sendCommandNoResponse(String command) async {
    await sendCommand(command, waitForResponse: false);
  }
  
  /// Read the notification characteristic directly (used for initial check)
  /// ESP32 sets initial value: pCharacteristic->setValue("Ready");
  Future<String> readNotificationCharacteristic() async {
    if (_connectedDeviceId == null) {
      throw Exception('No device connected');
    }
    
    try {
      final characteristic = QualifiedCharacteristic(
        serviceId: Uuid.parse(BleConstants.serviceUuid),
        characteristicId: Uuid.parse(BleConstants.notificationCharacteristicUuid),
        deviceId: _connectedDeviceId!,
      );
      
      final value = await _ble.readCharacteristic(characteristic);
      final response = String.fromCharCodes(value).trim();
      debugPrint('[CommandService] Read initial value: $response');
      return response;
    } catch (e) {
      debugPrint('[CommandService] Failed to read characteristic: $e');
      rethrow;
    }
  }
  
  /// Unsubscribe from notifications and clear device connection
  /// Called on disconnect
  Future<void> cleanup() async {
    debugPrint('[CommandService] Cleaning up...');
    await _notificationSubscription?.cancel();
    _notificationSubscription = null;
    _connectedDeviceId = null;
  }
  
  /// Dispose of all resources
  void dispose() {
    cleanup();
    _responseController.close();
  }
}
