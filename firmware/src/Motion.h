#pragma once
#include <math.h>
#include <stdint.h>

// How the Deck is being held and moved, from the Cardputer ADV's motion
// sensor (the original Cardputer has none, and every feature here simply
// stays off). Pure logic, fed one sample a frame by Imu.h, so the tests can
// play it made-up hand movements.
//
// Device frame, as in Hazke's Gyro.h: X right across the screen, Y up the
// screen, Z out of the screen. Lying flat, screen up, the accelerometer
// reads about (0, 0, +1) g.
//
// The vertical ("up") is a complementary filter: the gyro turns it, the
// accelerometer slowly pulls it back toward gravity. Adapted from Hazke
// (MIT, REZOR).

class Motion {
 public:
  // ---- Tuning ----
  static constexpr float DegToRad = 0.017453293f;
  static constexpr float Tau = 0.4f;                 // s, gravity's pull on "up"
  static constexpr float ShakeG = 0.25f;             // ignore gravity when this far off 1 g
  static constexpr float StillRate = 4.0f * DegToRad;
  static constexpr float StillG = 0.06f;
  static constexpr float BiasTau = 2.0f;             // s, gyro drift tracking while still
  static constexpr float FlickRate = 280.0f * DegToRad;   // a snap of the wrist, top edge back
  static constexpr float TwistRate = 200.0f * DegToRad;   // a quick turn like a steering wheel
  static constexpr float TwistWindow = 0.6f;              // s to swing back the other way
  static constexpr float GestureGap = 0.8f;               // s between gestures
  static constexpr float StepG = 0.10f;                   // a footstep's bounce, above the slow average
  static constexpr float StepMin = 0.33f, StepMax = 0.85f;   // s between steps: about 1.2 to 3 a second
  static constexpr int WalkSteps = 4;                     // steps in a row before it counts as walking

  // ---- State, read freely ----
  float up[3] = {0, 0, 1};
  float bias[3] = {0, 0, 0};
  float heading = 0;       // radians, + clockwise seen from above; drifts slowly
  float yawRate = 0;       // rad/s about the real vertical
  float stillFor = 0;      // seconds held steady
  float steadiness = 0;    // 0 shaking .. 1 rock steady, smoothed
  float faceDownFor = 0;   // seconds lying screen-down
  float awayFor = 0;       // seconds not held up to look at (in a pocket, say)
  float walkFor = 0;       // seconds walking without a break
  float walkTotal = 0;     // seconds walked since the Deck woke
  float t = 0;

  // Events, true for the one frame they happen.
  bool flick = false;      // a snap of the wrist, top edge back or forward: a pulse
  bool twist = false;      // turned one way and straight back: a ward
  bool lifted = false;     // picked up to look at, after lying still or being away

  // Held up to look at: the screen leans back at least a little (about 9
  // degrees) and isn't upside down. Upright in a pocket doesn't count.
  bool viewing() const { return up[2] > 0.15f && up[1] > -0.5f; }
  bool faceDown() const { return up[2] < -0.75f; }

  // Walking: footsteps' bounce at a walking pace, held in a hand or in a pocket.
  bool walking() const { return walking_; }
  // Seconds of walking in about the last five minutes.
  float walkedRecently() const {
    float s = 0;
    for (float b : walkBuckets_) s += b;
    return s;
  }

  // Tilt of the screen away from how it's usually held, -1..1 on each axis,
  // for the scrying glass to float its wells on.
  float tiltX() const { return clamp(-up[0] * 1.6f); }
  float tiltY() const { return clamp((up[1] - 0.55f) * 1.6f); }

  void level(const float acc[3]) {
    const float n = norm(acc);
    if (n > 0.1f) for (int i = 0; i < 3; i++) up[i] = acc[i] / n;
    primed_ = true;
  }

  // acc in g, gyr in rad/s, both in the device frame.
  void update(const float acc[3], const float gyr[3], float dt) {
    flick = twist = lifted = false;
    if (dt <= 0) return;
    if (!primed_) { level(acc); return; }
    t += dt;
    const float n = norm(acc);
    float w[3];
    for (int i = 0; i < 3; i++) w[i] = gyr[i] - bias[i];
    const float rate = norm(w);
    const bool still = rate < StillRate && fabsf(n - 1.0f) < StillG;
    if (still) {
      const float k = dt / (BiasTau + dt);
      for (int i = 0; i < 3; i++) bias[i] += w[i] * k;
      stillFor += dt;
    } else {
      stillFor = 0;
    }
    const float jitter = rate / (30 * DegToRad) + fabsf(n - 1.0f) / 0.15f;
    const float target = jitter >= 1 ? 0 : 1 - jitter;
    steadiness += (target - steadiness) * (dt / (0.5f + dt));

    // Turn "up" with the gyro, then nudge it toward gravity.
    const float v0 = up[0], v1 = up[1], v2 = up[2];
    up[0] += (v1 * w[2] - v2 * w[1]) * dt;
    up[1] += (v2 * w[0] - v0 * w[2]) * dt;
    up[2] += (v0 * w[1] - v1 * w[0]) * dt;
    if (fabsf(n - 1.0f) < ShakeG) {
      // Gravity pointing the opposite way means the estimate is lost (a
      // flip the gyro missed); start again from gravity.
      const float agree = (acc[0] * up[0] + acc[1] * up[1] + acc[2] * up[2]) / n;
      if (agree < -0.8f) {
        level(acc);
      } else {
        const float k = dt / (Tau + dt);
        for (int i = 0; i < 3; i++) up[i] += (acc[i] / n - up[i]) * k;
      }
    }
    const float len = norm(up);
    if (len > 1e-3f) for (int i = 0; i < 3; i++) up[i] /= len;

    // Turning about the real vertical, whatever the holding angle.
    yawRate = -(w[0] * up[0] + w[1] * up[1] + w[2] * up[2]);
    heading = wrap(heading + yawRate * dt);

    steps(n, dt);

    faceDownFor = faceDown() ? faceDownFor + dt : 0;
    const bool wasAway = awayFor > 1.0f, wasResting = restFor_ > 2.0f;
    awayFor = viewing() ? 0 : awayFor + dt;
    viewFor_ = viewing() ? viewFor_ + dt : 0;
    // How long it was held up before this frame's swing began (a flick can
    // tip it out of the pose in its first frame).
    if (rate < 150 * DegToRad) heldFor_ = viewFor_;
    restFor_ = still ? restFor_ + dt : 0;

    // Lifted: a quick move that ends up held to look at, after the Deck was
    // put away or lying still.
    if (rate > 60 * DegToRad) { if (wasAway || wasResting) liftArmed_ = 0.8f; }
    if (liftArmed_ > 0) {
      liftArmed_ -= dt;
      if (viewing() && rate < 120 * DegToRad && t - lastGesture_ > GestureGap * 0.5f) {
        lifted = true;
        liftArmed_ = 0;
      }
    }

    // Gestures, spaced out so one swing can't fire twice, and only from a
    // Deck that's been held up to look at: an arm swinging at your side only
    // passes through that pose for a moment.
    if (t - lastGesture_ > GestureGap && heldFor_ > 0.4f) {
      if (fabsf(w[0]) > FlickRate && fabsf(w[2]) < FlickRate * 0.6f) {
        flick = true;
        lastGesture_ = t;
        twistDir_ = 0;
      } else if (fabsf(w[2]) > TwistRate && fabsf(w[0]) < TwistRate) {
        const int dir = w[2] > 0 ? 1 : -1;
        if (twistDir_ == -dir && t - twistAt_ < TwistWindow) {
          twist = true;
          lastGesture_ = t;
          twistDir_ = 0;
        } else if (twistDir_ != dir) {
          twistDir_ = dir;
          twistAt_ = t;
        }
      }
    }
  }

  static float wrap(float a) {
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    return a;
  }

 private:
  // Footsteps: each one lifts the acceleration above its slow average and
  // lets it fall back. A walk is several of them, evenly spaced.
  void steps(float n, float dt) {
    gSlow_ += (n - gSlow_) * (dt / (0.6f + dt));
    const float bounce = n - gSlow_;
    if (bounce < 0.03f) stepArmed_ = true;
    if (stepArmed_ && bounce > StepG) {
      stepArmed_ = false;
      const float gap = t - lastStep_;
      stepsInRow_ = gap >= StepMin && gap <= StepMax ? stepsInRow_ + 1 : 1;
      lastStep_ = t;
    }
    if (t - lastStep_ > StepMax * 1.5f) stepsInRow_ = 0;
    walking_ = stepsInRow_ >= WalkSteps;
    walkFor = walking_ ? walkFor + dt : 0;
    if (walking_) walkTotal += dt;
    // Ten half-minute buckets of walking time, the oldest emptied as time moves on.
    const int b = (int)(t / 30);
    for (; bucket_ < b; bucket_++) walkBuckets_[(bucket_ + 1) % 10] = 0;
    if (walking_) walkBuckets_[b % 10] += dt;
  }

  static float norm(const float v[3]) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
  static float clamp(float v) { return v < -1 ? -1 : v > 1 ? 1 : v; }
  bool primed_ = false;
  float restFor_ = 0;
  float viewFor_ = 0;
  float heldFor_ = 0;
  float liftArmed_ = 0;
  float lastGesture_ = -10;
  int twistDir_ = 0;
  float twistAt_ = -10;
  float gSlow_ = 1;
  bool stepArmed_ = false;
  float lastStep_ = -10;
  int stepsInRow_ = 0;
  bool walking_ = false;
  int bucket_ = 0;
  float walkBuckets_[10] = {0};
};
