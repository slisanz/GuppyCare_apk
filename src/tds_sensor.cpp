#include "tds_sensor.h"
#include "config.h"
#include "firebase_client.h"
#include "fcm_sender.h"

namespace TdsSensor {

constexpr int  DEBOUNCE_COUNT = 3;     // 3x berturut-turut baru toggle alert
constexpr int  SAMPLES_PER_READ = 10;  // rata-rata 10 sample ADC per pembacaan

static float lastPpmVal     = -1.0f;
static float publishedPpm   = -1.0f;   // angka terakhir yg KONSISTEN dgn status
static bool  alerting       = false;
static int   highStreak     = 0;       // konter ppm > threshold berturut-turut
static int   lowStreak      = 0;       // konter ppm <= threshold berturut-turut

static float readOnce() {
    long sum = 0;
    for (int i = 0; i < SAMPLES_PER_READ; i++) {
        sum += analogRead(Pins::TDS_ADC);
        delay(2);
    }
    float raw = (float)sum / SAMPLES_PER_READ;
    float voltage = raw * 3.3f / 4095.0f;
    float ppm = (133.42f * voltage * voltage * voltage
               - 255.86f * voltage * voltage
               + 857.39f * voltage) * 0.5f;
    if (ppm < 0) ppm = 0;
    return ppm;
}

// Return true kalau status alert baru saja berubah (perlu sync angka ke RTDB).
static bool evaluateAlert(float ppm) {
    bool prevAlerting = alerting;

    if (ppm > Cfg::TDS_THRESHOLD_PPM) {
        highStreak++;
        lowStreak = 0;
        if (!alerting && highStreak >= DEBOUNCE_COUNT) {
            alerting = true;
            Serial.printf("[TDS] *** ALERT ON *** (%.1f ppm konsisten %dx)\n",
                          ppm, highStreak);
        }
    } else {
        lowStreak++;
        highStreak = 0;
        if (alerting && lowStreak >= DEBOUNCE_COUNT) {
            alerting = false;
            Serial.printf("[TDS] alert OFF — air pulih (%.1f ppm)\n", ppm);
        }
    }

    if (prevAlerting != alerting) {
        // Flip: ppm ini SUDAH di sisi yg sesuai status baru -> jadikan
        // acuan angka konsisten (dipakai "tahan angka" saat debounce).
        publishedPpm = ppm;
        // 1 request atomik: status + angka mendarat di HP barengan.
        char json[64];
        snprintf(json, sizeof(json),
                 "{\"tds_alert\":%s,\"tds_ppm\":%.2f}",
                 alerting ? "true" : "false", ppm);
        FirebaseClient::patchJson("", json);
        if (alerting) {
            // Baru saja flip ON -> push notif "ganti air" ke HP.
            char body[96];
            snprintf(body, sizeof(body),
                     "Water is dirty (TDS %.0f ppm). Time to change the aquarium water!",
                     ppm);
            FcmSender::sendAlert("GuppyCare \xF0\x9F\x90\x9F", body);
        }
        return true;
    }
    return false;
}

void begin() {
    Serial.println("[TDS] begin (sample 10s, upload 30s, debounce 3x).");
}

void poll() {
    static unsigned long lastSample = 0;
    static unsigned long lastUpload = 0;
    unsigned long now = millis();

    // Sample tiap 10 detik
    if (now - lastSample >= Cfg::TDS_SAMPLE_INTERVAL_MS) {
        lastSample = now;
        float ppm = readOnce();
        lastPpmVal = ppm;
        Serial.printf("[TDS] %.1f ppm  (high=%d low=%d alert=%s)\n",
                      ppm, highStreak, lowStreak, alerting ? "ON" : "off");
        // Kalau status alert flip, evaluateAlert() sudah PATCH tds_alert +
        // tds_ppm sekaligus (atomik). Reset lastUpload biar siklus 60s nggak
        // langsung nimpa dgn write redundan.
        if (evaluateAlert(ppm)) {
            lastUpload = now;
        }
    }

    // Upload berkala ke RTDB: 1 PATCH atomik bawa tds_ppm + tds_alert
    // sekaligus -> angka & status SELALU mendarat di app berbarengan
    // (1 event RTDB), nggak ada lagi "angka berubah duluan, status nyusul".
    // Jumlah request sama (1 per siklus), cuma PATCH 2 field, bukan PUT 1.
    //
    // Anti-kontradiksi: kalau angka mentah ada di sisi ambang yg BERLAWANAN
    // dgn status (lagi di tengah jendela debounce), JANGAN kirim angka itu.
    // Tahan di angka konsisten terakhir -> app nggak pernah lihat "status
    // Alert tapi angka < ambang" atau sebaliknya. Pas status flip, keduanya
    // loncat barengan (publishedPpm di-update di evaluateAlert).
    if (lastPpmVal >= 0 && now - lastUpload >= Cfg::TDS_UPLOAD_INTERVAL_MS) {
        lastUpload = now;
        float pub = lastPpmVal;
        bool ppmHigh = lastPpmVal > Cfg::TDS_THRESHOLD_PPM;
        if (ppmHigh != alerting && publishedPpm >= 0.0f) {
            pub = publishedPpm;            // mid-debounce: tahan angka
        } else {
            publishedPpm = lastPpmVal;     // konsisten: angka boleh maju
        }
        char json[64];
        snprintf(json, sizeof(json),
                 "{\"tds_ppm\":%.2f,\"tds_alert\":%s}",
                 pub, alerting ? "true" : "false");
        FirebaseClient::patchJson("", json);
    }
}

float lastPpm()    { return lastPpmVal; }
bool  isAlerting() { return alerting;   }

}  // namespace TdsSensor
