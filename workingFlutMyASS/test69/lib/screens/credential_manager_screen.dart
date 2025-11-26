import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter/foundation.dart';
import 'package:test69/models/auth_state.dart';
import '../providers/app_state_provider.dart';
import '../models/credential.dart';
import '../models/connection_state.dart' as app_conn;
import 'session_lock_overlay.dart';

/// Credential manager screen - view, add, edit, delete credentials
class CredentialManagerScreen extends ConsumerStatefulWidget {
  const CredentialManagerScreen({Key? key}) : super(key: key);

  @override
  ConsumerState<CredentialManagerScreen> createState() => _CredentialManagerScreenState();
}

class _CredentialManagerScreenState extends ConsumerState<CredentialManagerScreen> with WidgetsBindingObserver {
  // Heartbeat test to check connection
  Future<bool> _heartbeatTest() async {
    final notifier = ref.read(appStateProvider.notifier);
    final appState = ref.read(appStateProvider);
    if (appState.connectedDeviceId != null) {
      try {
        return await notifier.bleService.isDeviceConnected(appState.connectedDeviceId!);
      } catch (_) {
        return false;
      }
    }
    return false;
  }

  // WillPop handler for connection check
  Future<bool> _onWillPop() async {
    final isConnected = await _heartbeatTest();
    if (!isConnected && context.mounted) {
      await showDialog(
        context: context,
        builder: (context) => AlertDialog(
          title: const Text('Connection Lost'),
          content: const Text('Connection to ESP32 is no longer active. Please reconnect or go to device discovery.'),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context),
              child: const Text('OK'),
            ),
          ],
        ),
      );
    }
    return true;
  }

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    // Refresh credentials when screen opens
    WidgetsBinding.instance.addPostFrameCallback((_) {
      ref.read(appStateProvider.notifier).refreshCredentials();
    });
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    super.didChangeAppLifecycleState(state);
    if (state == AppLifecycleState.resumed) {
      final notifier = ref.read(appStateProvider.notifier);
      final appState = ref.read(appStateProvider);
      // Check BLE connection status when app resumes
      if (appState.connectionState == app_conn.ConnectionState.authenticated) {
        notifier.bleService.isDeviceConnected(appState.connectedDeviceId ?? '').then((isConnected) {
          if (!isConnected) {
            notifier.disconnect();
          }
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final appState = ref.watch(appStateProvider);
    final notifier = ref.read(appStateProvider.notifier);
    return WillPopScope(
      onWillPop: _onWillPop,
      child: Scaffold(
        appBar: AppBar(
          title: const Text('Credentials'),
          actions: [
            IconButton(
              icon: const Icon(Icons.refresh),
              onPressed: () => notifier.refreshCredentials(),
              tooltip: 'Refresh',
            ),
          ],
        ),
        body: Stack(
          children: [
            if (appState.connectionState == app_conn.ConnectionState.disconnected || appState.authState == AuthState.unauthenticated)
              Center(
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(Icons.bluetooth_disabled, size: 64, color: Colors.grey),
                    const SizedBox(height: 16),
                    Text(
                      'Not connected to device',
                      style: Theme.of(context).textTheme.titleLarge,
                    ),
                    const SizedBox(height: 8),
                    Text(
                      'Please connect to your ESP32 to view credentials.',
                      style: Theme.of(context).textTheme.bodyMedium,
                    ),
                  ],
                ),
              )
            else
              Column(
                children: [
                  if (appState.isLoading)
                    const LinearProgressIndicator(),
                  if (appState.errorMessage != null && appState.isReady)
                    Container(
                      color: Colors.red.shade50,
                      width: double.infinity,
                      padding: const EdgeInsets.all(16),
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
                  Padding(
                    padding: const EdgeInsets.all(16.0),
                    child: Text(
                      '${appState.credentials.length} credential(s) stored',
                      style: Theme.of(context).textTheme.bodyLarge,
                    ),
                  ),
                  Expanded(
                    child: appState.credentials.isEmpty
                        ? const Center(
                            child: Text('No credentials stored.\nTap + to add one.'),
                          )
                        : ListView.builder(
                            itemCount: appState.credentials.length,
                            itemBuilder: (context, index) {
                              final credential = appState.credentials[index];
                              return _buildCredentialTile(context, credential, notifier);
                            },
                          ),
                  ),
                ],
              ),
            if (!appState.isReady && appState.errorMessage != null && 
                appState.errorMessage!.contains('Session expired'))
              buildSessionLockOverlay(context, notifier, appState),
          ],
        ),
        floatingActionButton: (appState.connectionState == app_conn.ConnectionState.authenticated && appState.authState == AuthState.authenticated)
            ? FloatingActionButton(
                onPressed: () => _showAddCredentialDialog(context, notifier),
                child: const Icon(Icons.add),
                tooltip: 'Add Credential',
              )
            : null,
      ),
    );
  }

  Widget _buildCredentialTile(
    BuildContext context,
    Credential credential,
    AppStateNotifier notifier,
  ) {
    return Card(
      margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: ListTile(
        leading: const Icon(Icons.vpn_key),
        title: Text(credential.service),
        subtitle: Text(credential.username),
        trailing: PopupMenuButton(
          itemBuilder: (context) => [
            const PopupMenuItem(
              value: 'view',
              child: Row(
                children: [
                  Icon(Icons.visibility),
                  SizedBox(width: 8),
                  Text('View Password'),
                ],
              ),
            ),
            const PopupMenuItem(
              value: 'update',
              child: Row(
                children: [
                  Icon(Icons.edit),
                  SizedBox(width: 8),
                  Text('Update Password'),
                ],
              ),
            ),
            const PopupMenuItem(
              value: 'delete',
              child: Row(
                children: [
                  Icon(Icons.delete),
                  SizedBox(width: 8),
                  Text('Delete'),
                ],
              ),
            ),
          ],
          onSelected: (value) {
            switch (value) {
              case 'view':
                _viewPassword(context, credential, notifier);
                break;
              case 'update':
                _showUpdatePasswordDialog(context, credential, notifier);
                break;
              case 'delete':
                _deleteCredential(context, credential, notifier);
                break;
            }
          },
        ),
      ),
    );
  }

  Future<void> _viewPassword(
    BuildContext context,
    Credential credential,
    AppStateNotifier notifier,
  ) async {
    try {
      final password = await notifier.getPassword(
        service: credential.service,
        identifier: credential.identifier,
      );

      if (context.mounted) {
        showDialog(
          context: context,
          builder: (context) => AlertDialog(
            title: Text(credential.service),
            content: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text('Username: ${credential.username}'),
                const SizedBox(height: 16),
                Text('Password: $password'),
                const SizedBox(height: 16),
                ElevatedButton.icon(
                  onPressed: () {
                    Clipboard.setData(ClipboardData(text: password));
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(content: Text('Password copied to clipboard')),
                    );
                  },
                  icon: const Icon(Icons.copy),
                  label: const Text('Copy Password'),
                ),
              ],
            ),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(context),
                child: const Text('Close'),
              ),
            ],
          ),
        );
      }
    } catch (e) {
      if (context.mounted) {
        final errorMsg = e.toString();
        if (errorMsg.contains('SESSION_TIMEOUT') || errorMsg.contains('NOT AUTHORIZED')) {
          showDialog(
            context: context,
            builder: (context) => AlertDialog(
              title: const Text('Session Expired'),
              content: const Text('Your session has expired or you have been unpaired from the device. Please reconnect or discover a new device.'),
              actions: [
                TextButton(
                  onPressed: () {
                    Navigator.pop(context);
                    Navigator.of(context).pushReplacementNamed('/deviceScan');
                  },
                  child: const Text('Go to Device Discovery'),
                ),
              ],
            ),
          );
        } else {
          final msg = errorMsg.replaceFirst('Exception: ', '');
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text(msg),
              backgroundColor: Colors.red,
            ),
          );
        }
      }
    }
  }

  Future<void> _showAddCredentialDialog(
    BuildContext context,
    AppStateNotifier notifier,
  ) async {
    final serviceController = TextEditingController();
    final identifierController = TextEditingController();
    final passwordController = TextEditingController();
    bool passwordVisible = false;

    final result = await showDialog<bool>(
      context: context,
      builder: (context) {
        return StatefulBuilder(
          builder: (context, setState) => AlertDialog(
            title: const Text('Add Credential'),
            content: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                TextField(
                  controller: serviceController,
                  decoration: const InputDecoration(labelText: 'Service'),
                ),
                TextField(
                  controller: identifierController,
                  decoration: const InputDecoration(labelText: 'Username'),
                ),
                TextField(
                  controller: passwordController,
                  decoration: InputDecoration(
                    labelText: 'Password',
                    suffixIcon: IconButton(
                      icon: Icon(
                        passwordVisible ? Icons.visibility : Icons.visibility_off,
                      ),
                      onPressed: () => setState(() => passwordVisible = !passwordVisible),
                    ),
                  ),
                  obscureText: !passwordVisible,
                ),
              ],
            ),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(context, false),
                child: const Text('Cancel'),
              ),
              ElevatedButton(
                onPressed: () => Navigator.pop(context, true),
                child: const Text('Add'),
              ),
            ],
          ),
        );
      },
    );

    if (result == true) {
      try {
        await notifier.addCredential(
          service: serviceController.text.trim(),
          identifier: identifierController.text.trim(),
          password: passwordController.text,
        );
        
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Credential added successfully')),
          );
        }
      } catch (e) {
        if (context.mounted) {
          final errorMsg = e.toString();
          if (errorMsg.contains('SESSION_TIMEOUT') || errorMsg.contains('NOT AUTHORIZED')) {
            showDialog(
              context: context,
              builder: (context) => AlertDialog(
                title: const Text('Session Expired'),
                content: const Text('Your session has expired or you have been unpaired from the device. Please reconnect or discover a new device.'),
                actions: [
                  TextButton(
                    onPressed: () {
                      Navigator.pop(context);
                      Navigator.of(context).pushReplacementNamed('/deviceScan');
                    },
                    child: const Text('Go to Device Discovery'),
                  ),
                ],
              ),
            );
          } else {
            final msg = errorMsg.replaceFirst('Exception: ', '');
            ScaffoldMessenger.of(context).showSnackBar(
              SnackBar(
                content: Text(msg),
                backgroundColor: Colors.red,
              ),
            );
          }
        }
      }
    }
  }

  Future<void> _showUpdatePasswordDialog(
    BuildContext context,
    Credential credential,
    AppStateNotifier notifier,
  ) async {
    final passwordController = TextEditingController();

    final result = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Update Password'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Service: ${credential.service}'),
            Text('Username: ${credential.username}'),
            const SizedBox(height: 16),
            TextField(
              controller: passwordController,
              decoration: const InputDecoration(labelText: 'New Password'),
              obscureText: true,
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Update'),
          ),
        ],
      ),
    );

    if (result == true && passwordController.text.isNotEmpty) {
      try {
        await notifier.updateCredential(
          service: credential.service,
          identifier: credential.identifier,
          newPassword: passwordController.text,
        );
        
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Password updated successfully')),
          );
        }
      } catch (e) {
        if (context.mounted) {
          final errorMsg = e.toString();
          if (errorMsg.contains('SESSION_TIMEOUT') || errorMsg.contains('NOT AUTHORIZED')) {
            showDialog(
              context: context,
              builder: (context) => AlertDialog(
                title: const Text('Session Expired'),
                content: const Text('Your session has expired or you have been unpaired from the device. Please reconnect or discover a new device.'),
                actions: [
                  TextButton(
                    onPressed: () {
                      Navigator.pop(context);
                      Navigator.of(context).pushReplacementNamed('/deviceScan');
                    },
                    child: const Text('Go to Device Discovery'),
                  ),
                ],
              ),
            );
          } else {
            final msg = errorMsg.replaceFirst('Exception: ', '');
            ScaffoldMessenger.of(context).showSnackBar(
              SnackBar(
                content: Text(msg),
                backgroundColor: Colors.red,
              ),
            );
          }
        }
      }
    }
  }

  Future<void> _deleteCredential(
    BuildContext context,
    Credential credential,
    AppStateNotifier notifier,
  ) async {
    final confirm = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Delete Credential'),
        content: Text('Delete credential for ${credential.service}?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, true),
            style: TextButton.styleFrom(foregroundColor: Colors.red),
            child: const Text('Delete'),
          ),
        ],
      ),
    );

    if (confirm == true) {
      try {
        await notifier.deleteCredential(
          service: credential.service,
          identifier: credential.identifier,
        );
        
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Credential deleted successfully')),
          );
        }
      } catch (e) {
        if (context.mounted) {
          final errorMsg = e.toString();
          if (errorMsg.contains('SESSION_TIMEOUT') || errorMsg.contains('NOT AUTHORIZED')) {
            showDialog(
              context: context,
              builder: (context) => AlertDialog(
                title: const Text('Session Expired'),
                content: const Text('Your session has expired or you have been unpaired from the device. Please reconnect or discover a new device.'),
                actions: [
                  TextButton(
                    onPressed: () {
                      Navigator.pop(context);
                      Navigator.of(context).pushReplacementNamed('/deviceScan');
                    },
                    child: const Text('Go to Device Discovery'),
                  ),
                ],
              ),
            );
          } else {
            final msg = errorMsg.replaceFirst('Exception: ', '');
            ScaffoldMessenger.of(context).showSnackBar(
              SnackBar(
                content: Text(msg),
                backgroundColor: Colors.red,
              ),
            );
          }
        }
      }
    }
  }
}
