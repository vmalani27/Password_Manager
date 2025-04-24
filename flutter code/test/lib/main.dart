import 'dart:async';
import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:provider/provider.dart';
import 'package:lemonicetea/services/bleservice.dart';
import 'package:lemonicetea/services/device_state.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart' as reactive_ble;
import 'package:lemonicetea/pages/device_scan_page.dart';
import 'package:lemonicetea/pages/device_control_page.dart';

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
    return ChangeNotifierProvider(
      create: (context) => DeviceState(BLEService()),
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
    return Consumer<DeviceState>(
      builder: (context, deviceState, child) {
        return Scaffold(
          appBar: AppBar(
            title: const Text('Password Manager'),
            actions: [
              if (deviceState.isConnected)
                IconButton(
                  icon: const Icon(Icons.bluetooth_disabled),
                  onPressed: () async {
                    await deviceState.disconnect();
                  },
                ),
            ],
          ),
          body: Center(
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                if (deviceState.isConnected)
                  Card(
                    margin: const EdgeInsets.all(16),
                    child: Padding(
                      padding: const EdgeInsets.all(16),
                      child: Column(
                        children: [
                          const Icon(
                            Icons.bluetooth_connected,
                            size: 48,
                            color: Colors.blue,
                          ),
                          const SizedBox(height: 16),
                          Text(
                            'Connected to ${deviceState.connectedDeviceName}',
                            style: Theme.of(context).textTheme.titleLarge,
                          ),
                          const SizedBox(height: 16),
                          ElevatedButton.icon(
                            onPressed: () {
                              Navigator.push(
                                context,
                                MaterialPageRoute(
                                  builder: (context) => const DeviceControlPage(),
                                ),
                              );
                            },
                            icon: const Icon(Icons.settings),
                            label: const Text('Control Device'),
                          ),
                        ],
                      ),
                    ),
                  )
                else
                  Column(
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
                      if (deviceState.error != null) ...[
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
                            deviceState.error!,
                            style: const TextStyle(color: Colors.red),
                            textAlign: TextAlign.center,
                          ),
                        ),
                      ],
                      const SizedBox(height: 32),
                      if (deviceState.isConnecting)
                        const CircularProgressIndicator()
                      else
                        ElevatedButton.icon(
                          onPressed: () async {
                            try {
                              final device = await Navigator.push<reactive_ble.DiscoveredDevice>(
                                context,
                                MaterialPageRoute(
                                  builder: (context) => const DeviceScanPage(),
                                ),
                              );
                              if (device != null) {
                                await deviceState.connectToDevice(device);
                              }
                            } catch (e) {
                              // Error is handled by DeviceState
                            }
                          },
                          icon: const Icon(Icons.bluetooth_searching),
                          label: const Text('Scan for Devices'),
                        ),
                    ],
                  ),
              ],
            ),
          ),
        );
      },
    );
  }
}
