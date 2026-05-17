/// Representasi satu jadwal makan `/devices/{id}/schedules/{i}`.
class FeedSchedule {
  final String time; // "HH:MM" 24 jam (format simpan ESP32)
  final bool enabled;

  const FeedSchedule({required this.time, required this.enabled});

  factory FeedSchedule.fromMap(Map? m) {
    if (m == null) return const FeedSchedule(time: '08:00', enabled: false);
    return FeedSchedule(
      time: (m['time'] ?? '08:00').toString(),
      enabled: m['enabled'] == true,
    );
  }

  FeedSchedule copyWith({String? time, bool? enabled}) =>
      FeedSchedule(time: time ?? this.time, enabled: enabled ?? this.enabled);

  /// "08:00" -> "08:00 AM" untuk ditampilkan (simpan tetap 24 jam).
  String get displayTime {
    final parts = time.split(':');
    if (parts.length != 2) return time;
    final h = int.tryParse(parts[0]) ?? 0;
    final m = parts[1].padLeft(2, '0');
    final ampm = h < 12 ? 'AM' : 'PM';
    final h12 = h % 12 == 0 ? 12 : h % 12;
    return '${h12.toString().padLeft(2, '0')}:$m $ampm';
  }
}

/// Snapshot lengkap `/devices/{deviceId}` yang dibaca app.
/// Field ditulis ESP32: tds_ppm, tds_alert, online, last_feed_time, last_seen.
/// Field ditulis app: schedules, portion_mg, manual_feed_trigger.
class DeviceState {
  final List<FeedSchedule> schedules; // selalu 3 entri
  final int portionMg;
  final double tdsPpm;
  final bool tdsAlert;
  final bool online;
  final String lastFeedTime;
  final String lastSeen;

  const DeviceState({
    required this.schedules,
    required this.portionMg,
    required this.tdsPpm,
    required this.tdsAlert,
    required this.online,
    required this.lastFeedTime,
    required this.lastSeen,
  });

  /// Status online dihitung dari `last_seen` (heartbeat ESP32 tiap 30s),
  /// BUKAN dari field `online` — ESP32 cuma pernah nulis online=true dan
  /// tidak bisa nulis false saat listrik dicabut. Online kalau heartbeat
  /// terakhir < 90 detik (3x interval heartbeat).
  bool get isOnline {
    if (lastSeen.isEmpty) return false;
    try {
      final t = DateTime.parse(lastSeen).toUtc();
      return DateTime.now().toUtc().difference(t).inSeconds < 90;
    } catch (_) {
      return false;
    }
  }

  static const _defaults = [
    FeedSchedule(time: '08:00', enabled: true),
    FeedSchedule(time: '12:00', enabled: false),
    FeedSchedule(time: '17:00', enabled: true),
  ];

  factory DeviceState.fromSnapshot(Object? raw) {
    final m = (raw is Map) ? raw : const {};

    // /schedules bisa berbentuk List (index 0..2) atau Map {"0":..,"1":..}.
    final schedRaw = m['schedules'];
    final List<FeedSchedule> sched = List.generate(3, (i) {
      Object? entry;
      if (schedRaw is List && i < schedRaw.length) {
        entry = schedRaw[i];
      } else if (schedRaw is Map) {
        entry = schedRaw['$i'] ?? schedRaw[i];
      }
      return entry is Map
          ? FeedSchedule.fromMap(entry)
          : _defaults[i];
    });

    return DeviceState(
      schedules: sched,
      portionMg: _toInt(m['portion_mg'], 10),
      tdsPpm: _toDouble(m['tds_ppm'], 0),
      tdsAlert: m['tds_alert'] == true,
      online: m['online'] == true,
      lastFeedTime: (m['last_feed_time'] ?? '').toString(),
      lastSeen: (m['last_seen'] ?? '').toString(),
    );
  }

  static int _toInt(Object? v, int def) {
    if (v is int) return v;
    if (v is num) return v.round();
    return int.tryParse('$v') ?? def;
  }

  static double _toDouble(Object? v, double def) {
    if (v is num) return v.toDouble();
    return double.tryParse('$v') ?? def;
  }
}
