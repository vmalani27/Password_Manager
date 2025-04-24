import 'package:flutter/foundation.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart' as reactive_ble;
import 'package:lemonicetea/services/bleservice.dart';

class Credential {
  final String site;
  final String username;
  final String? password;

  Credential({
    required this.site,
    required this.username,
    this.password,
  });
}

class DeviceState extends ChangeNotifier {
  final BLEService _bleService;
  String? _connectedDeviceId;
  String? _connectedDeviceName;
  bool _isConnecting = false;
  String? _error;
  List<Credential> _credentials = [];

  DeviceState(this._bleService);

  String? get connectedDeviceId => _connectedDeviceId;
  String? get connectedDeviceName => _connectedDeviceName;
  bool get isConnecting => _isConnecting;
  String? get error => _error;
  bool get isConnected => _connectedDeviceId != null;
  List<Credential> get credentials => _credentials;

  Future<void> connectToDevice(reactive_ble.DiscoveredDevice device) async {
    if (_isConnecting) {
      debugPrint('Already connecting to a device.');
      return;
    }

    debugPrint('Connecting to device: ${device.id}');
    _isConnecting = true;
    _error = null;
    notifyListeners();

    try {
      await _bleService.connectToDevice(device.id);
      _connectedDeviceId = device.id;
      _connectedDeviceName = device.name;
      _error = null;
      debugPrint('Connected to device: ${device.id}');
      await _refreshCredentials();
    } catch (e) {
      debugPrint('Error connecting to device: $e');
      _error = e.toString();
      _connectedDeviceId = null;
      _connectedDeviceName = null;
    } finally {
      _isConnecting = false;
      notifyListeners();
    }
  }

  Future<void> disconnect() async {
    try {
      await _bleService.disconnect();
      _connectedDeviceId = null;
      _connectedDeviceName = null;
      _error = null;
      _credentials.clear();
    } catch (e) {
      _error = e.toString();
    } finally {
      notifyListeners();
    }
  }

  Future<void> _refreshCredentials() async {
    debugPrint('Refreshing credentials...');
    try {
      final response = await _bleService.sendCommand('list');
      debugPrint('Received credentials response: $response');
      _parseCredentials(response);
    } catch (e) {
      debugPrint('Error refreshing credentials: $e');
      _error = e.toString();
    }
    notifyListeners();
  }

  void _parseCredentials(String response) {
    _credentials.clear();
    final lines = response.split('\n');
    for (final line in lines) {
      if (line.startsWith('Site: ')) {
        final parts = line.split(' | ');
        if (parts.length == 2) {
          final site = parts[0].substring(6); // Remove "Site: "
          final username = parts[1].substring(6); // Remove "User: "
          _credentials.add(Credential(site: site, username: username));
        } else {
          debugPrint('Invalid credential format: $line');
        }
      }
    }
  }

  Future<void> addCredential(String site, String username, String password) async {
    if (!isConnected) {
      _error = 'No device connected';
      notifyListeners();
      return;
    }

    try {
      final command = 'add $site $username $password';
      await _bleService.sendCommand(command);
      await _refreshCredentials();
    } catch (e) {
      _error = e.toString();
      notifyListeners();
    }
  }

  Future<void> getCredential(String site, String username) async {
    if (!isConnected) {
      _error = 'No device connected';
      notifyListeners();
      return;
    }

    try {
      final command = 'get $site $username';
      final response = await _bleService.sendCommand(command);
      if (response.startsWith('Password: ')) {
        final password = response.substring(10);
        _credentials = _credentials.map((cred) {
          if (cred.site == site && cred.username == username) {
            return Credential(site: site, username: username, password: password);
          }
          return cred;
        }).toList();
        notifyListeners();
      }
    } catch (e) {
      _error = e.toString();
      notifyListeners();
    }
  }

  Future<void> updateCredential(String site, String username, String newPassword) async {
    if (!isConnected) {
      _error = 'No device connected';
      notifyListeners();
      return;
    }

    try {
      final command = 'update $site $username $newPassword';
      await _bleService.sendCommand(command);
      await _refreshCredentials();
    } catch (e) {
      _error = e.toString();
      notifyListeners();
    }
  }

  Future<void> deleteCredential(String site, String username) async {
    if (!isConnected) {
      _error = 'No device connected';
      notifyListeners();
      return;
    }

    try {
      final command = 'delete $site $username';
      await _bleService.sendCommand(command);
      await _refreshCredentials();
    } catch (e) {
      _error = e.toString();
      notifyListeners();
    }
  }
}