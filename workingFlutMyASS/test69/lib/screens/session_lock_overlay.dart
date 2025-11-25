import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import '../providers/app_state_provider.dart';

/// Build session lock overlay when session expires
Widget buildSessionLockOverlay(BuildContext context, AppStateNotifier notifier, appState) {
  return Container(
    color: Colors.black87,
    child: Center(
      child: Card(
        margin: const EdgeInsets.all(32),
        child: Padding(
          padding: const EdgeInsets.all(32.0),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(
                Icons.lock_clock,
                size: 80,
                color: Colors.orange.shade700,
              ),
              const SizedBox(height: 24),
              Text(
                'Session Expired',
                style: Theme.of(context).textTheme.headlineSmall?.copyWith(
                  fontWeight: FontWeight.bold,
                ),
              ),
              const SizedBox(height: 16),
              Text(
                'Your session has timed out due to inactivity.\nTap below to re-authenticate.',
                textAlign: TextAlign.center,
                style: Theme.of(context).textTheme.bodyMedium,
              ),
              const SizedBox(height: 24),
              ElevatedButton.icon(
                onPressed: () => _reauthenticate(context, notifier, appState),
                icon: const Icon(Icons.lock_open),
                label: const Text('Unlock'),
                style: ElevatedButton.styleFrom(
                  padding: const EdgeInsets.symmetric(horizontal: 32, vertical: 16),
                  backgroundColor: Colors.orange.shade700,
                  foregroundColor: Colors.white,
                ),
              ),
              const SizedBox(height: 12),
              TextButton(
                onPressed: () => Navigator.pop(context),
                child: const Text('Go Back'),
              ),
            ],
          ),
        ),
      ),
    ),
  );
}

/// Re-authenticate after session timeout
Future<void> _reauthenticate(BuildContext context, AppStateNotifier notifier, appState) async {
  final deviceId = appState.connectedDeviceId;
  final deviceName = appState.connectedDeviceName;
  
  if (deviceId == null) {
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Connection lost. Please reconnect from home screen.')),
    );
    Navigator.pop(context);
    return;
  }
  
  try {
    debugPrint('[SessionLock] Re-authenticating after session timeout...');
    
    // Use optimized reconnection: check_pairing + request_token + auth
    // This is fast (~1-2 seconds) and secure
    await notifier.connectAndAuthenticate(
      deviceId: deviceId,
      deviceName: deviceName ?? 'ESP32',
      pin: '123456', // Not used for reconnection, kept for compatibility
    );
    
    // Clear error
    notifier.clearError();
    
    if (context.mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('Session restored successfully'),
          backgroundColor: Colors.green,
        ),
      );
    }
    
    debugPrint('[SessionLock] Re-authentication successful');
  } catch (e) {
    debugPrint('[SessionLock] Re-authentication failed: $e');
    if (context.mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Re-authentication failed: $e'),
          backgroundColor: Colors.red,
        ),
      );
    }
  }
}
