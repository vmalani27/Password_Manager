/// Represents the current ESP32 session authentication state
enum AuthState {
  /// Not authenticated - no token requested yet
  unauthenticated,
  
  /// Token requested from ESP32, waiting for response
  requestingToken,
  
  /// Token received, attempting to authenticate
  authenticating,
  
  /// Successfully authenticated - can send commands
  authenticated,
  
  /// Authentication failed (wrong token, timeout, etc.)
  failed,
  
  /// Too many failed attempts - ESP32 is in lockout mode
  lockedOut,
}

extension AuthStateExtension on AuthState {
  /// Check if we can send credential commands
  bool get canSendCommands => this == AuthState.authenticated;
  
  /// Check if we're in a loading state
  bool get isLoading => this == AuthState.requestingToken || this == AuthState.authenticating;
  
  /// Check if we need to retry authentication
  bool get needsAuth => this == AuthState.unauthenticated || this == AuthState.failed;
  
  /// User-friendly display text
  String get displayText {
    switch (this) {
      case AuthState.unauthenticated:
        return 'Not Authenticated';
      case AuthState.requestingToken:
        return 'Requesting Token...';
      case AuthState.authenticating:
        return 'Authenticating...';
      case AuthState.authenticated:
        return 'Authenticated';
      case AuthState.failed:
        return 'Authentication Failed';
      case AuthState.lockedOut:
        return 'Locked Out (Too Many Attempts)';
    }
  }
}
