import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:lemonicetea/services/device_state.dart';

class DeviceControlPage extends StatefulWidget {
  const DeviceControlPage({Key? key}) : super(key: key);

  @override
  State<DeviceControlPage> createState() => _DeviceControlPageState();
}

class _DeviceControlPageState extends State<DeviceControlPage> {
  final _siteController = TextEditingController();
  final _usernameController = TextEditingController();
  final _passwordController = TextEditingController();

  @override
  void dispose() {
    _siteController.dispose();
    _usernameController.dispose();
    _passwordController.dispose();
    super.dispose();
  }

  Future<void> _showAddCredentialDialog() async {
    _siteController.clear();
    _usernameController.clear();
    _passwordController.clear();

    final result = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Add Credential'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: _siteController,
              decoration: const InputDecoration(labelText: 'Site'),
            ),
            TextField(
              controller: _usernameController,
              decoration: const InputDecoration(labelText: 'Username'),
            ),
            TextField(
              controller: _passwordController,
              decoration: const InputDecoration(labelText: 'Password'),
              obscureText: true,
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Add'),
          ),
        ],
      ),
    );

    if (result == true) {
      final deviceState = context.read<DeviceState>();
      await deviceState.addCredential(
        _siteController.text,
        _usernameController.text,
        _passwordController.text,
      );
    }
  }

  Future<void> _showGetCredentialDialog() async {
    _siteController.clear();
    _usernameController.clear();

    final result = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Get Credential'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: _siteController,
              decoration: const InputDecoration(labelText: 'Site'),
            ),
            TextField(
              controller: _usernameController,
              decoration: const InputDecoration(labelText: 'Username'),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Get'),
          ),
        ],
      ),
    );

    if (result == true) {
      final deviceState = context.read<DeviceState>();
      await deviceState.getCredential(
        _siteController.text,
        _usernameController.text,
      );
    }
  }

  Future<void> _showUpdateCredentialDialog(Credential credential) async {
    _passwordController.clear();

    final result = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Update Credential'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text('Site: ${credential.site}'),
            Text('Username: ${credential.username}'),
            TextField(
              controller: _passwordController,
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
          TextButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Update'),
          ),
        ],
      ),
    );

    if (result == true) {
      final deviceState = context.read<DeviceState>();
      await deviceState.updateCredential(
        credential.site,
        credential.username,
        _passwordController.text,
      );
    }
  }

  Future<void> _showDeleteConfirmation(Credential credential) async {
    final result = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Delete Credential'),
        content: Text(
          'Are you sure you want to delete the credential for ${credential.site} (${credential.username})?',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Delete'),
          ),
        ],
      ),
    );

    if (result == true) {
      final deviceState = context.read<DeviceState>();
      await deviceState.deleteCredential(
        credential.site,
        credential.username,
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Device Control'),
        actions: [
          IconButton(
            icon: const Icon(Icons.bluetooth_disabled),
            onPressed: () {
              context.read<DeviceState>().disconnect();
              Navigator.pop(context);
            },
          ),
        ],
      ),
      body: Consumer<DeviceState>(
        builder: (context, deviceState, child) {
          return Column(
            children: [
              // Connection Status
              Container(
                padding: const EdgeInsets.all(16),
                color: deviceState.isConnected
                    ? Colors.green.withOpacity(0.1)
                    : Colors.red.withOpacity(0.1),
                child: Row(
                  children: [
                    Icon(
                      deviceState.isConnected
                          ? Icons.bluetooth_connected
                          : Icons.bluetooth_disabled,
                      color: deviceState.isConnected ? Colors.green : Colors.red,
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            deviceState.isConnected
                                ? 'Connected to ${deviceState.connectedDeviceName}'
                                : 'Not Connected',
                            style: Theme.of(context).textTheme.titleMedium,
                          ),
                          if (deviceState.connectedDeviceId != null)
                            Text(
                              'Device ID: ${deviceState.connectedDeviceId}',
                              style: Theme.of(context).textTheme.bodySmall,
                            ),
                        ],
                      ),
                    ),
                  ],
                ),
              ),

              // Error Message
              if (deviceState.error != null)
                Container(
                  padding: const EdgeInsets.all(16),
                  margin: const EdgeInsets.all(8),
                  decoration: BoxDecoration(
                    color: Colors.red.withOpacity(0.1),
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(color: Colors.red.withOpacity(0.3)),
                  ),
                  child: Text(
                    deviceState.error!,
                    style: const TextStyle(color: Colors.red),
                  ),
                ),

              // Credential List
              Expanded(
                child: ListView.builder(
                  padding: const EdgeInsets.all(16),
                  itemCount: deviceState.credentials.length,
                  itemBuilder: (context, index) {
                    final credential = deviceState.credentials[index];
                    return Card(
                      child: ListTile(
                        leading: const Icon(Icons.lock),
                        title: Text(credential.site),
                        subtitle: Text(credential.username),
                        trailing: Row(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            if (credential.password != null)
                              IconButton(
                                icon: const Icon(Icons.visibility),
                                onPressed: () {
                                  ScaffoldMessenger.of(context).showSnackBar(
                                    SnackBar(
                                      content: Text('Password: ${credential.password}'),
                                      duration: const Duration(seconds: 2),
                                    ),
                                  );
                                },
                              ),
                            IconButton(
                              icon: const Icon(Icons.edit),
                              onPressed: () => _showUpdateCredentialDialog(credential),
                            ),
                            IconButton(
                              icon: const Icon(Icons.delete),
                              onPressed: () => _showDeleteConfirmation(credential),
                            ),
                          ],
                        ),
                      ),
                    );
                  },
                ),
              ),
            ],
          );
        },
      ),
      floatingActionButton: Column(
        mainAxisAlignment: MainAxisAlignment.end,
        children: [
          FloatingActionButton(
            heroTag: 'get',
            onPressed: _showGetCredentialDialog,
            child: const Icon(Icons.search),
          ),
          const SizedBox(height: 16),
          FloatingActionButton(
            heroTag: 'add',
            onPressed: _showAddCredentialDialog,
            child: const Icon(Icons.add),
          ),
        ],
      ),
    );
  }
} 