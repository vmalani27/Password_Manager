import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../providers/device_scan_provider.dart';

/// Device scan screen - discovers and connects to ESP32 devices
class DeviceScanScreen extends ConsumerWidget {
  const DeviceScanScreen({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final scanState = ref.watch(deviceScanProvider);
    final scanNotifier = ref.read(deviceScanProvider.notifier);
    
    // Start scan on first build
    ref.listen(deviceScanProvider, (previous, next) {
      if (previous == null && next.devices.isEmpty && !next.isScanning) {
        Future.microtask(() => scanNotifier.startScan());
      }
    });

    return Scaffold(
      appBar: AppBar(
        title: const Text('Connect to ESP32'),
        actions: [
          if (!scanState.isScanning && !scanState.isConnecting)
            IconButton(
              icon: const Icon(Icons.refresh),
              onPressed: () => scanNotifier.startScan(),
              tooltip: 'Rescan',
            ),
        ],
      ),
      body: Column(
        children: [
          // Scanning indicator
          if (scanState.isScanning)
            const LinearProgressIndicator(),

          // Error message
          if (scanState.errorMessage != null)
            _buildErrorMessage(context, scanState.errorMessage!, scanNotifier),

          // Status message
          Padding(
            padding: const EdgeInsets.all(16.0),
            child: Text(
              scanState.isScanning
                  ? 'Scanning for ESP32 devices...'
                  : scanState.devices.isEmpty
                      ? 'No devices found. Make sure ESP32 is powered on.'
                      : 'Found ${scanState.devices.length} device(s)',
              style: Theme.of(context).textTheme.bodyLarge,
            ),
          ),

          // Device list or connecting indicator
          Expanded(
            child: scanState.isConnecting
                ? _buildConnectingIndicator()
                : _buildDeviceList(context, scanState.devices, scanNotifier),
          ),
        ],
      ),
    );
  }

  Widget _buildErrorMessage(BuildContext context, String errorMessage, DeviceScanNotifier notifier) {
    return Container(
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
                  errorMessage,
                  style: TextStyle(color: Colors.red.shade700),
                ),
              ),
            ],
          ),
          if (errorMessage.contains('Bluetooth is not enabled'))
            Padding(
              padding: const EdgeInsets.only(top: 12, left: 36),
              child: ElevatedButton.icon(
                onPressed: () => notifier.startScan(),
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
    );
  }

  Widget _buildConnectingIndicator() {
    return Center(
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
    );
  }

  Widget _buildDeviceList(BuildContext context, List devices, DeviceScanNotifier notifier) {
    if (devices.isEmpty) {
      return const Center(
        child: Text('No devices found.\nTap refresh to scan again.'),
      );
    }

    return ListView.builder(
      itemCount: devices.length,
      itemBuilder: (context, index) {
        final device = devices[index];
        return ListTile(
          leading: const Icon(Icons.bluetooth),
          title: Text(device.name.isNotEmpty ? device.name : 'Unknown Device'),
          subtitle: Text(device.id),
          trailing: const Icon(Icons.arrow_forward),
          onTap: () => _handleDeviceConnect(context, device, notifier),
        );
      },
    );
  }

  Future<void> _handleDeviceConnect(BuildContext context, device, DeviceScanNotifier notifier) async {
    try {
      final isNewPairing = await notifier.connectToDevice(device);
      
      if (context.mounted) {
        // Show feedback based on pairing status
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(
              isNewPairing 
                  ? 'Device paired successfully!' 
                  : 'Reconnected to paired device'
            ),
            backgroundColor: isNewPairing ? Colors.green : null,
            duration: Duration(seconds: isNewPairing ? 3 : 2),
          ),
        );
        
        // Success - pop back to home screen
        Navigator.pop(context);
      }
    } catch (e) {
      // Error already set in provider state
    }
  }
}
