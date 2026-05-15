#pragma once
#include <Arduino.h>

namespace Scheduler {
    // Polling default tiap 15 detik (POLL_INTERVAL_MS).
    void begin();

    // Panggil di loop(). Internal di-throttle, aman dipanggil tiap iterasi.
    void poll();

    // Tulis 3 jadwal default + portion_mg ke Firebase (helper, dipanggil
    // dari Serial command "seed" kalau RTDB masih kosong).
    void seedDefaults();
}
