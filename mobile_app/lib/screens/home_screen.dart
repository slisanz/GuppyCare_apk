import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../models/device_state.dart';
import '../services/auth_service.dart';
import '../services/device_service.dart';
import '../theme.dart';
import 'device_manager_screen.dart';

/// Layar utama mengikuti layout GuppyCare.jpeg:
/// Feeding Schedule (3 jadwal) + Feed Amount (slider) + Alert banner.
class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  final _svc = DeviceService.instance;

  // State lokal slider supaya mulus saat digeser (tidak nunggu RTDB).
  double? _portionDrag;
  Timer? _portionDebounce;

  // Tick periodik supaya status online (dihitung dari last_seen) tetap
  // ke-refresh walau RTDB stream berhenti kirim update (device mati).
  Timer? _onlineTicker;

  // Input angka via keyboard. Controller disinkron dari nilai RTDB/slider
  // hanya saat field TIDAK fokus (biar tidak ganggu user yang lagi ngetik).
  final TextEditingController _portionCtrl = TextEditingController();
  final FocusNode _portionFocus = FocusNode();

  static const int _portionMin = 1;
  static const int _portionMax = 1000;

  @override
  void initState() {
    super.initState();
    _portionFocus.addListener(() {
      if (!_portionFocus.hasFocus) _commitPortionField();
    });
    _onlineTicker = Timer.periodic(
      const Duration(seconds: 20),
      (_) {
        if (mounted) setState(() {});
      },
    );
  }

  @override
  void dispose() {
    _portionDebounce?.cancel();
    _onlineTicker?.cancel();
    _portionCtrl.dispose();
    _portionFocus.dispose();
    super.dispose();
  }

  /// Commit nilai dari TextField (saat submit / kehilangan fokus).
  /// Kosong/invalid -> diabaikan, build() akan sinkron ulang dari nilai terakhir.
  void _commitPortionField() {
    final parsed = int.tryParse(_portionCtrl.text.trim());
    if (parsed == null) return;
    final clamped = parsed.clamp(_portionMin, _portionMax);
    setState(() => _portionDrag = clamped.toDouble());
    _portionDebounce?.cancel();
    _svc.setPortion(clamped);
  }

  Future<void> _editTime(int index, FeedSchedule s) async {
    final parts = s.time.split(':');
    final initial = TimeOfDay(
      hour: int.tryParse(parts.isNotEmpty ? parts[0] : '8') ?? 8,
      minute: int.tryParse(parts.length > 1 ? parts[1] : '0') ?? 0,
    );
    final picked = await showTimePicker(
      context: context,
      initialTime: initial,
    );
    if (picked == null) return;
    final hh = picked.hour.toString().padLeft(2, '0');
    final mm = picked.minute.toString().padLeft(2, '0');
    await _svc.setSchedule(index, s.copyWith(time: '$hh:$mm'));
  }

  void _onPortionChanged(double v) {
    setState(() => _portionDrag = v);
    _portionDebounce?.cancel();
    _portionDebounce = Timer(const Duration(milliseconds: 400), () {
      _svc.setPortion(v.round());
    });
  }

  Future<void> _manualFeed() async {
    await _svc.triggerManualFeed();
    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Perintah feed dikirim ke device.')),
      );
    }
  }

  void _showInfo() {
    showDialog(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text('GuppyCare'),
        content: Text(
          'Device: ${_svc.deviceId}\n\n'
          'Jadwal & porsi yang kamu set di sini langsung dikirim ke '
          'ESP32 lewat Firebase. Kalau air kotor (TDS > 600 ppm) kamu '
          'akan dapat notifikasi.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('OK'),
          ),
        ],
      ),
    );
  }

  Future<bool> _confirm(String title, String message) async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (_) => AlertDialog(
        title: Text(title),
        content: Text(message),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Batal'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Lanjut'),
          ),
        ],
      ),
    );
    return ok ?? false;
  }

  void _openDeviceManager() {
    Navigator.of(context).push(
      MaterialPageRoute(builder: (_) => const DeviceManagerScreen()),
    );
  }

  /// Hapus device tersimpan + logout Google. `signOut()` -> stream auth
  /// emit null -> AuthGate rebuild -> LoginScreen (tanpa loading).
  Future<void> _logout() async {
    if (!await _confirm(
      'Keluar',
      'Keluar dari akun? Kamu perlu login Google & isi Device ID lagi.',
    )) {
      return;
    }
    await DeviceService.instance.clearDevice();
    await AuthService.instance.signOut();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppColors.bg,
      appBar: AppBar(
        backgroundColor: AppColors.navy,
        surfaceTintColor: Colors.transparent,
        elevation: 4,
        scrolledUnderElevation: 4,
        shadowColor: Colors.black54,
        titleSpacing: 18,
        toolbarHeight: 62,
        title: Row(
          children: [
            Container(
              width: 34,
              height: 34,
              decoration: BoxDecoration(
                color: Colors.white.withValues(alpha: 0.12),
                borderRadius: BorderRadius.circular(10),
              ),
              child: const Icon(Icons.set_meal, size: 19,
                  color: Colors.white),
            ),
            const SizedBox(width: 10),
            const Text(
              'Feeding Schedule',
              style: TextStyle(
                fontSize: 18,
                fontWeight: FontWeight.w700,
                letterSpacing: 0.2,
              ),
            ),
          ],
        ),
        actions: [
          IconButton(
            tooltip: 'Info',
            onPressed: _showInfo,
            icon: const Icon(Icons.info_outline),
          ),
          IconButton(
            tooltip: 'Akun & Device',
            onPressed: _openDeviceManager,
            icon: const Icon(Icons.devices_other),
          ),
          IconButton(
            tooltip: 'Keluar',
            onPressed: _logout,
            icon: const Icon(Icons.logout),
          ),
          const SizedBox(width: 4),
        ],
      ),
      body: StreamBuilder<DeviceState>(
        stream: _svc.deviceStream(),
        builder: (context, snap) {
          if (!snap.hasData) {
            return const Center(child: CircularProgressIndicator());
          }
          final d = snap.data!;
          final portion = (_portionDrag ?? d.portionMg.toDouble())
              .clamp(_portionMin.toDouble(), _portionMax.toDouble());
          final portionInt = portion.round();
          // Sinkron TextField hanya kalau user tidak sedang mengetik.
          if (!_portionFocus.hasFocus) {
            final t = portionInt.toString();
            if (_portionCtrl.text != t) {
              _portionCtrl.value = TextEditingValue(
                text: t,
                selection: TextSelection.collapsed(offset: t.length),
              );
            }
          }

          return ListView(
            padding: const EdgeInsets.all(16),
            children: [
              _onlineChip(d.isOnline),
              const SizedBox(height: 12),
              _scheduleCard(d),
              const SizedBox(height: 16),
              _feedAmountCard(portion),
              const SizedBox(height: 16),
              _alertSection(d),
              const SizedBox(height: 24),
              FilledButton.icon(
                style: FilledButton.styleFrom(
                  backgroundColor: AppColors.feedBtn,
                  foregroundColor: AppColors.feedBtnText,
                  padding: const EdgeInsets.symmetric(vertical: 16),
                  textStyle: const TextStyle(
                    fontSize: 16,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                onPressed: _manualFeed,
                icon: const Icon(Icons.restaurant),
                label: const Text('Beri Makan Sekarang'),
              ),
            ],
          );
        },
      ),
    );
  }

  Widget _onlineChip(bool online) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.end,
      children: [
        Icon(Icons.circle, size: 10,
            color: online ? AppColors.ok : AppColors.subtleOnDark),
        const SizedBox(width: 6),
        Text(
          online ? 'Device online' : 'Device offline',
          style: const TextStyle(
              color: AppColors.subtleOnDark, fontSize: 12),
        ),
      ],
    );
  }

  Widget _card({required Widget child}) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(18),
      decoration: BoxDecoration(
        color: AppColors.card,
        borderRadius: BorderRadius.circular(16),
        boxShadow: const [
          BoxShadow(color: Color(0x14000000), blurRadius: 10, offset: Offset(0, 4)),
        ],
      ),
      child: child,
    );
  }

  Widget _sectionTitle(String t, {bool onDark = false}) => Text(
        t,
        style: TextStyle(
          fontSize: 16,
          fontWeight: FontWeight.bold,
          color: onDark ? Colors.white : AppColors.cardText,
        ),
      );

  Widget _scheduleCard(DeviceState d) {
    return _card(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _sectionTitle('Set Time:'),
          const SizedBox(height: 8),
          for (int i = 0; i < d.schedules.length; i++)
            _scheduleRow(i, d.schedules[i]),
        ],
      ),
    );
  }

  Widget _scheduleRow(int i, FeedSchedule s) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        children: [
          const Icon(Icons.access_time, color: AppColors.cardText, size: 22),
          const SizedBox(width: 12),
          Expanded(
            child: InkWell(
              onTap: () => _editTime(i, s),
              child: Text(
                s.displayTime,
                style: const TextStyle(
                  fontSize: 18,
                  fontWeight: FontWeight.w600,
                  color: AppColors.cardText,
                ),
              ),
            ),
          ),
          Switch(
            value: s.enabled,
            onChanged: (v) => _svc.setSchedule(i, s.copyWith(enabled: v)),
          ),
        ],
      ),
    );
  }

  Widget _feedAmountCard(double portion) {
    return _card(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _sectionTitle('Feed Amount'),
          const SizedBox(height: 12),
          Row(
            children: [
              const Text('Set Portion:',
                  style: TextStyle(color: AppColors.cardText)),
              const Spacer(),
              SizedBox(
                width: 104,
                child: TextField(
                  controller: _portionCtrl,
                  focusNode: _portionFocus,
                  keyboardType: TextInputType.number,
                  textInputAction: TextInputAction.done,
                  textAlign: TextAlign.end,
                  onSubmitted: (_) => _commitPortionField(),
                  inputFormatters: [
                    FilteringTextInputFormatter.digitsOnly,
                    LengthLimitingTextInputFormatter(4),
                  ],
                  style: const TextStyle(
                    fontWeight: FontWeight.bold,
                    color: AppColors.cardText,
                  ),
                  decoration: InputDecoration(
                    isDense: true,
                    suffixText: 'mg',
                    contentPadding: const EdgeInsets.symmetric(
                        horizontal: 12, vertical: 10),
                    border: OutlineInputBorder(
                      borderRadius: BorderRadius.circular(12),
                    ),
                    focusedBorder: OutlineInputBorder(
                      borderRadius: BorderRadius.circular(12),
                      borderSide:
                          const BorderSide(color: AppColors.accent, width: 1.6),
                    ),
                  ),
                ),
              ),
            ],
          ),
          Slider(
            min: _portionMin.toDouble(),
            max: _portionMax.toDouble(),
            divisions: 199,
            value: portion.clamp(
                _portionMin.toDouble(), _portionMax.toDouble()),
            label: '${portion.round()} mg',
            onChanged: _onPortionChanged,
          ),
        ],
      ),
    );
  }

  Widget _alertSection(DeviceState d) {
    final alert = d.tdsAlert;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            _sectionTitle('Alert', onDark: true),
            const SizedBox(width: 8),
            Icon(Icons.warning_amber_rounded,
                color: alert ? AppColors.alert : AppColors.subtleOnDark,
                size: 20),
          ],
        ),
        const SizedBox(height: 10),
        Container(
          width: double.infinity,
          padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 16),
          decoration: BoxDecoration(
            color: AppColors.card,
            borderRadius: BorderRadius.circular(16),
            border: Border.all(
              color: alert ? AppColors.alert : AppColors.ok,
              width: 1.5,
            ),
          ),
          child: Row(
            children: [
              Icon(
                alert ? Icons.error : Icons.check_circle,
                color: alert ? AppColors.alert : AppColors.ok,
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Text(
                  alert
                      ? 'Status: Ganti air (TDS ${d.tdsPpm.round()} ppm)'
                      : 'Status: Normal (TDS ${d.tdsPpm.round()} ppm)',
                  style: TextStyle(
                    fontWeight: FontWeight.bold,
                    color: alert ? AppColors.alert : AppColors.cardText,
                  ),
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }
}
