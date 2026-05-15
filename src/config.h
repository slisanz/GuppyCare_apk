#pragma once

// Pin assignments (sesuai wiring user)
namespace Pins {
    constexpr int LED      = 13;   // Status LED
    constexpr int SERVO    = 18;   // Servo SG90 signal (kuning)
    constexpr int TDS_ADC  = 34;   // TDS Meter V1.0 analog out
}

// Behavior constants
namespace Cfg {
    // TDS alert threshold — di atas ini, kirim notifikasi "ganti air"
    constexpr int TDS_THRESHOLD_PPM = 600;

    // ==================================================================
    // SERVO — Continuous rotation (label "SG90 9g" tapi behavior FS90R).
    // Hasil kalibrasi user 2026-05-12 via main.cpp v4/v5 (mode setting).
    // ==================================================================
    // Pulse 1500us  = stop / neutral (servo benar-benar diam)
    // Pulse 2100us  = arah BUKA katup pakan
    // Pulse 900us   = arah TUTUP katup pakan
    // Durasi 200ms  = waktu rotasi servo (SAMA untuk buka & tutup)
    //
    // Sequence pemberian pakan:
    //   1. Pulse OPEN  (2100us)  selama SERVO_ROTATE_MS (200ms) -> rotasi buka
    //   2. Pulse NEUTRAL (1500us) selama portionMs (= portion_mg * SERVO_MS_PER_MG)
    //      -> gerbang ditahan terbuka, pakan jatuh sesuai porsi
    //   3. Pulse CLOSE (900us)   selama SERVO_ROTATE_MS (200ms) -> rotasi tutup
    //   4. Pulse NEUTRAL (1500us) -> servo diam, tunggu jadwal berikutnya
    constexpr int SERVO_PULSE_NEUTRAL_US = 1500;   // stop
    constexpr int SERVO_PULSE_OPEN_US    = 2100;   // arah buka
    constexpr int SERVO_PULSE_CLOSE_US   = 900;    // arah tutup
    constexpr int SERVO_ROTATE_MS        = 200;    // durasi rotasi (buka & tutup)

    // Servo PWM attach range
    constexpr int SERVO_PWM_MIN_US = 500;
    constexpr int SERVO_PWM_MAX_US = 2500;

    // Kalibrasi gram pakan -> durasi BUKA (besaranWaktuGram).
    // Estimasi awal: 50ms per mg (10mg = 500ms). Di-tune lewat app nanti.
    constexpr int SERVO_MS_PER_MG = 50;

    // Sampling intervals
    constexpr unsigned long TDS_SAMPLE_INTERVAL_MS  = 10000;   // baca sensor tiap 10s
    constexpr unsigned long TDS_UPLOAD_INTERVAL_MS  = 60000;   // upload ke Firebase tiap 1m
    constexpr unsigned long SCHEDULE_CHECK_INTERVAL_MS = 30000;  // cek jadwal tiap 30s

    // WiFiManager
    constexpr char AP_NAME[] = "GuppyCare-Setup";
    constexpr int AP_TIMEOUT_S = 180;       // 3 menit timeout config portal

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
