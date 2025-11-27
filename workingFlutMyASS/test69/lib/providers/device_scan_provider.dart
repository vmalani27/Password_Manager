import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'app_state_provider.dart';

/// State for device scanning
class DeviceScanState {
  final List<DiscoveredDevice> devices;
  final bool isScanning;
  final bool isConnecting;
  final String? errorMessage;

  const DeviceScanState({
    this.devices = const [],
    this.isScanning = false,
    this.isConnecting = false,
    this.errorMessage,
  });

  DeviceScanState copyWith({
    List<DiscoveredDevice>? devices,
    bool? isScanning,
    bool? isConnecting,
    String? errorMessage,
    bool clearError = false,
  }) {
    return DeviceScanState(
      devices: devices ?? this.devices,
      isScanning: isScanning ?? this.isScanning,
      isConnecting: isConnecting ?? this.isConnecting,
      errorMessage: clearError ? null : (errorMessage ?? this.errorMessage),
    );
  }
}

/// Provider for device scanning state
final deviceScanProvider = StateNotifierProvider<DeviceScanNotifier, DeviceScanState>((ref) {
  return DeviceScanNotifier(ref);
});

class DeviceScanNotifier extends StateNotifier<DeviceScanState> {
  final Ref _ref;
  bool _bluetoothReady = false;

  DeviceScanNotifier(this._ref) : super(const DeviceScanState());

  bool get bluetoothReady => _bluetoothReady;

  /// Start scanning for ESP32 devices
  Future<void> startScan({Duration timeout = const Duration(seconds: 10), void Function()? onBluetoothReady, void Function()? onDeviceFound}) async {
    state = state.copyWith(
      isScanning: true,
      clearError: true,
      devices: [],
    );

    try {
      final appNotifier = _ref.read(appStateProvider.notifier);
      final bleService = _ref.read(bleConnectionServiceProvider);
      final devices = await appNotifier.scanForDevices(timeout: timeout);
      bool foundEsp32 = false;
      for (final device in devices) {
        if (!_bluetoothReady) {
          _bluetoothReady = true;
          if (onBluetoothReady != null) onBluetoothReady();
        }
        // Add device to state
        state = state.copyWith(devices: [...state.devices, device]);
        // If first ESP32 device found, stop scan and exit loop
        if (device.name.contains('ESP32')) {
          foundEsp32 = true;
          bleService.stopScan();
          state = state.copyWith(isScanning: false);
          if (onDeviceFound != null) onDeviceFound();
          // Exit loop and prevent further state updates
          break;
        }
      }
      // Only update state if no ESP32 was found
      if (!foundEsp32) {
        state = state.copyWith(isScanning: false);
      }
    } catch (e) {
      debugPrint('[DeviceScan] Scan failed: $e');
      state = state.copyWith(
        isScanning: false,
        errorMessage: _formatErrorMessage(e),
      );
    }
  }

  /// Connect to a device
  Future<bool> connectToDevice(DiscoveredDevice device) async {
    state = state.copyWith(
      isConnecting: true,
      clearError: true,
    );

    try {
      final appNotifier = _ref.read(appStateProvider.notifier);
      
      final isNewPairing = await appNotifier.connectAndAuthenticate(
        deviceId: device.id,
        deviceName: device.name,
        pin: '123456',
      );

      state = state.copyWith(isConnecting: false);
      return isNewPairing;
      
    } catch (e) {
      state = state.copyWith(
        isConnecting: false,
        errorMessage: 'Connection failed: $e',
      );
      rethrow;
    }
  }

  /// Clear error message
  void clearError() {
    state = state.copyWith(clearError: true);
  }

  String _formatErrorMessage(dynamic error) {
    final errorStr = error.toString();
    
    if (errorStr.contains('Bluetooth is not ready')) {
      return 'Bluetooth is not enabled. Please enable Bluetooth and try again.';
    } else if (errorStr.contains('permission')) {
      return 'Bluetooth permission denied. Please grant permission in settings.';
    } else {
      return 'Scan failed: $errorStr';
    }
  }
}
