/// Represents the current BLE connection state
enum ConnectionState {
  /// Not connected to any device
  disconnected,
  
  /// Actively scanning for BLE devices
  scanning,
  
  /// Attempting to connect to a device
  connecting,
  
  /// Connected to device but not yet authenticated
  connected,
  
  /// Connected and performing authentication
  authenticating,
  
  /// Connected and authenticated (ready to use)
  authenticated,
  
  /// Connection failed or error occurred
  error,
}

extension ConnectionStateExtension on ConnectionState {
  /// Check if we can send commands (must be authenticated)
  bool get canSendCommands => this == ConnectionState.authenticated;
  
  /// Check if we're in a loading state
  bool get isLoading => 
      this == ConnectionState.connecting || 
      this == ConnectionState.scanning ||
      this == ConnectionState.authenticating;
  
  /// Check if we're connected (but may not be authenticated yet)
  bool get isConnected => 
      this == ConnectionState.connected || 
      this == ConnectionState.authenticating ||
      this == ConnectionState.authenticated;
  
  /// User-friendly display text
  String get displayText {
    switch (this) {
      case ConnectionState.disconnected:
        return 'Disconnected';
      case ConnectionState.scanning:
        return 'Scanning...';
      case ConnectionState.connecting:
        return 'Connecting...';
      case ConnectionState.connected:
        return 'Connected';
      case ConnectionState.authenticating:
        return 'Authenticating...';
      case ConnectionState.authenticated:
        return 'Connected & Ready';
      case ConnectionState.error:
        return 'Connection Error';
    }
  }
}
