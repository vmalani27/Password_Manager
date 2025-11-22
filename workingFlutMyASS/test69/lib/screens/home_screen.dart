import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../providers/app_state_provider.dart';
import '../models/connection_state.dart' as models;
import '../services/permission_service.dart';
import 'device_scan_screen.dart';
import 'credential_manager_screen.dart';

/// Home screen - entry point of the app
/// Shows connection status and navigation options
class HomeScreen extends ConsumerWidget {
  const HomeScreen({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final appState = ref.watch(appStateProvider);
    final notifier = ref.read(appStateProvider.notifier);

    return Scaffold(
      appBar: AppBar(
        title: const Text('ESP32 Password Manager'),
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            // Connection status card
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16.0),
                child: Column(
                  children: [
                    Icon(
                      _getStatusIcon(appState.connectionState),
                      size: 64,
                      color: _getStatusColor(appState.connectionState),
                    ),
                    const SizedBox(height: 16),
                    Text(
                      appState.connectionState.displayText,
                      style: Theme.of(context).textTheme.titleLarge,
                    ),
                    if (appState.connectedDeviceName != null) ...[
                      const SizedBox(height: 8),
                      Text(
                        appState.connectedDeviceName!,
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                    ],
                    if (appState.isReady) ...[
                      const SizedBox(height: 8),
                      Text(
                        '${appState.credentials.length} credentials stored',
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                    ],
                  ],
                ),
              ),
            ),

            const SizedBox(height: 32),

            // Action buttons
            if (!appState.isReady) ...[
              ElevatedButton.icon(
                onPressed: appState.isLoading
                    ? null
                    : () => _navigateToScan(context),
                icon: const Icon(Icons.bluetooth_searching),
                label: Text(appState.isLoading ? 'Connecting...' : 'Connect to ESP32'),
                style: ElevatedButton.styleFrom(
                  padding: const EdgeInsets.all(16),
                ),
              ),
            ] else ...[
              ElevatedButton.icon(
                onPressed: () => _navigateToCredentials(context),
                icon: const Icon(Icons.vpn_key),
                label: const Text('Manage Credentials'),
                style: ElevatedButton.styleFrom(
                  padding: const EdgeInsets.all(16),
                ),
              ),
              const SizedBox(height: 16),
              OutlinedButton.icon(
                onPressed: () => _disconnect(context, notifier),
                icon: const Icon(Icons.logout),
                label: const Text('Disconnect'),
                style: OutlinedButton.styleFrom(
                  padding: const EdgeInsets.all(16),
                ),
              ),
              const SizedBox(height: 16),
              OutlinedButton.icon(
                onPressed: () => _unpairDevice(context, notifier),
                icon: const Icon(Icons.link_off),
                label: const Text('Unpair Device'),
                style: OutlinedButton.styleFrom(
                  padding: const EdgeInsets.all(16),
                  foregroundColor: Colors.red,
                ),
              ),
            ],

            // Error display
            if (appState.errorMessage != null) ...[
              const SizedBox(height: 24),
              Card(
                color: Colors.red.shade50,
                child: Padding(
                  padding: const EdgeInsets.all(16.0),
                  child: Row(
                    children: [
                      Icon(Icons.error_outline, color: Colors.red.shade700),
                      const SizedBox(width: 12),
                      Expanded(
                        child: Text(
                          appState.errorMessage!,
                          style: TextStyle(color: Colors.red.shade700),
                        ),
                      ),
                      IconButton(
                        icon: const Icon(Icons.close),
                        onPressed: () => notifier.clearError(),
                      ),
                    ],
                  ),
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }

  IconData _getStatusIcon(models.ConnectionState state) {
    switch (state) {
      case models.ConnectionState.disconnected:
        return Icons.bluetooth_disabled;
      case models.ConnectionState.scanning:
        return Icons.bluetooth_searching;
      case models.ConnectionState.connecting:
      case models.ConnectionState.authenticating:
        return Icons.bluetooth_connected;
      case models.ConnectionState.connected:
      case models.ConnectionState.authenticated:
        return Icons.bluetooth_connected;
      case models.ConnectionState.error:
        return Icons.error_outline;
    }
  }

  Color _getStatusColor(models.ConnectionState state) {
    switch (state) {
      case models.ConnectionState.disconnected:
        return Colors.grey;
      case models.ConnectionState.scanning:
      case models.ConnectionState.connecting:
      case models.ConnectionState.authenticating:
      case models.ConnectionState.connected:
        return Colors.orange;
      case models.ConnectionState.authenticated:
        return Colors.green;
      case models.ConnectionState.error:
        return Colors.red;
    }
  }

  void _navigateToScan(BuildContext context) async {
    // Request permissions before navigating
    final permissionService = PermissionService();
    
    final hasPermissions = await permissionService.hasRequiredPermissions();
    
    if (!hasPermissions) {
      if (context.mounted) {
        // Show permission rationale dialog
        final shouldRequest = await showDialog<bool>(
          context: context,
          builder: (context) => AlertDialog(
            title: const Text('Bluetooth Permission Required'),
            content: const Text(
              'This app needs Bluetooth permission to scan for and connect to your ESP32 password manager.\n\n'
              'On Android 12+, you may also see a location permission request - this is required by Android for BLE scanning.',
            ),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(context, false),
                child: const Text('Cancel'),
              ),
              ElevatedButton(
                onPressed: () => Navigator.pop(context, true),
                child: const Text('Grant Permission'),
              ),
            ],
          ),
        );
        
        if (shouldRequest != true) return;
      }
      
      // Request permissions
      final result = await permissionService.requestPermissions();
      
      if (result == PermissionResult.denied) {
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text('Bluetooth permission is required to use this app'),
              duration: Duration(seconds: 3),
            ),
          );
        }
        return;
      } else if (result == PermissionResult.permanentlyDenied) {
        if (context.mounted) {
          final openSettings = await showDialog<bool>(
            context: context,
            builder: (context) => AlertDialog(
              title: const Text('Permission Required'),
              content: const Text(
                'Bluetooth permission has been permanently denied. '
                'Please enable it in app settings to use this feature.',
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.pop(context, false),
                  child: const Text('Cancel'),
                ),
                ElevatedButton(
                  onPressed: () => Navigator.pop(context, true),
                  child: const Text('Open Settings'),
                ),
              ],
            ),
          );
          
          if (openSettings == true) {
            await permissionService.openAppSettings();
          }
        }
        return;
      }
    }
    
    // Permissions granted, navigate to scan screen
    if (context.mounted) {
      Navigator.push(
        context,
        MaterialPageRoute(builder: (context) => const DeviceScanScreen()),
      );
    }
  }

  void _navigateToCredentials(BuildContext context) {
    Navigator.push(
      context,
      MaterialPageRoute(builder: (context) => const CredentialManagerScreen()),
    );
  }

  Future<void> _disconnect(BuildContext context, AppStateNotifier notifier) async {
    final confirm = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Disconnect'),
        content: const Text('Are you sure you want to disconnect from ESP32?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Disconnect'),
          ),
        ],
      ),
    );

    if (confirm == true) {
      await notifier.disconnect();
    }
  }

  Future<void> _unpairDevice(BuildContext context, AppStateNotifier notifier) async {
    final confirm = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Unpair Device'),
        content: const Text(
          'This will remove the pairing between this phone and the ESP32.\n\n'
          'You will need to pair again on next connection.\n\n'
          'Are you sure?'
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, true),
            style: TextButton.styleFrom(foregroundColor: Colors.red),
            child: const Text('Unpair'),
          ),
        ],
      ),
    );

    if (confirm == true) {
      try {
        await notifier.unpairDevice();
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text('Device unpaired successfully'),
              backgroundColor: Colors.green,
            ),
          );
        }
      } catch (e) {
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text('Failed to unpair: $e'),
              backgroundColor: Colors.red,
            ),
          );
        }
      }
    }
  }
}
