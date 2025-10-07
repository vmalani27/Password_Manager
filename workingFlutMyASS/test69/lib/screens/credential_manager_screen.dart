import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../providers/app_state_provider.dart';
import '../models/credential.dart';

/// Credential manager screen - view, add, edit, delete credentials
class CredentialManagerScreen extends ConsumerStatefulWidget {
  const CredentialManagerScreen({Key? key}) : super(key: key);

  @override
  ConsumerState<CredentialManagerScreen> createState() => _CredentialManagerScreenState();
}

class _CredentialManagerScreenState extends ConsumerState<CredentialManagerScreen> {
  @override
  void initState() {
    super.initState();
    // Refresh credentials when screen opens
    WidgetsBinding.instance.addPostFrameCallback((_) {
      ref.read(appStateProvider.notifier).refreshCredentials();
    });
  }

  @override
  Widget build(BuildContext context) {
    final appState = ref.watch(appStateProvider);
    final notifier = ref.read(appStateProvider.notifier);

    return Scaffold(
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
      body: Column(
        children: [
          // Loading indicator
          if (appState.isLoading)
            const LinearProgressIndicator(),

          // Error message
          if (appState.errorMessage != null)
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

          // Credential count
          Padding(
            padding: const EdgeInsets.all(16.0),
            child: Text(
              '${appState.credentials.length} credential(s) stored',
              style: Theme.of(context).textTheme.bodyLarge,
            ),
          ),

          // Credential list
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
      floatingActionButton: FloatingActionButton(
        onPressed: () => _showAddCredentialDialog(context, notifier),
        child: const Icon(Icons.add),
        tooltip: 'Add Credential',
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
        title: Text(credential.site),
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
        site: credential.site,
        username: credential.username,
      );

      if (context.mounted) {
        showDialog(
          context: context,
          builder: (context) => AlertDialog(
            title: Text(credential.site),
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
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Failed to get password: $e')),
        );
      }
    }
  }

  Future<void> _showAddCredentialDialog(
    BuildContext context,
    AppStateNotifier notifier,
  ) async {
    final siteController = TextEditingController();
    final usernameController = TextEditingController();
    final passwordController = TextEditingController();

    final result = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Add Credential'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: siteController,
              decoration: const InputDecoration(labelText: 'Site'),
            ),
            TextField(
              controller: usernameController,
              decoration: const InputDecoration(labelText: 'Username'),
            ),
            TextField(
              controller: passwordController,
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
          ElevatedButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Add'),
          ),
        ],
      ),
    );

    if (result == true) {
      try {
        await notifier.addCredential(
          site: siteController.text.trim(),
          username: usernameController.text.trim(),
          password: passwordController.text,
        );
        
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Credential added successfully')),
          );
        }
      } catch (e) {
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Failed to add credential: $e')),
          );
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
            Text('Site: ${credential.site}'),
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
          site: credential.site,
          username: credential.username,
          newPassword: passwordController.text,
        );
        
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Password updated successfully')),
          );
        }
      } catch (e) {
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Failed to update password: $e')),
          );
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
        content: Text('Delete credential for ${credential.site}?'),
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
          site: credential.site,
          username: credential.username,
        );
        
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Credential deleted successfully')),
          );
        }
      } catch (e) {
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Failed to delete credential: $e')),
          );
        }
      }
    }
  }
}
