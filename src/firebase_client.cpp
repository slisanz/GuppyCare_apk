#include "firebase_client.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace FirebaseClient {

static String s_deviceId = "UNKNOWN";

void setDeviceId(const String& id) { s_deviceId = id; }

// Build full URL: https://<DATABASE_URL>/devices/<deviceId>/<subPath>.json
static String buildUrl(const String& subPath) {
    String p = subPath;
    if (!p.startsWith("/")) p = "/" + p;
    return String(FbCfg::DATABASE_URL)
         + "/devices/" + s_deviceId + p + ".json";
}

// Jumlah percobaan utk error koneksi/TLS transient (SSL EOF, connection
// refused). code <= 0 = level koneksi -> layak retry. code >= 400 =
// ditolak server (rules/format) -> percuma retry.
static constexpr int FB_MAX_TRIES = 3;

// PUT helper. body adalah JSON literal (mis. "true", "12.5", "\"hello\"").
static bool putRaw(const String& subPath, const String& body) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[FB] PUT skip — WiFi DOWN");
        return false;
    }

    String url = buildUrl(subPath);
    for (int attempt = 1; attempt <= FB_MAX_TRIES; attempt++) {
        WiFiClientSecure ssl;
        ssl.setInsecure();  // skip CA validation untuk simplicity

        HTTPClient http;
        if (!http.begin(ssl, url)) {
            Serial.printf("[FB] PUT begin() FAIL  url=%s\n", url.c_str());
            return false;
        }
        http.addHeader("Content-Type", "application/json");

        int code = http.PUT(body);
        String resp = http.getString();
        http.end();

        if (code >= 200 && code < 300) {
            Serial.printf("[FB] PUT %s OK (%d)%s\n", subPath.c_str(), code,
                          attempt > 1 ? " [retry]" : "");
            return true;
        }
        if (code > 0) {  // ditolak server, retry percuma
            Serial.printf("[FB] PUT %s FAIL code=%d resp=%s\n",
                          subPath.c_str(), code, resp.c_str());
            return false;
        }
        Serial.printf("[FB] PUT %s koneksi gagal code=%d (coba %d/%d)\n",
                      subPath.c_str(), code, attempt, FB_MAX_TRIES);
        if (attempt < FB_MAX_TRIES) delay(400);
    }
    Serial.printf("[FB] PUT %s FAIL — habis %d kali coba\n",
                  subPath.c_str(), FB_MAX_TRIES);
    return false;
}

bool putBool(const String& subPath, bool value) {
    return putRaw(subPath, value ? "true" : "false");
}

bool putFloat(const String& subPath, float value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", value);
    return putRaw(subPath, buf);
}

bool putInt(const String& subPath, int value) {
    return putRaw(subPath, String(value));
}

bool putString(const String& subPath, const String& value) {
    // Quote-escape minimal — value tidak boleh berisi " atau backslash.
    return putRaw(subPath, "\"" + value + "\"");
}

bool patchJson(const String& subPath, const String& jsonBody) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[FB] PATCH skip — WiFi DOWN");
        return false;
    }

    WiFiClientSecure ssl;
    ssl.setInsecure();

    HTTPClient http;
    // subPath "" -> PATCH di root device (/devices/{id}.json), update banyak
    // child sekaligus dalam 1 request -> 1 handshake TLS, 1 event RTDB.
    String p = subPath;
    if (p.length() && !p.startsWith("/")) p = "/" + p;
    String url = String(FbCfg::DATABASE_URL)
               + "/devices/" + s_deviceId + p + ".json";
    if (!http.begin(ssl, url)) {
        Serial.printf("[FB] PATCH begin() FAIL  url=%s\n", url.c_str());
        return false;
    }
    http.addHeader("Content-Type", "application/json");

    int code = http.sendRequest("PATCH", jsonBody);
    String resp = http.getString();
    http.end();

    if (code >= 200 && code < 300) {
        Serial.printf("[FB] PATCH OK (%d) %s\n", code, jsonBody.c_str());
        return true;
    } else {
        Serial.printf("[FB] PATCH FAIL code=%d resp=%s\n",
                      code, resp.c_str());
        return false;
    }
}

String getRaw(const String& subPath) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[FB] GET skip — WiFi DOWN");
        return String();
    }

    String url = buildUrl(subPath);
    for (int attempt = 1; attempt <= FB_MAX_TRIES; attempt++) {
        WiFiClientSecure ssl;
        ssl.setInsecure();

        HTTPClient http;
        if (!http.begin(ssl, url)) {
            Serial.printf("[FB] GET begin() FAIL  url=%s\n", url.c_str());
            return String();
        }

        int code = http.GET();
        String resp = http.getString();
        http.end();

        if (code >= 200 && code < 300) return resp;
        if (code > 0) {  // ditolak server, retry percuma
            Serial.printf("[FB] GET %s FAIL code=%d\n",
                          subPath.c_str(), code);
            return String();
        }
        Serial.printf("[FB] GET %s koneksi gagal code=%d (coba %d/%d)\n",
                      subPath.c_str(), code, attempt, FB_MAX_TRIES);
        if (attempt < FB_MAX_TRIES) delay(400);
    }
    Serial.printf("[FB] GET %s FAIL — habis %d kali coba\n",
                  subPath.c_str(), FB_MAX_TRIES);
    return String();
}

}  // namespace FirebaseClient
