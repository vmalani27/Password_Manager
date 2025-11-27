import 'dart:async';
import 'dart:typed_data';
import 'package:flutter/foundation.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import '../constants/ble_constants.dart';
import 'ecdh_service.dart';
import 'pairing_service.dart';
import '../models/pairing_device.dart';
import 'session_crypto.dart';

/// Low-level service for sending commands and receiving responses from ESP32
/// Handles BLE characteristic read/write operations
class CommandService {
  final FlutterReactiveBle _ble = FlutterReactiveBle();
  
  String? _connectedDeviceId;
  StreamSubscription<List<int>>? _notificationSubscription;
  final _responseController = StreamController<String>.broadcast();
  
  // Session management
  String? _sessionId;
  SessionCrypto? _sessionCrypto;
  
  /// Stream of all responses received from ESP32 notification characteristic
  Stream<String> get responseStream => _responseController.stream;
  
  /// Check if we have an active connection
  bool get isConnected => _connectedDeviceId != null;
  
  /// Get current session ID if authenticated
  String? get sessionId => _sessionId;
  
  /// Check if session encryption is active
  bool get isEncryptionActive => _sessionCrypto != null;
  
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
    debugPrint('[CommandService] subscribeToNotifications called for $deviceId');
    debugPrint('[CommandService] Current subscription status: ${_notificationSubscription != null ? "EXISTS" : "NULL"}');
    debugPrint('[CommandService] Current device: $_connectedDeviceId');
    
    // Always cleanup old subscription to prevent stale subscriptions
    if (_notificationSubscription != null) {
      debugPrint('[CommandService] Cleaning up old subscription...');
      await _notificationSubscription?.cancel();
      _notificationSubscription = null;
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
            String response = String.fromCharCodes(data).trim();
            debugPrint('[CommandService] ← Received: "$response"');
            
            // Decrypt response if encrypted
            if (response.startsWith('ENC:')) {
              debugPrint('[CommandService] Encrypted notification detected');
              if (_sessionCrypto != null) {
                try {
                  final decrypted = _sessionCrypto!.decryptResponse(response);
                  debugPrint('[CommandService] ✓ Decrypted: "$decrypted" (${decrypted.length} bytes)');
                  response = decrypted;
                } catch (e) {
                  debugPrint('[CommandService] ✗ Decryption failed: $e');
                  debugPrint('[CommandService] ✗ Keeping encrypted response: $response');
                  // Keep the encrypted response for error handling
                }
              } else {
                debugPrint('[CommandService] ⚠ WARNING: Received encrypted response but _sessionCrypto is NULL!');
                debugPrint('[CommandService] ⚠ Response: $response');
                debugPrint('[CommandService] ⚠ This indicates encryption not initialized yet');
                // Keep encrypted response - will fail later with clear error
              }
            }
            
            // Check for SESSION_TIMEOUT notification
            if (response == Esp32Commands.sessionTimeout) {
              debugPrint('[CommandService] !!! SESSION TIMEOUT DETECTED !!!');
              debugPrint('[CommandService] Session expired - need to re-authenticate');
              // The response will be broadcast to listeners who can handle it
            }
            
            // Never log actual passwords
            if (response.startsWith('Password:')) {
              debugPrint('[CommandService] Password retrieved successfully (hidden from logs)');
            } else {
              debugPrint('[CommandService] Decrypted response: $response');
            }
            
            // Broadcast decrypted response to stream
            _responseController.add(response);
            debugPrint('[CommandService] Broadcast to ${_responseController.hasListener ? "ACTIVE" : "NO"} listeners');
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
    // Commands that should ALWAYS be plaintext (even after ECDH)
    final alwaysPlaintextCommands = [
      'ecdh',           // ECDH must be plaintext to establish encryption
      'status',         // Optional: could encrypt, but not critical
    ];
    final commandName = command.split(' ').first.toLowerCase();
    final shouldEncrypt = _sessionCrypto != null && !alwaysPlaintextCommands.contains(commandName);
    String commandToSend = command;
    if (shouldEncrypt) {
      commandToSend = _sessionCrypto!.encryptCommand(command);
      debugPrint('[CommandService] Sending encrypted command: ${commandName.toUpperCase()}');
    } else {
      debugPrint('[CommandService] Sending plaintext command: $commandName');
    }
    
    debugPrint('[CommandService] → Sending: $commandToSend');
    
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
        value: commandToSend.codeUnits,
      );
      
      debugPrint('[CommandService] Write successful, waiting for response...');
      
      if (!waitForResponse) {
        return '';
      }
      
      // Wait for ESP32 response via notification
      // ESP32 code: sendNotification(response);
      // Note: Response is already decrypted by notification handler
      final response = await responseFuture;
      
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
    _sessionId = null;
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
    debugPrint('[ECDH] ========================================');
    debugPrint('[ECDH] STARTING HANDSHAKE');
    debugPrint('[ECDH] Device: $deviceName ($deviceId)');
    debugPrint('[ECDH] ========================================');
    
    // Verify notification subscription is active
    if (_notificationSubscription == null) {
      throw Exception('Cannot perform ECDH: notification subscription not active. Call subscribeToNotifications first.');
    }
    
    debugPrint('[ECDH] Notification subscription status: ACTIVE');
    debugPrint('[ECDH] Response stream has listeners: ${_responseController.hasListener}');
    
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
      
      // CRITICAL: Initialize session encryption BEFORE sending public key
      // ESP32 will respond with encrypted ECDH_OK/ECDH_OK_PAIRED
      // We need to be able to decrypt the response
      if (ecdh.sessionKey != null) {
        _sessionCrypto = SessionCrypto(ecdh.sessionKey!);
        debugPrint('[ECDH] ✓ Session encryption initialized with new session key (${ecdh.sessionKey!.length} bytes)');
        debugPrint('[ECDH] ✓ Encryption active: ${_sessionCrypto != null}');
        debugPrint('[ECDH] ✓ Ready to decrypt ESP32 encrypted response');
      } else {
        debugPrint('[ECDH] ✗ CRITICAL: Session key is NULL after computing shared secret!');
        throw Exception('Session key not available after computing shared secret');
      }
      
      // Start listening for ECDH response BEFORE sending public key
      debugPrint('[ECDH] Setting up listener for ECDH response...');
      debugPrint('[ECDH] Response stream has listeners: ${_responseController.hasListener}');
      
      final completer = Completer<String>();
      late StreamSubscription<String> subscription;
      
      // Buffer for responses that arrive before we're fully ready
      String? bufferedResponse;
      
      subscription = responseStream.listen((response) {
        debugPrint('[ECDH] Received notification: $response');
        debugPrint('[ECDH] Encryption status at receive: ${_sessionCrypto != null ? "ACTIVE" : "NULL"}');
        if (!completer.isCompleted) {
          completer.complete(response);
          subscription.cancel();
        } else {
          bufferedResponse = response;
        }
      });
      
      debugPrint('[ECDH] Listener attached, subscription active: ${!subscription.isPaused}');
      
      // Give stream listener time to attach (prevent race condition)
      await Future.delayed(const Duration(milliseconds: 100));
      
      // Set timeout (increased to 10 seconds for reliability)
      Timer? timeoutTimer;
      timeoutTimer = Timer(const Duration(seconds: 10), () {
        if (!completer.isCompleted) {
          subscription.cancel();
          debugPrint('[ECDH] TIMEOUT! No response received. Subscription active: ${!subscription.isPaused}');
          completer.completeError(
            TimeoutException(
              'No ECDH response from ESP32 after 10 seconds. '
              'ESP32 sent response but Flutter did not receive it. '
              'Check notification characteristic subscription.'
            )
          );
        }
      });
      
      // Send our public key to ESP32 using writeWithResponse for acknowledgment
      debugPrint('[ECDH] Sending client public key (${clientPubKey.length} bytes)...');
      try {
        await _ble.writeCharacteristicWithResponse(
          QualifiedCharacteristic(
            serviceId: Uuid.parse(BleConstants.serviceUuid),
            characteristicId: Uuid.parse(BleConstants.ecdhCharacteristicUuid),
            deviceId: deviceId,
          ),
          value: clientPubKey,
        );
        debugPrint('[ECDH] Write acknowledged by ESP32');
      } catch (e) {
        debugPrint('[ECDH] Write failed: $e');
        timeoutTimer.cancel();
        subscription.cancel();
        rethrow;
      }
      
      // Give ESP32 time to process and send notification
      debugPrint('[ECDH] Waiting 300ms for ESP32 to process and send notification...');
      await Future.delayed(const Duration(milliseconds: 300));
      
      debugPrint('[ECDH] Write complete, waiting for ECDH response via notification characteristic...');
      debugPrint('[ECDH] Completer status: ${completer.isCompleted ? "COMPLETED" : "WAITING"}');
      debugPrint('[ECDH] Subscription paused: ${subscription.isPaused}');
      debugPrint('[ECDH] Notification subscription active: ${_notificationSubscription != null}');
      
      // Wait for ECDH response
      final response = await completer.future;
      timeoutTimer.cancel(); // Cancel timeout if we got response
      
      // Check buffered response in case we missed it
      final actualResponse = response.isEmpty && bufferedResponse != null 
          ? bufferedResponse! 
          : response;
      
      debugPrint('[ECDH] Final response: $actualResponse');
      
      // Handle different response types
      if (actualResponse == Esp32Commands.ecdhOk) {
        // Reconnection successful
        debugPrint('[ECDH] \u2713 Received ECDH_OK - reconnection successful!');
        debugPrint('[ECDH] ========================================');
        debugPrint('[ECDH] HANDSHAKE COMPLETE (RECONNECTION)');
        debugPrint('[ECDH] SESSION ENCRYPTION ACTIVE');
        debugPrint('[ECDH] ========================================');
        return (ecdh: ecdh, isNewPairing: false);
        
      } else if (actualResponse == Esp32Commands.ecdhOkPaired) {
        // New pairing successful - save pairing data
        debugPrint('[ECDH] \u2713 Received ECDH_OK_PAIRED - new pairing established!');
        
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
        debugPrint('[ECDH] ========================================');
        debugPrint('[ECDH] HANDSHAKE COMPLETE (NEW PAIRING)');
        debugPrint('[ECDH] SESSION ENCRYPTION ACTIVE');
        debugPrint('[ECDH] ========================================');
        return (ecdh: ecdh, isNewPairing: true);
        
      } else if (actualResponse == Esp32Commands.ecdhAlreadyPaired) {
        // ESP32 is paired with a different device
        debugPrint('[ECDH] \u2717 ESP32 already paired to different device');
        throw Exception(
          'ESP32 is already paired with a different device. '
          'Unpair the ESP32 first using the device button.'
        );
        
      } else {
        debugPrint('[ECDH] \u2717 Unexpected response: $actualResponse');
        throw Exception('Unexpected ECDH response: $actualResponse');
      }
      
    } catch (e, stackTrace) {
      debugPrint('[ECDH] ========================================');
      debugPrint('[ECDH] HANDSHAKE FAILED');
      debugPrint('[ECDH] Error: $e');
      debugPrint('[ECDH] Stack trace: $stackTrace');
      debugPrint('[ECDH] ========================================');
      ecdh.clear();
      rethrow;
    }
  }
  
  /// Unpair from the current device
  /// Sends unpair command to ESP32 and removes local pairing data
  Future<void> unpairDevice() async {
    try {
      debugPrint('[ECDH] Sending unpair command...');
      
      // Don't wait for response - ESP32 will disconnect immediately
      // The connection drop is the confirmation
      await sendCommand(
        Esp32Commands.unpair,
        waitForResponse: false, // Don't wait - connection will drop
      );
      
      debugPrint('[ECDH] Unpair command sent (connection will drop)');
      
      // Give ESP32 a moment to process before we clean up locally
      await Future.delayed(const Duration(milliseconds: 500));
      
      // Remove local pairing data - this is the critical part
      final pairingService = PairingService();
      await pairingService.removePairing();
      debugPrint('[ECDH] Local pairing data removed successfully');
      
    } catch (e) {
      debugPrint('[ECDH] Unpair error: $e');
      // Still try to remove local data even if ESP32 command fails
      final pairingService = PairingService();
      try {
        await pairingService.removePairing();
        debugPrint('[ECDH] Local pairing data removed (despite error)');
      } catch (e2) {
        debugPrint('[ECDH] Failed to remove local pairing data: $e2');
      }
      rethrow;
    }
  }
  
  /// Optimized reconnection: Check pairing first, skip ECDH if valid
  /// Returns ecdh service, whether it's new pairing, and whether ECDH was skipped
  Future<({EcdhService? ecdh, bool isNewPairing, bool skippedEcdh})> performOptimizedReconnection(
    String deviceId,
    String deviceName,
  ) async {
    debugPrint('[ECDH] ========================================');
    debugPrint('[ECDH] STARTING OPTIMIZED RECONNECTION');
    debugPrint('[ECDH] ========================================');
    
    final pairingService = PairingService();
    
    try {
      // Step 1: Check if we have local pairing data
      final pairedDevice = await pairingService.getPairedDevice();
      final isAlreadyPaired = pairedDevice?.deviceId == deviceId;
      
      if (!isAlreadyPaired || pairedDevice == null) {
        debugPrint('[ECDH] No local pairing found - doing full ECDH handshake');
        final result = await performEcdhHandshake(deviceId, deviceName);
        return (ecdh: result.ecdh, isNewPairing: result.isNewPairing, skippedEcdh: false);
      }
      
      debugPrint('[ECDH] Local pairing exists - checking ESP32 pairing status...');
      
      // Step 2: Send check_pairing command to ESP32
      debugPrint('[ECDH] Sending check_pairing command to ESP32...');
      final response = await sendCommand(
        Esp32Commands.checkPairing,
        timeout: const Duration(seconds: 5),
      );
      
      debugPrint('[ECDH] ESP32 pairing status response: "$response"');
      debugPrint('[ECDH] Response starts with PAIRED: ${response.startsWith(Esp32Commands.pairedPrefix)}');
      debugPrint('[ECDH] Response equals UNPAIRED: ${response == Esp32Commands.unpaired}');
      
      if (response.startsWith(Esp32Commands.pairedPrefix)) {
        // Step 3: Extract stored client public key from ESP32
        final storedKeyHex = response.substring(Esp32Commands.pairedPrefix.length).trim();
        debugPrint('[ECDH] ESP32 stored key (first 20 chars): ${storedKeyHex.substring(0, 20)}...');
        
        // Step 4: Compare with our saved public key
        final ourPublicKeyHex = _bytesToHex(pairedDevice.clientPublicKey).toUpperCase();
        debugPrint('[ECDH] Our saved key (first 20 chars): ${ourPublicKeyHex.substring(0, 20)}...');
        
        if (storedKeyHex.toUpperCase() == ourPublicKeyHex) {
          // Keys match! But we still need to send our public key to ESP32
          // so it can also derive the session key for encryption
          debugPrint('[ECDH] Pairing verified! Keys match - but need to trigger ESP32 session key derivation');
          debugPrint('[ECDH] Sending our public key to ESP32 to establish session encryption...');
          
          // Create ECDH service and load saved keys
          final ecdh = EcdhService();
          ecdh.loadKeyPair(pairedDevice.clientPrivateKey, pairedDevice.clientPublicKey);
          ecdh.computeSharedSecret(pairedDevice.esp32PublicKey);
          
          // CRITICAL: Initialize session encryption BEFORE sending public key
          // ESP32 will respond with encrypted ECDH_OK, so we need to be able to decrypt it
          if (ecdh.sessionKey != null) {
            _sessionCrypto = SessionCrypto(ecdh.sessionKey!);
            debugPrint('[ECDH] Session encryption initialized (${ecdh.sessionKey!.length} bytes)');
          } else {
            throw Exception('Session key not available after computing shared secret');
          }
          
          // Now send our public key to ESP32 so it can also derive session key
          // This triggers ESP32 to call crypto.deriveSessionKey()
          debugPrint('[ECDH] Writing public key to ECDH characteristic...');
          await _ble.writeCharacteristicWithResponse(
            QualifiedCharacteristic(
              serviceId: Uuid.parse(BleConstants.serviceUuid),
              characteristicId: Uuid.parse(BleConstants.ecdhCharacteristicUuid),
              deviceId: deviceId,
            ),
            value: pairedDevice.clientPublicKey,
          );
          
          // Wait for ESP32 to process and send encrypted ECDH_OK
          debugPrint('[ECDH] Waiting for ECDH_OK from ESP32 (will be encrypted)...');
          final ecdhResponse = await responseStream.first.timeout(
            const Duration(seconds: 5),
            onTimeout: () => throw TimeoutException('No ECDH response from ESP32'),
          );
          
          debugPrint('[ECDH] ESP32 response (decrypted): $ecdhResponse');
          
          if (ecdhResponse != Esp32Commands.ecdhOk && ecdhResponse != Esp32Commands.ecdhOkPaired) {
            throw Exception('Unexpected ECDH response: $ecdhResponse');
          }
          
          debugPrint('[ECDH] ========================================');
          debugPrint('[ECDH] OPTIMIZED RECONNECTION COMPLETE');
          debugPrint('[ECDH] SESSION ENCRYPTION ACTIVE (both sides)');
          debugPrint('[ECDH] Time saved: ~1 second (skipped key generation)');
          debugPrint('[ECDH] ========================================');
          
          return (ecdh: ecdh, isNewPairing: false, skippedEcdh: true);
          
        } else {
          // Keys don't match - pairing corrupted
          debugPrint('[ECDH] Pairing mismatch detected!');
          debugPrint('[ECDH] ESP32 key != Our saved key');
          debugPrint('[ECDH] Falling back to full ECDH handshake...');
        }
        
      } else if (response == Esp32Commands.unpaired) {
        debugPrint('[ECDH] ESP32 reports UNPAIRED - doing full handshake');
      } else {
        debugPrint('[ECDH] Unexpected response: $response - doing full handshake');
      }
      
      // Step 5: Fallback to full ECDH handshake
      debugPrint('[ECDH] Performing full ECDH handshake...');
      final result = await performEcdhHandshake(deviceId, deviceName);
      return (ecdh: result.ecdh, isNewPairing: result.isNewPairing, skippedEcdh: false);
      
    } catch (e, stackTrace) {
      debugPrint('[ECDH] ========================================');
      debugPrint('[ECDH] OPTIMIZED RECONNECTION FAILED');
      debugPrint('[ECDH] Error: $e');
      debugPrint('[ECDH] Stack trace: $stackTrace');
      debugPrint('[ECDH] ========================================');
      
      // On error, try full ECDH handshake
      debugPrint('[ECDH] Falling back to full ECDH handshake...');
      try {
        final result = await performEcdhHandshake(deviceId, deviceName);
        return (ecdh: result.ecdh, isNewPairing: result.isNewPairing, skippedEcdh: false);
      } catch (e2) {
        rethrow;
      }
    }
  }
  
  /// Helper to convert bytes to hex string
  String _bytesToHex(Uint8List bytes) {
    return bytes.map((b) => b.toRadixString(16).padLeft(2, '0')).join('');
  }
  
  /// Authenticate using standard token-based session authentication
  /// This is separate from ECDH pairing - ECDH handles device binding,
  /// token auth handles session authorization for commands
  /// 
  /// NEW: Returns session ID for future resume capability
  Future<bool> authenticateWithToken() async {
    debugPrint('[Auth] ===== STARTING TOKEN AUTHENTICATION =====');
    
    try {
      // 1. Request token from ESP32
      debugPrint('[Auth] Step 1/3: Requesting session token...');
      final response = await sendCommand(
        Esp32Commands.requestToken,
        timeout: const Duration(seconds: 10),
      );
      debugPrint('[Auth] Step 1/3: Received response: "$response"');
      
      // 2. Parse token response
      // Format: "TOKEN:XXXXXXXX" (colon, not space)
      if (!response.startsWith(Esp32Commands.tokenPrefix)) {
        final error = 'Expected TOKEN: response, got: "$response"';
        debugPrint('[Auth] ERROR: $error');
        throw Exception(error);
      }
      
      final token = response.substring(Esp32Commands.tokenPrefix.length).trim();
      debugPrint('[Auth] Step 2/3: Extracted token: "$token"');
      
      if (token.isEmpty) {
        throw Exception('Token is empty after parsing');
      }
      
      // 3. Send auth command with token
      debugPrint('[Auth] Step 3/3: Sending auth command: "auth $token"');
      final authResult = await sendCommand(
        Esp32Commands.auth(token),
        timeout: const Duration(seconds: 10),
      );
      debugPrint('[Auth] Step 3/3: Auth response: "$authResult"');
      
      // 4. Check result: AUTH OK:<session_id> or AUTH OK (legacy) or AUTH FAIL
      if (authResult.startsWith(Esp32Commands.authOkPrefix)) {
        // New format: AUTH OK:<32-char-hex-session-id>
        _sessionId = authResult.substring(Esp32Commands.authOkPrefix.length).trim();
        debugPrint('[Auth] Session ID stored: $_sessionId');
        debugPrint('[Auth] ===== TOKEN AUTHENTICATION SUCCESS =====');
        return true;
      } else if (authResult == Esp32Commands.authOk) {
        // Legacy format: AUTH OK (no session ID)
        debugPrint('[Auth] Legacy AUTH OK response (no session ID)');
        _sessionId = null;
        debugPrint('[Auth] ===== TOKEN AUTHENTICATION SUCCESS =====');
        return true;
      } else if (authResult == Esp32Commands.authFail) {
        throw Exception('ESP32 rejected token (AUTH FAIL)');
      } else if (authResult == Esp32Commands.locked) {
        throw Exception('ESP32 is locked due to too many failed attempts');
      } else {
        throw Exception('Unexpected auth response: "$authResult"');
      }
      
    } catch (e) {
      debugPrint('[Auth] ===== TOKEN AUTHENTICATION FAILED =====');
      debugPrint('[Auth] Error: $e');
      rethrow;
    }
  }
  
  /// Check device session status without authentication
  /// Returns: STATUS CONNECTED AUTHORIZED / STATUS CONNECTED UNAUTHORIZED / STATUS NOT_CONNECTED
  Future<String> getStatus() async {
    debugPrint('[Session] Checking device status...');
    
    try {
      final response = await sendCommand(
        Esp32Commands.status,
        timeout: const Duration(seconds: 5),
      );
      
      debugPrint('[Session] Status response: "$response"');
      return response;
      
    } catch (e) {
      debugPrint('[Session] Status check failed: $e');
      rethrow;
    }
  }
  
  /// Force ESP32 to disconnect and reset session
  /// Use this instead of graceful disconnect to ensure ESP32 cleans up properly
  Future<void> forceDisconnect() async {
    debugPrint('[Session] Sending force_disconnect command...');
    
    try {
      final response = await sendCommand(
        Esp32Commands.forceDisconnect,
        timeout: const Duration(seconds: 5),
      );
      
      debugPrint('[Session] Force disconnect response: "$response"');
      
      if (response == Esp32Commands.disconnecting) {
        debugPrint('[Session] ESP32 confirmed disconnect');
      } else {
        debugPrint('[Session] Unexpected response: "$response"');
      }
      
    } catch (e) {
      debugPrint('[Session] Force disconnect failed: $e');
      // Not critical - device will timeout anyway
    }
  }
  
  /// Resume session using saved session ID
  /// Returns true if session resumed successfully, false otherwise
  Future<bool> resumeSession(String sessionId) async {
    debugPrint('[Session] ===== RESUMING SESSION =====');
    debugPrint('[Session] Session ID: $sessionId');
    
    try {
      final response = await sendCommand(
        Esp32Commands.resume(sessionId),
        timeout: const Duration(seconds: 5),
      );
      
      debugPrint('[Session] Resume response: "$response"');
      
      if (response == Esp32Commands.resumeOk) {
        _sessionId = sessionId;
        debugPrint('[Session] ===== SESSION RESUMED SUCCESSFULLY =====');
        return true;
      } else if (response == Esp32Commands.resumeFail) {
        debugPrint('[Session] Session resume failed - session ID invalid or expired');
        _sessionId = null;
        return false;
      } else {
        debugPrint('[Session] Unexpected resume response: "$response"');
        _sessionId = null;
        return false;
      }
      
    } catch (e) {
      debugPrint('[Session] ===== SESSION RESUME FAILED =====');
      debugPrint('[Session] Error: $e');
      _sessionId = null;
      return false;
    }
  }
  
  /// After ECDH handshake, initialize session encryption
  void initializeSessionCrypto(Uint8List sessionKey) {
    _sessionCrypto = SessionCrypto(sessionKey);
    debugPrint('[CommandService] Session encryption initialized');
  }
  
  /// After ECDH handshake, call this to set up session encryption
  Future<void> handleEcdhComplete(EcdhService ecdh) async {
    if (ecdh.sessionKey != null) {
      initializeSessionCrypto(ecdh.sessionKey!);
    }
  }
  
}
