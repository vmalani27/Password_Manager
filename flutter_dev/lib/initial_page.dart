import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:trial2/pages/homescreen.dart';

class InitialPage extends StatefulWidget {
  @override
  _InitialPageState createState() => _InitialPageState();
}

class _InitialPageState extends State<InitialPage> {
  @override
  void initState() {
    super.initState();
    _startSplashScreen();
  }

  Future<void> _startSplashScreen() async {
    // Show splash screen for 3 seconds
    await Future.delayed(Duration(seconds: 3));
    _checkPermissions();
  }

  Future<void> _checkPermissions() async {
    // Check for necessary permissions in parallel
    final statuses = await Future.wait([
      Permission.bluetooth.status,
      // Add other permissions you need to check here
    ]);

    if (statuses.every((status) => status.isGranted)) {
      // Navigate to the home screen if all permissions are granted
      Navigator.pushReplacement(
        context,
        MaterialPageRoute(builder: (context) => HomeScreen()),
      );
    } else {
      // Request permissions if not granted
      await Future.wait([
        Permission.bluetooth.request(),
        // Add other permissions you need to request here
      ]);
      // Re-check permissions after requesting
      _checkPermissions();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Center(
        child: CircularProgressIndicator(),
      ),
    );
  }
}