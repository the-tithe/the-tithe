#pragma once
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include "Config.h"
#include "Well.h"

// Listens for routers in the background. A passive scan only hears the
// beacons routers broadcast to everyone, the same list a phone's WiFi
// menu shows. The Deck never joins a network or sends anything.
//
// Network names are hashed with the device key the moment a scan is read,
// then dropped. Nothing else in the firmware ever sees them.

namespace Scanner {

constexpr int MaxNets = 96;

inline bool ok = false;
inline bool running = false;
inline uint32_t nextAt = 0;
inline uint32_t failures = 0;
inline Net nets[MaxNets];
inline int count = 0;      // networks in the last finished scan
inline uint32_t waited = 0;   // scans put off for want of memory (see poll)

// When a scan ends, the WiFi library borrows one block for every network heard,
// about 80 bytes each (not just the MaxNets kept), and a failed borrow there
// restarts the Deck. 300 networks need 24 KB, so a scan waits until a block
// that big is free.
constexpr uint32_t ScanNeedsBytes = 24 * 1024;

// Dowsing listens to one channel only, so readings come about 7 times a
// second instead of once every 2.4 s. Those quick scans see only part of
// the sky, so they're marked and kept out of the well list and the log.
inline uint8_t focusChannel = 0;   // 0 = every channel
inline bool runningFocused = false;
inline bool lastFocused = false;   // the scan in `nets` was a quick one
constexpr uint32_t FocusMsPerChannel = 110;

inline bool begin() {
  WiFi.persistent(false);   // never write WiFi settings to flash
  ok = WiFi.mode(WIFI_STA);
  if (ok) {
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, true);   // forget any saved network (in RAM only, see above)
  }
  return ok;
}

inline Lock lockOf(wifi_auth_mode_t m) {
  switch (m) {
    case WIFI_AUTH_OPEN: return Lock::Open;
    case WIFI_AUTH_WEP: return Lock::Wep;
    case WIFI_AUTH_WPA_PSK: return Lock::Wpa;
    case WIFI_AUTH_WPA2_PSK: case WIFI_AUTH_WPA_WPA2_PSK: return Lock::Wpa2;
    case WIFI_AUTH_WPA3_PSK: return Lock::Wpa3;
    case WIFI_AUTH_WPA2_WPA3_PSK: return Lock::Wpa23;
    case WIFI_AUTH_WPA2_ENTERPRISE: case WIFI_AUTH_WPA3_ENT_192: return Lock::Enterprise;
    case WIFI_AUTH_WAPI_PSK: return Lock::Wapi;
    default: return Lock::Other;
  }
}

// Call every frame. Returns true when a new scan has just finished and
// `nets` / `count` hold it.
inline bool poll(uint32_t nowMs) {
  if (!ok) return false;
  if (!running) {
    if ((int32_t)(nowMs - nextAt) < 0) return false;
    if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < ScanNeedsBytes) { waited++; nextAt = nowMs + 1000; return false; }
    runningFocused = focusChannel != 0;
    const int16_t r = runningFocused
                          ? WiFi.scanNetworks(true, true, true, FocusMsPerChannel, focusChannel)
                          : WiFi.scanNetworks(true, true, true, Config::ScanMsPerChannel);
    running = r == WIFI_SCAN_RUNNING || r >= 0;
    if (!running) { failures++; nextAt = nowMs + 1000; }
    return false;
  }
  const int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return false;
  running = false;
  lastFocused = runningFocused;
  nextAt = nowMs + (focusChannel ? 0 : Config::ScanGapMs);
  if (n < 0) { failures++; return false; }

  count = 0;
  for (int i = 0; i < n && count < MaxNets; i++) {
    const wifi_ap_record_t* ap = (const wifi_ap_record_t*)WiFi.getScanInfoByIndex(i);
    if (!ap) continue;
    Net& o = nets[count++];
    memcpy(o.bssid, ap->bssid, 6);
    o.rssi = ap->rssi;
    o.channel = ap->primary;
    o.lock = lockOf(ap->authmode);
    const int len = (int)strnlen((const char*)ap->ssid, sizeof(ap->ssid));
    o.hidden = WellRules::isHidden((const char*)ap->ssid, len);
    o.nameKey = o.hidden ? 0 : Keys::deviceHash(ap->ssid, len);
    o.guest = !o.hidden && WellRules::isGuestName((const char*)ap->ssid, len);
    o.phone = !o.hidden && WellRules::isPhoneName((const char*)ap->ssid, len);
  }
  WiFi.scanDelete();
  return true;
}

// Start the next scan now instead of after the usual pause.
inline void hurry(uint32_t nowMs) { if (!running) nextAt = nowMs; }

} // namespace Scanner
