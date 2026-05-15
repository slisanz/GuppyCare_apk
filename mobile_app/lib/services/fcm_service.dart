import 'package:firebase_messaging/firebase_messaging.dart';

import 'device_service.dart';

/// Handler pesan FCM saat app di background / mati.
/// Wajib top-level + @pragma vm:entry-point.
@pragma('vm:entry-point')
Future<void> firebaseMessagingBackgroundHandler(RemoteMessage message) async {
  // Notifikasi "ganti air" ditampilkan otomatis oleh sistem (notification
  // payload dari Cloud Function Phase 7). Tidak perlu kerja tambahan.
}

class FcmService {
  FcmService._();
  static final FcmService instance = FcmService._();

  final _fm = FirebaseMessaging.instance;

  /// Minta izin notifikasi, ambil token, simpan ke RTDB, dan pasang
  /// listener refresh token. Dipanggil setelah device terklaim.
  Future<void> registerForDevice() async {
    await _fm.requestPermission(alert: true, badge: true, sound: true);

    final token = await _fm.getToken();
    if (token != null && DeviceService.instance.hasDevice) {
      await DeviceService.instance.saveFcmToken(token);
    }

    _fm.onTokenRefresh.listen((t) {
      if (DeviceService.instance.hasDevice) {
        DeviceService.instance.saveFcmToken(t);
      }
    });
  }
}
