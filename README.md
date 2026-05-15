# GuppyCare

Sistem otomatis untuk akuarium ikan guppy berbasis ESP32 dan aplikasi Android.

## Fitur

- Jadwal makan: 3 jadwal yang bisa diaktifkan atau dinonaktifkan dari HP.
- Feed Amount: atur porsi pakan 1 sampai 1000 mg lewat slider atau input angka. Servo SG90 membuka pintu pakan sesuai porsi.
- Monitoring TDS: jika air kotor (di atas 600 ppm), aplikasi mengirim notifikasi ke HP.

Distribusi aplikasi gratis lewat sideload APK (tidak melalui Play Store).

## Struktur

- `src/` firmware ESP32 (PlatformIO, framework Arduino).
- `mobile_app/` aplikasi Android (Flutter).
- `GuppyCare_image_logo/` aset logo dan ikon.
- `platformio.ini` konfigurasi build firmware.

## Hardware

| Komponen | Pin ESP32 | Power | Catatan |
|---|---|---|---|
| LED status | GPIO13 | - | Anoda ke pin via resistor 220 ohm, katoda ke GND |
| Servo SG90 sinyal | GPIO18 | 5V dan GND | Kabel merah ke 5V, coklat ke GND |
| TDS Meter AO | GPIO34 | 3.3V dan GND | Input ADC saja |

## Build firmware

1. Install PlatformIO.
2. Salin `src/fcm_credentials.h.example` menjadi `src/fcm_credentials.h` lalu isi kredensial service account Firebase Anda. File ini tidak ikut ke repo.
3. Build dan upload ke board (COM port sesuai, baud 921600).
4. Serial Monitor 115200 untuk melihat log dan device id.

WiFi diatur lewat captive portal "GuppyCare-Setup" saat pertama kali, tidak perlu hardcode kredensial WiFi.

## Build aplikasi

```
cd mobile_app
flutter pub get
flutter build apk --release
```

APK hasil build ada di `mobile_app/build/app/outputs/flutter-apk/`.

## Install APK di HP

1. Buka Settings, Apps, Install unknown apps, lalu izinkan sumber yang dipakai (browser atau file manager).
2. Buka file APK, tekan Install.
3. Buka aplikasi, login dengan akun Google, lalu hubungkan device id yang tampil di Serial Monitor.

## Catatan Firebase

Aplikasi memakai Firebase Authentication, Realtime Database, dan Cloud Messaging. File `google-services.json` dan `firebase_options.dart` berisi konfigurasi project (API key client, bukan kunci rahasia, sama dengan yang ada di dalam APK). Pastikan Realtime Database Security Rules dikunci dengan benar sebelum dipakai luas, jangan biarkan dalam mode test terbuka.
