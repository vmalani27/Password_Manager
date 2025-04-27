import 'package:flutter/material.dart';

class AppTheme {
  // Colors
  static const Color darkTeal = Color(0xFF003344);
  static const Color brightTeal = Color(0xFF008B9E);
  static const Color lightTeal = Color(0xFF00B4D8);
  static const Color veryDarkTeal = Color(0xFF002233);

  // Common styles
  static BoxDecoration get cardDecoration => BoxDecoration(
    color: Colors.white.withAlpha(26), // 0.1 opacity
    borderRadius: BorderRadius.circular(16),
    border: Border.all(
      color: Colors.white.withAlpha(51), // 0.2 opacity
    ),
    boxShadow: [
      BoxShadow(
        color: veryDarkTeal.withAlpha(77), // 0.3 opacity
        blurRadius: 15,
        offset: const Offset(0, 5),
      ),
    ],
  );

  static ThemeData get theme => ThemeData(
    scaffoldBackgroundColor: darkTeal,
    primaryColor: lightTeal,
    colorScheme: const ColorScheme.dark(
      primary: lightTeal,
      secondary: brightTeal,
      background: darkTeal,
    ),
    appBarTheme: const AppBarTheme(
      backgroundColor: darkTeal,
      elevation: 0,
    ),
    elevatedButtonTheme: ElevatedButtonThemeData(
      style: ElevatedButton.styleFrom(
        backgroundColor: lightTeal,
        foregroundColor: Colors.white,
      ),
    ),
    textTheme: const TextTheme(
      titleLarge: TextStyle(color: Colors.white, fontSize: 24, fontWeight: FontWeight.bold),
      bodyLarge: TextStyle(color: Colors.white, fontSize: 16),
      bodyMedium: TextStyle(color: Colors.white70, fontSize: 14),
    ),
  );
}

class Credential {
  final String site;
  final String username;
  final String? password;

  Credential({
    required this.site,
    required this.username,
    this.password,
  });
}