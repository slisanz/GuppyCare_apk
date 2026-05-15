#include "tds_sensor.h"
#include "config.h"
#include "firebase_client.h"
#include "fcm_sender.h"

namespace TdsSensor {

constexpr int  DEBOUNCE_COUNT = 3;     // 3x berturut-turut baru toggle alert
constexpr int  SAMPLES_PER_READ = 10;  // rata-rata 10 sample ADC per pembacaan

static float lastPpmVal     = -1.0f;
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

static void evaluateAlert(float ppm) {
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
        FirebaseClient::putBool("/tds_alert", alerting);
        if (alerting) {
            // Baru saja flip ON -> push notif "ganti air" ke HP.
            char body[96];
            snprintf(body, sizeof(body),
                     "Air kotor (TDS %.0f ppm). Saatnya ganti air akuarium!",
                     ppm);
            FcmSender::sendAlert("GuppyCare \xF0\x9F\x90\x9F", body);
        }
    }
}

void begin() {
    Serial.println("[TDS] begin (sample 10s, upload 60s, debounce 3x).");
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
        evaluateAlert(ppm);
    }

    // Upload ke RTDB tiap 60 detik
    if (lastPpmVal >= 0 && now - lastUpload >= Cfg::TDS_UPLOAD_INTERVAL_MS) {
        lastUpload = now;
        FirebaseClient::putFloat("/tds_ppm", lastPpmVal);
    }
}

float lastPpm()    { return lastPpmVal; }
bool  isAlerting() { return alerting;   }

}  // namespace TdsSensor
