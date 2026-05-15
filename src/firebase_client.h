#pragma once
#include <Arduino.h>

namespace FirebaseClient {
    // Set device_id yang dipakai sebagai key Firebase. Dipanggil sekali
    // saat boot setelah makeDeviceId().
    void setDeviceId(const String& id);

    // PUT bool ke path relatif device, mis. "/online" -> /devices/{id}/online
    bool putBool(const String& subPath, bool value);

    // PUT angka (float, int via overload via toString)
    bool putFloat(const String& subPath, float value);
    bool putInt(const String& subPath, int value);

    // PUT string (akan di-wrap quote oleh JSON)
    bool putString(const String& subPath, const String& value);

    // GET raw JSON dari path. Return string kosong kalau error.
    String getRaw(const String& subPath);
}
