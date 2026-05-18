import 'package:firebase_auth/firebase_auth.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../services/auth_service.dart';
import '../services/device_service.dart';
import '../theme.dart';

/// Layar "Akun & Device": info akun Google, hubungkan device baru, dan
/// daftar device yang tertaut ke email ini (pindah / lepas).
///
/// [onLinked] di-set saat dipanggil dari AuthGate (belum ada device aktif):
/// dipanggil setelah device jadi aktif supaya AuthGate rebuild ke Home.
/// Null saat di-push dari Home (tinggal pop balik).
class DeviceManagerScreen extends StatefulWidget {
  final VoidCallback? onLinked;
  const DeviceManagerScreen({super.key, this.onLinked});

  @override
  State<DeviceManagerScreen> createState() => _DeviceManagerScreenState();
}

class _DeviceManagerScreenState extends State<DeviceManagerScreen> {
  final _svc = DeviceService.instance;
  final _ctrl = TextEditingController(text: '383B67C8E720');

  bool _loading = true;
  bool _showForm = false;
  bool _busy = false;
  String? _formError;

  @override
  void initState() {
    super.initState();
    _reload();
  }

  Future<void> _reload() async {
    await _svc.loadLinkedForUser();
    if (!mounted) return;
    setState(() {
      _loading = false;
      _showForm = _svc.linkedDevices.isEmpty;
    });
  }

  @override
  void dispose() {
    _ctrl.dispose();
    super.dispose();
  }

  /// Device sudah jadi aktif. Kalau dari AuthGate -> trigger rebuild ke
  /// Home; kalau di-push dari Home -> pop balik (revision sudah rebuild).
  void _afterActive() {
    if (widget.onLinked != null) {
      widget.onLinked!();
    } else if (mounted) {
      Navigator.of(context).maybePop();
    }
  }

  Future<bool> _confirm(String title, String message, String okLabel) async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (_) => AlertDialog(
        title: Text(title),
        content: Text(message),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: Text(okLabel),
          ),
        ],
      ),
    );
    return ok ?? false;
  }

  Future<void> _logout() async {
    if (!await _confirm('Sign out',
        'Sign out of your account? You will need to sign in with Google '
        'again.', 'Sign out')) {
      return;
    }
    await _svc.clearDevice();
    await AuthService.instance.signOut();
    // Layar ini bisa di-push di atas Home. signOut bikin AuthGate (di bawah
    // stack) jadi LoginScreen, tapi route ini masih nutupin -> keliatan
    // "nggak keluar". Pop balik ke route pertama biar LoginScreen tampil.
    if (mounted) {
      Navigator.of(context).popUntil((route) => route.isFirst);
    }
  }

  Future<void> _link() async {
    final id = _ctrl.text.trim().toUpperCase();
    if (id.length != 12) {
      setState(() =>
          _formError = 'Device ID must be 12 hex characters (ESP32 MAC).');
      return;
    }
    setState(() {
      _busy = true;
      _formError = null;
    });
    try {
      final r = await _svc.claimOrVerify(id);
      switch (r) {
        case ClaimResult.ok:
        case ClaimResult.claimed:
          _afterActive();
          break;
        case ClaimResult.notFound:
          setState(() => _formError =
              'Device $id has never been online. Turn on the ESP32 and make '
              'sure it is connected to WiFi first.');
          break;
        case ClaimResult.ownedByOther:
          setState(() => _formError =
              'This device is already owned by another account. The owner '
              'must unlink it first (or add your email to shared_uids).');
          break;
      }
    } catch (e) {
      setState(() => _formError = 'Failed to connect: $e');
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _switchTo(String id) async {
    try {
      final r = await _svc.claimOrVerify(id);
      if (!mounted) return;
      switch (r) {
        case ClaimResult.ok:
        case ClaimResult.claimed:
          _afterActive();
          break;
        case ClaimResult.ownedByOther:
          _snackRemove(id, 'Device $id is now owned by another account.');
          break;
        case ClaimResult.notFound:
          _snackRemove(id, 'Device $id has never been online.');
          break;
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Failed to switch: $e')),
        );
      }
    }
  }

  void _snackRemove(String id, String msg) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(msg),
        action: SnackBarAction(
          label: 'Remove from list',
          onPressed: () async {
            await _svc.removeFromLinked(id);
            if (mounted) setState(() {});
          },
        ),
      ),
    );
  }

  Future<void> _unlink(String id) async {
    if (!await _confirm(
      'Unlink Device',
      'Unlink device $id from your account? Another email can use it '
          'after it is unlinked.',
      'Unlink',
    )) {
      return;
    }
    await _svc.unlinkDevice(id);
    if (mounted) {
      setState(() => _showForm = _svc.linkedDevices.isEmpty);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppColors.bg,
      appBar: AppBar(
        backgroundColor: AppColors.navy,
        surfaceTintColor: Colors.transparent,
        elevation: 4,
        title: const Text('Account & Device'),
      ),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : ListView(
              padding: const EdgeInsets.all(16),
              children: [
                _accountCard(),
                const SizedBox(height: 20),
                if (_showForm)
                  _linkForm()
                else
                  FilledButton.icon(
                    style: FilledButton.styleFrom(
                      backgroundColor: AppColors.accent,
                      padding: const EdgeInsets.symmetric(vertical: 14),
                    ),
                    onPressed: () => setState(() => _showForm = true),
                    icon: const Icon(Icons.add),
                    label: const Text('Link New Device'),
                  ),
                const SizedBox(height: 24),
                const Text(
                  'Linked devices:',
                  style: TextStyle(
                    color: Colors.white,
                    fontSize: 16,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 10),
                ..._deviceList(),
              ],
            ),
    );
  }

  Widget _card({required Widget child}) => Container(
        width: double.infinity,
        padding: const EdgeInsets.all(16),
        decoration: BoxDecoration(
          color: AppColors.card,
          borderRadius: BorderRadius.circular(16),
          boxShadow: const [
            BoxShadow(
                color: Color(0x14000000),
                blurRadius: 10,
                offset: Offset(0, 4)),
          ],
        ),
        child: child,
      );

  Widget _accountCard() {
    final u = FirebaseAuth.instance.currentUser;
    final photo = u?.photoURL;
    final name = (u?.displayName ?? '').trim();
    final email = u?.email ?? '(no email)';
    return _card(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              CircleAvatar(
                radius: 22,
                backgroundColor: AppColors.bg,
                backgroundImage:
                    (photo != null && photo.isNotEmpty)
                        ? NetworkImage(photo)
                        : null,
                child: (photo == null || photo.isEmpty)
                    ? const Icon(Icons.person, color: Colors.white)
                    : null,
              ),
              const SizedBox(width: 14),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    if (name.isNotEmpty)
                      Text(
                        name,
                        style: const TextStyle(
                          color: AppColors.cardText,
                          fontWeight: FontWeight.bold,
                          fontSize: 15,
                        ),
                      ),
                    Text(
                      email,
                      style: const TextStyle(
                          color: AppColors.subtle, fontSize: 13),
                    ),
                  ],
                ),
              ),
            ],
          ),
          const SizedBox(height: 14),
          SizedBox(
            width: double.infinity,
            child: OutlinedButton.icon(
              style: OutlinedButton.styleFrom(
                foregroundColor: AppColors.alert,
                side: const BorderSide(color: AppColors.alert),
                padding: const EdgeInsets.symmetric(vertical: 12),
              ),
              onPressed: _logout,
              icon: const Icon(Icons.logout, size: 18),
              label: const Text('Sign out'),
            ),
          ),
        ],
      ),
    );
  }

  Widget _linkForm() {
    return _card(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text(
            'Link New Device',
            style: TextStyle(
              color: AppColors.cardText,
              fontSize: 16,
              fontWeight: FontWeight.bold,
            ),
          ),
          const SizedBox(height: 4),
          const Text(
            'Device ID = the ESP32 MAC address (shown in the Serial Monitor '
            'at boot, e.g. 383B67C8E720).',
            style: TextStyle(color: AppColors.subtle, fontSize: 12),
          ),
          const SizedBox(height: 14),
          TextField(
            controller: _ctrl,
            autocorrect: false,
            textCapitalization: TextCapitalization.characters,
            inputFormatters: [
              LengthLimitingTextInputFormatter(12),
              FilteringTextInputFormatter.allow(RegExp('[0-9a-fA-F]')),
            ],
            style: const TextStyle(
              color: AppColors.cardText,
              letterSpacing: 2,
              fontFeatures: [FontFeature.tabularFigures()],
            ),
            decoration: InputDecoration(
              isDense: true,
              labelText: 'Device ID',
              labelStyle: const TextStyle(color: AppColors.subtle),
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
          if (_formError != null) ...[
            const SizedBox(height: 12),
            Text(_formError!,
                style: const TextStyle(color: AppColors.alert)),
          ],
          const SizedBox(height: 16),
          Row(
            children: [
              if (_svc.linkedDevices.isNotEmpty)
                TextButton(
                  onPressed: _busy
                      ? null
                      : () => setState(() {
                            _showForm = false;
                            _formError = null;
                          }),
                  child: const Text('Cancel'),
                ),
              const Spacer(),
              FilledButton(
                style: FilledButton.styleFrom(
                  backgroundColor: AppColors.accent,
                  padding: const EdgeInsets.symmetric(
                      horizontal: 24, vertical: 12),
                ),
                onPressed: _busy ? null : _link,
                child: _busy
                    ? const SizedBox(
                        width: 18,
                        height: 18,
                        child: CircularProgressIndicator(
                            strokeWidth: 2, color: Colors.white),
                      )
                    : const Text('Link'),
              ),
            ],
          ),
        ],
      ),
    );
  }

  List<Widget> _deviceList() {
    final ids = _svc.linkedDevices;
    if (ids.isEmpty) {
      return const [
        Text(
          'No linked devices yet. Tap "Link New Device".',
          style: TextStyle(color: AppColors.subtleOnDark),
        ),
      ];
    }
    final active = _svc.deviceId.toUpperCase();
    return ids
        .map((id) => Padding(
              padding: const EdgeInsets.only(bottom: 10),
              child: _deviceCard(id, id.toUpperCase() == active),
            ))
        .toList();
  }

  Widget _deviceCard(String id, bool isActive) {
    return _card(
      child: Row(
        children: [
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  id,
                  style: const TextStyle(
                    color: AppColors.cardText,
                    fontWeight: FontWeight.bold,
                    letterSpacing: 2,
                    fontFeatures: [FontFeature.tabularFigures()],
                  ),
                ),
                const SizedBox(height: 4),
                if (isActive)
                  Row(
                    children: const [
                      Icon(Icons.circle, size: 9, color: AppColors.ok),
                      SizedBox(width: 6),
                      Text('active',
                          style: TextStyle(
                              color: AppColors.ok,
                              fontSize: 12,
                              fontWeight: FontWeight.w600)),
                    ],
                  )
                else
                  TextButton(
                    style: TextButton.styleFrom(
                      foregroundColor: AppColors.accent,
                      padding: EdgeInsets.zero,
                      minimumSize: const Size(0, 0),
                      tapTargetSize: MaterialTapTargetSize.shrinkWrap,
                    ),
                    onPressed: () => _switchTo(id),
                    child: const Text('Switch to this device'),
                  ),
              ],
            ),
          ),
          IconButton(
            tooltip: 'Unlink device',
            color: AppColors.alert,
            onPressed: () => _unlink(id),
            icon: const Icon(Icons.link_off),
          ),
        ],
      ),
    );
  }
}
