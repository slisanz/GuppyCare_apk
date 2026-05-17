#include "time_sync.h"
#include "config.h"

namespace TimeSync {

static bool          synced              = false;
static unsigned long lastArmMs           = 0;

// Arm/restart SNTP. configTime() men-trigger client SNTP yang lanjut
// jalan di background walau begin() keburu timeout.
static void armSntp() {
    configTime(Cfg::GMT_OFFSET_SEC, Cfg::DST_OFFSET_SEC, Cfg::NTP_SERVER);
    lastArmMs = millis();
}

bool begin(unsigned long timeoutMs) {
    Serial.printf("[NTP] Sync via %s (offset WIB +%lds)...\n",
                  Cfg::NTP_SERVER, Cfg::GMT_OFFSET_SEC);

    armSntp();

    unsigned long t0 = millis();
    struct tm timeinfo;
    while (millis() - t0 < timeoutMs) {
        if (getLocalTime(&timeinfo, 1000)) {
            // Tahun di struct tm = years since 1900
            if (timeinfo.tm_year + 1900 > 2024) {
                synced = true;
                Serial.printf("[NTP] OK  %04d-%02d-%02d %02d:%02d:%02d WIB\n",
                              timeinfo.tm_year + 1900,
                              timeinfo.tm_mon + 1,
                              timeinfo.tm_mday,
                              timeinfo.tm_hour,
                              timeinfo.tm_min,
                              timeinfo.tm_sec);
                return true;
            }
        }
        delay(200);
    }

    Serial.println("[NTP] FAIL — timeout. SNTP lanjut retry di background.");
    return false;
}

// Self-heal: kalau begin() timeout, SNTP background bisa sync belakangan.
// Cek apakah jam sudah valid (late-sync). Kalau belum & sudah lewat 60s
// sejak arm terakhir, arm ulang SNTP. Dipanggil tiap fast tick (~3s).
bool isSynced() {
    if (synced) return true;

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 50) && timeinfo.tm_year + 1900 > 2024) {
        synced = true;
        Serial.printf("[NTP] late-sync OK  %04d-%02d-%02d %02d:%02d WIB\n",
                      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                      timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min);
        return true;
    }

    if (millis() - lastArmMs > 60000) {
        Serial.println("[NTP] masih belum sync — arm ulang SNTP.");
        armSntp();
    }
    return false;
}

String currentHhMm() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return String("??:??");
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    return String(buf);
}

String currentIso8601Utc() {
    time_t t = ::time(nullptr);
    struct tm tmUtc;
    gmtime_r(&t, &tmUtc);
    char buf[25];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             tmUtc.tm_year + 1900,
             tmUtc.tm_mon + 1,
             tmUtc.tm_mday,
             tmUtc.tm_hour,
             tmUtc.tm_min,
             tmUtc.tm_sec);
    return String(buf);
}

time_t now() { return ::time(nullptr); }

}  // namespace TimeSync
