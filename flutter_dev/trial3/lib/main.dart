import 'package:flutter/material.dart';
import 'package:trial3/initial_page.dart';
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

