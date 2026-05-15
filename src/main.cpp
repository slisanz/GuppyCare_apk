#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "wifi_setup.h"
#include "time_sync.h"
#include "firebase_client.h"
#include "feeder.h"
#include "scheduler.h"
#include "tds_sensor.h"
#include "fcm_sender.h"

// =====================================================================
// PHASE 4 — Full system: WiFi + NTP + Firebase + Feeder + Scheduler + TDS
// ---------------------------------------------------------------------
// Yang aktif:
//   - WiFiManager captive portal
//   - NTP sync WIB
//   - Firebase REST write (online + heartbeat)
//   - Feeder servo (kalibrasi pulse)
//   - Scheduler dual-poll (fast 5s manual+match, slow 60s refresh cache)
//   - TDS sensor 10s sample, 60s upload, alert debounce 3x
//
// Serial command:
//   "?"           -> status sekarang
//   "wifi_reset"  -> hapus credential WiFi & reboot
//   "ping"        -> heartbeat manual ke Firebase
//   "feed N"      -> feed N mg langsung (bypass scheduler)
//   "seed"        -> tulis 3 jadwal default ke RTDB
//   "trigger"     -> set manual_feed_trigger=true
//   "alert on"    -> paksa tds_alert true (test FCM nanti)
//   "alert off"   -> paksa tds_alert false
// =====================================================================

static String deviceId;

constexpr unsigned long HEARTBEAT_MS = 30000;
constexpr unsigned long LED_BLINK_MS = 1000;

String makeDeviceId() {
    uint64_t mac = ESP.getEfuseMac();
    char buf[13];
    snprintf(buf, sizeof(buf), "%012llX", mac);
    return String(buf);
}

void pingFirebase() {
    FirebaseClient::putBool  ("/online",    true);
    FirebaseClient::putString("/last_seen", TimeSync::currentIso8601Utc());
}

void printStatus() {
    Serial.println();
    Serial.println("=============== STATUS ===============");
    Serial.printf(" device_id : %s\n", deviceId.c_str());
    Serial.printf(" WiFi      : %s\n",
                  WifiSetup::isConnected()
                    ? (String("OK ") + WiFi.SSID() + " " + WifiSetup::localIp()).c_str()
                    : "DISCONNECTED");
    Serial.printf(" NTP       : %s\n",
                  TimeSync::isSynced()
                    ? (String("synced ") + TimeSync::currentHhMm() + " WIB").c_str()
                    : "not synced");
    Serial.printf(" TDS       : %.1f ppm  alert=%s  threshold=%d\n",
                  TdsSensor::lastPpm(),
                  TdsSensor::isAlerting() ? "ON" : "off",
                  Cfg::TDS_THRESHOLD_PPM);
    Serial.printf(" FB path   : /devices/%s/\n", deviceId.c_str());
    Serial.println("======================================");
    Serial.println();
}

void handleSerialCommand(String cmd) {
    cmd.trim();
    if (cmd == "?") {
        printStatus();
    } else if (cmd == "wifi_reset") {
        WifiSetup::resetCredentials();
    } else if (cmd == "ping") {
        pingFirebase();
    } else if (cmd == "seed") {
        Scheduler::seedDefaults();
    } else if (cmd == "trigger") {
        FirebaseClient::putBool("/manual_feed_trigger", true);
        Serial.println("[main] manual_feed_trigger=true (akan diproses max 5s).");
    } else if (cmd.startsWith("feed ")) {
        int mg = cmd.substring(5).toInt();
        if (mg < 1) mg = 10;
        Feeder::feedPortion(mg);
    } else if (cmd == "alert on") {
        FirebaseClient::putBool("/tds_alert", true);
        Serial.println("[main] tds_alert=true (forced, untuk test FCM Phase 7).");
    } else if (cmd == "alert off") {
        FirebaseClient::putBool("/tds_alert", false);
        Serial.println("[main] tds_alert=false (forced).");
    } else if (cmd == "notif") {
        Serial.println("[main] kirim test notif FCM...");
        int n = FcmSender::sendAlert("GuppyCare \xF0\x9F\x90\x9F",
                                     "Test notif — kalau ini masuk, FCM jalan!");
        Serial.printf("[main] hasil: %d notif terkirim\n", n);
    } else if (cmd.length() > 0) {
        Serial.printf("[?] Command '%s' tidak dikenali.\n", cmd.c_str());
        Serial.println("    Pakai: ? | wifi_reset | ping | seed | trigger | feed N | alert on|off | notif");
    }
}

// =====================================================================
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("===========================================");
    Serial.println(" GuppyCare — Phase 4 (Full System)");
    Serial.println("===========================================");

    pinMode(Pins::LED, OUTPUT);
    analogReadResolution(12);

    deviceId = makeDeviceId();
    FirebaseClient::setDeviceId(deviceId);
    Serial.printf(" device_id = %s\n", deviceId.c_str());
    Serial.printf(" FB path   = /devices/%s/\n", deviceId.c_str());
    Serial.println("-------------------------------------------");

    Feeder::begin();
    TdsSensor::begin();
    FcmSender::begin();

    if (!WifiSetup::begin()) {
        Serial.println("[BOOT] WiFi gagal, reboot dalam 5 detik...");
        delay(5000);
        ESP.restart();
    }

    if (!TimeSync::begin()) {
        Serial.println("[BOOT] NTP gagal, lanjut tapi jam tidak akurat.");
    }

    Serial.println("[BOOT] Tulis status awal ke Firebase...");
    FirebaseClient::putBool  ("/online",       true);
    FirebaseClient::putString("/device_label", "GuppyCare ESP32");
    FirebaseClient::putString("/last_seen",    TimeSync::currentIso8601Utc());

    Scheduler::begin();

    Serial.println();
    Serial.println(">>> Phase 4 siap. Full system jalan.");
    Serial.println(">>> Command: ? | seed | trigger | feed N | alert on|off | notif | wifi_reset");
    Serial.println();
}

// =====================================================================
void loop() {
    static unsigned long lastLed       = 0;
    static unsigned long lastHeartbeat = 0;
    static String        buf;
    unsigned long now = millis();

    if (now - lastLed >= LED_BLINK_MS) {
        lastLed = now;
        digitalWrite(Pins::LED, !digitalRead(Pins::LED));
    }

    if (now - lastHeartbeat >= HEARTBEAT_MS) {
        lastHeartbeat = now;
        pingFirebase();
    }

    Scheduler::poll();
    TdsSensor::poll();

    while (Serial.available()) {
        char ch = (char)Serial.read();
        if (ch == '\n' || ch == '\r') {
            if (buf.length() > 0) {
                handleSerialCommand(buf);
                buf = "";
            }
        } else {
            buf += ch;
            if (buf.length() > 64) buf = "";
        }
    }
}
