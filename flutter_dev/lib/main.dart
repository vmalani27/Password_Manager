import 'package:flutter/material.dart';
import 'package:trial2/initial_page.dart';
// import 'package:your_app/pages/initial_page.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Password Manager',
      theme: ThemeData(
        primarySwatch: Colors.blue,
      ),
      home: InitialPage(),
    );
  }
}

class InitialPage extends StatelessWidget {
  const InitialPage({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Initial Page'),
      ),
      body: Center(
        child: ElevatedButton(
          onPressed: () async {
            await Navigator.push(
              context,
              MaterialPageRoute(
                builder: (context) => const DeviceScanScreen(),
              ),
            );
          },
          child: const Text('Go to Device Scan Screen'),
        ),
      ),
    );
  }
}

class DeviceScanScreen extends StatelessWidget {
  const DeviceScanScreen({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Device Scan Screen'),
      ),
      body: const Center(
        child: Text('Scanning for devices...'),
      ),
    );
  }
}

