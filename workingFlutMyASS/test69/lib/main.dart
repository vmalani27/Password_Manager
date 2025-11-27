import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'screens/auth_gate.dart';
import 'screens/home_screen.dart';

void main() {
  runApp(
    const ProviderScope(
      child: PasswordManagerApp(),
    ),
  );
}

class PasswordManagerApp extends StatelessWidget {
  const PasswordManagerApp({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'ESP32 Password Manager',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        primarySwatch: Colors.blue,
        useMaterial3: true,
      ),
      home: const AuthGate(child: HomeScreen(), reason: 'Authenticate to access Password Manager'),
    );
  }
}