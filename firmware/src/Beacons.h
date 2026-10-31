#pragma once
#include <Arduino.h>
#include <esp_wifi.h>
#include "Well.h"
#include "game/RealClock.h"

// Reads routers' timers (game/RealClock.h). Between scans the radio listens on
// a few channels for a moment and keeps each beacon's timer. Listening only:
// nothing is sent. BSSIDs stay in RAM and are hashed with the device key
// before anything is kept.

namespace Beacons {

constexpr int Max = 24;
constexpr uint32_t DwellMs = 120;   // a router beacons about every 102 ms
constexpr int MaxChannels = 3;

struct Heard {
  uint8_t bssid[6];
  uint8_t channel;
  int8_t rssi;
  uint64_t tsf;
};

inline Heard heard[Max];
inline volatile int count = 0;
inline portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
inline bool listening = false;
inline uint8_t channels[MaxChannels];
inline int nChannels = 0, at = 0;
inline uint32_t channelAt = 0;

// Runs on the radio's task (not an interrupt) for every management frame heard.
inline void onFrame(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*)buf;
  const uint8_t* f = p->payload;
  if (p->rx_ctrl.sig_len < 36 || f[0] != 0x80) return;   // beacons only
  uint64_t tsf = 0;
  for (int i = 7; i >= 0; i--) tsf = (tsf << 8) | f[24 + i];
  portENTER_CRITICAL(&lock);
  int i = 0;
  while (i < count && memcmp(heard[i].bssid, f + 16, 6) != 0) i++;
  if (i < Max) {
    if (i == count) { memcpy(heard[i].bssid, f + 16, 6); count = i + 1; }
    heard[i].tsf = tsf;
    heard[i].rssi = p->rx_ctrl.rssi;
    heard[i].channel = (uint8_t)p->rx_ctrl.channel;
  }
  portEXIT_CRITICAL(&lock);
}

// Start listening on up to three channels. Only call while no scan is running.
inline bool start(const uint8_t* ch, int n, uint32_t nowMs) {
  nChannels = 0;
  for (int i = 0; i < n && nChannels < MaxChannels; i++) {
    if (ch[i] < 1 || ch[i] > 14) continue;
    bool dup = false;
    for (int k = 0; k < nChannels; k++) dup |= channels[k] == ch[i];
    if (!dup) channels[nChannels++] = ch[i];
  }
  if (nChannels == 0) return false;
  count = 0;
  wifi_promiscuous_filter_t filter = {WIFI_PROMIS_FILTER_MASK_MGMT};
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(onFrame);
  if (esp_wifi_set_promiscuous(true) != ESP_OK) return false;
  at = 0;
  esp_wifi_set_channel(channels[0], WIFI_SECOND_CHAN_NONE);
  channelAt = nowMs;
  listening = true;
  return true;
}

// Call every frame while listening. Returns true once it has finished.
inline bool poll(uint32_t nowMs) {
  if (!listening || nowMs - channelAt < DwellMs) return false;
  if (++at < nChannels) {
    esp_wifi_set_channel(channels[at], WIFI_SECOND_CHAN_NONE);
    channelAt = nowMs;
    return false;
  }
  esp_wifi_set_promiscuous(false);
  listening = false;
  return true;
}

// The strongest routers heard, as marks for a note (or to compare with one).
inline int marks(Game::ClockMark* out, int max) {
  Heard h[Max];
  portENTER_CRITICAL(&lock);
  const int n = count;
  memcpy(h, heard, sizeof(Heard) * n);
  portEXIT_CRITICAL(&lock);
  for (int a = 1; a < n; a++)
    for (int b = a; b > 0 && h[b].rssi > h[b - 1].rssi; b--) { const Heard t = h[b]; h[b] = h[b - 1]; h[b - 1] = t; }
  int k = 0;
  for (int i = 0; i < n && k < max; i++, k++) {
    out[k] = Game::ClockMark{};
    out[k].id = Keys::deviceHash(h[i].bssid, 6);
    out[k].channel = h[i].channel;
    out[k].tsf = h[i].tsf;
  }
  return k;
}

} // namespace Beacons
