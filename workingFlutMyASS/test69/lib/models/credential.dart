import 'package:equatable/equatable.dart';

/// Represents a single credential stored on the ESP32
class Credential extends Equatable {
  // For UI compatibility: treat identifier as username
  String get username => identifier;
  final String service;
  final String identifier;
  final String password;

  const Credential({
    required this.service,
    required this.identifier,
    required this.password,
  });

  /// Create a Credential from ESP32 response format
  /// Expected format: "github.com user@email.com" (space-separated)
  factory Credential.fromEsp32Response(String line) {
    // Split on whitespace, trim spaces
    final parts = line.trim().split(RegExp(r'\s+'));
    if (parts.length != 2) {
      throw FormatException('Invalid credential format (expected "service username"): $line');
    }
    final service = parts[0].trim();
    final username = parts[1].trim();
    return Credential(
      service: service,
      identifier: username,
      password: '', // Password is not included in list response
    );
  }

  /// Create a copy with updated password (used after GET command)
  Credential copyWithPassword(String password) {
    return Credential(
      service: service,
      identifier: identifier,
      password: password,
    );
  }

  @override
  List<Object?> get props => [service, identifier, password];

  @override
  String toString() => 'Credential(service: $service, identifier: $identifier)';
}
