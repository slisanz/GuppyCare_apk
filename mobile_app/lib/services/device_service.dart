import 'dart:convert';

import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../models/device_state.dart';

/// Hasil percobaan klaim/akses device.
enum ClaimResult { ok, claimed, ownedByOther, notFound }

/// Akses Realtime DB `/devices/{deviceId}`.
/// URL RTDB di-set eksplisit karena firebase_options belum tentu memuat
/// databaseURL (project di region asia-southeast1).
class DeviceService {
  DeviceService._();
  static final DeviceService instance = DeviceService._();

  static const String databaseUrl =
      'https://guppycare-default-rtdb.asia-southeast1.firebasedatabase.app';

  late final FirebaseDatabase _db = FirebaseDatabase.instanceFor(
    app: Firebase.app(),
    databaseURL: databaseUrl,
  );

  static const _prefsKey = 'linked_device_id';

  String? _deviceId;
  String get deviceId => _deviceId ?? '';
  bool get hasDevice => (_deviceId ?? '').isNotEmpty;

  /// Bump tiap device dipasang/dihapus. AuthGate dengar ini supaya
  /// re-evaluasi layar TANPA bikin AuthGate baru / loading screen.
  final ValueNotifier<int> revision = ValueNotifier<int>(0);

  DatabaseReference _root(String id) => _db.ref('devices/$id');

  String get _uid => FirebaseAuth.instance.currentUser!.uid;
  String? get _uidOrNull => FirebaseAuth.instance.currentUser?.uid;

  // ---- Daftar device per akun (lokal, key per-uid) ------------------
  // Indeks kenyamanan saja. Sumber kebenaran kepemilikan tetap
  // /devices/{id}/owner_uid di RTDB.
  final List<String> _linked = [];
  List<String> get linkedDevices => List.unmodifiable(_linked);

  String _linkedKey(String uid) => 'linked_devices_$uid';

  /// Muat daftar device milik akun yang sedang login. Migrasi sekali:
  /// kalau daftar kosong tapi ada active device lama (linked_device_id),
  /// jadikan seed daftar. Dipanggil dari DeviceManagerScreen.initState.
  Future<void> loadLinkedForUser() async {
    final uid = _uidOrNull;
    _linked.clear();
    if (uid == null) return;
    final p = await SharedPreferences.getInstance();
    final raw = p.getString(_linkedKey(uid));
    if (raw != null && raw.isNotEmpty) {
      try {
        final decoded = jsonDecode(raw);
        if (decoded is List) {
          _linked.addAll(decoded.map((e) => e.toString().toUpperCase()));
        }
      } catch (_) {
        // data korup -> abaikan, mulai dari kosong
      }
    }
    final active = (_deviceId ?? '').toUpperCase();
    if (_linked.isEmpty && active.isNotEmpty) {
      _linked.add(active);
      await _saveLinked();
    }
  }

  Future<void> _saveLinked() async {
    final uid = _uidOrNull;
    if (uid == null) return;
    final p = await SharedPreferences.getInstance();
    await p.setString(_linkedKey(uid), jsonEncode(_linked));
  }

  Future<void> addToLinked(String id) async {
    final n = id.trim().toUpperCase();
    if (n.isEmpty || _linked.contains(n)) return;
    _linked.add(n);
    await _saveLinked();
  }

  Future<void> removeFromLinked(String id) async {
    final n = id.trim().toUpperCase();
    if (_linked.remove(n)) await _saveLinked();
  }

  /// Muat device_id yang sudah pernah dihubungkan (dipanggil saat boot).
  Future<void> loadPersisted() async {
    final p = await SharedPreferences.getInstance();
    final saved = p.getString(_prefsKey);
    if (saved != null && saved.isNotEmpty) _deviceId = saved;
  }

  Future<void> useDevice(String id) async {
    _deviceId = id.trim().toUpperCase();
    final p = await SharedPreferences.getInstance();
    await p.setString(_prefsKey, _deviceId!);
    revision.value++;
  }

  Future<void> clearDevice() async {
    _deviceId = null;
    final p = await SharedPreferences.getInstance();
    await p.remove(_prefsKey);
    revision.value++;
  }

  /// Klaim kepemilikan device, atau verifikasi akses kalau sudah ada owner.
  /// - owner_uid kosong  -> set owner_uid = uid (claimed)
  /// - owner_uid == uid   -> ok
  /// - uid ada di shared  -> ok
  /// - device belum pernah online (snapshot null) -> notFound
  /// - selain itu         -> ownedByOther
  Future<ClaimResult> claimOrVerify(String id) async {
    final normId = id.trim().toUpperCase();
    final ref = _root(normId);
    final snap = await ref.get();

    if (!snap.exists) return ClaimResult.notFound;

    final data = (snap.value is Map) ? snap.value as Map : const {};
    final owner = (data['owner_uid'] ?? '').toString();
    final shared = data['shared_uids'];
    final sharedList = shared is List
        ? shared.map((e) => e.toString()).toList()
        : (shared is Map
            ? shared.values.map((e) => e.toString()).toList()
            : const <String>[]);

    if (owner.isEmpty) {
      await ref.child('owner_uid').set(_uid);
      await useDevice(normId);
      await addToLinked(normId);
      return ClaimResult.claimed;
    }
    if (owner == _uid || sharedList.contains(_uid)) {
      await useDevice(normId);
      await addToLinked(normId);
      return ClaimResult.ok;
    }
    return ClaimResult.ownedByOther;
  }

  /// Lepas device dari akun ini: kosongkan owner_uid (kalau kita owner),
  /// buang uid dari shared_uids, hapus fcm token kita, dan buang dari
  /// daftar lokal. Setelah ini device bisa diklaim email lain.
  Future<void> unlinkDevice(String id) async {
    final normId = id.trim().toUpperCase();
    final uid = _uidOrNull;
    if (uid == null) return;
    final ref = _root(normId);

    final snap = await ref.get();
    if (snap.exists && snap.value is Map) {
      final data = snap.value as Map;

      if ((data['owner_uid'] ?? '').toString() == uid) {
        await ref.child('owner_uid').set('');
      }

      final shared = data['shared_uids'];
      final sharedList = shared is List
          ? shared.map((e) => e.toString()).toList()
          : (shared is Map
              ? shared.values.map((e) => e.toString()).toList()
              : <String>[]);
      if (sharedList.contains(uid)) {
        sharedList.remove(uid);
        if (sharedList.isEmpty) {
          await ref.child('shared_uids').remove();
        } else {
          await ref.child('shared_uids').set(sharedList);
        }
      }

      await ref.child('fcm_tokens/$uid').remove();
    }

    await removeFromLinked(normId);

    if ((_deviceId ?? '').toUpperCase() == normId) {
      await clearDevice(); // bump revision -> AuthGate keluar dari device ini
    } else {
      revision.value++; // refresh layar manager
    }
  }

  /// Stream state device untuk StreamBuilder di home screen.
  Stream<DeviceState> deviceStream() {
    return _root(deviceId).onValue.map(
          (e) => DeviceState.fromSnapshot(e.snapshot.value),
        );
  }

  // ---- Tulis perubahan dari app -------------------------------------

  Future<void> setSchedule(int index, FeedSchedule s) {
    return _root(deviceId).child('schedules/$index').update({
      'time': s.time,
      'enabled': s.enabled,
    });
  }

  Future<void> setPortion(int mg) {
    final clamped = mg.clamp(1, 1000);
    return _root(deviceId).child('portion_mg').set(clamped);
  }

  Future<void> triggerManualFeed() {
    return _root(deviceId).child('manual_feed_trigger').set(true);
  }

  // ---- FCM ----------------------------------------------------------

  Future<void> saveFcmToken(String token) {
    return _root(deviceId).child('fcm_tokens/$_uid').set(token);
  }
}
