#include "feeder.h"
#include "config.h"
#include <ESP32Servo.h>

namespace Feeder {

static Servo servo;

void begin() {
    ESP32PWM::allocateTimer(0);
    servo.setPeriodHertz(50);
    servo.attach(Pins::SERVO,
                 Cfg::SERVO_PWM_MIN_US,
                 Cfg::SERVO_PWM_MAX_US);
    servo.write(Cfg::SERVO_ANGLE_CLOSED);
    Serial.printf("[Feeder] servo positional armed di CLOSED (%d deg).\n",
                  Cfg::SERVO_ANGLE_CLOSED);
}

void feedPortion(int portion_mg) {
    if (portion_mg < 1)    portion_mg = 1;
    if (portion_mg > 1000) portion_mg = 1000;  // sanity cap (1000mg = ~50s buka)

    int holdMs = portion_mg * Cfg::SERVO_MS_PER_MG;

    Serial.printf("[Feeder] FEED %d mg (hold open %d ms)\n", portion_mg, holdMs);

    // 1) Buka: putar servo ke sudut OPEN (90 deg), tunggu sampai posisi
    servo.write(Cfg::SERVO_ANGLE_OPEN);
    delay(Cfg::SERVO_MOVE_MS);

    // 2) Gerbang tahan terbuka — pakan jatuh sesuai porsi
    delay(holdMs);

    // 3) Tutup: putar balik ke sudut CLOSED (90 deg lawan arah), tunggu
    servo.write(Cfg::SERVO_ANGLE_CLOSED);
    delay(Cfg::SERVO_MOVE_MS);

    Serial.println("[Feeder] DONE.");
}

void moveTo(int deg) {
    if (deg < 0)   deg = 0;
    if (deg > 180) deg = 180;
    servo.write(deg);
    Serial.printf("[Feeder] servo -> %d deg\n", deg);
}

}  // namespace Feeder
