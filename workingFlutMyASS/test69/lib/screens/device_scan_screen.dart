import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../providers/device_scan_provider.dart';

/// Device scan screen - discovers and connects to ESP32 devices
class DeviceScanScreen extends ConsumerStatefulWidget {
  const DeviceScanScreen({Key? key}) : super(key: key);

  @override
  ConsumerState<DeviceScanScreen> createState() => _DeviceScanScreenState();
}

class _DeviceScanScreenState extends ConsumerState<DeviceScanScreen> {
  bool _showSetupScreen = true;
  bool _autoRetrying = false;
  bool _bluetoothReady = false;
  bool _foundDevice = false;

  @override
  void initState() {
    super.initState();
    // Start scan when screen is first loaded
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _startScanWithSetup();
    });
  }

  Future<void> _startScanWithSetup() async {
    debugPrint('[DeviceScanScreen] _startScanWithSetup called');
    setState(() {
      _showSetupScreen = true;
      _autoRetrying = false;
      _bluetoothReady = false;
      _foundDevice = false;
    });
    debugPrint('[DeviceScanScreen] State after reset: showSetup=$_showSetupScreen, autoRetry=$_autoRetrying, btReady=$_bluetoothReady, foundDevice=$_foundDevice');
    final scanNotifier = ref.read(deviceScanProvider.notifier);
    await scanNotifier.startScan(
      onBluetoothReady: () {
        debugPrint('[DeviceScanScreen] onBluetoothReady called');
        setState(() {
          _bluetoothReady = true;
        });
        debugPrint('[DeviceScanScreen] State after btReady: showSetup=$_showSetupScreen, btReady=$_bluetoothReady');
      },
      onDeviceFound: () async {
        debugPrint('[DeviceScanScreen] onDeviceFound called');
        setState(() {
          _foundDevice = true;
          _showSetupScreen = false;
        });
        debugPrint('[DeviceScanScreen] State after deviceFound: showSetup=$_showSetupScreen, foundDevice=$_foundDevice');
        // Show 'Device found!' message for 1 second before proceeding
        await Future.delayed(const Duration(seconds: 1));
      },
    );
    debugPrint('[DeviceScanScreen] Scan completed, foundDevice=$_foundDevice');
    if (!_foundDevice) {
      setState(() {
        _showSetupScreen = false;
      });
      debugPrint('[DeviceScanScreen] State after scan complete: showSetup=$_showSetupScreen');
    }
  }

  @override
  Widget build(BuildContext context) {
    final scanState = ref.watch(deviceScanProvider);
    final scanNotifier = ref.read(deviceScanProvider.notifier);

    debugPrint('[DeviceScanScreen] build called: showSetup=$_showSetupScreen, btReady=$_bluetoothReady, foundDevice=$_foundDevice');
    if (_showSetupScreen) {
      return Scaffold(
        appBar: AppBar(title: const Text('Connect to ESP32')),
        body: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              if (_foundDevice)
                Column(
                  children: [
                    Icon(Icons.check_circle, color: Colors.green, size: 48),
                    const SizedBox(height: 16),
                    const Text('Device found!', style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold, color: Colors.green)),
                  ],
                )
              else ...[
                const CircularProgressIndicator(),
                const SizedBox(height: 24),
                if (!_bluetoothReady)
                  Text(_autoRetrying
                      ? 'Bluetooth is initializing... Retrying scan.'
                      : 'Setting up services...'),
                if (_bluetoothReady && !_foundDevice)
                  const Text('Searching for nearby devices...'),
              ],
            ],
          ),
        ),
      );
    }

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
    debugPrint('[DeviceScanScreen] ========================================');
    debugPrint('[DeviceScanScreen] Starting device connection...');
    debugPrint('[DeviceScanScreen] Device: ${device.name} (${device.id})');
    
    // Capture the navigator before the async operation
    final navigator = Navigator.of(context);
    final scaffoldMessenger = ScaffoldMessenger.of(context);
    
    try {
      debugPrint('[DeviceScanScreen] Calling connectToDevice...');
      final isNewPairing = await notifier.connectToDevice(device);
      debugPrint('[DeviceScanScreen] connectToDevice completed successfully');
      debugPrint('[DeviceScanScreen] isNewPairing: $isNewPairing');
      
      // Use captured messenger and navigator instead of checking context.mounted
      debugPrint('[DeviceScanScreen] Showing SnackBar...');
      scaffoldMessenger.showSnackBar(
        SnackBar(
          content: Text(
            isNewPairing 
                ? 'Device paired successfully!' 
                : 'Reconnected to paired device'
          ),
          backgroundColor: isNewPairing ? Colors.green : null,
          duration: const Duration(seconds: 1),
        ),
      );
      
      debugPrint('[DeviceScanScreen] SnackBar shown, waiting 500ms...');
      // Small delay to ensure snackbar is visible
      await Future.delayed(const Duration(milliseconds: 500));
      
      // Navigate back using captured navigator
      debugPrint('[DeviceScanScreen] Calling navigator.pop to return to home screen...');
      navigator.pop();
      debugPrint('[DeviceScanScreen] Navigation completed');
      debugPrint('[DeviceScanScreen] ========================================');
      
    } catch (e) {
      debugPrint('[DeviceScanScreen] ========================================');
      debugPrint('[DeviceScanScreen] Connection failed with error: $e');
      
      // Check if it's a "already paired" error
      final errorString = e.toString();
      if (errorString.contains('already paired with a different device')) {
        debugPrint('[DeviceScanScreen] Detected ESP32 already paired error - showing guidance dialog');
        if (context.mounted) {
          _showAlreadyPairedDialog(context);
        }
      }
      
      debugPrint('[DeviceScanScreen] ========================================');
      // Error already set in provider state
    }
  }

  void _showAlreadyPairedDialog(BuildContext context) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: Row(
          children: [
            Icon(Icons.warning_amber_rounded, color: Colors.orange[700], size: 28),
            const SizedBox(width: 12),
            const Text('ESP32 Already Paired'),
          ],
        ),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'The ESP32 is already paired with another device or has old pairing data.',
              style: TextStyle(fontWeight: FontWeight.w500),
            ),
            const SizedBox(height: 16),
            const Text('To fix this, you need to:'),
            const SizedBox(height: 12),
            _buildStep('1', 'Go back to the home screen'),
            _buildStep('2', 'Find the "Repair Device" section'),
            _buildStep('3', 'Press the "Unpair Device" button'),
            _buildStep('4', 'Return here and scan again'),
            const SizedBox(height: 16),
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: Colors.blue.shade50,
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: Colors.blue.shade200),
              ),
              child: Row(
                children: [
                  Icon(Icons.info_outline, color: Colors.blue.shade700, size: 20),
                  const SizedBox(width: 8),
                  const Expanded(
                    child: Text(
                      'This will clear old pairing data from both devices.',
                      style: TextStyle(fontSize: 12),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () {
              Navigator.pop(context); // Close dialog
              Navigator.pop(context); // Go back to home screen
            },
            child: const Text('Go to Home Screen'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
        ],
      ),
    );
  }

  Widget _buildStep(String number, String text) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Container(
            width: 24,
            height: 24,
            decoration: BoxDecoration(
              color: Colors.blue.shade700,
              shape: BoxShape.circle,
            ),
            child: Center(
              child: Text(
                number,
                style: const TextStyle(
                  color: Colors.white,
                  fontSize: 12,
                  fontWeight: FontWeight.bold,
                ),
              ),
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Padding(
              padding: const EdgeInsets.only(top: 2),
              child: Text(text),
            ),
          ),
        ],
      ),
    );
  }
}
