import 'package:flutter/material.dart';

/// Branded loading screen GuppyCare.
///
/// Dipakai di AuthGate selama Firebase/auth state belum resolve, supaya brand
/// tetap tampil (sambungan mulus dari native splash flutter_native_splash yang
/// pakai aset & warna cream yang sama).
class SplashScreen extends StatelessWidget {
  const SplashScreen({super.key});

  /// Warna background brand (sama persis dgn native splash & adaptive icon).
  static const Color _cream = Color(0xFFFCFCF7);

  @override
  Widget build(BuildContext context) {
    return const Scaffold(
      backgroundColor: _cream,
      body: SafeArea(
        child: Stack(
          children: [
            Positioned.fill(
              child: Image(
                image: AssetImage('assets/branding/full_display.png'),
                fit: BoxFit.contain,
              ),
            ),
            Align(
              alignment: Alignment(0, 0.88),
              child: SizedBox(
                width: 28,
                height: 28,
                child: CircularProgressIndicator(
                  strokeWidth: 2.6,
                  valueColor: AlwaysStoppedAnimation<Color>(Color(0xFF152A4E)),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
