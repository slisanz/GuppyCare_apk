#pragma once
#include <Arduino.h>

// Kirim push notification FCM HTTP v1 LANGSUNG dari ESP32 (tanpa Cloud
// Function / Blaze). Auth pakai service account -> JWT RS256 -> OAuth2
// access token (di-cache ~55 menit) -> POST ke endpoint FCM v1.
namespace FcmSender {

    // Panggil sekali di setup() (cuma logging info).
    void begin();

    // Ambil semua token di /devices/{id}/fcm_tokens lalu kirim notif.
    // Return jumlah token yang sukses terkirim (-1 kalau gagal auth).
    int sendAlert(const String& title, const String& body);
}
