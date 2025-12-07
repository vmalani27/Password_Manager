import 'package:flutter/material.dart';

class ActionButton extends StatelessWidget {
  final IconData icon;
  final String label;
  final VoidCallback? onTap;
  final Color? backgroundColor;
  final Color? foregroundColor;
  final bool expanded;

  const ActionButton({
    Key? key,
    required this.icon,
    required this.label,
    required this.onTap,
    this.backgroundColor,
    this.foregroundColor,
    this.expanded = false,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    final button = ElevatedButton.icon(
      icon: Icon(icon),
      label: Text(label),
      onPressed: onTap,
      style: ElevatedButton.styleFrom(
        backgroundColor: backgroundColor,
        foregroundColor: foregroundColor,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(6), // Smaller, more rectangular
        ),
      ),
    );
    return expanded ? Expanded(child: button) : button;
  }
}
