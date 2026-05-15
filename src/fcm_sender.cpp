#include "fcm_sender.h"
#include "fcm_credentials.h"
#include "firebase_client.h"
#include "time_sync.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "mbedtls/version.h"
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

namespace FcmSender {

// Access token OAuth2 di-cache. expires_in biasanya 3599s; refresh
// sebelum habis (pakai millis sebagai patokan).
static String        s_accessToken  = "";
static unsigned long s_tokenGotMs   = 0;
static unsigned long s_tokenTtlMs   = 0;

// ---- base64url (tanpa padding) dari mbedtls base64 standar -----------
static String b64url(const uint8_t* data, size_t len) {
    size_t olen = 0;
    mbedtls_base64_encode(nullptr, 0, &olen, data, len);
    std::vector<unsigned char> buf(olen + 1, 0);
    if (mbedtls_base64_encode(buf.data(), buf.size(), &olen, data, len) != 0)
        return String();
    String s = String((const char*)buf.data());
    s.replace("+", "-");
    s.replace("/", "_");
    while (s.endsWith("=")) s.remove(s.length() - 1);
    return s;
}

static String b64url(const String& in) {
    return b64url((const uint8_t*)in.c_str(), in.length());
}

// ---- RS256 sign (SHA-256 + RSA) pakai private key service account ----
static bool rs256Sign(const String& signingInput, String& sigB64Url) {
    // 1) SHA-256 dari signing input
    uint8_t hash[32];
    const mbedtls_md_info_t* mdInfo =
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (mbedtls_md(mdInfo, (const uint8_t*)signingInput.c_str(),
                   signingInput.length(), hash) != 0) {
        Serial.println("[FCM] SHA256 gagal");
        return false;
    }

    // 2) RNG
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context ctr;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr);
    const char* pers = "guppycare_fcm";
    bool ok = false;
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    if (mbedtls_ctr_drbg_seed(&ctr, mbedtls_entropy_func, &entropy,
                              (const uint8_t*)pers, strlen(pers)) != 0) {
        Serial.println("[FCM] ctr_drbg seed gagal");
        goto cleanup;
    }

    // 3) Parse private key (PEM PKCS#8). keylen HARUS termasuk '\0'.
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
    if (mbedtls_pk_parse_key(
            &pk, (const uint8_t*)FcmCred::PRIVATE_KEY,
            strlen(FcmCred::PRIVATE_KEY) + 1, nullptr, 0,
            mbedtls_ctr_drbg_random, &ctr) != 0) {
#else
    if (mbedtls_pk_parse_key(
            &pk, (const uint8_t*)FcmCred::PRIVATE_KEY,
            strlen(FcmCred::PRIVATE_KEY) + 1, nullptr, 0) != 0) {
#endif
        Serial.println("[FCM] parse private key GAGAL — cek fcm_credentials.h");
        goto cleanup;
    }

    {
        uint8_t sig[512];   // cukup untuk RSA-2048/4096
        size_t  sigLen = 0;
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
        int r = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, 32,
                                sig, sizeof(sig), &sigLen,
                                mbedtls_ctr_drbg_random, &ctr);
#else
        int r = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, 32,
                                sig, &sigLen,
                                mbedtls_ctr_drbg_random, &ctr);
#endif
        if (r != 0) {
            Serial.printf("[FCM] pk_sign gagal (-0x%04X)\n", -r);
            goto cleanup;
        }
        sigB64Url = b64url(sig, sigLen);
        ok = sigB64Url.length() > 0;
    }

cleanup:
    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr);
    mbedtls_entropy_free(&entropy);
    return ok;
}

// ---- Tukar JWT -> OAuth2 access token (cache ~55 menit) -------------
static bool ensureAccessToken() {
    unsigned long nowMs = millis();
    if (s_accessToken.length() > 0 && s_tokenTtlMs > 0 &&
        (nowMs - s_tokenGotMs) < s_tokenTtlMs) {
        return true;  // masih valid
    }
    if (!TimeSync::isSynced()) {
        Serial.println("[FCM] NTP belum sync — JWT butuh jam akurat. Skip.");
        return false;
    }

    time_t iat = TimeSync::now();
    time_t exp = iat + 3600;

    // Header & claim JWT
    String header = F("{\"alg\":\"RS256\",\"typ\":\"JWT\"}");
    String claim  = String("{\"iss\":\"") + FcmCred::CLIENT_EMAIL +
        "\",\"scope\":\"https://www.googleapis.com/auth/firebase.messaging\","
        "\"aud\":\"https://oauth2.googleapis.com/token\",\"iat\":" +
        String((uint32_t)iat) + ",\"exp\":" + String((uint32_t)exp) + "}";

    String signingInput = b64url(header) + "." + b64url(claim);
    String sig;
    if (!rs256Sign(signingInput, sig)) return false;
    String jwt = signingInput + "." + sig;
    // Bebasin string gede sebelum buka TLS (kurangi tekanan heap)
    signingInput = String();
    sig = String();
    header = String();
    claim = String();

    String body =
        "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer"
        "&assertion=" + jwt;

    Serial.printf("[FCM] pre-TLS heap: free=%u maxBlock=%u\n",
                  ESP.getFreeHeap(), ESP.getMaxAllocHeap());

    int    code = -1;
    String resp;
    for (int attempt = 1; attempt <= 3; attempt++) {
        WiFiClientSecure ssl;
        ssl.setInsecure();
        ssl.setHandshakeTimeout(20);  // detik
        HTTPClient http;
        http.setConnectTimeout(15000);  // ms
        http.setTimeout(20000);         // ms
        if (!http.begin(ssl, "https://oauth2.googleapis.com/token")) {
            Serial.printf("[FCM] begin() oauth gagal (attempt %d)\n", attempt);
            delay(800);
            continue;
        }
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        code = http.POST(body);
        resp = http.getString();
        http.end();
        if (code == 200) break;
        Serial.printf("[FCM] oauth attempt %d gagal code=%d heap=%u\n",
                      attempt, code, ESP.getFreeHeap());
        delay(1000);
    }

    if (code != 200) {
        Serial.printf("[FCM] oauth token FAIL code=%d resp=%s\n",
                      code, resp.c_str());
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, resp)) {
        Serial.println("[FCM] oauth resp parse gagal");
        return false;
    }
    s_accessToken = doc["access_token"] | "";
    long expiresIn = doc["expires_in"] | 3599;
    if (s_accessToken.length() == 0) {
        Serial.println("[FCM] access_token kosong");
        return false;
    }
    s_tokenGotMs = millis();
    s_tokenTtlMs = (unsigned long)(expiresIn - 300) * 1000UL;  // refresh -5mnt
    Serial.printf("[FCM] access token OK (expires_in=%ld)\n", expiresIn);
    return true;
}

// ---- Kirim 1 notif ke 1 token --------------------------------------
static bool sendToToken(const String& token, const String& title,
                        const String& bodyText) {
    WiFiClientSecure ssl;
    ssl.setInsecure();
    ssl.setHandshakeTimeout(20);
    HTTPClient http;
    http.setConnectTimeout(15000);
    http.setTimeout(20000);
    String url = String("https://fcm.googleapis.com/v1/projects/") +
                 FcmCred::PROJECT_ID + "/messages:send";
    if (!http.begin(ssl, url)) return false;
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + s_accessToken);

    JsonDocument doc;
    JsonObject msg = doc["message"].to<JsonObject>();
    msg["token"] = token;
    JsonObject notif = msg["notification"].to<JsonObject>();
    notif["title"] = title;
    notif["body"]  = bodyText;
    JsonObject androidCfg = msg["android"].to<JsonObject>();
    androidCfg["priority"] = "high";

    String payload;
    serializeJson(doc, payload);

    int code = http.POST(payload);
    String resp = http.getString();
    http.end();

    if (code == 200) {
        Serial.println("[FCM] notif terkirim ke 1 token");
        return true;
    }
    Serial.printf("[FCM] send FAIL code=%d resp=%s\n", code, resp.c_str());
    return false;
}

void begin() {
    Serial.println("[FCM] sender siap (HTTP v1, auth service account).");
}

int sendAlert(const String& title, const String& body) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[FCM] WiFi DOWN — skip notif");
        return -1;
    }
    if (!ensureAccessToken()) return -1;

    String tokensJson = FirebaseClient::getRaw("/fcm_tokens");
    if (tokensJson.length() == 0 || tokensJson == "null") {
        Serial.println("[FCM] belum ada token terdaftar (app belum login?)");
        return 0;
    }

    JsonDocument doc;
    if (deserializeJson(doc, tokensJson)) {
        Serial.println("[FCM] parse fcm_tokens gagal");
        return -1;
    }

    int sent = 0;
    for (JsonPair kv : doc.as<JsonObject>()) {
        String tok = kv.value().as<String>();
        if (tok.length() < 10) continue;
        if (sendToToken(tok, title, body)) sent++;
    }
    Serial.printf("[FCM] sendAlert selesai — %d notif terkirim\n", sent);
    return sent;
}

}  // namespace FcmSender
