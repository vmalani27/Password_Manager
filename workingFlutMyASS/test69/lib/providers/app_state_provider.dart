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
import '../services/auth_service.dart';
import '../services/credential_service.dart';
import '../services/session_manager.dart';
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
final commandServiceProvider = Provider<CommandService>((ref) {
  return CommandService();
});

/// Provider for BleConnectionService
final bleConnectionServiceProvider = Provider<BleConnectionService>((ref) {
  final commandService = ref.watch(commandServiceProvider);
  return BleConnectionService(commandService);
});

/// Provider for AuthService
final authServiceProvider = Provider<AuthService>((ref) {
  final commandService = ref.watch(commandServiceProvider);
  return AuthService(commandService);
});

/// Provider for CredentialService
final credentialServiceProvider = Provider<CredentialService>((ref) {
  final commandService = ref.watch(commandServiceProvider);
  return CredentialService(commandService);
});

/// Provider for SessionManager
final sessionManagerProvider = Provider<SessionManager>((ref) {
  return SessionManager();
});

/// Main application state provider
final appStateProvider = StateNotifierProvider<AppStateNotifier, AppState>((ref) {
  final bleService = ref.watch(bleConnectionServiceProvider);
  final authService = ref.watch(authServiceProvider);
  final credentialService = ref.watch(credentialServiceProvider);
  final sessionManager = ref.watch(sessionManagerProvider);
  
  return AppStateNotifier(
    bleService: bleService,
    authService: authService,
    credentialService: credentialService,
    sessionManager: sessionManager,
  );
});

/// State notifier that manages the entire application flow
class AppStateNotifier extends StateNotifier<AppState> {
  final BleConnectionService bleService;
  final AuthService authService;
  final CredentialService credentialService;
  final SessionManager sessionManager;

  AppStateNotifier({
    required this.bleService,
    required this.authService,
    required this.credentialService,
    required this.sessionManager,
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

  /// Complete connection and authentication flow
  /// 
  /// Steps:
  /// 1. Connect to BLE device (triggers OS pairing if not bonded)
  /// 2. Subscribe to notifications
  /// 3. Request session token from ESP32
  /// 4. Authenticate with PIN
  /// 5. Update state to ready
  Future<void> connectAndAuthenticate({
    required String deviceId,
    required String deviceName,
    required String pin,
  }) async {
    try {
      debugPrint('[AppState] Starting connection flow for $deviceName ($deviceId)');
      
      // Step 1: Connect to device
      state = state.copyWith(
        connectionState: ConnectionState.connecting,
        connectedDeviceId: deviceId,
        connectedDeviceName: deviceName,
        clearError: true,
      );
      
      await bleService.connect(deviceId);
      debugPrint('[AppState] BLE connected (OS bonding handled automatically)');
      
      // Update session manager
      sessionManager.setConnectionState(ConnectionState.connected);
      
      state = state.copyWith(
        connectionState: ConnectionState.connected,
      );
      
      // Step 2: Subscribe to notifications (with retry logic for bonding)
      // Brief pause to allow bonding dialog to appear if needed
      debugPrint('[AppState] Starting service discovery with retry logic...');
      await Future.delayed(const Duration(milliseconds: 800));
      
      await bleService.subscribeToNotifications(deviceId);
      debugPrint('[AppState] Subscribed to notifications');
      
      // Give ESP32 time to be ready for commands
      debugPrint('[AppState] Waiting for ESP32 to be ready...');
      await Future.delayed(const Duration(milliseconds: 500));
      debugPrint('[AppState] Ready to send commands');
      
      // Step 3 & 4: Authenticate (request token + auth)
      state = state.copyWith(
        authState: AuthState.requestingToken,
        connectionState: ConnectionState.authenticating,
      );
      
      await authService.performAuthentication(deviceId, pin);
      debugPrint('[AppState] Authentication successful');
      
      // Update session manager
      sessionManager.setAuthState(AuthState.authenticated);
      sessionManager.setConnectionState(ConnectionState.authenticated);
      
      // Step 5: Update to ready state
      state = state.copyWith(
        connectionState: ConnectionState.authenticated,
        authState: AuthState.authenticated,
      );
      
      // Load initial credentials
      await refreshCredentials();
      
      debugPrint('[AppState] Connection flow complete - ready for operations');
      
    } catch (e) {
      debugPrint('[AppState] Connection flow failed: $e');
      
      // Clean up on error
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
      
      // Reset session manager
      sessionManager.setConnectionState(ConnectionState.disconnected);
      sessionManager.setAuthState(AuthState.unauthenticated);
      
      state = const AppState(); // Reset to initial state
      
      debugPrint('[AppState] Disconnected and state reset');
    } catch (e) {
      debugPrint('[AppState] Disconnect error: $e');
      // Still reset state even if disconnect fails
      state = const AppState();
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
