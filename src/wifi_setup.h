#pragma once
#include <Arduino.h>

namespace WifiSetup {
    // Blocking. Coba konek pakai credential tersimpan. Kalau gagal/belum
    // pernah setup, buka captive portal AP "GuppyCare-Setup" selama
    // AP_TIMEOUT_S detik. Return true kalau akhirnya konek WiFi.
    bool begin();

    // Hapus credential tersimpan + reboot (dipanggil dari tombol/serial
    // command kalau user mau pindah jaringan).
    void resetCredentials();

    bool isConnected();
    String localIp();
}
