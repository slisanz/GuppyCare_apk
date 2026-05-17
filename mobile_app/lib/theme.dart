import 'package:flutter/material.dart';

/// Palet warna mengikuti desain `GuppyCare.jpeg`:
/// header navy gelap, card putih, accent biru, banner status hijau/merah.
class AppColors {
  static const navy = Color(0xFF152A4E); // header & app bar
  static const navyDark = Color(0xFF0F2038); // background scaffold (lama)
  static const bg = Color(0xFF1D385A); // background utama (semua layar)
  static const accent = Color(0xFF2F6BFF); // toggle & slider aktif
  static const card = Colors.white;
  static const cardText = Color(0xFF1B2A41);
  static const subtle = Color(0xFF7A8AA3);
  static const subtleOnDark = Color(0xFFB8C4D8); // teks redup di atas bg navy
  static const ok = Color(0xFF2E9E5B); // banner "Normal"
  static const alert = Color(0xFFE0414B); // banner "Ganti air"
  static const feedBtn = Color(0xFFFFB300); // tombol "Beri Makan" (amber)
  static const feedBtnText = Color(0xFF1D385A); // teks tombol amber
}

ThemeData buildTheme() {
  final base = ThemeData(
    useMaterial3: true,
    brightness: Brightness.dark,
    scaffoldBackgroundColor: AppColors.bg,
    colorScheme: ColorScheme.fromSeed(
      seedColor: AppColors.accent,
      brightness: Brightness.dark,
    ).copyWith(primary: AppColors.accent),
  );
  return base.copyWith(
    appBarTheme: const AppBarTheme(
      backgroundColor: AppColors.bg,
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
