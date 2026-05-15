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

// PUT helper. body adalah JSON literal (mis. "true", "12.5", "\"hello\"").
static bool putRaw(const String& subPath, const String& body) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[FB] PUT skip — WiFi DOWN");
        return false;
    }

    WiFiClientSecure ssl;
    ssl.setInsecure();  // skip CA validation untuk simplicity

    HTTPClient http;
    String url = buildUrl(subPath);
    if (!http.begin(ssl, url)) {
        Serial.printf("[FB] PUT begin() FAIL  url=%s\n", url.c_str());
        return false;
    }
    http.addHeader("Content-Type", "application/json");

    int code = http.PUT(body);
    String resp = http.getString();
    http.end();

    if (code >= 200 && code < 300) {
        Serial.printf("[FB] PUT %s OK (%d)\n", subPath.c_str(), code);
        return true;
    } else {
        Serial.printf("[FB] PUT %s FAIL code=%d resp=%s\n",
                      subPath.c_str(), code, resp.c_str());
        return false;
    }
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

String getRaw(const String& subPath) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[FB] GET skip — WiFi DOWN");
        return String();
    }

    WiFiClientSecure ssl;
    ssl.setInsecure();

    HTTPClient http;
    String url = buildUrl(subPath);
    if (!http.begin(ssl, url)) {
        Serial.printf("[FB] GET begin() FAIL  url=%s\n", url.c_str());
        return String();
    }

    int code = http.GET();
    String resp = http.getString();
    http.end();

    if (code >= 200 && code < 300) {
        return resp;
    } else {
        Serial.printf("[FB] GET %s FAIL code=%d\n", subPath.c_str(), code);
        return String();
    }
}

}  // namespace FirebaseClient
