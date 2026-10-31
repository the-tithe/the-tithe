#pragma once
#include <M5Cardputer.h>
#include <math.h>

// How loud it is around the Deck, for the Din relic: a tenth of a second
// from the microphone, as 0 (quiet room) to 1 (loud street). The speaker and
// microphone share the audio chip, so the speaker is paused while it listens.

namespace Mic {

inline double listen() {
  static int16_t buf[1600];
  memset(buf, 0, sizeof(buf));
  M5Cardputer.Speaker.end();
  if (!M5Cardputer.Mic.begin()) {
    M5Cardputer.Speaker.begin();
    return 0.5;
  }
  const bool ok = M5Cardputer.Mic.record(buf, sizeof(buf) / sizeof(buf[0]), 16000);
  const uint32_t t0 = millis();
  while (ok && M5Cardputer.Mic.isRecording() && millis() - t0 < 400) delay(2);
  const bool timedOut = M5Cardputer.Mic.isRecording();
  M5Cardputer.Mic.end();
  M5Cardputer.Speaker.begin();
  if (!ok || timedOut) return 0.5;
  const int n = sizeof(buf) / sizeof(buf[0]);
  double mean = 0, sum = 0;
  for (int i = 0; i < n; i++) mean += buf[i];
  mean /= n;
  for (int i = 0; i < n; i++) { const double d = buf[i] - mean; sum += d * d; }
  const double rms = sqrt(sum / n) / 32768.0;
  const double db = 20 * log10(rms + 1e-9);   // dB below full scale
  const double v = (db + 60) / 40;            // -60 dB quiet .. -20 dB loud
  return v < 0 ? 0 : v > 1 ? 1 : v;
}

} // namespace Mic
