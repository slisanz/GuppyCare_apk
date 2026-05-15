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
    servo.writeMicroseconds(Cfg::SERVO_PULSE_NEUTRAL_US);
    Serial.println("[Feeder] servo armed di NEUTRAL.");
}

void feedPortion(int portion_mg) {
    if (portion_mg < 1)    portion_mg = 1;
    if (portion_mg > 1000) portion_mg = 1000;  // sanity cap (1000mg = ~50s buka)

    int holdMs = portion_mg * Cfg::SERVO_MS_PER_MG;

    Serial.printf("[Feeder] FEED %d mg (hold open %d ms)\n", portion_mg, holdMs);

    // 1) Rotasi buka
    servo.writeMicroseconds(Cfg::SERVO_PULSE_OPEN_US);
    delay(Cfg::SERVO_ROTATE_MS);

    // 2) Gerbang stay open — pakan jatuh
    servo.writeMicroseconds(Cfg::SERVO_PULSE_NEUTRAL_US);
    delay(holdMs);

    // 3) Rotasi tutup
    servo.writeMicroseconds(Cfg::SERVO_PULSE_CLOSE_US);
    delay(Cfg::SERVO_ROTATE_MS);

    // 4) Diam
    servo.writeMicroseconds(Cfg::SERVO_PULSE_NEUTRAL_US);

    Serial.println("[Feeder] DONE.");
}

}  // namespace Feeder
