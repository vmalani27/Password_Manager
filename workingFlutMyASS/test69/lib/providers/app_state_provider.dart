/// AppStateProvider - Orchestrates all services and manages app-wide state
/// 
/// This provider is the single source of truth for the application state.
/// It coordinates BLE connection, authentication, and credential operations.
/// 
/// Usage in UI:
/// ```dart
/// final appState = ref.watch(appStateProvider);
/// if (appState.connectionState == ConnectionState.authenticated) {
///   // Show credential manager
/// }
/// ```

import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import '../models/connection_state.dart';
import '../models/auth_state.dart';
import '../models/credential.dart';
import '../services/ble_connection_service.dart';
import '../services/credential_service.dart';
import '../services/command_service.dart';

/// Application state that combines all service states
class AppState {
  final ConnectionState connectionState;
  final AuthState authState;
  final String? connectedDeviceId;
  final String? connectedDeviceName;
  final String? errorMessage;
  final List<Credential> credentials;
  final bool isLoading;

  const AppState({
    this.connectionState = ConnectionState.disconnected,
    this.authState = AuthState.unauthenticated,
    this.connectedDeviceId,
    this.connectedDeviceName,
    this.errorMessage,
    this.credentials = const [],
    this.isLoading = false,
  });

  /// Convenience getter: are we fully connected and authenticated?
  bool get isReady => 
    connectionState == ConnectionState.authenticated &&
    authState == AuthState.authenticated;

  AppState copyWith({
    ConnectionState? connectionState,
    AuthState? authState,
    String? connectedDeviceId,
    String? connectedDeviceName,
    String? errorMessage,
    List<Credential>? credentials,
    bool? isLoading,
    bool clearError = false,
  }) {
    return AppState(
      connectionState: connectionState ?? this.connectionState,
      authState: authState ?? this.authState,
      connectedDeviceId: connectedDeviceId ?? this.connectedDeviceId,
      connectedDeviceName: connectedDeviceName ?? this.connectedDeviceName,
      errorMessage: clearError ? null : (errorMessage ?? this.errorMessage),
      credentials: credentials ?? this.credentials,
      isLoading: isLoading ?? this.isLoading,
    );
  }

  @override
  String toString() => 'AppState(conn: $connectionState, auth: $authState, '
      'device: $connectedDeviceName, credentials: ${credentials.length})';
}

/// Provider for CommandService (needed by other services)
final commandServiceProvider = Provider.autoDispose<CommandService>((ref) {
  final service = CommandService();
  ref.onDispose(() => service.dispose());
  return service;
});

/// Provider for BleConnectionService
final bleConnectionServiceProvider = Provider.autoDispose<BleConnectionService>((ref) {
  final commandService = ref.watch(commandServiceProvider);
  final service = BleConnectionService(commandService);
  ref.onDispose(() => service.dispose());
  return service;
});

/// Provider for CredentialService
final credentialServiceProvider = Provider.autoDispose<CredentialService>((ref) {
  final commandService = ref.watch(commandServiceProvider);
  return CredentialService(commandService);
});

/// Main application state provider
final appStateProvider = StateNotifierProvider<AppStateNotifier, AppState>((ref) {
  final commandService = ref.watch(commandServiceProvider);
  final bleService = ref.watch(bleConnectionServiceProvider);
  final credentialService = ref.watch(credentialServiceProvider);
  
  return AppStateNotifier(
    commandService: commandService,
    bleService: bleService,
    credentialService: credentialService,
  );
});

/// State notifier that manages the entire application flow
class AppStateNotifier extends StateNotifier<AppState> {
  final CommandService commandService;
  final BleConnectionService bleService;
  final CredentialService credentialService;

  AppStateNotifier({
    required this.commandService,
    required this.bleService,
    required this.credentialService,
  }) : super(const AppState());

  /// Scan for ESP32 devices
  /// Returns list of discovered device IDs
  Future<List<DiscoveredDevice>> scanForDevices({Duration timeout = const Duration(seconds: 10)}) async {
    try {
      state = state.copyWith(isLoading: true, clearError: true);
      
      final devices = await bleService.scanForDevices(timeout: timeout);
      
      state = state.copyWith(isLoading: false);
      return devices;
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        errorMessage: 'Scan failed: $e',
      );
      rethrow;
    }
  }

  /// Complete connection and authentication flow (with device pairing support)
  /// 
  /// Steps:
  /// 1. Connect to BLE device (triggers OS pairing if not bonded)
  /// 2. Subscribe to notifications
  /// 3. Perform ECDH key exchange (loads saved keys for reconnection or generates new for first pairing)
  /// 4. Authenticate with ECDH challenge-response
  /// 5. Update state to ready
  /// 
  /// Returns true if this is a new pairing (vs reconnection)
  Future<bool> connectAndAuthenticate({
    required String deviceId,
    required String deviceName,
    required String pin,
  }) async {
    debugPrint('[AppState] ========================================');
    debugPrint('[AppState] STARTING CONNECTION FLOW');
    debugPrint('[AppState] Device: $deviceName');
    debugPrint('[AppState] ID: $deviceId');
    debugPrint('[AppState] ========================================');
    
    try {
      // Step 1: Connect to device
      debugPrint('[AppState] STEP 1: Initiating BLE connection...');
      state = state.copyWith(
        connectionState: ConnectionState.connecting,
        connectedDeviceId: deviceId,
        connectedDeviceName: deviceName,
        clearError: true,
      );
      
      await bleService.connect(deviceId);
      debugPrint('[AppState] STEP 1: BLE connection established ✓');
      
      state = state.copyWith(
        connectionState: ConnectionState.connected,
      );
      
      // Step 2: Subscribe to notifications (with retry logic for bonding)
      debugPrint('[AppState] STEP 2: Waiting for bonding dialog (800ms)...');
      await Future.delayed(const Duration(milliseconds: 800));
      
      debugPrint('[AppState] STEP 2: Starting notification subscription...');
      await bleService.subscribeToNotifications(deviceId);
      debugPrint('[AppState] STEP 2: Notification subscription successful ✓');
      
      // Give ESP32 time to be ready for commands and ensure notification stream is stable
      debugPrint('[AppState] Waiting for ESP32 stabilization (800ms)...');
      await Future.delayed(const Duration(milliseconds: 800));
      debugPrint('[AppState] ESP32 ready for commands ✓');
      
      // Step 3: Perform ECDH key exchange (with pairing support)
      debugPrint('[AppState] STEP 3: Starting ECDH handshake...');
      state = state.copyWith(
        authState: AuthState.requestingToken,
        connectionState: ConnectionState.authenticating,
      );
      
      final result = await commandService.performEcdhHandshake(deviceId, deviceName);
      final isNewPairing = result.isNewPairing;
      debugPrint('[AppState] STEP 3: ECDH handshake complete ✓ (${isNewPairing ? "NEW PAIRING" : "RECONNECTION"})');
      
      // Step 4: Authenticate session with token (separate from ECDH pairing)
      debugPrint('[AppState] STEP 4: Starting token authentication...');
      final authenticated = await commandService.authenticateWithToken();
      
      if (!authenticated) {
        throw Exception('Session authentication failed');
      }
      
      debugPrint('[AppState] STEP 4: Token authentication complete ✓');
      
      // Step 5: Update to ready state
      state = state.copyWith(
        connectionState: ConnectionState.authenticated,
        authState: AuthState.authenticated,
      );
      
      // Load initial credentials
      debugPrint('[AppState] STEP 5: Loading credentials...');
      await refreshCredentials();
      
      debugPrint('[AppState] ========================================');
      debugPrint('[AppState] CONNECTION FLOW COMPLETE ✓');
      debugPrint('[AppState] Ready for operations');
      debugPrint('[AppState] ========================================');
      
      return isNewPairing;
      
    } catch (e, stackTrace) {
      debugPrint('[AppState] ========================================');
      debugPrint('[AppState] CONNECTION FLOW FAILED ✗');
      debugPrint('[AppState] Error: $e');
      debugPrint('[AppState] Stack trace:');
      debugPrint('$stackTrace');
      debugPrint('[AppState] ========================================');
      
      // Clean up on error
      debugPrint('[AppState] Cleaning up failed connection...');
      await disconnect();
      
      state = state.copyWith(
        connectionState: ConnectionState.disconnected,
        authState: AuthState.unauthenticated,
        errorMessage: 'Connection failed: $e',
        connectedDeviceId: null,
        connectedDeviceName: null,
      );
      
      rethrow;
    }
  }

  /// Disconnect from ESP32 and reset state
  Future<void> disconnect() async {
    try {
      final deviceId = state.connectedDeviceId;
      if (deviceId != null) {
        await bleService.disconnect(deviceId);
      }
      
      state = const AppState(); // Reset to initial state
      
      debugPrint('[AppState] Disconnected and state reset');
    } catch (e) {
      debugPrint('[AppState] Disconnect error: $e');
      // Still reset state even if disconnect fails
      state = const AppState();
    }
  }
  
  /// Unpair from the current device
  /// This will disconnect, send unpair command to ESP32, and remove local pairing data
  Future<void> unpairDevice() async {
    try {
      debugPrint('[AppState] Unpairing device...');
      
      // Disconnect first (if connected)
      if (state.connectedDeviceId != null) {
        await disconnect();
      }
      
      // Send unpair command and remove local data
      await commandService.unpairDevice();
      
      state = state.copyWith(
        clearError: true,
      );
      
      debugPrint('[AppState] Device unpaired successfully');
      
    } catch (e) {
      debugPrint('[AppState] Unpair error: $e');
      state = state.copyWith(
        errorMessage: 'Failed to unpair: $e',
      );
      rethrow;
    }
  }

  /// Refresh the list of credentials from ESP32
  Future<void> refreshCredentials() async {
    final deviceId = state.connectedDeviceId;
    if (deviceId == null || !state.isReady) {
      debugPrint('[AppState] Cannot refresh credentials - not connected/authenticated');
      return;
    }

    try {
      state = state.copyWith(isLoading: true, clearError: true);
      
      final credentials = await credentialService.listCredentials(deviceId);
      
      state = state.copyWith(
        credentials: credentials,
        isLoading: false,
      );
      
      debugPrint('[AppState] Refreshed ${credentials.length} credentials');
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        errorMessage: 'Failed to load credentials: $e',
      );
      rethrow;
    }
  }

  /// Add a new credential
  Future<void> addCredential({
    required String site,
    required String username,
    required String password,
  }) async {
    final deviceId = state.connectedDeviceId;
    if (deviceId == null || !state.isReady) {
      throw Exception('Not connected or authenticated');
    }

    try {
      state = state.copyWith(isLoading: true, clearError: true);
      
      await credentialService.addCredential(
        deviceId: deviceId,
        site: site,
        username: username,
        password: password,
      );
      
      // Refresh list after adding
      await refreshCredentials();
      
      debugPrint('[AppState] Added credential for $site');
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        errorMessage: 'Failed to add credential: $e',
      );
      rethrow;
    }
  }

  /// Get password for a specific credential
  Future<String> getPassword({
    required String site,
    required String username,
  }) async {
    final deviceId = state.connectedDeviceId;
    if (deviceId == null || !state.isReady) {
      throw Exception('Not connected or authenticated');
    }

    try {
      state = state.copyWith(isLoading: true, clearError: true);
      
      final password = await credentialService.getPassword(
        deviceId: deviceId,
        site: site,
        username: username,
      );
      
      state = state.copyWith(isLoading: false);
      
      if (password == null) {
        throw Exception('Password not found');
      }
      
      debugPrint('[AppState] Retrieved password for $site / $username');
      return password;
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        errorMessage: 'Failed to get password: $e',
      );
      rethrow;
    }
  }

  /// Update an existing credential's password
  Future<void> updateCredential({
    required String site,
    required String username,
    required String newPassword,
  }) async {
    final deviceId = state.connectedDeviceId;
    if (deviceId == null || !state.isReady) {
      throw Exception('Not connected or authenticated');
    }

    try {
      state = state.copyWith(isLoading: true, clearError: true);
      
      await credentialService.updateCredential(
        deviceId: deviceId,
        site: site,
        username: username,
        newPassword: newPassword,
      );
      
      // Refresh list after updating
      await refreshCredentials();
      
      debugPrint('[AppState] Updated credential for $site / $username');
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        errorMessage: 'Failed to update credential: $e',
      );
      rethrow;
    }
  }

  /// Delete a credential
  Future<void> deleteCredential({
    required String site,
    required String username,
  }) async {
    final deviceId = state.connectedDeviceId;
    if (deviceId == null || !state.isReady) {
      throw Exception('Not connected or authenticated');
    }

    try {
      state = state.copyWith(isLoading: true, clearError: true);
      
      await credentialService.deleteCredential(
        deviceId: deviceId,
        site: site,
        username: username,
      );
      
      // Refresh list after deleting
      await refreshCredentials();
      
      debugPrint('[AppState] Deleted credential for $site / $username');
    } catch (e) {
      state = state.copyWith(
        isLoading: false,
        errorMessage: 'Failed to delete credential: $e',
      );
      rethrow;
    }
  }

  /// Clear any error message
  void clearError() {
    state = state.copyWith(clearError: true);
  }
}
