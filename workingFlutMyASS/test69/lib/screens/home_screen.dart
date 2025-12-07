import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../providers/app_state_provider.dart';
import '../models/connection_state.dart' as models;
import '../models/auth_state.dart';
import '../services/permission_service.dart';
import '../services/pairing_service.dart';
import 'device_scan_screen.dart';
import 'credential_manager_screen.dart';
import 'auth_gate.dart';

/// Home screen - entry point of the app
/// Shows connection status and navigation options
class HomeScreen extends ConsumerWidget {
  const HomeScreen({Key? key}) : super(key: key);

  static bool preventAutoReconnect = false;

  Future<void> _autoReconnectIfPaired(WidgetRef ref, BuildContext context) async {
    if (preventAutoReconnect) return;
    final notifier = ref.read(appStateProvider.notifier);
    final pairingService = PairingService();
    final isPaired = await pairingService.isPaired();
    if (isPaired) {
      final device = await pairingService.getPairedDevice();
      if (device != null) {
        await notifier.connectAndAuthenticate(
          deviceId: device.deviceId,
          deviceName: device.deviceName,
          pin: '123456', // TODO: Replace with actual PIN logic if needed
        );
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text('Auto-reconnected to paired device'),
              backgroundColor: Colors.green,
            ),
          );
        }
      }
    }
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final appState = ref.watch(appStateProvider);
    final notifier = ref.read(appStateProvider.notifier);

    // Auto-reconnect logic: only runs once when widget builds and not already connected
    if (!appState.isReady && appState.connectedDeviceId == null && !preventAutoReconnect) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        _autoReconnectIfPaired(ref, context);
      });
    }

    // If session timed out, show reconnect/login UI only
    final sessionTimedOut = appState.authState == AuthState.unauthenticated || appState.connectionState != models.ConnectionState.authenticated;

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
                    if (appState.isReady && !sessionTimedOut) ...[
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
            // Paired device info (when disconnected but paired)
            if (!appState.isReady && !appState.isLoading && sessionTimedOut)
              FutureBuilder<bool>(
                future: _checkIfPaired(),
                builder: (context, snapshot) {
                  if (snapshot.hasData && snapshot.data == true) {
                    return FutureBuilder<Map<String, String>>(
                      future: _getPairedDeviceInfo(),
                      builder: (context, deviceSnapshot) {
                        if (deviceSnapshot.hasData) {
                          final deviceInfo = deviceSnapshot.data!;
                          return Column(
                            children: [
                              Card(
                                color: Colors.blue.shade50,
                                child: Padding(
                                  padding: const EdgeInsets.all(16.0),
                                  child: Column(
                                    crossAxisAlignment: CrossAxisAlignment.start,
                                    children: [
                                      Row(
                                        children: [
                                          Icon(Icons.devices, color: Colors.blue.shade700),
                                          const SizedBox(width: 12),
                                          Text(
                                            'Paired Device',
                                            style: TextStyle(
                                              fontSize: 16,
                                              fontWeight: FontWeight.bold,
                                              color: Colors.blue.shade700,
                                            ),
                                          ),
                                        ],
                                      ),
                                      const SizedBox(height: 12),
                                      _buildInfoRow('Device Name', deviceInfo['name'] ?? 'Unknown'),
                                      const SizedBox(height: 4),
                                      _buildInfoRow('Device ID', deviceInfo['id'] ?? 'Unknown'),
                                      const SizedBox(height: 4),
                                      _buildInfoRow('Paired On', deviceInfo['date'] ?? 'Unknown'),
                                      const SizedBox(height: 16),
                                      SizedBox(
                                        width: double.infinity,
                                        child: ElevatedButton.icon(
                                          onPressed: () => _reconnectToPairedDevice(context, notifier, deviceInfo),
                                          icon: const Icon(Icons.link),
                                          label: const Text('Reconnect to Paired Device'),
                                          style: ElevatedButton.styleFrom(
                                            padding: const EdgeInsets.all(12),
                                          ),
                                        ),
                                      ),
                                      const SizedBox(height: 8),
                                      SizedBox(
                                        width: double.infinity,
                                        child: OutlinedButton.icon(
                                          onPressed: () => _unpairFromHome(context, notifier),
                                          icon: const Icon(Icons.link_off),
                                          label: const Text('Unpair Device'),
                                          style: OutlinedButton.styleFrom(
                                            foregroundColor: Colors.red,
                                            padding: const EdgeInsets.all(12),
                                          ),
                                        ),
                                      ),
                                    ],
                                  ),
                                ),
                              ),
                              const SizedBox(height: 16),
                              const Text(
                                'OR',
                                style: TextStyle(
                                  color: Colors.grey,
                                  fontWeight: FontWeight.bold,
                                ),
                              ),
                              const SizedBox(height: 16),
                            ],
                          );
                        }
                        return const SizedBox.shrink();
                      },
                    );
                  }
                  return const SizedBox.shrink();
                },
              ),
            // Action buttons
            if (!appState.isReady || sessionTimedOut) ...[
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
              Row(
                children: [
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: () => _disconnect(context, notifier),
                      icon: const Icon(Icons.logout),
                      label: const Text('Disconnect'),
                      style: OutlinedButton.styleFrom(
                        padding: const EdgeInsets.all(16),
                      ),
                    ),
                  ),
                  const SizedBox(width: 8),
                  Tooltip(
                    message: 'Disconnect from ESP32.\nStays paired - you can reconnect without re-pairing.\nSession token will be cleared.',
                    child: Icon(Icons.info_outline, size: 20, color: Colors.grey[600]),
                  ),
                ],
              ),
              const SizedBox(height: 16),
              Row(
                children: [
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: () => _unpairDevice(context, notifier),
                      icon: const Icon(Icons.link_off),
                      label: const Text('Unpair Device'),
                      style: OutlinedButton.styleFrom(
                        padding: const EdgeInsets.all(16),
                        foregroundColor: Colors.red,
                      ),
                    ),
                  ),
                  const SizedBox(width: 8),
                  Tooltip(
                    message: 'Permanently unpair this device.\nErases ECDH keys from both phone and ESP32.\nRequires fresh pairing to reconnect.',
                    child: Icon(Icons.info_outline, size: 20, color: Colors.red[300]),
                  ),
                ],
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
      preventAutoReconnect = true;
      await notifier.disconnect();
      // Optionally reset flag after some navigation or timeout
    }
  }

  Future<void> _unpairDevice(BuildContext context, AppStateNotifier notifier) async {
    final confirm = await showDialog<bool>(
      context: context,
      builder: (context) => AuthGate(
        reason: 'Authenticate to unpair device',
        child: AlertDialog(
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
      ),
    );

    if (confirm == true) {
      try {
        await notifier.unpairDevice();
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text('Device unpaired successfully. Returning to device discovery...'),
              backgroundColor: Colors.green,
              duration: Duration(seconds: 2),
            ),
          );
          // Navigate back to device discovery screen after unpair
          Navigator.of(context).pushReplacement(
            MaterialPageRoute(builder: (context) => const DeviceScanScreen()),
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

  /// Check if a device is currently paired
  Future<bool> _checkIfPaired() async {
    try {
      final pairingService = PairingService();
      return await pairingService.isPaired();
    } catch (e) {
      return false;
    }
  }

  /// Get paired device information
  Future<Map<String, String>> _getPairedDeviceInfo() async {
    try {
      final pairingService = PairingService();
      final device = await pairingService.getPairedDevice();
      
      if (device != null) {
        return {
          'name': device.deviceName,
          'id': device.deviceId,
          'date': '${device.pairedAt.year}-${device.pairedAt.month.toString().padLeft(2, '0')}-${device.pairedAt.day.toString().padLeft(2, '0')}',
        };
      }
    } catch (e) {
      debugPrint('[HomeScreen] Error getting paired device info: $e');
    }
    
    return {};
  }

  /// Build an info row with label and value
  Widget _buildInfoRow(String label, String value) {
    return Row(
      children: [
        Text(
          '$label: ',
          style: const TextStyle(
            fontWeight: FontWeight.w500,
            color: Colors.grey,
          ),
        ),
        Expanded(
          child: Text(
            value,
            style: const TextStyle(fontWeight: FontWeight.w500),
            overflow: TextOverflow.ellipsis,
          ),
        ),
      ],
    );
  }

  /// Reconnect to the paired device
  Future<void> _reconnectToPairedDevice(
    BuildContext context,
    AppStateNotifier notifier,
    Map<String, String> deviceInfo,
  ) async {
    try {
      await notifier.connectAndAuthenticate(
        deviceId: deviceInfo['id']!,
        deviceName: deviceInfo['name']!,
        pin: '123456',
      );
      
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Reconnected successfully'),
            backgroundColor: Colors.green,
          ),
        );
      }
    } catch (e) {
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Reconnection failed: $e'),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }

  /// Unpair device from home screen (when disconnected)
  Future<void> _unpairFromHome(BuildContext context, AppStateNotifier notifier) async {
    debugPrint('[HomeScreen] _unpairFromHome called');
    
    final confirm = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Unpair Device'),
        content: const Text(
          'This will remove the pairing between this phone and the ESP32.\n\n'
          'You will need to scan and pair again on next connection.\n\n'
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

    debugPrint('[HomeScreen] User confirmation: $confirm');

    if (confirm == true) {
      try {
        debugPrint('[HomeScreen] Starting unpair process (disconnected state)...');
        
        // Remove local pairing data
        final pairingService = PairingService();
        await pairingService.removePairing();
        debugPrint('[HomeScreen] Local pairing data removed');
        
        // Force a state refresh to update UI immediately
        // Use a small delay to ensure file system writes complete
        await Future.delayed(const Duration(milliseconds: 100));
        debugPrint('[HomeScreen] Triggering state refresh...');
        
        // Trigger a state update to refresh the UI
        notifier.clearError();
        debugPrint('[HomeScreen] State refresh triggered');
        
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text('Device unpaired successfully'),
              backgroundColor: Colors.green,
            ),
          );
          debugPrint('[HomeScreen] Success SnackBar shown');
        }
      } catch (e) {
        debugPrint('[HomeScreen] Unpair error: $e');
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
