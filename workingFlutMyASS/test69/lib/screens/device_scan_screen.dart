import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import '../providers/app_state_provider.dart';

/// Device scan screen - discovers and connects to ESP32 devices
class DeviceScanScreen extends ConsumerStatefulWidget {
  const DeviceScanScreen({Key? key}) : super(key: key);

  @override
  ConsumerState<DeviceScanScreen> createState() => _DeviceScanScreenState();
}

class _DeviceScanScreenState extends ConsumerState<DeviceScanScreen> {
  List<DiscoveredDevice> _devices = [];
  bool _isScanning = false;
  bool _isConnecting = false;
  String? _errorMessage;

  @override
  void initState() {
    super.initState();
    // Small delay to ensure providers are ready
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _startScan();
    });
  }

  Future<void> _startScan() async {
    if (!mounted) return;
    
    setState(() {
      _isScanning = true;
      _errorMessage = null;
      _devices = [];
    });

    try {
      final notifier = ref.read(appStateProvider.notifier);
      final devices = await notifier.scanForDevices(
        timeout: const Duration(seconds: 10),
      );
      
      if (mounted) {
        setState(() {
          _devices = devices;
          _isScanning = false;
        });
      }
    } catch (e, stackTrace) {
      debugPrint('[DeviceScan] Scan failed: $e');
      debugPrint('[DeviceScan] Stack trace: $stackTrace');
      
      if (mounted) {
        setState(() {
          _isScanning = false;
          _errorMessage = _formatErrorMessage(e);
        });
      }
    }
  }
  
  String _formatErrorMessage(dynamic error) {
    final errorStr = error.toString();
    
    if (errorStr.contains('Bluetooth is not ready')) {
      return 'Bluetooth is not enabled. Please enable Bluetooth and try again.';
    } else if (errorStr.contains('permission')) {
      return 'Bluetooth permission denied. Please grant permission in settings.';
    } else {
      return 'Scan failed: $errorStr';
    }
  }

  Future<void> _connectToDevice(DiscoveredDevice device) async {
    setState(() {
      _isConnecting = true;
      _errorMessage = null;
    });

    try {
      final notifier = ref.read(appStateProvider.notifier);
      
      // Connect and authenticate (PIN hardcoded for now, ESP32 doesn't use it yet)
      final isNewPairing = await notifier.connectAndAuthenticate(
        deviceId: device.id,
        deviceName: device.name,
        pin: '123456',
      );

      if (mounted) {
        // Show feedback based on pairing status
        if (isNewPairing) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text('Device paired successfully!'),
              backgroundColor: Colors.green,
            ),
          );
        } else {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text('Reconnected to paired device'),
              duration: Duration(seconds: 2),
            ),
          );
        }
        
        // Success - pop back to home screen
        Navigator.pop(context);
      }
    } catch (e) {
      if (mounted) {
        setState(() {
          _isConnecting = false;
          _errorMessage = 'Connection failed: $e';
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Connect to ESP32'),
        actions: [
          if (!_isScanning && !_isConnecting)
            IconButton(
              icon: const Icon(Icons.refresh),
              onPressed: _startScan,
              tooltip: 'Rescan',
            ),
        ],
      ),
      body: Column(
        children: [
          // Scanning indicator
          if (_isScanning)
            const LinearProgressIndicator(),

          // Error message
          if (_errorMessage != null)
            Container(
              color: Colors.red.shade50,
              width: double.infinity,
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Icon(Icons.error_outline, color: Colors.red.shade700),
                      const SizedBox(width: 12),
                      Expanded(
                        child: Text(
                          _errorMessage!,
                          style: TextStyle(color: Colors.red.shade700),
                        ),
                      ),
                    ],
                  ),
                  if (_errorMessage!.contains('Bluetooth is not enabled'))
                    Padding(
                      padding: const EdgeInsets.only(top: 12, left: 36),
                      child: ElevatedButton.icon(
                        onPressed: _startScan,
                        icon: const Icon(Icons.refresh, size: 16),
                        label: const Text('Retry'),
                        style: ElevatedButton.styleFrom(
                          backgroundColor: Colors.red.shade700,
                          foregroundColor: Colors.white,
                        ),
                      ),
                    ),
                ],
              ),
            ),

          // Status message
          Padding(
            padding: const EdgeInsets.all(16.0),
            child: Text(
              _isScanning
                  ? 'Scanning for ESP32 devices...'
                  : _devices.isEmpty
                      ? 'No devices found. Make sure ESP32 is powered on.'
                      : 'Found ${_devices.length} device(s)',
              style: Theme.of(context).textTheme.bodyLarge,
            ),
          ),

          // Device list
          Expanded(
            child: _isConnecting
                ? Center(
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        const CircularProgressIndicator(),
                        const SizedBox(height: 24),
                        const Text(
                          'Connecting to ESP32...',
                          style: TextStyle(
                            fontSize: 16,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                        const SizedBox(height: 16),
                        Container(
                          padding: const EdgeInsets.all(16),
                          margin: const EdgeInsets.symmetric(horizontal: 32),
                          decoration: BoxDecoration(
                            color: Colors.blue.shade50,
                            borderRadius: BorderRadius.circular(8),
                            border: Border.all(color: Colors.blue.shade200),
                          ),
                          child: Column(
                            children: [
                              Icon(Icons.security, color: Colors.blue.shade700, size: 32),
                              const SizedBox(height: 12),
                              Text(
                                'Bonding in Progress',
                                style: TextStyle(
                                  fontSize: 14,
                                  fontWeight: FontWeight.w600,
                                  color: Colors.blue.shade900,
                                ),
                              ),
                              const SizedBox(height: 8),
                              const Text(
                                'Please enter PIN when prompted:',
                                style: TextStyle(fontSize: 13),
                                textAlign: TextAlign.center,
                              ),
                              const SizedBox(height: 8),
                              Container(
                                padding: const EdgeInsets.symmetric(
                                  horizontal: 16,
                                  vertical: 8,
                                ),
                                decoration: BoxDecoration(
                                  color: Colors.white,
                                  borderRadius: BorderRadius.circular(4),
                                  border: Border.all(color: Colors.blue.shade300),
                                ),
                                child: Text(
                                  '123456',
                                  style: TextStyle(
                                    fontSize: 20,
                                    fontWeight: FontWeight.bold,
                                    letterSpacing: 4,
                                    color: Colors.blue.shade900,
                                    fontFamily: 'monospace',
                                  ),
                                ),
                              ),
                              const SizedBox(height: 12),
                              const Text(
                                'This may take a few moments...',
                                style: TextStyle(
                                  fontSize: 11,
                                  fontStyle: FontStyle.italic,
                                ),
                                textAlign: TextAlign.center,
                              ),
                            ],
                          ),
                        ),
                      ],
                    ),
                  )
                : ListView.builder(
                    itemCount: _devices.length,
                    itemBuilder: (context, index) {
                      final device = _devices[index];
                      return ListTile(
                        leading: const Icon(Icons.bluetooth),
                        title: Text(device.name.isNotEmpty ? device.name : 'Unknown Device'),
                        subtitle: Text(device.id),
                        trailing: ElevatedButton(
                          onPressed: () => _connectToDevice(device),
                          child: const Text('Connect'),
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
