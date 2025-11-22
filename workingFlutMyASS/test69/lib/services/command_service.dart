import 'dart:async';
import 'dart:typed_data';
import 'package:flutter/foundation.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:convert/convert.dart';
import '../constants/ble_constants.dart';
import 'ecdh_service.dart';
import 'pairing_service.dart';
import '../models/pairing_device.dart';

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
        debugPrint('[CommandService] Discovering services (attempt $attempt/$maxAttempts)');
        
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
          debugPrint('[CommandService] Service discovery successful!');
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
        debugPrint('[CommandService] Attempt $attempt failed: $truncatedMsg');
        
        // Cancel any partial subscription
        await _notificationSubscription?.cancel();
        _notificationSubscription = null;
        await _notificationSubscription?.cancel();
        _notificationSubscription = null;
        
        // If this was the last attempt, give up
        if (attempt == maxAttempts) {
          debugPrint('[CommandService] All $maxAttempts attempts failed. Bonding did not complete in time.');
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
        
        debugPrint('[CommandService] Waiting ${delay.inMilliseconds}ms before retry (bonding in progress)...');
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
                debugPrint('[CommandService] Timeout waiting for response to: $command');
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
      
      debugPrint('[CommandService] Write successful, waiting for response...');
      
      if (!waitForResponse) {
        return '';
      }
      
      // Wait for ESP32 response via notification
      // ESP32 code: sendNotification(response);
      final response = await responseFuture;
      
      debugPrint('[CommandService] Response: $response');
      return response;
      
    } catch (e) {
      debugPrint('[CommandService] Command failed: $e');
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
  
  /// Perform ECDH key exchange with ESP32 (with persistent pairing support)
  /// 
  /// Returns a record containing:
  /// - ecdh: The ECDH service instance with session key
  /// - isNewPairing: true if this is a new pairing, false if reconnecting
  /// 
  /// Pairing Flow:
  /// 1. Check if device is already paired
  /// 2. If paired: Load saved keys, send pubkey, wait for ECDH_OK
  /// 3. If not paired: Generate keys, exchange, wait for ECDH_OK_PAIRED, save pairing
  /// 4. ESP32 may respond with ECDH_ALREADY_PAIRED if ESP32 has a different pairing
  Future<({EcdhService ecdh, bool isNewPairing})> performEcdhHandshake(
    String deviceId,
    String deviceName,
  ) async {
    debugPrint('[ECDH] Starting handshake with device: $deviceName ($deviceId)...');
    
    final pairingService = PairingService();
    final ecdh = EcdhService();
    
    try {
      // Check if device is already paired
      final isPaired = await pairingService.isPaired();
      final pairedDevice = isPaired ? await pairingService.getPairedDevice() : null;
      
      // Read ESP32's public key (always needed for shared secret computation)
      debugPrint('[ECDH] Reading ESP32 public key...');
      final esp32PubKey = await _ble.readCharacteristic(
        QualifiedCharacteristic(
          serviceId: Uuid.parse(BleConstants.serviceUuid),
          characteristicId: Uuid.parse(BleConstants.ecdhCharacteristicUuid),
          deviceId: deviceId,
        ),
      );
      debugPrint('[ECDH] Read ESP32 public key: ${esp32PubKey.length} bytes');
      
      Uint8List clientPubKey;
      
      if (isPaired && pairedDevice != null && pairedDevice.deviceId == deviceId) {
        // Reconnecting to paired device - load saved keys
        debugPrint('[ECDH] Device is paired, loading saved keys...');
        ecdh.loadKeyPair(
          Uint8List.fromList(pairedDevice.clientPrivateKey),
          Uint8List.fromList(pairedDevice.clientPublicKey),
        );
        clientPubKey = Uint8List.fromList(pairedDevice.clientPublicKey);
        
      } else if (isPaired && pairedDevice != null && pairedDevice.deviceId != deviceId) {
        // Trying to connect to different device while paired to another
        throw Exception(
          'Already paired to device "${pairedDevice.deviceName}". '
          'Unpair first before connecting to a different device.'
        );
        
      } else {
        // First time pairing - generate new keys
        debugPrint('[ECDH] First time pairing, generating new keys...');
        ecdh.generateKeyPair();
        clientPubKey = ecdh.getPublicKeyBytes();
      }
      
      // Compute shared secret (for both new pairing and reconnection)
      debugPrint('[ECDH] Computing shared secret...');
      ecdh.computeSharedSecret(Uint8List.fromList(esp32PubKey));
      
      // Start listening for ECDH response BEFORE sending public key
      debugPrint('[ECDH] Setting up listener for ECDH response...');
      final completer = Completer<String>();
      late StreamSubscription<String> subscription;
      
      subscription = responseStream.listen((response) {
        if (!completer.isCompleted) {
          debugPrint('[ECDH] Received notification: $response');
          completer.complete(response);
          subscription.cancel();
        }
      });
      
      // Set timeout
      Future.delayed(Duration(seconds: 5), () {
        if (!completer.isCompleted) {
          subscription.cancel();
          completer.completeError(TimeoutException('No ECDH response from ESP32'));
        }
      });
      
      // Send our public key to ESP32
      debugPrint('[ECDH] Sending client public key...');
      await _ble.writeCharacteristicWithoutResponse(
        QualifiedCharacteristic(
          serviceId: Uuid.parse(BleConstants.serviceUuid),
          characteristicId: Uuid.parse(BleConstants.ecdhCharacteristicUuid),
          deviceId: deviceId,
        ),
        value: clientPubKey,
      );
      
      debugPrint('[ECDH] Write complete, waiting for ECDH response...');
      
      // Wait for ECDH response
      final response = await completer.future;
      
      // Handle different response types
      if (response == Esp32Commands.ecdhOk) {
        // Reconnection successful
        debugPrint('[ECDH] Received ECDH_OK - reconnection successful!');
        return (ecdh: ecdh, isNewPairing: false);
        
      } else if (response == Esp32Commands.ecdhOkPaired) {
        // New pairing successful - save pairing data
        debugPrint('[ECDH] Received ECDH_OK_PAIRED - new pairing established!');
        
        final pairedDevice = PairedDevice(
          deviceId: deviceId,
          deviceName: deviceName,
          clientPrivateKey: ecdh.getPrivateKeyBytes(),
          clientPublicKey: clientPubKey,
          esp32PublicKey: Uint8List.fromList(esp32PubKey),
          pairedAt: DateTime.now(),
        );
        
        await pairingService.savePairing(pairedDevice);
        
        debugPrint('[ECDH] Pairing saved to persistent storage');
        return (ecdh: ecdh, isNewPairing: true);
        
      } else if (response == Esp32Commands.ecdhAlreadyPaired) {
        // ESP32 is paired with a different device
        throw Exception(
          'ESP32 is already paired with a different device. '
          'Unpair the ESP32 first using the device button.'
        );
        
      } else {
        throw Exception('Unexpected ECDH response: $response');
      }
      
    } catch (e) {
      debugPrint('[ECDH] Handshake failed: $e');
      ecdh.clear();
      rethrow;
    }
  }
  
  /// Unpair from the current device
  /// Sends unpair command to ESP32 and removes local pairing data
  Future<void> unpairDevice() async {
    try {
      debugPrint('[ECDH] Sending unpair command...');
      final response = await sendCommand(Esp32Commands.unpair);
      
      if (response != Esp32Commands.unpaired) {
        debugPrint('[ECDH] Unexpected unpair response: $response');
      } else {
        debugPrint('[ECDH] ESP32 confirmed unpaired');
      }
      
      // Remove local pairing data regardless of ESP32 response
      final pairingService = PairingService();
      await pairingService.removePairing();
      debugPrint('[ECDH] Local pairing data removed');
      
    } catch (e) {
      debugPrint('[ECDH] Unpair error: $e');
      // Still try to remove local data even if ESP32 command fails
      final pairingService = PairingService();
      await pairingService.removePairing();
      rethrow;
    }
  }
  
  /// Authenticate using ECDH challenge-response
  Future<bool> authenticateWithEcdh(EcdhService ecdh) async {
    try {
      debugPrint('[ECDH Auth] Requesting challenge...');
      
      // 1. Request challenge
      final response = await sendCommand(Esp32Commands.ecdhAuth);
      
      // 2. Parse challenge response
      // Format: "CHALLENGE <32-char-hex>"
      if (!response.startsWith(Esp32Commands.challengePrefix)) {
        debugPrint('[ECDH Auth] Expected challenge, got: $response');
        return false;
      }
      
      final challengeHex = response.substring(Esp32Commands.challengePrefix.length).trim();
      debugPrint('[ECDH Auth] Received challenge: $challengeHex');
      
      // 3. Convert challenge from hex to bytes
      final challengeBytes = Uint8List.fromList(hex.decode(challengeHex));
      
      // 4. Compute HMAC response
      final hmacBytes = ecdh.computeHmac(challengeBytes);
      final hmacHex = hex.encode(hmacBytes);
      debugPrint('[ECDH Auth] Computed HMAC: $hmacHex');
      
      // 5. Send response
      final authResult = await sendCommand(Esp32Commands.respond(hmacHex));
      
      // 6. Check result: AUTH OK or AUTH FAIL
      if (authResult == Esp32Commands.authOk) {
        debugPrint('[ECDH Auth] Authentication successful!');
        return true;
      } else {
        debugPrint('[ECDH Auth] Authentication failed: $authResult');
        return false;
      }
      
    } catch (e) {
      debugPrint('[ECDH Auth] Error: $e');
      return false;
    }
  }
}
