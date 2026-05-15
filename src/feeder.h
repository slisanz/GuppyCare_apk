#pragma once
#include <Arduino.h>

namespace Feeder {
    // Attach servo ke pin & set NEUTRAL. Panggil sekali di setup().
    void begin();

    // Jalankan satu siklus feed dengan porsi tertentu (mg). Di-clamp 1..1000 mg.
    // Sequence: OPEN 200ms -> hold NEUTRAL (portion_mg * SERVO_MS_PER_MG) -> CLOSE 200ms -> NEUTRAL.
    // Blocking, durasi total ~400ms + portion_mg*50ms (1000mg = ~50.4s).
    void feedPortion(int portion_mg);
}
