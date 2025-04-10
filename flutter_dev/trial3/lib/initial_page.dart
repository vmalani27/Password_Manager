import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:trial3/pages/homescreen.dart';
import 'package:trial3/pages/onboarding/register_device.dart';

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
    await Future.delayed(Duration(seconds: 3)); // Simulate splash screen delay
    _checkPermissions();
  }

  Future<void> _checkPermissions() async {
    List<Permission> permissions = [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.bluetoothAdvertise
    ];

    // Check permissions status
    Map<Permission, PermissionStatus> statuses = await permissions.request();

    bool allGranted = statuses.values.every((status) => status.isGranted);
    
    if (allGranted) {
      if (!mounted) return; // Prevent navigation error if widget is disposed
      Navigator.pushReplacement(
        context,
        MaterialPageRoute(builder: (context) => RegisterDevicePage()),
      );
    } else {
      // Handle permanently denied permissions
      if (statuses.values.any((status) => status.isPermanentlyDenied)) {
        _showSettingsDialog();
      }
    }
  }

  void _showSettingsDialog() {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: Text("Permissions Required"),
        content: Text("Please enable Bluetooth permissions in settings."),
        actions: [
          TextButton(
            onPressed: () async {
              await openAppSettings(); // Open settings
              Navigator.pop(context);
              _checkPermissions(); // Re-check after returning
            },
            child: Text("Open Settings"),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: Text("Cancel"),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Center(child: CircularProgressIndicator()),
    );
  }
}
