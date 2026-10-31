#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>

// Dowsing: turn slowly in a circle and the Deck works out which way a well
// lies. It's real: your body blocks part of the signal from behind you, so
// a router reads several dB weaker when you face away from it.
//
// Each signal reading is filed under the direction you faced (one of 12
// slices of 30 degrees). Once most of the circle is covered, the
// circle's overall shape gives the answer. Pure logic, so tests can play
// it made-up sweeps.

class Dowse {
 public:
  static constexpr int Bins = 12;
  static constexpr float BinRad = 6.2831853f / Bins;
  static constexpr int NeedBins = 10;     // slices covered before it answers

  void reset() {
    memset(sum_, 0, sizeof(sum_));
    memset(n_, 0, sizeof(n_));
    samples_ = 0;
    last_ = -127;
  }

  // heading in radians (Motion::heading), rssi in dBm.
  void add(float heading, int rssi) {
    const int b = binOf(heading);
    sum_[b] += rssi;
    n_[b]++;
    samples_++;
    last_ = rssi;
  }

  int covered() const {
    int c = 0;
    for (int i = 0; i < Bins; i++) c += n_[i] > 0;
    return c;
  }
  static constexpr int NeedSamples = 24;  // and readings, so a fast spin isn't trusted
  bool ready() const { return covered() >= NeedBins && samples_ >= NeedSamples; }
  int samples() const { return samples_; }
  int last() const { return last_; }
  bool has(int bin) const { return n_[bin] > 0; }
  float average(int bin) const { return n_[bin] ? sum_[bin] / n_[bin] : -127; }
  static float binCenter(int bin) { return -3.14159265f + (bin + 0.5f) * BinRad; }

  // The heading the well lies along, in the same frame as Motion::heading.
  // Fits one smooth wave around the circle (its first harmonic): that uses
  // the dip behind you as well as the high side, which matters because the
  // body's shadow is a dip, with the front half nearly flat.
  float bearing() const {
    float c, s;
    harmonic(c, s);
    return atan2f(s, c);
  }

  // Peak-to-peak size of that wave, in dB. Under about 2.5 dB the pull is
  // too weak to trust.
  float contrast() const {
    float c, s;
    harmonic(c, s);
    return 2 * sqrtf(c * c + s * s);
  }

  static int binOf(float heading) {
    float a = heading + 3.14159265f;
    while (a < 0) a += 6.2831853f;
    while (a >= 6.2831853f) a -= 6.2831853f;
    int b = (int)(a / BinRad);
    return b >= Bins ? Bins - 1 : b;
  }

 private:
  // First harmonic of the slice averages, over the slices covered.
  void harmonic(float& c, float& s) const {
    float mean = 0;
    int k = 0;
    for (int i = 0; i < Bins; i++) if (has(i)) { mean += average(i); k++; }
    c = s = 0;
    if (k < 3) return;
    mean /= k;
    for (int i = 0; i < Bins; i++) {
      if (!has(i)) continue;
      const float a = binCenter(i), v = average(i) - mean;
      c += v * cosf(a);
      s += v * sinf(a);
    }
    c *= 2.0f / k;
    s *= 2.0f / k;
  }

  float sum_[Bins] = {0};
  int n_[Bins] = {0};
  int samples_ = 0;
  int last_ = -127;
};
