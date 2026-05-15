#pragma once
#include <Arduino.h>

namespace TdsSensor {
    // Init ADC. Panggil sekali di setup().
    void begin();

    // Panggil di loop(). Internal di-throttle:
    //   - sample tiap TDS_SAMPLE_INTERVAL_MS (10s)
    //   - upload tds_ppm ke RTDB tiap TDS_UPLOAD_INTERVAL_MS (60s)
    //   - update tds_alert flag saat ppm >600 (3x konsekutif) atau pulih
    void poll();

    // Latest ppm yang sudah di-sample. -1 kalau belum pernah.
    float lastPpm();

    bool isAlerting();
}
