import 'package:flutter/material.dart';

class CustomButton extends StatelessWidget {
  final String label;
  final IconData? icon;
  final VoidCallback onPressed;
  final bool isPrimary;

  const CustomButton({
    Key? key,
    required this.label,
    required this.onPressed,
    this.icon,
    this.isPrimary = true,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return ElevatedButton.icon(
      style: ElevatedButton.styleFrom(
        backgroundColor: isPrimary ? Theme.of(context).colorScheme.primary : Theme.of(context).colorScheme.secondary,
        foregroundColor: isPrimary ? Colors.white : Theme.of(context).colorScheme.onSecondary,
        padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 14),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
        ),
        elevation: 2,
        textStyle: const TextStyle(fontWeight: FontWeight.bold, fontSize: 16),
      ),
      icon: icon != null ? Icon(icon, size: 22) : const SizedBox.shrink(),
      label: Text(label),
      onPressed: onPressed,
    );
  }
}
