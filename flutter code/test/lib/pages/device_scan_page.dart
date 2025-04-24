import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart' as reactive_ble;

class DeviceScanPage extends StatefulWidget {
  const DeviceScanPage({Key? key}) : super(key: key);

  @override
  State<DeviceScanPage> createState() => _DeviceScanPageState();
}

class _DeviceScanPageState extends State<DeviceScanPage> {
  final _ble = reactive_ble.FlutterReactiveBle();
  final List<reactive_ble.DiscoveredDevice> _devices = [];
  bool _isScanning = false;
  String? _error;
  StreamSubscription? _scanSubscription;
  bool _mounted = true;

  @override
  void initState() {
    super.initState();
    _startScan();
  }

  @override
  void dispose() {
    _mounted = false;
    _stopScan();
    super.dispose();
  }

  void _stopScan() {
    _scanSubscription?.cancel();
    _scanSubscription = null;
    if (_mounted) {
      setState(() {
        _isScanning = false;
      });
    }
  }

  void _startScan() {
    if (_isScanning) return;

    if (_mounted) {
      setState(() {
        _isScanning = true;
        _error = null;
        _devices.clear();
      });
    }

    _scanSubscription = _ble.scanForDevices(
      withServices: [],
      scanMode: reactive_ble.ScanMode.lowLatency,
    ).listen(
      (device) {
        if (!_mounted) return;
        setState(() {
          if (!_devices.any((d) => d.id == device.id)) {
            _devices.add(device);
          }
        });
      },
      onError: (error) {
        if (!_mounted) return;
        setState(() {
          _error = error.toString();
          _isScanning = false;
        });
      },
      onDone: () {
        if (!_mounted) return;
        setState(() {
          _isScanning = false;
        });
      },
    );

    // Stop scan after 10 seconds
    Future.delayed(const Duration(seconds: 10), () {
      if (_mounted) {
        _stopScan();
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Scan for Devices'),
        actions: [
          IconButton(
            icon: Icon(_isScanning ? Icons.stop : Icons.refresh),
            onPressed: _isScanning ? _stopScan : _startScan,
          ),
        ],
      ),
      body: Column(
        children: [
          if (_error != null)
            Container(
              padding: const EdgeInsets.all(16),
              margin: const EdgeInsets.all(8),
              decoration: BoxDecoration(
                color: Colors.red.withOpacity(0.1),
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: Colors.red.withOpacity(0.3)),
              ),
              child: Text(
                _error!,
                style: const TextStyle(color: Colors.red),
              ),
            ),
          Expanded(
            child: _isScanning && _devices.isEmpty
                ? const Center(
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        CircularProgressIndicator(),
                        SizedBox(height: 16),
                        Text('Searching for devices...'),
                      ],
                    ),
                  )
                : _devices.isEmpty
                    ? Center(
                        child: Column(
                          mainAxisAlignment: MainAxisAlignment.center,
                          children: [
                            Icon(
                              Icons.bluetooth_disabled,
                              size: 64,
                              color: Colors.grey[400],
                            ),
                            const SizedBox(height: 16),
                            Text(
                              'No devices found',
                              style: TextStyle(
                                color: Colors.grey[400],
                                fontSize: 16,
                              ),
                            ),
                            const SizedBox(height: 24),
                            ElevatedButton.icon(
                              onPressed: _startScan,
                              icon: const Icon(Icons.refresh),
                              label: const Text('Scan Again'),
                            ),
                          ],
                        ),
                      )
                    : ListView.builder(
                        itemCount: _devices.length,
                        itemBuilder: (context, index) {
                          final device = _devices[index];
                          return Card(
                            margin: const EdgeInsets.symmetric(
                              horizontal: 16,
                              vertical: 8,
                            ),
                            child: ListTile(
                              leading: Container(
                                padding: const EdgeInsets.all(8),
                                decoration: BoxDecoration(
                                  color: Theme.of(context)
                                      .primaryColor
                                      .withOpacity(0.1),
                                  shape: BoxShape.circle,
                                ),
                                child: Icon(
                                  Icons.bluetooth,
                                  color: Theme.of(context).primaryColor,
                                ),
                              ),
                              title: Text(device.name),
                              subtitle: Text(device.id),
                              trailing: ElevatedButton(
                                onPressed: () {
                                  _stopScan();
                                  Navigator.pop(context, device);
                                },
                                child: const Text('Connect'),
                              ),
                            ),
                          );
                        },
                      ),
          ),
        ],
      ),
    );
  }
} 