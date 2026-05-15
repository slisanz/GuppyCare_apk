import 'package:flutter/material.dart';

/// Palet warna mengikuti desain `GuppyCare.jpeg`:
/// header navy gelap, card putih, accent biru, banner status hijau/merah.
class AppColors {
  static const navy = Color(0xFF152A4E); // header & app bar
  static const navyDark = Color(0xFF0F2038); // background scaffold
  static const accent = Color(0xFF2F6BFF); // toggle & slider aktif
  static const card = Colors.white;
  static const cardText = Color(0xFF1B2A41);
  static const subtle = Color(0xFF7A8AA3);
  static const ok = Color(0xFF2E9E5B); // banner "Normal"
  static const alert = Color(0xFFE0414B); // banner "Ganti air"
}

ThemeData buildTheme() {
  final base = ThemeData(
    useMaterial3: true,
    brightness: Brightness.dark,
    scaffoldBackgroundColor: AppColors.navyDark,
    colorScheme: ColorScheme.fromSeed(
      seedColor: AppColors.accent,
      brightness: Brightness.dark,
    ).copyWith(primary: AppColors.accent),
  );
  return base.copyWith(
    appBarTheme: const AppBarTheme(
      backgroundColor: AppColors.navy,
      foregroundColor: Colors.white,
      elevation: 0,
      centerTitle: false,
    ),
    switchTheme: SwitchThemeData(
      thumbColor: WidgetStateProperty.resolveWith(
        (s) => s.contains(WidgetState.selected) ? Colors.white : Colors.white,
      ),
      trackColor: WidgetStateProperty.resolveWith(
        (s) => s.contains(WidgetState.selected)
            ? AppColors.accent
            : const Color(0xFFC4CDDB),
      ),
    ),
    sliderTheme: const SliderThemeData(
      activeTrackColor: AppColors.accent,
      inactiveTrackColor: Color(0xFFD5DCE7),
      thumbColor: AppColors.accent,
      overlayColor: Color(0x332F6BFF),
    ),
  );
}
