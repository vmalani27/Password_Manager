import 'package:flutter/foundation.dart';
import '../models/connection_state.dart';
import '../models/auth_state.dart';

/// Manages session state across the app
/// Tracks device connection, authentication, and errors
class SessionManager {
  // Connection state
  String? _connectedDeviceId;
  String? _connectedDeviceName;
  ConnectionState _connectionState = ConnectionState.disconnected;
  
  // Authentication state
  AuthState _authState = AuthState.unauthenticated;
  String? _sessionToken;
  
  // Error tracking
  String? _lastError;
  
  /// Currently connected device ID
  String? get connectedDeviceId => _connectedDeviceId;
  
  /// Currently connected device name
  String? get connectedDeviceName => _connectedDeviceName;
  
  /// Current connection state
  ConnectionState get connectionState => _connectionState;
  
  /// Current authentication state
  AuthState get authState => _authState;
  
  /// Current session token
  String? get sessionToken => _sessionToken;
  
  /// Last error message
  String? get lastError => _lastError;
  
  /// Check if fully ready to send commands (connected AND authenticated)
  bool get isReady => 
      _connectionState == ConnectionState.authenticated && 
      _authState == AuthState.authenticated;
  
  /// Check if connected (may not be authenticated yet)
  bool get isConnected => _connectionState.isConnected;
  
  /// Update connection state
  void updateConnectionState(ConnectionState state) {
    if (_connectionState != state) {
      _connectionState = state;
      debugPrint('[Session] Connection state: ${state.displayText}');
      
      // If disconnected, reset everything
      if (state == ConnectionState.disconnected) {
        clearSession();
      }
    }
  }
  
  /// Set connection state (alias for updateConnectionState for consistency)
  void setConnectionState(ConnectionState state) {
    updateConnectionState(state);
  }
  
  /// Update authentication state
  void updateAuthState(AuthState state) {
    if (_authState != state) {
      _authState = state;
      debugPrint('[Session] Auth state: ${state.displayText}');
      
      // Update connection state when authenticated
      if (state == AuthState.authenticated && 
          _connectionState == ConnectionState.connected) {
        _connectionState = ConnectionState.authenticated;
      }
    }
  }
  
  /// Set authentication state (alias for updateAuthState for consistency)
  void setAuthState(AuthState state) {
    updateAuthState(state);
  }
  
  /// Set connected device info
  void setConnectedDevice(String deviceId, String deviceName) {
    _connectedDeviceId = deviceId;
    _connectedDeviceName = deviceName;
    debugPrint('[Session] Connected to: $deviceName ($deviceId)');
  }
  
  /// Set session token (received from ESP32)
  void setSessionToken(String token) {
    _sessionToken = token;
    debugPrint('[Session] Token set: $token');
  }
  
  /// Set error message
  void setError(String error) {
    _lastError = error;
    debugPrint('[Session] Error: $error');
  }
  
  /// Clear error message
  void clearError() {
    _lastError = null;
  }
  
  /// Clear entire session (called on disconnect or logout)
  void clearSession() {
    debugPrint('[Session] Clearing session');
    _connectedDeviceId = null;
    _connectedDeviceName = null;
    _connectionState = ConnectionState.disconnected;
    _authState = AuthState.unauthenticated;
    _sessionToken = null;
    _lastError = null;
  }
  
  /// Get session summary for debugging
  Map<String, dynamic> getSessionInfo() {
    return {
      'deviceId': _connectedDeviceId,
      'deviceName': _connectedDeviceName,
      'connectionState': _connectionState.displayText,
      'authState': _authState.displayText,
      'hasToken': _sessionToken != null,
      'isReady': isReady,
      'lastError': _lastError,
    };
  }
  
  @override
  String toString() {
    return 'Session(device: $_connectedDeviceName, conn: ${_connectionState.displayText}, auth: ${_authState.displayText}, ready: $isReady)';
  }
}
