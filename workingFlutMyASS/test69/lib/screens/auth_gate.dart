import 'package:flutter/material.dart';
import '../services/auth_service.dart';

class AuthGate extends StatefulWidget {
  final Widget child;
  final String reason;
  const AuthGate({required this.child, this.reason = 'Authenticate to continue', Key? key}) : super(key: key);

  @override
  State<AuthGate> createState() => _AuthGateState();
}

class _AuthGateState extends State<AuthGate> {
  bool? _authenticated;
  String? _error;

  @override
  void initState() {
    super.initState();
    _authenticate();
  }

  Future<void> _authenticate() async {
    setState(() {
      _error = null;
      _authenticated = null;
    });
    try {
      final result = await AuthService().authenticate(reason: widget.reason);
      setState(() {
        _authenticated = result;
      });
      if (!result) {
        setState(() {
          _error = 'Authentication failed or cancelled.';
        });
      }
    } catch (e) {
      setState(() {
        _authenticated = false;
        _error = 'Error: $e';
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    if (_authenticated == null) {
      return const Scaffold(body: Center(child: CircularProgressIndicator()));
    }
    if (_authenticated == true) {
      return widget.child;
    }
    return Scaffold(
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Text('Authentication required'),
            if (_error != null) ...[
              const SizedBox(height: 12),
              Text(_error!, style: const TextStyle(color: Colors.red)),
            ],
            const SizedBox(height: 16),
            ElevatedButton(
              onPressed: _authenticate,
              child: const Text('Try Again'),
            ),
          ],
        ),
      ),
    );
  }
}
