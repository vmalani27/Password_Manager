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
  /// Expected format: "github.com user@email.com" (space-separated)
  factory Credential.fromEsp32Response(String line) {
    final parts = line.trim().split(RegExp(r'\s+'));
    if (parts.length < 2) {
      throw FormatException('Invalid credential format: $line');
    }

    // Format: <site> <username>
    // If username contains spaces, join all parts after first
    final site = parts[0];
    final username = parts.sublist(1).join(' ');

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
