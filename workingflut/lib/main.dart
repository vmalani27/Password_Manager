import 'dart:async';
import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:provider/provider.dart';
import 'providers/ble_provider.dart';
import 'screens/device_scan_screen.dart';
import 'services/bleservice.dart';
import 'services/device_state.dart';
import 'pages/device_control_page.dart';
import 'pages/device_control_page.dart'; // Newly added import

void main() async {
  try {
    WidgetsFlutterBinding.ensureInitialized();
    await SharedPreferences.getInstance();
    await [
      Permission.bluetooth,
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.location,
    ].request();
    runApp(const MyApp());
  } catch (e) {
    debugPrint('Error in main: $e');
    runApp(const MyApp());
  }
}

class MyApp extends StatelessWidget {
  const MyApp({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (context) => BleProvider()),
        ChangeNotifierProvider(create: (context) => DeviceState(BLEService())),
      ],
      child: MaterialApp(
        title: 'Password Manager',
        theme: ThemeData(
          primarySwatch: Colors.blue,
          useMaterial3: true,
        ),
        home: const HomePage(),
      ),
    );
  }
}

class HomePage extends StatelessWidget {
  const HomePage({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Consumer<BleProvider>(
      builder: (context, bleProvider, child) {
        return Scaffold(
          appBar: AppBar(
            title: const Text('Password Manager'),
          ),
          body: Center(
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                const Icon(
                  Icons.bluetooth_searching,
                  size: 64,
                  color: Colors.blue,
                ),
                const SizedBox(height: 16),
                const Text(
                  'Connect to Your Password Manager',
                  style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold),
                  textAlign: TextAlign.center,
                ),
                const SizedBox(height: 8),
                const Text(
                  'Make sure your physical password manager is turned on and nearby',
                  textAlign: TextAlign.center,
                  style: TextStyle(color: Colors.grey),
                ),
                if (bleProvider.connectionError != null) ...[
                  const SizedBox(height: 16),
                  Container(
                    padding: const EdgeInsets.all(16),
                    margin: const EdgeInsets.symmetric(horizontal: 32),
                    decoration: BoxDecoration(
                      color: Colors.red.withOpacity(0.1),
                      borderRadius: BorderRadius.circular(8),
                      border: Border.all(color: Colors.red.withOpacity(0.3)),
                    ),
                    child: Text(
                      bleProvider.connectionError!,
                      style: const TextStyle(color: Colors.red),
                      textAlign: TextAlign.center,
                    ),
                  ),
                ],
                const SizedBox(height: 32),
                ElevatedButton.icon(
                  onPressed: () async {
                    final device = await Navigator.push(
                      context,
                      MaterialPageRoute(
                        builder: (context) => const DeviceScanScreen(),
                      ),
                    );
                    if (device != null) {
                      final bleProvider = context.read<BleProvider>();
                      await bleProvider.connectToDevice(device);
                      if (bleProvider.isConnected && context.mounted) {
                        Navigator.of(context).pushReplacement(
                          MaterialPageRoute(
                            builder: (context) => const DeviceControlPage(),
                          ),
                        );
                      }
                    }
                  },
                  icon: const Icon(Icons.bluetooth_searching),
                  label: const Text('Scan for Devices'),
                ),
              ],
            ),
          ),
        );
      },
    );
  }
}
// ...existing code...
          
