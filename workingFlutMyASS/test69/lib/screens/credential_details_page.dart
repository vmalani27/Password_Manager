import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import '../widgets/action_button.dart';
import '../models/credential.dart';
import '../providers/app_state_provider.dart';

class CredentialDetailsPage extends StatefulWidget {
  final Credential credential;
  final AppStateNotifier notifier;

  const CredentialDetailsPage({Key? key, required this.credential, required this.notifier}) : super(key: key);

  @override
  State<CredentialDetailsPage> createState() => _CredentialDetailsPageState();
}

class _CredentialDetailsPageState extends State<CredentialDetailsPage> {
  bool _showPassword = false;
  Future<String>? _passwordFuture;
  String? _password;
  bool _loading = false;
  String? _error;

  void _togglePassword() async {
    if (_showPassword) {
      setState(() {
        _showPassword = false;
      });
      return;
    }
    setState(() {
      _loading = true;
      _error = null;
    });
    try {
      final pw = await widget.notifier.getPassword(
        service: widget.credential.service,
        identifier: widget.credential.username,
      );
      setState(() {
        _password = pw;
        _showPassword = true;
        _loading = false;
      });
    } catch (e) {
      setState(() {
        _error = e.toString();
        _loading = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final credential = widget.credential;
    return Scaffold(
      appBar: AppBar(
        title: Text(credential.service),
        actions: [
          IconButton(
            icon: const Icon(Icons.copy),
            tooltip: 'Copy Username',
            onPressed: () {
              Clipboard.setData(ClipboardData(text: credential.username));
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(content: Text('Username copied to clipboard')),
              );
            },
          ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(24.0),
        children: [
          ListTile(
            leading: const Icon(Icons.account_circle),
            title: const Text('Username'),
            subtitle: SelectableText(credential.username),
            trailing: IconButton(
              icon: const Icon(Icons.copy),
              tooltip: 'Copy Username',
              onPressed: () {
                Clipboard.setData(ClipboardData(text: credential.username));
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(content: Text('Username copied to clipboard')),
                );
              },
            ),
          ),
          const Divider(),
          ListTile(
            leading: const Icon(Icons.lock),
            title: const Text('Password'),
            subtitle: _loading
                ? const LinearProgressIndicator()
                : _error != null
                    ? Text('Error: $_error', style: const TextStyle(color: Colors.red))
                    : _showPassword
                        ? SelectableText(_password ?? '')
                        : const Text('••••••••'),
            trailing: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                IconButton(
                  icon: Icon(_showPassword ? Icons.visibility_off : Icons.visibility),
                  tooltip: _showPassword ? 'Hide Password' : 'Show Password',
                  onPressed: _togglePassword,
                ),
                if (_showPassword && _password != null)
                  IconButton(
                    icon: const Icon(Icons.copy),
                    tooltip: 'Copy Password',
                    onPressed: () {
                      Clipboard.setData(ClipboardData(text: _password!));
                      ScaffoldMessenger.of(context).showSnackBar(
                        const SnackBar(content: Text('Password copied to clipboard')),
                      );
                    },
                  ),
              ],
            ),
          ),
          const Divider(),
          ListTile(
            leading: const Icon(Icons.info_outline),
            title: const Text('Service'),
            subtitle: Text(credential.service),
          ),
          const SizedBox(height: 32),
          Row(
            children: [
              ActionButton(
                icon: Icons.edit,
                label: 'Update Password',
                onTap: () {
                  Navigator.pop(context);
                  // Optionally trigger update dialog here
                },
                expanded: true,
              ),
              const SizedBox(width: 16),
              ActionButton(
                icon: Icons.delete,
                label: 'Delete',
                backgroundColor: Colors.red.shade100,
                onTap: () {
                  Navigator.pop(context);
                  // Optionally trigger delete dialog here
                },
                expanded: true,
              ),
            ],
          ),
        ],
      ),
    );
  }
}
