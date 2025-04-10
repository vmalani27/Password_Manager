import 'package:flutter_blue_plus/flutter_blue_plus.dart';

class BLEHelper {
  void scanForDevices() {
    FlutterBluePlus.startScan(timeout: Duration(seconds: 4));
    
    FlutterBluePlus.scanResults.listen((results) {
      for (ScanResult r in results) {
        print('Device found: ${r.device.platformName} (${r.device.remoteId})');
      }
    });
  }

  Future<void> connectToDevice(BluetoothDevice device) async {
    await device.connect();
    print('Connected to ${device.platformName}');
  }
}
