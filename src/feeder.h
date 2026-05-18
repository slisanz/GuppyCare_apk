#pragma once
#include <Arduino.h>

namespace Feeder {
    // Attach servo positional ke pin & set ke sudut CLOSED. Panggil sekali
    // di setup().
    void begin();

    // Jalankan satu siklus feed dengan porsi tertentu (mg). Di-clamp 1..1000 mg.
    // Servo positional: putar ke SERVO_ANGLE_OPEN (90 deg) -> tahan terbuka
    // (portion_mg * SERVO_MS_PER_MG) -> putar balik ke SERVO_ANGLE_CLOSED.
    // Blocking, durasi total ~(2*SERVO_MOVE_MS) + portion_mg*50ms.
    void feedPortion(int portion_mg);

    // Putar servo langsung ke sudut deg (0..180). Untuk tuning mounting
    // lewat command Serial `servo`. Tidak dipakai di alur feed normal.
    void moveTo(int deg);
}
