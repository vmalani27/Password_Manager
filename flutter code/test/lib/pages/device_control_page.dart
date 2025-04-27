import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:lemonicetea/services/device_state.dart' show DeviceState, Credential;
import 'package:flutter/services.dart';
import 'package:lemonicetea/theme/app_theme.dart' hide Credential;

class DeviceControlPage extends StatefulWidget {
  const DeviceControlPage({Key? key}) : super(key: key);

  @override
  State<DeviceControlPage> createState() => _DeviceControlPageState();
}

class _DeviceControlPageState extends State<DeviceControlPage> {
  final _siteController = TextEditingController();
  final _usernameController = TextEditingController();
  final _passwordController = TextEditingController();

  // Add this map to store visibility state for each credential
  final Map<String, ValueNotifier<bool>> _passwordVisibilityNotifiers = {};

  // Define color scheme for teal theme
  static const Color darkTeal = Color(0xFF003344);      // Dark teal background
  static const Color brightTeal = Color(0xFF008B9E);    // Bright teal
  static const Color lightTeal = Color(0xFF00B4D8);     // Light teal accent
  static const Color veryDarkTeal = Color(0xFF002233);  // Very dark teal

  @override
  void dispose() {
    _siteController.dispose();
    _usernameController.dispose();
    _passwordController.dispose();
    // Clean up notifiers
    for (var notifier in _passwordVisibilityNotifiers.values) {
      notifier.dispose();
    }
    _passwordVisibilityNotifiers.clear();
    super.dispose();
  }

  // Get or create visibility notifier for a credential
  ValueNotifier<bool> _getVisibilityNotifier(Credential credential) {
    final key = '${credential.site}_${credential.username}';
    return _passwordVisibilityNotifiers.putIfAbsent(
      key,
      () => ValueNotifier<bool>(false),
    );
  }

  Future<void> _showAddCredentialDialog() async {
    _siteController.clear();
    _usernameController.clear();
    _passwordController.clear();

    final result = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: darkTeal,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(16),
          side: BorderSide(
            color: Colors.white.withOpacity(0.2),
          ),
        ),
        title: const Text(
          'Add Credential',
          style: TextStyle(
            color: Colors.white,
            fontSize: 20,
            fontWeight: FontWeight.bold,
          ),
        ),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: _siteController,
              decoration: InputDecoration(
                labelText: 'Site',
                labelStyle: TextStyle(color: Colors.white.withOpacity(0.7)),
                enabledBorder: OutlineInputBorder(
                  borderSide: BorderSide(color: Colors.white.withOpacity(0.2)),
                  borderRadius: BorderRadius.circular(8),
                ),
                focusedBorder: OutlineInputBorder(
                  borderSide: BorderSide(color: lightTeal),
                  borderRadius: BorderRadius.circular(8),
                ),
              ),
              style: const TextStyle(color: Colors.white),
            ),
            const SizedBox(height: 16),
            TextField(
              controller: _usernameController,
              decoration: InputDecoration(
                labelText: 'Username',
                labelStyle: TextStyle(color: Colors.white.withOpacity(0.7)),
                enabledBorder: OutlineInputBorder(
                  borderSide: BorderSide(color: Colors.white.withOpacity(0.2)),
                  borderRadius: BorderRadius.circular(8),
                ),
                focusedBorder: OutlineInputBorder(
                  borderSide: BorderSide(color: lightTeal),
                  borderRadius: BorderRadius.circular(8),
                ),
              ),
              style: const TextStyle(color: Colors.white),
            ),
            const SizedBox(height: 16),
            TextField(
              controller: _passwordController,
              decoration: InputDecoration(
                labelText: 'Password',
                labelStyle: TextStyle(color: Colors.white.withOpacity(0.7)),
                enabledBorder: OutlineInputBorder(
                  borderSide: BorderSide(color: Colors.white.withOpacity(0.2)),
                  borderRadius: BorderRadius.circular(8),
                ),
                focusedBorder: OutlineInputBorder(
                  borderSide: BorderSide(color: lightTeal),
                  borderRadius: BorderRadius.circular(8),
                ),
              ),
              style: const TextStyle(color: Colors.white),
              obscureText: true,
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: Text(
              'Cancel',
              style: TextStyle(color: Colors.white.withOpacity(0.7)),
            ),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, true),
            child: Text(
              'Add',
              style: TextStyle(color: lightTeal),
            ),
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
    final oldPasswordController = TextEditingController();
    final newPasswordController = TextEditingController();
    bool isOldPasswordVerified = false;
    String? currentPassword;

    await showDialog<bool>(
      context: context,
      barrierDismissible: false,
      builder: (context) => StatefulBuilder(
        builder: (context, setState) {
          return AlertDialog(
            title: const Text('Update Credential'),
            content: SingleChildScrollView(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Text('Site: ${credential.site}'),
                  Text('Username: ${credential.username}'),
                  if (!isOldPasswordVerified) ...[
                    const SizedBox(height: 16),
                    TextField(
                      controller: oldPasswordController,
                      decoration: const InputDecoration(
                        labelText: 'Current Password',
                        hintText: 'Enter current password',
                        border: OutlineInputBorder(),
                      ),
                      obscureText: true,
                    ),
                    const SizedBox(height: 8),
                    ElevatedButton(
                      onPressed: () async {
                        try {
                          showDialog(
                            context: context,
                            barrierDismissible: false,
                            builder: (context) => const Center(
                              child: CircularProgressIndicator(),
                            ),
                          );

                          await context.read<DeviceState>().getCredential(
                            credential.site,
                            credential.username,
                          );

                          await Future.delayed(const Duration(milliseconds: 500));

                          final updatedCredential = context
                              .read<DeviceState>()
                              .credentials
                              .firstWhere(
                                (c) =>
                                    c.site == credential.site &&
                                    c.username == credential.username,
                              );

                          if (context.mounted) {
                            Navigator.pop(context);
                          }

                          if (updatedCredential.password == oldPasswordController.text) {
                            setState(() {
                              isOldPasswordVerified = true;
                              currentPassword = updatedCredential.password;
                            });
                            if (context.mounted) {
                              ScaffoldMessenger.of(context).showSnackBar(
                                const SnackBar(
                                  content: Text('Password verified successfully'),
                                  backgroundColor: Colors.green,
                                ),
                              );
                            }
                          } else {
                            if (context.mounted) {
                              ScaffoldMessenger.of(context).showSnackBar(
                                const SnackBar(
                                  content: Text('Incorrect current password'),
                                  backgroundColor: Colors.red,
                                ),
                              );
                            }
                          }
                        } catch (e) {
                          if (context.mounted && Navigator.canPop(context)) {
                            Navigator.pop(context);
                          }
                          
                          if (context.mounted) {
                            ScaffoldMessenger.of(context).showSnackBar(
                              SnackBar(
                                content: Text('Error verifying password: $e'),
                                backgroundColor: Colors.red,
                              ),
                            );
                          }
                        }
                      },
                      child: const Text('Verify Current Password'),
                    ),
                  ] else ...[
                    const SizedBox(height: 16),
                    TextField(
                      controller: newPasswordController,
                      decoration: const InputDecoration(
                        labelText: 'New Password',
                        hintText: 'Enter new password',
                        border: OutlineInputBorder(),
                      ),
                      obscureText: true,
                    ),
                  ],
                ],
              ),
            ),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(context, false),
                child: const Text('Cancel'),
              ),
              if (isOldPasswordVerified)
                TextButton(
                  onPressed: () async {
                    if (newPasswordController.text.isEmpty) {
                      ScaffoldMessenger.of(context).showSnackBar(
                        const SnackBar(
                          content: Text('Please enter a new password'),
                          backgroundColor: Colors.red,
                        ),
                      );
                      return;
                    }

                    try {
                      showDialog(
                        context: context,
                        barrierDismissible: false,
                        builder: (context) => const Center(
                          child: CircularProgressIndicator(),
                        ),
                      );

                      final deviceState = context.read<DeviceState>();
                      await deviceState.updateCredential(
                        credential.site,
                        credential.username,
                        newPasswordController.text,
                      );

                      if (context.mounted && Navigator.canPop(context)) {
                        Navigator.pop(context);
                      }

                      if (context.mounted) {
                        Navigator.pop(context, true);
                        ScaffoldMessenger.of(context).showSnackBar(
                          const SnackBar(
                            content: Text('Password updated successfully'),
                            backgroundColor: Colors.green,
                          ),
                        );
                      }
                    } catch (e) {
                      if (context.mounted && Navigator.canPop(context)) {
                        Navigator.pop(context);
                      }

                      if (context.mounted) {
                        ScaffoldMessenger.of(context).showSnackBar(
                          SnackBar(
                            content: Text('Error updating password: $e'),
                            backgroundColor: Colors.red,
                          ),
                        );
                      }
                    }
                  },
                  child: const Text('Update'),
                ),
            ],
          );
        },
      ),
    );

    // Clean up
    oldPasswordController.dispose();
    newPasswordController.dispose();
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
      // Create a gradient background
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [
              darkTeal,
              brightTeal,
            ],
          ),
        ),
        child: SafeArea(
          child: Padding(
            padding: const EdgeInsets.all(16.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // Welcome Header
                Container(
                  padding: const EdgeInsets.all(20),
                  decoration: BoxDecoration(
                    color: Colors.white.withOpacity(0.1),
                    borderRadius: BorderRadius.circular(16),
                    border: Border.all(
                      color: Colors.white.withOpacity(0.2),
                      width: 1,
                    ),
                    boxShadow: [
                      BoxShadow(
                        color: veryDarkTeal.withOpacity(0.3),
                        blurRadius: 15,
                        offset: const Offset(0, 5),
                      ),
                    ],
                  ),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Expanded(
                            child: Row(
                              children: [
                                Icon(
                                  Icons.security,
                                  color: Colors.white.withOpacity(0.9),
                                  size: 28,
                                ),
                                const SizedBox(width: 12),
                                const Flexible(
                                  child: Text(
                                    'Password Manager',
                                    style: TextStyle(
                                      color: Colors.white,
                                      fontSize: 24,
                                      fontWeight: FontWeight.bold,
                                      letterSpacing: 1,
                                    ),
                                    overflow: TextOverflow.ellipsis,
                                  ),
                                ),
                              ],
                            ),
                          ),
                          const SizedBox(width: 8),
                          IconButton(
                            icon: const Icon(
                              Icons.bluetooth_disabled,
                              color: Colors.red,
                              size: 24,
                            ),
                            onPressed: () async {
                              await context.read<DeviceState>().disconnect();
                              if (mounted) {
                                Navigator.of(context).pop();
                              }
                            },
                            tooltip: 'Disconnect',
                          ),
                        ],
                      ),
                      const SizedBox(height: 8),
                      Text(
                        'Secure Your Digital Life',
                        style: TextStyle(
                          color: Colors.white.withOpacity(0.7),
                          fontSize: 16,
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 24),

                // Password Stats Section
                Container(
                  padding: const EdgeInsets.all(20),
                  decoration: BoxDecoration(
                    color: Colors.white.withOpacity(0.1),
                    borderRadius: BorderRadius.circular(16),
                    border: Border.all(
                      color: Colors.white.withOpacity(0.2),
                      width: 1,
                    ),
                    boxShadow: [
                      BoxShadow(
                        color: veryDarkTeal.withOpacity(0.3),
                        blurRadius: 15,
                        offset: const Offset(0, 5),
                      ),
                    ],
                  ),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const Text(
                        'Password Health',
                        style: TextStyle(
                          color: Colors.white,
                          fontSize: 20,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      const SizedBox(height: 16),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceAround,
                        children: [
                          _buildStatCard(
                            'Total',
                            context.watch<DeviceState>().credentials.length.toString(),
                          ),
                          _buildStatCard(
                            'Strong',
                            '${(context.watch<DeviceState>().credentials.length * 0.7).floor()}',
                          ),
                          _buildStatCard(
                            'Weak',
                            '${(context.watch<DeviceState>().credentials.length * 0.3).floor()}',
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 24),

                // Credentials List
                Expanded(
                  child: Container(
                    padding: const EdgeInsets.all(20),
                    decoration: BoxDecoration(
                      color: Colors.white.withOpacity(0.1),
                      borderRadius: BorderRadius.circular(16),
                      border: Border.all(
                        color: Colors.white.withOpacity(0.2),
                        width: 1,
                      ),
                      boxShadow: [
                        BoxShadow(
                          color: veryDarkTeal.withOpacity(0.3),
                          blurRadius: 15,
                          offset: const Offset(0, 5),
                        ),
                      ],
                    ),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text(
                          'Stored Passwords',
                          style: TextStyle(
                            color: Colors.white,
                            fontSize: 20,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                        const SizedBox(height: 16),
                        Expanded(
                          child: Consumer<DeviceState>(
                            builder: (context, deviceState, _) {
                              return ListView.builder(
                                itemCount: deviceState.credentials.length,
                                itemBuilder: (context, index) {
                                  final credential = deviceState.credentials[index];
                                  return _buildCredentialCard(credential);
                                },
                              );
                            },
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: () => _showAddCredentialDialog(),
        backgroundColor: lightTeal,
        child: const Icon(Icons.add, color: Colors.white),
      ),
    );
  }

  Widget _buildStatCard(String title, String value) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      decoration: BoxDecoration(
        color: Colors.white.withOpacity(0.1),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: Colors.white.withOpacity(0.2),
          width: 1,
        ),
      ),
      child: Column(
        children: [
          Text(
            value,
            style: const TextStyle(
              color: Colors.white,
              fontSize: 24,
              fontWeight: FontWeight.bold,
            ),
          ),
          const SizedBox(height: 4),
          Text(
            title,
            style: TextStyle(
              color: Colors.white.withOpacity(0.7),
              fontSize: 14,
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildCredentialCard(Credential credential) {
    final isPasswordVisible = _getVisibilityNotifier(credential);

    return Container(
      margin: const EdgeInsets.only(bottom: 12),
      decoration: BoxDecoration(
        color: Colors.white.withOpacity(0.1),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: Colors.white.withOpacity(0.2),
          width: 1,
        ),
      ),
      child: ExpansionTile(
        leading: Container(
          padding: const EdgeInsets.all(8),
          decoration: BoxDecoration(
            color: lightTeal.withOpacity(0.2),
            borderRadius: BorderRadius.circular(8),
          ),
          child: const Icon(
            Icons.lock_outline,
            color: Colors.white,
          ),
        ),
        trailing: IconButton(
          icon: const Icon(
            Icons.delete_outline,
            color: Colors.red,
          ),
          onPressed: () => _showDeleteConfirmation(credential),
        ),
        title: Text(
          credential.site,
          style: const TextStyle(
            color: Colors.white,
            fontWeight: FontWeight.bold,
          ),
        ),
        subtitle: Text(
          credential.username,
          style: TextStyle(
            color: Colors.white.withOpacity(0.7),
          ),
        ),
        children: [
          Padding(
            padding: const EdgeInsets.all(16.0),
            child: Row(
              children: [
                const Text(
                  'Password: ',
                  style: TextStyle(
                    color: Colors.white,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                Expanded(
                  child: ValueListenableBuilder<bool>(
                    valueListenable: isPasswordVisible,
                    builder: (context, isVisible, _) {
                      return Text(
                        credential.password != null
                            ? (isVisible ? credential.password! : '•' * credential.password!.length)
                            : 'Click eye icon to show password',
                        style: const TextStyle(
                          color: Colors.white,
                          fontFamily: 'Monospace',
                        ),
                      );
                    },
                  ),
                ),
                IconButton(
                  icon: ValueListenableBuilder<bool>(
                    valueListenable: isPasswordVisible,
                    builder: (context, isVisible, _) => Icon(
                      isVisible ? Icons.visibility_off : Icons.visibility,
                      color: Colors.white.withOpacity(0.9),
                    ),
                  ),
                  onPressed: () => _togglePasswordVisibility(credential, isPasswordVisible),
                ),
                IconButton(
                  icon: Icon(
                    Icons.copy,
                    color: Colors.white.withOpacity(0.9),
                  ),
                  onPressed: () => _copyPassword(credential),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  // Helper methods
  Future<void> _togglePasswordVisibility(
    Credential credential,
    ValueNotifier<bool> isPasswordVisible,
  ) async {
    if (!isPasswordVisible.value && credential.password == null) {
      try {
        if (mounted) {
          showDialog(
            context: context,
            barrierDismissible: false,
            builder: (context) => const Center(
              child: CircularProgressIndicator(
                color: Colors.purple,
              ),
            ),
          );
        }

        await context.read<DeviceState>().getCredential(
          credential.site,
          credential.username,
        );

        if (mounted) {
          Navigator.of(context).pop();
          isPasswordVisible.value = true;
        }
      } catch (e) {
        if (mounted) {
          Navigator.of(context).pop();
        }
      }
    } else {
      isPasswordVisible.value = !isPasswordVisible.value;
    }
  }

  Future<void> _copyPassword(Credential credential) async {
    if (credential.password != null) {
      await Clipboard.setData(ClipboardData(text: credential.password!));
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: const Row(
              children: [
                Icon(
                  Icons.check_circle_outline,
                  color: Colors.white,
                ),
                SizedBox(width: 8),
                Text(
                  'Password copied',
                  style: TextStyle(color: Colors.white),
                ),
              ],
            ),
            duration: const Duration(seconds: 1),
            behavior: SnackBarBehavior.floating,
            backgroundColor: const Color.fromARGB(255, 6, 63, 74).withAlpha(229),
            margin: const EdgeInsets.all(8),
          ),
        );
      }
    }
  }
} 