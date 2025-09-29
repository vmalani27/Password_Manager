# Flutter BLE Password Manager (Bluetooth Only)

This project demonstrates a clean, maintainable implementation of Bluetooth Low Energy (BLE) device discovery and connection in Flutter using [`flutter_reactive_ble`](https://pub.dev/packages/flutter_reactive_ble).

---

## Features
- Scan for nearby BLE devices
- Connect to a selected BLE device
- Modern, provider-based state management
- Modular, scalable project structure

---

## Project Structure

```
lib/
	main.dart
	providers/
		ble_provider.dart
	screens/
		device_discovery_screen.dart
		device_control_screen.dart
	widgets/
		custom_button.dart
```

- **providers/**: BLE logic and state management
- **screens/**: UI pages (device discovery, control, etc.)
- **widgets/**: Reusable UI components

---

## Dependencies

See `pubspec.yaml`:
```yaml
dependencies:
	flutter:
		sdk: flutter
	flutter_reactive_ble: ^5.3.1
	provider: ^6.1.1
	permission_handler: ^11.0.1
```

---

## BLE Provider Example

`lib/providers/ble_provider.dart`:
```dart
import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

class BleProvider with ChangeNotifier {
	final FlutterReactiveBle _ble = FlutterReactiveBle();
	final List<DiscoveredDevice> _devices = [];
	StreamSubscription<DiscoveredDevice>? _scanSub;
	StreamSubscription<ConnectionStateUpdate>? _connSub;
	bool _isScanning = false;
	bool _isConnecting = false;
	bool _isConnected = false;
	String? _error;
	DiscoveredDevice? _connectedDevice;

	List<DiscoveredDevice> get devices => List.unmodifiable(_devices);
	bool get isScanning => _isScanning;
	bool get isConnected => _isConnected;
	String? get error => _error;

	void startScan() {
		_devices.clear();
		_error = null;
		_isScanning = true;
		notifyListeners();

		_scanSub?.cancel();
		_scanSub = _ble.scanForDevices(
			withServices: [],
			scanMode: ScanMode.lowLatency,
		).listen((device) {
			if (!_devices.any((d) => d.id == device.id)) {
				_devices.add(device);
				notifyListeners();
			}
		}, onError: (e) {
			_error = e.toString();
			_isScanning = false;
			notifyListeners();
		});

		// Auto-stop after 10 seconds
		Future.delayed(const Duration(seconds: 10), stopScan);
	}

	void stopScan() {
		_scanSub?.cancel();
		_scanSub = null;
		_isScanning = false;
		notifyListeners();
	}

	Future<void> connectToDevice(DiscoveredDevice device) async {
		_isConnecting = true;
		_error = null;
		notifyListeners();

		_connSub?.cancel();
		_connSub = _ble.connectToDevice(
			id: device.id,
			connectionTimeout: const Duration(seconds: 10),
		).listen((update) {
			if (update.connectionState == DeviceConnectionState.connected) {
				_isConnected = true;
				_connectedDevice = device;
				_isConnecting = false;
				notifyListeners();
			} else if (update.connectionState == DeviceConnectionState.disconnected) {
				_isConnected = false;
				_isConnecting = false;
				_connectedDevice = null;
				notifyListeners();
			}
		}, onError: (e) {
			_error = e.toString();
			_isConnecting = false;
			_isConnected = false;
			notifyListeners();
		});
	}

	@override
	void dispose() {
		_scanSub?.cancel();
		_connSub?.cancel();
		super.dispose();
	}
}
```

---

## Device Discovery Screen Example

`lib/screens/device_discovery_screen.dart`:
```dart
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/ble_provider.dart';

class DeviceDiscoveryScreen extends StatelessWidget {
	const DeviceDiscoveryScreen({Key? key}) : super(key: key);

	@override
	Widget build(BuildContext context) {
		final ble = context.watch<BleProvider>();

		return Scaffold(
			appBar: AppBar(
				title: const Text('Discover Devices'),
				actions: [
					IconButton(
						icon: Icon(ble.isScanning ? Icons.stop : Icons.refresh),
						onPressed: ble.isScanning ? ble.stopScan : ble.startScan,
					),
				],
			),
			body: Column(
				children: [
					if (ble.error != null)
						Padding(
							padding: const EdgeInsets.all(16),
							child: Text(ble.error!, style: const TextStyle(color: Colors.red)),
						),
					Expanded(
						child: ble.isScanning && ble.devices.isEmpty
								? const Center(child: CircularProgressIndicator())
								: ListView.builder(
										itemCount: ble.devices.length,
										itemBuilder: (context, idx) {
											final device = ble.devices[idx];
											return ListTile(
												leading: const Icon(Icons.bluetooth),
												title: Text(device.name.isNotEmpty ? device.name : 'Unknown'),
												subtitle: Text(device.id),
												onTap: () async {
													await ble.connectToDevice(device);
													if (ble.isConnected && context.mounted) {
														// Navigate to control screen or show connected state
													}
												},
											);
										},
									),
					),
				],
			),
			floatingActionButton: !ble.isScanning
					? FloatingActionButton(
							onPressed: ble.startScan,
							child: const Icon(Icons.search),
						)
					: null,
		);
	}
}
```

---

## Main Entry Example

`lib/main.dart`:
```dart
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'providers/ble_provider.dart';
import 'screens/device_discovery_screen.dart';

void main() {
	runApp(
		ChangeNotifierProvider(
			create: (_) => BleProvider(),
			child: const MyApp(),
		),
	);
}

class MyApp extends StatelessWidget {
	const MyApp({super.key});
	@override
	Widget build(BuildContext context) {
		return MaterialApp(
			title: 'Password Manager BLE',
			theme: ThemeData(
				colorScheme: ColorScheme.fromSeed(seedColor: Colors.blue),
				useMaterial3: true,
			),
			home: const DeviceDiscoveryScreen(),
		);
	}
}
```

---

## Permissions
- Use [`permission_handler`](https://pub.dev/packages/permission_handler) to request Bluetooth and location permissions before scanning.

---

## Extending
- Add device control, notifications, and write logic in the provider.
- Keep business logic out of UI code for maintainability.

---

**This structure is robust, testable, and easy to maintain.**
# test

A new Flutter project.

## Getting Started

This project is a starting point for a Flutter application.

A few resources to get you started if this is your first Flutter project:

- [Lab: Write your first Flutter app](https://docs.flutter.dev/get-started/codelab)
- [Cookbook: Useful Flutter samples](https://docs.flutter.dev/cookbook)

For help getting started with Flutter development, view the
[online documentation](https://docs.flutter.dev/), which offers tutorials,
samples, guidance on mobile development, and a full API reference.
