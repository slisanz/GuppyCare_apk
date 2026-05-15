#pragma once
#include <Arduino.h>
#include <time.h>

namespace TimeSync {
    // Blocking. Trigger NTP sync via configTime() lalu tunggu sampai
    // tahun > 2024 (bukti jam beneran ke-sync, bukan epoch 1970).
    // timeoutMs default 15 detik. Return true kalau sukses.
    bool begin(unsigned long timeoutMs = 15000);

    bool isSynced();

    // "HH:MM" untuk match jadwal — pakai zona WIB (sudah di-offset).
    String currentHhMm();

    // ISO 8601 UTC, contoh "2026-05-12T08:00:15Z" — buat last_feed_time.
    String currentIso8601Utc();

    // Detik sejak epoch
    time_t now();
}
