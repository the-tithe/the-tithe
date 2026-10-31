#pragma once
#include <M5Cardputer.h>
#include "Motion.h"

// Feeds the Cardputer ADV's motion sensor (BMI270) into Motion, once a
// frame. On a Cardputer without one, `available` stays false and every
// motion feature stays off.

namespace Imu {

inline Motion motion;
inline bool available = false;

inline void begin() { available = M5.Imu.isEnabled(); }

inline void update(float dt) {
  if (!available) return;
  M5.Imu.update();
  const auto& d = M5.Imu.getImuData();
  float a[3], g[3];
  for (int i = 0; i < 3; i++) {
    a[i] = d.accel.value[i];
    g[i] = d.gyro.value[i] * Motion::DegToRad;
  }
  motion.update(a, g, dt);
}

} // namespace Imu
