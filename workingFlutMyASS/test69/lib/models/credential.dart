import 'package:equatable/equatable.dart';

/// Represents a single credential stored on the ESP32
class Credential extends Equatable {
  // ESP32 uses 'site' for service and 'username' for identifier
  final String service;  // Called 'site' in ESP32 database
  final String username; // Called 'username' in ESP32 database
  final String password; // Not included in LIST response, only in GET response

  const Credential({
    required this.service,
    required this.username,
    required this.password,
  });

  /// Create a Credential from ESP32 LIST response format
  /// ESP32 actual format: "jdjdir nfkfjd" (space-separated)
  /// Each line format: "<site> <username>"
  /// Example: "instagram vmalanixx" or "jdjdir nfkfjd"
  factory Credential.fromEsp32Response(String line) {
    final trimmed = line.trim();
    
    // Split on first space only (username might contain spaces)
    final firstSpace = trimmed.indexOf(' ');
    if (firstSpace == -1) {
      throw FormatException('Invalid credential format (expected "site username"): $line');
    }
    
    final service = trimmed.substring(0, firstSpace).trim();
    final username = trimmed.substring(firstSpace + 1).trim();
    
    if (service.isEmpty || username.isEmpty) {
      throw FormatException('Invalid credential format (empty site or username): $line');
    }
    
    return Credential(
      service: service,
      username: username,
      password: '', // Password is not included in LIST response
    );
  }

  /// Create a Credential from ESP32 GET response
  /// ESP32 sends: "Password: <actual_password>"
  /// This is used after calling "get <site> <username>"
  factory Credential.fromPasswordResponse(String service, String username, String response) {
    // Expected format: "Password: mypassword123"
    if (!response.startsWith('Password: ')) {
      throw FormatException('Invalid password response format: $response');
    }
    
    final password = response.substring('Password: '.length).trim();
    
    return Credential(
      service: service,
      username: username,
      password: password,
    );
  }

  /// Create a copy with updated password (used after GET command)
  Credential copyWithPassword(String password) {
    return Credential(
      service: service,
      username: username,
      password: password,
    );
  }

  /// Format for ESP32 commands
  /// Used in: add, get, update, delete commands
  String toEsp32Format() => '$service $username';

  @override
  List<Object?> get props => [service, username, password];

  @override
  String toString() => 'Credential(service: $service, username: $username)';
}