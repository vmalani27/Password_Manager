import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart' as reactive_ble;
import 'package:provider/provider.dart';
import '../providers/ble_provider.dart';
import '../widgets/custom_button.dart';

class DeviceScanScreen extends StatefulWidget {
  const DeviceScanScreen({Key? key}) : super(key: key);

  @override
  State<DeviceScanScreen> createState() => _DeviceScanScreenState();
}

class _DeviceScanScreenState extends State<DeviceScanScreen> {
  StreamSubscription? _scanSubscription;
  bool _mounted = true;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) _startScan();
    });
  }

  @override
  void dispose() {
    _mounted = false;
    _stopScan();
    super.dispose();
  }

  void _stopScan() {
    debugPrint('Stopping BLE scan');
    _scanSubscription?.cancel();
    _scanSubscription = null;
    if (_mounted) {
      context.read<BleProvider>().setScanning(false);
    }
  }

  void _startScan() {
    final bleProvider = context.read<BleProvider>();
    debugPrint('Attempting to start BLE scan...');
    if (bleProvider.isScanning) {
      debugPrint('Scan already in progress, skipping.');
      return;
    }

    bleProvider.startScan();
    debugPrint('BLE scan started.');
    _scanSubscription = bleProvider.ble.scanForDevices(
      withServices: [],
      scanMode: reactive_ble.ScanMode.lowLatency,
    ).listen(
      (device) {
        debugPrint('Discovered device: name=${device.name}, id=${device.id}');
        if (!_mounted) return;
        bleProvider.addDevice(device);
      },
      onError: (error) {
        debugPrint('BLE scan error: $error');
        if (!_mounted) return;
        bleProvider.setError(error.toString());
        bleProvider.setScanning(false);
      },
      onDone: () {
        debugPrint('BLE scan completed.');
        if (!_mounted) return;
        bleProvider.setScanning(false);
      },
    );
    // Stop scan after 10 seconds
    Future.delayed(const Duration(seconds: 10), () {
      if (_mounted) {
        debugPrint('Auto-stopping BLE scan after 10 seconds.');
        _stopScan();
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<BleProvider>(
      builder: (context, bleProvider, child) {
        return Scaffold(
          appBar: AppBar(
            title: const Text('Scan for Devices'),
            actions: [
              IconButton(
                icon: Icon(bleProvider.isScanning ? Icons.stop : Icons.refresh),
                onPressed: bleProvider.isScanning ? _stopScan : _startScan,
                tooltip: bleProvider.isScanning ? 'Stop Scan' : 'Start Scan',
              ),
            ],
          ),
          body: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 0, vertical: 8),
            child: Column(
              children: [
                if (bleProvider.error != null)
                  Container(
                    padding: const EdgeInsets.all(18),
                    margin: const EdgeInsets.all(12),
                    decoration: BoxDecoration(
                      color: Colors.red.withOpacity(0.12),
                      borderRadius: BorderRadius.circular(12),
                      border: Border.all(color: Colors.red.withOpacity(0.25)),
                    ),
                    child: Row(
                      children: [
                        const Icon(Icons.error_outline, color: Colors.red),
                        const SizedBox(width: 12),
                        Expanded(
                          child: Text(
                            bleProvider.error!,
                            style: const TextStyle(color: Colors.red, fontWeight: FontWeight.w600),
                          ),
                        ),
                      ],
                    ),
                  ),
                Expanded(
                  child: bleProvider.isScanning && bleProvider.devices.isEmpty
                      ? const Center(
                          child: Column(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              CircularProgressIndicator(),
                              SizedBox(height: 18),
                              Text('Searching for devices...', style: TextStyle(fontSize: 16)),
                            ],
                          ),
                        )
                      : bleProvider.devices.isEmpty
                          ? Center(
                              child: Column(
                                mainAxisAlignment: MainAxisAlignment.center,
                                children: [
                                  Icon(
                                    Icons.bluetooth_disabled,
                                    size: 70,
                                    color: Colors.grey[400],
                                  ),
                                  const SizedBox(height: 18),
                                  Text(
                                    'No devices found',
                                    style: TextStyle(
                                      color: Colors.grey[500],
                                      fontSize: 17,
                                      fontWeight: FontWeight.w500,
                                    ),
                                  ),
                                  const SizedBox(height: 28),
                                  CustomButton(
                                    onPressed: _startScan,
                                    icon: Icons.refresh,
                                    label: 'Scan Again',
                                  ),
                                ],
                              ),
                            )
                          : ListView.builder(
                              itemCount: bleProvider.devices.length,
                              itemBuilder: (context, index) {
                                final device = bleProvider.devices[index];
                                return Card(
                                  margin: const EdgeInsets.symmetric(
                                    horizontal: 18,
                                    vertical: 10,
                                  ),
                                  shape: RoundedRectangleBorder(
                                    borderRadius: BorderRadius.circular(16),
                                  ),
                                  elevation: 3,
                                  child: ListTile(
                                    contentPadding: const EdgeInsets.symmetric(horizontal: 18, vertical: 10),
                                    leading: Container(
                                      padding: const EdgeInsets.all(10),
                                      decoration: BoxDecoration(
                                        color: Theme.of(context).colorScheme.primary.withOpacity(0.13),
                                        shape: BoxShape.circle,
                                      ),
                                      child: Icon(
                                        Icons.bluetooth,
                                        color: Theme.of(context).colorScheme.primary,
                                      ),
                                    ),
                                    title: Text(device.name.isNotEmpty ? device.name : 'Unknown Device',
                                        style: const TextStyle(fontWeight: FontWeight.w600)),
                                    subtitle: Text(device.id, style: const TextStyle(fontSize: 13)),
                                    trailing: CustomButton(
                                      onPressed: () {
                                        _stopScan();
                                        Navigator.pop(context, device);
                                      },
                                      label: 'Connect',
                                      icon: Icons.link,
                                    ),
                                  ),
                                );
                              },
                            ),
                ),
              ],
            ),
          ),
        );
      },
    );
  }
}
