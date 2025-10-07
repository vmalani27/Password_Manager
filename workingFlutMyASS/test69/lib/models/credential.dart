import 'package:equatable/equatable.dart';

/// Represents a single credential stored on the ESP32
class Credential extends Equatable {
  final String site;
  final String username;
  final String password;

  const Credential({
    required this.site,
    required this.username,
    required this.password,
  });

  /// Create a Credential from ESP32 response format
  /// Expected format: "Site: example.com | User: john@example.com"
  factory Credential.fromEsp32Response(String line) {
    final parts = line.split('|');
    if (parts.length != 2) {
      throw FormatException('Invalid credential format: $line');
    }

    final sitePart = parts[0].trim();
    final userPart = parts[1].trim();

    // Extract site value (remove "Site: " prefix)
    final site = sitePart.replaceFirst(RegExp(r'^Site:\s*'), '').trim();
    
    // Extract username value (remove "User: " prefix)
    final username = userPart.replaceFirst(RegExp(r'^User:\s*'), '').trim();

    return Credential(
      site: site,
      username: username,
      password: '', // Password is not included in list response
    );
  }

  /// Create a copy with updated password (used after GET command)
  Credential copyWithPassword(String password) {
    return Credential(
      site: site,
      username: username,
      password: password,
    );
  }

  @override
  List<Object?> get props => [site, username, password];

  @override
  String toString() => 'Credential(site: $site, username: $username)';
}
