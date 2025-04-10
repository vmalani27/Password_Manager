import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';

class RegisterDevicePage extends StatefulWidget {
  @override
  _RegisterDevicePageState createState() => _RegisterDevicePageState();
}

class _RegisterDevicePageState extends State<RegisterDevicePage> {
  // final FlutterBluePlus flutterBlue = FlutterBluePlus.instance;
  final List<ScanResult> scanResults = [];
  bool isScanning = false;
  BluetoothDevice? connectedDevice;
  final _deviceNameController = TextEditingController();

  @override
  void initState() {
    super.initState();
    checkPermissionsAndScan();
  }

  /// ✅ Check & Request Bluetooth & Location Permissions
  Future<void> checkPermissionsAndScan() async {
    await Permission.bluetooth.request();
    await Permission.bluetoothScan.request();
    await Permission.bluetoothConnect.request();
    await Permission.locationWhenInUse.request();

    if (await Permission.bluetooth.isGranted &&
        await Permission.bluetoothScan.isGranted &&
        await Permission.bluetoothConnect.isGranted &&
        await Permission.locationWhenInUse.isGranted) {
      scanForDevices();
    } else {
      print("❌ Permissions not granted!");
    }
  }

  /// ✅ Start Scanning for Bluetooth Devices
  void scanForDevices() async {
    // Clear previous scan results
    scanResults.clear();
    setState(() {
      isScanning = true;
    });

    // Start scanning
    FlutterBluePlus.startScan(timeout: const Duration(seconds: 5));

    // Listen for scan results
    FlutterBluePlus.scanResults.listen((results) {
      setState(() {
        scanResults.clear();
        scanResults.addAll(results);
      });
    });

    // Stop scanning after 5 seconds
    await Future.delayed(const Duration(seconds: 5));
    FlutterBluePlus.stopScan();

    setState(() {
      isScanning = false;
    });

    if (scanResults.isEmpty) {
      print("🔍 No Bluetooth devices found.");
    }
  }

  /// ✅ Connect to a selected device
  void connectToDevice(BluetoothDevice device) async {
    try {
      await device.connect();
      setState(() {
        connectedDevice = device;
      });
      print("✅ Connected to ${device.name}");
    } catch (e) {
      print("❌ Connection failed: $e");
    }
  }

  /// ✅ Disconnect from a device
  void disconnectDevice() async {
    if (connectedDevice != null) {
      await connectedDevice!.disconnect();
      setState(() {
        connectedDevice = null;
      });
      print("🔌 Device disconnected");
    }
  }

  @override
  void dispose() {
    _deviceNameController.dispose();
    connectedDevice?.disconnect();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Register Device'),
        actions: [
          IconButton(
            icon: Icon(isScanning ? Icons.stop : Icons.refresh),
            onPressed: checkPermissionsAndScan,
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            TextFormField(
              controller: _deviceNameController,
              decoration: const InputDecoration(labelText: 'Device Name'),
            ),
            const SizedBox(height: 20),
            isScanning
                ? const CircularProgressIndicator()
                : scanResults.isEmpty
                    ? const Text("No Bluetooth devices found. Make sure they are in pairing mode!")
                    : Expanded(
                        child: ListView.builder(
                          itemCount: scanResults.length,
                          itemBuilder: (context, index) {
                            final result = scanResults[index];
                            return ListTile(
                              title: Text(result.device.name.isNotEmpty
                                  ? result.device.name
                                  : "Unknown Device"),
                              subtitle: Text(result.device.id.toString()),
                              trailing: connectedDevice == result.device
                                  ? ElevatedButton(
                                      onPressed: disconnectDevice,
                                      child: const Text("Disconnect"),
                                      style: ElevatedButton.styleFrom(backgroundColor: Colors.red),
                                    )
                                  : ElevatedButton(
                                      onPressed: () => connectToDevice(result.device),
                                      child: const Text("Connect"),
                                    ),
                            );
                          },
                        ),
                      ),
            if (connectedDevice != null)
              Text(
                'Connected to ${connectedDevice!.name}',
                style: const TextStyle(fontWeight: FontWeight.bold),
              ),
          ],
        ),
      ),
    );
  }
}
