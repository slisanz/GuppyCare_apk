import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_messaging/firebase_messaging.dart';
import 'package:flutter/material.dart';

import 'firebase_options.dart';
import 'screens/device_setup_screen.dart';
import 'screens/home_screen.dart';
import 'screens/login_screen.dart';
import 'screens/splash_screen.dart';
import 'services/auth_service.dart';
import 'services/device_service.dart';
import 'services/fcm_service.dart';
import 'theme.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Firebase.initializeApp(
    options: DefaultFirebaseOptions.currentPlatform,
  );
  FirebaseMessaging.onBackgroundMessage(firebaseMessagingBackgroundHandler);
  await DeviceService.instance.loadPersisted();
  runApp(const GuppyCareApp());
}

class GuppyCareApp extends StatelessWidget {
  const GuppyCareApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'GuppyCare',
      debugShowCheckedModeBanner: false,
      theme: buildTheme(),
      home: const AuthGate(),
    );
  }
}

/// Routing: belum login -> Login. Login tapi belum punya device ->
/// DeviceSetup. Sudah punya device -> Home (+ daftar FCM token).
class AuthGate extends StatefulWidget {
  const AuthGate({super.key});

  @override
  State<AuthGate> createState() => _AuthGateState();
}

class _AuthGateState extends State<AuthGate> {
  bool _fcmRegistered = false;

  @override
  Widget build(BuildContext context) {
    // ValueListenableBuilder: AuthGate rebuild saat device dipasang/dihapus
    // (Ganti Device) TANPA bikin route/AuthGate baru -> tidak ada loading
    // screen di sela alur. StreamBuilder pakai stream auth yang di-cache
    // (lihat AuthService.authState) jadi connection tetap `active` lintas
    // rebuild -> SplashScreen hanya muncul di cold start.
    return ValueListenableBuilder<int>(
      valueListenable: DeviceService.instance.revision,
      builder: (context, _, child) {
        return StreamBuilder(
          stream: AuthService.instance.authState,
          builder: (context, snap) {
            if (snap.connectionState == ConnectionState.waiting) {
              return const SplashScreen();
            }

            final user = snap.data;
            if (user == null) {
              _fcmRegistered = false;
              return const LoginScreen();
            }

            if (!DeviceService.instance.hasDevice) {
              return DeviceSetupScreen(onLinked: () => setState(() {}));
            }

            if (!_fcmRegistered) {
              _fcmRegistered = true;
              FcmService.instance.registerForDevice();
            }
            return const HomeScreen();
          },
        );
      },
    );
  }
}
