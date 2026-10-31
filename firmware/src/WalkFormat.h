#pragma once
#include <stdint.h>
#include <stdio.h>
#include "Well.h"

// Lines of a walk log, the CSV file the Deck writes to the SD card while
// you walk. tools/walk_report.py reads them. Every ID is a device-keyed
// hash: no network names, no hardware IDs.
//
//   S,ms,scan,place,networks,routers,battery     one per scan
//   N,scan,net,router,name,rssi,channel,lock,hidden,guest
//                                                one per network heard
//   P,ms,place                                   you changed the place tag
//
// net = the network's own ID, router = the router it belongs to (see
// Well.h), name = the network name's hash (- when hidden).

namespace WalkFormat {

constexpr int Version = 2;   // 2 added the guest column

enum class Place : uint8_t { None, Home, Street, Transit, Shop, Work, Park, Count };

inline const char* placeName(Place p) {
  static const char* const k[] = {"none", "home", "street", "transit", "shop", "work", "park"};
  return k[(int)p];
}
inline const char* placeLabel(Place p) {
  static const char* const k[] = {"NO PLACE", "HOME", "STREET", "TRANSIT", "SHOP", "WORK", "PARK"};
  return k[(int)p];
}

inline int header(char* buf, size_t cap, const char* fw, uint16_t deck, uint32_t walk) {
  return snprintf(buf, cap,
                  "# tithe walk log %d, firmware %s, deck %04x, walk %lu\n"
                  "# S,ms,scan,place,networks,routers,battery\n"
                  "# N,scan,net,router,name,rssi,channel,lock,hidden,guest\n"
                  "# P,ms,place\n",
                  Version, fw, deck, (unsigned long)walk);
}

inline int scanLine(char* buf, size_t cap, uint32_t ms, uint32_t scan, Place p, int nets, int routers, int battery) {
  return snprintf(buf, cap, "S,%lu,%lu,%s,%d,%d,%d\n", (unsigned long)ms, (unsigned long)scan, placeName(p), nets,
                  routers, battery);
}

inline int netLine(char* buf, size_t cap, uint32_t scan, const Net& n) {
  uint8_t rb[5];
  WellRules::routerBytes(n.bssid, rb);
  const uint32_t net = Keys::deviceHash(n.bssid, 6);
  const uint32_t router = Keys::deviceHash(rb, 5);
  char name[9] = "-";
  if (!n.hidden) snprintf(name, sizeof(name), "%08lx", (unsigned long)n.nameKey);
  return snprintf(buf, cap, "N,%lu,%08lx,%08lx,%s,%d,%u,%s,%d,%d\n", (unsigned long)scan, (unsigned long)net,
                  (unsigned long)router, name, n.rssi, n.channel, lockCode(n.lock), n.hidden ? 1 : 0, n.guest ? 1 : 0);
}

inline int placeLine(char* buf, size_t cap, uint32_t ms, Place p) {
  return snprintf(buf, cap, "P,%lu,%s\n", (unsigned long)ms, placeName(p));
}

} // namespace WalkFormat
