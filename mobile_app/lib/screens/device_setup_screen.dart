import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../services/auth_service.dart';
import '../services/device_service.dart';
import '../theme.dart';

/// Input device ID (MAC ESP32). Default device user: 383B67C8E720.
class DeviceSetupScreen extends StatefulWidget {
  final VoidCallback onLinked;
  const DeviceSetupScreen({super.key, required this.onLinked});

  @override
  State<DeviceSetupScreen> createState() => _DeviceSetupScreenState();
}

class _DeviceSetupScreenState extends State<DeviceSetupScreen> {
  final _ctrl = TextEditingController(text: '383B67C8E720');
  bool _busy = false;
  String? _error;

  @override
  void dispose() {
    _ctrl.dispose();
    super.dispose();
  }

  Future<void> _logout() async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text('Keluar'),
        content: const Text(
            'Keluar dari akun? Kamu perlu login Google lagi.'),
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
    if (ok != true) return;
    await DeviceService.instance.clearDevice();
    await AuthService.instance.signOut();
  }

  Future<void> _link() async {
    final id = _ctrl.text.trim().toUpperCase();
    if (id.length != 12) {
      setState(() => _error = 'Device ID harus 12 karakter hex (MAC ESP32).');
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final r = await DeviceService.instance.claimOrVerify(id);
      switch (r) {
        case ClaimResult.ok:
        case ClaimResult.claimed:
          widget.onLinked();
          break;
        case ClaimResult.notFound:
          setState(() => _error =
              'Device $id belum pernah online. Nyalakan ESP32 & pastikan '
              'terhubung WiFi dulu.');
          break;
        case ClaimResult.ownedByOther:
          setState(() => _error =
              'Device ini sudah dimiliki akun lain. Minta pemilik untuk '
              'menambahkan email kamu ke shared_uids.');
          break;
      }
    } catch (e) {
      setState(() => _error = 'Gagal menghubungkan: $e');
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Hubungkan Device'),
        actions: [
          IconButton(
            tooltip: 'Keluar',
            onPressed: _logout,
            icon: const Icon(Icons.logout),
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            const SizedBox(height: 12),
            const Text(
              'Masukkan Device ID',
              style: TextStyle(
                fontSize: 20,
                fontWeight: FontWeight.bold,
                color: Colors.white,
              ),
            ),
            const SizedBox(height: 6),
            const Text(
              'Device ID = MAC address ESP32 (tampil di Serial Monitor '
              'saat boot, mis. 383B67C8E720).',
              style: TextStyle(color: AppColors.subtleOnDark),
            ),
            const SizedBox(height: 24),
            TextField(
              controller: _ctrl,
              autocorrect: false,
              textCapitalization: TextCapitalization.characters,
              inputFormatters: [
                LengthLimitingTextInputFormatter(12),
                FilteringTextInputFormatter.allow(RegExp('[0-9a-fA-F]')),
              ],
              style: const TextStyle(
                color: Colors.white,
                letterSpacing: 2,
                fontFeatures: [FontFeature.tabularFigures()],
              ),
              decoration: InputDecoration(
                filled: true,
                fillColor: Colors.white10,
                labelText: 'Device ID',
                labelStyle: const TextStyle(color: AppColors.subtle),
                border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(12),
                ),
              ),
            ),
            if (_error != null) ...[
              const SizedBox(height: 16),
              Text(_error!, style: const TextStyle(color: AppColors.alert)),
            ],
            const SizedBox(height: 24),
            FilledButton(
              style: FilledButton.styleFrom(
                backgroundColor: AppColors.accent,
                padding: const EdgeInsets.symmetric(vertical: 14),
              ),
              onPressed: _busy ? null : _link,
              child: _busy
                  ? const SizedBox(
                      width: 18,
                      height: 18,
                      child: CircularProgressIndicator(
                        strokeWidth: 2,
                        color: Colors.white,
                      ),
                    )
                  : const Text('Hubungkan'),
            ),
          ],
        ),
      ),
    );
  }
}
