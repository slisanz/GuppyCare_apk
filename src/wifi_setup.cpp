#include "wifi_setup.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiManager.h>

namespace WifiSetup {

bool begin() {
    WiFiManager wm;

    // Timeout captive portal supaya gak nungguin selamanya
    wm.setConfigPortalTimeout(Cfg::AP_TIMEOUT_S);

    // Title halaman portal
    wm.setTitle("GuppyCare WiFi Setup");

    Serial.println();
    Serial.println("[WiFi] autoConnect mulai...");
    Serial.printf("[WiFi] Kalau credential belum ada, HP konek ke AP: %s\n",
                  Cfg::AP_NAME);
    Serial.println("[WiFi] Lalu buka 192.168.4.1 di browser, pilih SSID & masukin password.");

    bool ok = wm.autoConnect(Cfg::AP_NAME);

    if (!ok) {
        Serial.println("[WiFi] FAIL — captive portal timeout / user batal.");
        return false;
    }

    // Matikan modem-sleep: default ESP32 nidurin radio di antara beacon
    // -> TLS handshake ke Firebase suka keputus (-29312 SSL EOF /
    // connection refused) walau WiFi "connected". Ini penyebab utama
    // device keliatan offline intermiten. Auto-reconnect kalau AP drop.
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);

    Serial.printf("[WiFi] OK  SSID=%s  IP=%s  RSSI=%d dBm\n",
                  WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());
    return true;
}

void resetCredentials() {
    Serial.println("[WiFi] Reset credentials & reboot...");
    WiFiManager wm;
    wm.resetSettings();
    delay(500);
    ESP.restart();
}

bool isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String localIp() {
    return WiFi.localIP().toString();
}

}  // namespace WifiSetup
