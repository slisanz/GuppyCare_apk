#include "scheduler.h"
#include "config.h"
#include "firebase_client.h"
#include "time_sync.h"
#include "feeder.h"
#include <ArduinoJson.h>

namespace Scheduler {

// Dual-poll: fast tick untuk trigger & time-match, slow tick untuk refresh
// cached schedules + portion (jarang berubah, hemat request).
// Dipercepat 2026-05-15 (req user: Set Time responsif, toleransi <5s).
// TDS sengaja TIDAK diubah (tetap di config.h: sample 10s, upload 60s).
constexpr unsigned long FAST_POLL_MS = 3000;    // tiap 3s — match jadwal + manual feed
constexpr unsigned long SLOW_POLL_MS = 5000;    // tiap 5s — tarik jadwal/porsi terbaru dari app
constexpr int MAX_SCHEDULES = 3;

struct ScheduleEntry {
    String time;     // "HH:MM"
    bool   enabled;
};

static ScheduleEntry cachedSchedules[MAX_SCHEDULES];
static int           cachedPortionMg = 10;
static bool          cacheReady      = false;

// Anti double-feed dalam menit yg sama: "YYYYMMDDHHMM"
static String lastFedMinute[MAX_SCHEDULES] = {"", "", ""};

static String minuteKey() {
    time_t t = TimeSync::now();
    struct tm tmLoc;
    localtime_r(&t, &tmLoc);
    char buf[13];
    snprintf(buf, sizeof(buf), "%04d%02d%02d%02d%02d",
             tmLoc.tm_year + 1900, tmLoc.tm_mon + 1, tmLoc.tm_mday,
             tmLoc.tm_hour, tmLoc.tm_min);
    return String(buf);
}

static void doFeed(int portion_mg, const char* reason) {
    Serial.printf("[Scheduler] TRIGGER FEED (%s) portion=%d mg\n",
                  reason, portion_mg);
    Feeder::feedPortion(portion_mg);
    FirebaseClient::putString("/last_feed_time", TimeSync::currentIso8601Utc());
}

// Ambil & parse /schedules + /portion_mg dari RTDB, simpan di cache.
static void refreshCache() {
    String schedJson = FirebaseClient::getRaw("/schedules");
    if (schedJson.length() == 0 || schedJson == "null") {
        cacheReady = false;
        Serial.println("[Scheduler] cache refresh: /schedules kosong. Pakai 'seed'.");
        return;
    }

    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, schedJson);
    if (err) {
        Serial.printf("[Scheduler] cache JSON parse error: %s\n", err.c_str());
        return;
    }

    // Reset cache
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        cachedSchedules[i].time    = "";
        cachedSchedules[i].enabled = false;
    }

    auto loadOne = [&](int idx, const String& t, bool en) {
        if (idx < 0 || idx >= MAX_SCHEDULES) return;
        cachedSchedules[idx].time    = t;
        cachedSchedules[idx].enabled = en;
    };

    if (doc.is<JsonArray>()) {
        JsonArray arr = doc.as<JsonArray>();
        for (size_t i = 0; i < arr.size() && i < MAX_SCHEDULES; i++) {
            JsonObject o = arr[i];
            loadOne(i, o["time"] | String(""), o["enabled"] | false);
        }
    } else if (doc.is<JsonObject>()) {
        for (int i = 0; i < MAX_SCHEDULES; i++) {
            JsonObject o = doc[String(i)];
            if (o.isNull()) continue;
            loadOne(i, o["time"] | String(""), o["enabled"] | false);
        }
    }

    // Portion
    String portionStr = FirebaseClient::getRaw("/portion_mg");
    int p = portionStr.toInt();
    if (p >= 1 && p <= 1000) cachedPortionMg = p;

    cacheReady = true;
    Serial.printf("[Scheduler] cache refreshed — portion=%d mg | ", cachedPortionMg);
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        Serial.printf("[%d:%s%s] ", i,
                      cachedSchedules[i].time.c_str(),
                      cachedSchedules[i].enabled ? "*" : "-");
    }
    Serial.println();
}

void begin() {
    Serial.println("[Scheduler] begin (fast=5s, slow=60s).");
}

void seedDefaults() {
    Serial.println("[Scheduler] seed default schedules + portion ke RTDB...");
    FirebaseClient::putString("/schedules/0/time",    "08:00");
    FirebaseClient::putBool  ("/schedules/0/enabled", true);
    FirebaseClient::putString("/schedules/1/time",    "12:00");
    FirebaseClient::putBool  ("/schedules/1/enabled", false);
    FirebaseClient::putString("/schedules/2/time",    "17:00");
    FirebaseClient::putBool  ("/schedules/2/enabled", true);
    FirebaseClient::putInt   ("/portion_mg",          10);
    FirebaseClient::putBool  ("/manual_feed_trigger", false);
    FirebaseClient::putBool  ("/tds_alert",           false);
    Serial.println("[Scheduler] seed DONE.");
    // Refresh cache supaya langsung kepakai
    refreshCache();
}

// Fast tick: time-match dari cache + cek manual_feed_trigger
static void fastTick() {
    // 1) Manual trigger — TIDAK butuh jam. Proses walau NTP belum sync,
    //    biar tombol "Beri Makan Sekarang" tetap jalan tanpa clock.
    String mft = FirebaseClient::getRaw("/manual_feed_trigger");
    if (mft == "true") {
        Serial.println("[Scheduler] manual_feed_trigger=true detected.");
        doFeed(cachedPortionMg, "manual");
        FirebaseClient::putBool("/manual_feed_trigger", false);
    }

    // 2) Schedule match (local check) — BUTUH jam. Skip kalau NTP belum sync.
    if (!TimeSync::isSynced()) return;
    if (!cacheReady) return;
    String currentHhMm = TimeSync::currentHhMm();
    String curMinKey   = minuteKey();
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (!cachedSchedules[i].enabled) continue;
        if (cachedSchedules[i].time != currentHhMm) continue;
        if (lastFedMinute[i] == curMinKey) continue;  // sudah feed menit ini
        doFeed(cachedPortionMg, ("schedule[" + String(i) + "]").c_str());
        lastFedMinute[i] = curMinKey;
    }
}

void poll() {
    static unsigned long lastFast = 0;
    static unsigned long lastSlow = 0;
    static bool          firstRun = true;
    unsigned long nowMs = millis();

    // Pertama kali masuk: refresh cache supaya gak nungguin 60s
    if (firstRun) {
        firstRun = false;
        refreshCache();
        lastSlow = nowMs;
    }

    if (nowMs - lastFast >= FAST_POLL_MS) {
        lastFast = nowMs;
        fastTick();
    }

    if (nowMs - lastSlow >= SLOW_POLL_MS) {
        lastSlow = nowMs;
        refreshCache();
    }
}

}  // namespace Scheduler
