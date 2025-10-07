import 'dart:async';
import 'package:flutter/foundation.dart';
import '../constants/ble_constants.dart';
import '../models/auth_state.dart';
import 'command_service.dart';

/// Handles ESP32 session authentication flow
/// ESP32 requires: request_token → auth <token> before accepting commands
/// ESP32 code: sessionAuthorized flag, generateSessionToken(), handleCommand()
class AuthService {
  final CommandService _commandService;
  
  AuthState _currentState = AuthState.unauthenticated;
  String? _sessionToken;
  
  final _authStateController = StreamController<AuthState>.broadcast();
  
  /// Stream of authentication state changes
  Stream<AuthState> get authStateStream => _authStateController.stream;
  
  /// Current authentication state
  AuthState get authState => _currentState;
  
  /// Current session token (if received)
  String? get sessionToken => _sessionToken;
  
  AuthService(this._commandService);
  
  /// Request a session token from ESP32
  /// ESP32 code: if (cmd.equalsIgnoreCase("request_token")) { sessionToken = generateSessionToken(); sendNotification("TOKEN " + sessionToken); }
  Future<String> requestToken() async {
    _updateAuthState(AuthState.requestingToken);
    debugPrint('[Auth] Requesting session token...');
    
    try {
      // Send request_token command to ESP32
      final response = await _commandService.sendCommand(
        Esp32Commands.requestToken,
        timeout: BleConstants.commandTimeout,
      );
      
      // Parse response: "TOKEN 12AB34CD"
      if (response.startsWith(Esp32Commands.tokenPrefix)) {
        _sessionToken = response.substring(Esp32Commands.tokenPrefix.length).trim();
        debugPrint('[Auth] ✓ Token received: $_sessionToken');
        
        // Stay in unauthenticated state until auth command is sent
        _updateAuthState(AuthState.unauthenticated);
        return _sessionToken!;
      } else {
        throw Exception('Unexpected token response: $response');
      }
      
    } catch (e) {
      debugPrint('[Auth] ✗ Token request failed: $e');
      _updateAuthState(AuthState.failed);
      rethrow;
    }
  }
  
  /// Authenticate with the received token
  /// ESP32 code: if (cmd.equalsIgnoreCase("auth") && tokens.size() == 2) { if (provided == sessionToken) { sessionAuthorized = true; sendNotification("AUTH OK"); } }
  Future<bool> authenticate(String token) async {
    _updateAuthState(AuthState.authenticating);
    debugPrint('[Auth] Authenticating with token: $token');
    
    try {
      // Send auth command to ESP32
      final response = await _commandService.sendCommand(
        Esp32Commands.auth(token),
        timeout: BleConstants.commandTimeout,
      );
      
      // Parse response
      if (response == Esp32Commands.authOk) {
        _updateAuthState(AuthState.authenticated);
        debugPrint('[Auth] ✓ Authentication successful');
        return true;
        
      } else if (response == Esp32Commands.authFail) {
        _updateAuthState(AuthState.failed);
        debugPrint('[Auth] ✗ Authentication failed (wrong token)');
        return false;
        
      } else if (response == Esp32Commands.locked) {
        _updateAuthState(AuthState.lockedOut);
        debugPrint('[Auth] ✗ Device locked out (too many failed attempts)');
        // ESP32 code: if (failedAuthAttempts >= MAX_FAILED_ATTEMPTS) { lockoutUntilMs = millis() + LOCKOUT_DURATION_MS; }
        return false;
        
      } else {
        throw Exception('Unexpected auth response: $response');
      }
      
    } catch (e) {
      debugPrint('[Auth] ✗ Authentication error: $e');
      _updateAuthState(AuthState.failed);
      rethrow;
    }
  }
  
  /// Complete authentication flow: request token + authenticate with PIN
  /// This is the main method to call from UI/provider
  Future<bool> performAuthentication(String deviceId, String pin) async {
    try {
      // Step 1: Request token
      final token = await requestToken();
      
      // Step 2: Authenticate with received token
      // Note: PIN parameter currently not used by ESP32 (only validates token)
      // In future, could be used for additional authentication layer
      return await authenticate(token);
      
    } catch (e) {
      debugPrint('[Auth] ✗ Full authentication flow failed: $e');
      return false;
    }
  }
  
  /// Logout (clear session)
  /// ESP32 code: if (cmd.equalsIgnoreCase("logout")) { sessionAuthorized = false; sessionToken = ""; sendNotification("LOGOUT"); }
  Future<void> logout() async {
    debugPrint('[Auth] Logging out...');
    
    try {
      // Send logout command to ESP32
      await _commandService.sendCommand(
        Esp32Commands.logout,
        timeout: BleConstants.commandTimeout,
      );
      
      _clearSession();
      debugPrint('[Auth] ✓ Logged out');
      
    } catch (e) {
      debugPrint('[Auth] Logout error: $e (clearing local session anyway)');
      _clearSession();
    }
  }
  
  /// Clear local session state
  void _clearSession() {
    _sessionToken = null;
    _updateAuthState(AuthState.unauthenticated);
  }
  
  /// Reset authentication state (called on disconnect)
  /// ESP32 code: ServerCallbacks::onDisconnect() { sessionAuthorized = false; sessionToken = ""; }
  void reset() {
    debugPrint('[Auth] Resetting authentication state');
    _clearSession();
  }
  
  /// Update auth state and notify listeners
  void _updateAuthState(AuthState newState) {
    if (_currentState != newState) {
      _currentState = newState;
      _authStateController.add(newState);
      debugPrint('[Auth] State: ${newState.displayText}');
    }
  }
  
  /// Dispose of resources
  void dispose() {
    _authStateController.close();
  }
}
