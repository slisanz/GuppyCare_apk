#pragma once

// Pin assignments (sesuai wiring user)
namespace Pins {
    constexpr int WIFI_RESET_BTN = 13;   // Tombol fisik reset WiFi (ke GND, pull-up internal)
    constexpr int SERVO          = 18;   // Servo SG90 signal (kuning)
    constexpr int TDS_ADC        = 34;   // TDS Meter V1.0 analog out
}

// Behavior constants
namespace Cfg {
    // TDS alert threshold — di atas ini, kirim notifikasi "ganti air"
    constexpr int TDS_THRESHOLD_PPM = 600;

    // ==================================================================
    // SERVO POSITIONAL (AKTIF — migrasi 2026-05-18).
    // Servo SG90 positional (0-180 deg). Saat feed: putar ke OPEN, tahan
    // selama porsi (pakan jatuh), lalu balik ke CLOSED (90 deg lawan arah).
    // Sudut bisa di-tune live via command Serial `servo` tanpa re-flash.
    // ==================================================================
    constexpr int SERVO_ANGLE_CLOSED = 0;     // rest / gerbang tutup
    constexpr int SERVO_ANGLE_OPEN   = 90;    // gerbang buka (90 deg dari closed)
    constexpr int SERVO_MOVE_MS      = 500;   // tunggu servo capai sudut (settle)

    // Servo PWM attach range (dipakai positional & legacy). SG90 positional
    // spec ~500-2400us utk 0-180 deg; sudut yg dipakai (0 deg=500us,
    // 90 deg=1500us) jauh di dalam range, aman.
    constexpr int SERVO_PWM_MIN_US = 500;
    constexpr int SERVO_PWM_MAX_US = 2500;

    // Kalibrasi gram pakan -> durasi gerbang TERBUKA. TIDAK berubah saat
    // migrasi positional. Estimasi awal: 50ms per mg (10mg = 500ms).
    // Di-tune lewat app/timbangan nanti.
    constexpr int SERVO_MS_PER_MG = 50;

    // ==================================================================
    // === LEGACY (continuous rotation) — TIDAK DIPAKAI lagi sejak migrasi
    // === positional 2026-05-18. Disimpan utuh utk referensi kalibrasi lama
    // === (kalibrasi user 2026-05-12 via main.cpp v4/v5 mode setting).
    // ------------------------------------------------------------------
    // Unit lama berlabel "SG90 9g" tapi behavior FS90R (continuous):
    // pulse width = kecepatan & arah, BUKAN sudut.
    // Pulse 1500us  = stop / neutral (servo benar-benar diam)
    // Pulse 2100us  = arah BUKA katup pakan
    // Pulse 900us   = arah TUTUP katup pakan
    // Durasi 200ms  = waktu rotasi servo (SAMA untuk buka & tutup)
    //
    // Sequence lama (continuous):
    //   1. Pulse OPEN  (2100us)  selama SERVO_ROTATE_MS (200ms) -> rotasi buka
    //   2. Pulse NEUTRAL (1500us) selama portionMs (= portion_mg * SERVO_MS_PER_MG)
    //      -> gerbang ditahan terbuka, pakan jatuh sesuai porsi
    //   3. Pulse CLOSE (900us)   selama SERVO_ROTATE_MS (200ms) -> rotasi tutup
    //   4. Pulse NEUTRAL (1500us) -> servo diam, tunggu jadwal berikutnya
    // ==================================================================
    constexpr int SERVO_PULSE_NEUTRAL_US = 1500;   // [legacy] stop
    constexpr int SERVO_PULSE_OPEN_US    = 2100;   // [legacy] arah buka
    constexpr int SERVO_PULSE_CLOSE_US   = 900;    // [legacy] arah tutup
    constexpr int SERVO_ROTATE_MS        = 200;    // [legacy] durasi rotasi

    // Sampling intervals
    constexpr unsigned long TDS_SAMPLE_INTERVAL_MS  = 10000;   // baca sensor tiap 10s
    constexpr unsigned long TDS_UPLOAD_INTERVAL_MS  = 30000;   // upload ke Firebase tiap 30s (req user 2026-05-18, dulu 60s)
    constexpr unsigned long SCHEDULE_CHECK_INTERVAL_MS = 30000;  // cek jadwal tiap 30s

    // WiFiManager
    constexpr char AP_NAME[] = "GuppyCare-Setup";
    constexpr int AP_TIMEOUT_S = 180;       // 3 menit timeout config portal

    // Tombol fisik reset WiFi: tahan selama ini (ms) baru creds dihapus & reboot
    constexpr unsigned long WIFI_RESET_HOLD_MS = 3000;

    // NTP
    constexpr char NTP_SERVER[]   = "pool.ntp.org";
    constexpr long GMT_OFFSET_SEC = 7 * 3600;  // WIB (UTC+7)
    constexpr int  DST_OFFSET_SEC = 0;
}

// Firebase project "guppycare" (Singapore region) — diisi 2026-05-12.
// CATATAN: kalau project ini di-publish ke GitHub, pertimbangkan pindah
// kredensial ini ke file terpisah yang masuk .gitignore.
namespace FbCfg {
    constexpr char API_KEY[]       = "AIzaSyCSGcc0zyMrjkhjC3sBb00O4_8_z2Zs-bk";
    constexpr char DATABASE_URL[]  = "https://guppycare-default-rtdb.asia-southeast1.firebasedatabase.app";
    constexpr char PROJECT_ID[]    = "guppycare";
}
