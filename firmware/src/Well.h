#pragma once
#include <stdint.h>
#include <string.h>
#include "SipHash.h"

// How a real WiFi network becomes part of the secret world.
//
// One router is one well. A router often broadcasts several networks (a
// home network and a guest network, say), each with its own hardware ID,
// and those IDs usually differ only in the first and last bytes. Masking
// those bytes gives the router's ID, so its networks fold into one well.
//
// Two keys, for two jobs:
//   * The device key is random, made on first boot and kept in the Deck's
//     flash. Everything the Deck stores or logs uses it, so an SD card on
//     its own can't be matched to real networks.
//   * The world key is the same on every Deck, so a router has the same
//     name and nature for every player. Nothing derived from it is logged.

enum class Lock : uint8_t { Open, Owe, Wep, Wpa, Wpa2, Wpa3, Wpa23, Enterprise, Wapi, Other };
enum class Energy : uint8_t { Ember, Tide, Gale, Hush, Static };
enum class Faction : uint8_t { Hedge, Hearth, Warded, Syndicate };
enum class WardKind : uint8_t { Hollow, Fringe, Ward, Nexus };

constexpr int EnergyCount = 5;
constexpr int WardCount = 4;

inline const char* lockCode(Lock l) {
  static const char* const k[] = {"open", "owe", "wep", "wpa", "wpa2", "wpa3", "wpa23", "ent", "wapi", "other"};
  return k[(int)l];
}
inline const char* energyName(Energy e) {
  static const char* const k[] = {"EMBER", "TIDE", "GALE", "HUSH", "STATIC"};
  return k[(int)e];
}
inline const char* factionName(Faction f) {
  static const char* const k[] = {"HEDGE", "HEARTH", "WARDED", "SYNDICATE"};
  return k[(int)f];
}
inline const char* wardName(WardKind w) {
  static const char* const k[] = {"HOLLOW", "FRINGE", "QUARTER", "NEXUS"};
  return k[(int)w];
}

// One network from one scan. Lives only until the scan is processed.
struct Net {
  uint8_t bssid[6];
  uint32_t nameKey;   // device-keyed hash of the network name, 0 when hidden
  int8_t rssi;        // dBm
  uint8_t channel;
  Lock lock;
  bool hidden;
  bool guest;         // named like a guest or free network: anyone can join
  bool phone;         // named like a phone's hotspot: it may be in someone's pocket
};

namespace Keys {
// Filled from flash at boot (Persist.h). Zeros only in tests.
inline uint8_t device[16] = {0};
inline constexpr uint8_t world[16] = {0x44, 0x45, 0x43, 0x4b, 0x2f, 0x2f, 0x77, 0x65,
                                      0x6c, 0x6c, 0x73, 0x2f, 0x2f, 0x76, 0x31, 0x00};

inline uint32_t deviceHash(const uint8_t* p, size_t n) { return (uint32_t)SipHash::hash(device, p, n); }
inline uint32_t worldHash(const uint8_t* p, size_t n) { return (uint32_t)SipHash::hash(world, p, n); }
// Short tag that tells logs from different Decks apart without revealing the key.
inline uint16_t deckTag() { return (uint16_t)SipHash::hash(device, (const uint8_t*)"deck", 4); }
} // namespace Keys

namespace WellRules {

// The router part of a hardware ID: the middle four bytes plus the top half
// of the last one. See the note at the top of this file.
inline void routerBytes(const uint8_t bssid[6], uint8_t out[5]) {
  out[0] = bssid[1]; out[1] = bssid[2]; out[2] = bssid[3]; out[3] = bssid[4];
  out[4] = bssid[5] & 0xF0;
}

inline bool isHidden(const char* ssid, int len) {
  for (int i = 0; i < len; i++) if (ssid[i] != 0) return false;
  return true;
}

// A guest or free network, by its name: anyone can join it, so it's the
// Hedge's even behind a password. Matched without regard to case.
inline bool isGuestName(const char* ssid, int len) {
  static const char* const words[] = {"guest", "invitad", "gratis", "visit", "public", "freewifi", "free wifi",
                                      "free_wifi", "free-wifi", "wifi free", "clientes"};
  char low[33];
  const int n = len < 32 ? len : 32;
  for (int i = 0; i < n; i++) low[i] = (char)(ssid[i] >= 'A' && ssid[i] <= 'Z' ? ssid[i] + 32 : ssid[i]);
  low[n] = 0;
  for (const char* w : words) if (strstr(low, w)) return true;
  return false;
}

// A phone's or tablet's hotspot, by its name. Only this flag is kept, never
// the name. A hotspot can walk past, or walk with you, so nothing that marks
// a well as found or special ever lands on one (Sightings.h, fixedish).
inline bool isPhoneName(const char* ssid, int len) {
  static const char* const words[] = {"iphone", "ipad", "android", "galaxy", "pixel", "hotspot", "phone", "redmi",
                                      "xiaomi", "oneplus", "huawei", "motorola", "nokia", "realme", "mifi", "jetpack"};
  char low[33];
  const int n = len < 32 ? len : 32;
  for (int i = 0; i < n; i++) low[i] = (char)(ssid[i] >= 'A' && ssid[i] <= 'Z' ? ssid[i] + 32 : ssid[i]);
  low[n] = 0;
  for (const char* w : words) if (strstr(low, w)) return true;
  return false;
}

inline Faction factionOf(Lock l) {
  switch (l) {
    case Lock::Open: case Lock::Owe: return Faction::Hedge;
    case Lock::Wpa3: case Lock::Wpa23: return Faction::Warded;
    case Lock::Enterprise: return Faction::Syndicate;
    default: return Faction::Hearth;
  }
}

// When a router broadcasts several networks, its faction comes from the
// most guarded one: a home router with an open guest network is still Hearth.
inline int guardRank(Faction f) {
  switch (f) {
    case Faction::Hedge: return 0;
    case Faction::Hearth: return 1;
    case Faction::Warded: return 2;
    default: return 3;
  }
}

// A router's faction from everything it has broadcast: its most guarded lock,
// whether it ever offered a guest network, and the most networks it ran at
// once. Three or more networks is a business box (shops, offices): Syndicate.
inline Faction routerFaction(int maxGuard, bool guest, int maxNets) {
  if (maxGuard >= 3 || maxNets >= 3) return Faction::Syndicate;
  if (maxGuard == 2) return Faction::Warded;
  if (guest || maxGuard <= 0) return Faction::Hedge;
  return Faction::Hearth;
}

inline Energy energyOf(bool allHidden, Faction f, uint8_t channel) {
  if (allHidden) return Energy::Hush;
  if (f == Faction::Hedge) return Energy::Static;
  if (channel <= 4) return Energy::Ember;
  if (channel <= 9) return Energy::Tide;
  return Energy::Gale;
}

// The FAR / FAINT line. A faint well (below it) is listed but never counts
// toward the ward or toward "wells in range": heard one scan, gone the next.
constexpr float FaintDbm = -78;

inline WardKind wardOf(int routers) {
  if (routers <= 3) return WardKind::Hollow;
  if (routers <= 10) return WardKind::Fringe;
  if (routers <= 25) return WardKind::Ward;
  return WardKind::Nexus;
}

// ---- Names -------------------------------------------------------------
// An adjective from the world key, and a noun that fits the faction. Every
// name fits in 16 characters.

inline const char* const Adjectives[64] = {
  "ASHEN", "BLACK", "BRASS", "CHROME", "CINDER", "COPPER", "CROOKED", "DROWNED",
  "DUSK", "GILT", "GLASS", "GREY", "HEX", "IVORY", "JADE", "LAST",
  "LOW", "LUNAR", "MIRROR", "MOTH", "NEON", "OLD", "PALE", "RED",
  "RUST", "SABLE", "SODIUM", "SUNKEN", "TALLOW", "THORN", "VELVET", "VESPER",
  "BITTER", "BLIND", "BRIGHT", "BROKEN", "BURNT", "CARMINE", "CRACKED", "DAMP",
  "DEEP", "DUSTY", "ELDER", "EMPTY", "FALLEN", "FERAL", "FROST", "GHOST",
  "HUNGRY", "INKED", "LONG", "MERCURY", "MOSS", "NARROW", "OPAL", "RAVEN",
  "SAINT", "SCARLET", "SMOKE", "STRAY", "UMBER", "WANING", "WIRED", "ZINC",
};

inline const char* const Nouns[4][32] = {
  // Hedge: open networks, street mages
  {"THICKET", "GUTTER", "CROSSING", "ALLEY", "STALL", "BRAMBLE", "DITCH", "MARKET",
   "ARCH", "STAIR", "SQUAT", "CORNER", "GULLY", "VERGE", "CISTERN", "DRAIN",
   "BAZAAR", "BENCH", "BRIDGE", "CANAL", "CULVERT", "KIOSK", "OVERPASS", "PIER",
   "QUAY", "ROOFTOP", "SEWER", "SHRINE", "SPRING", "TUNNEL", "WHARF", "YARD"},
  // Hearth: home networks, local covens
  {"HOB", "KILN", "LAMP", "LARDER", "STOOP", "PORCH", "ATTIC", "CELLAR",
   "CRADLE", "CHIMNEY", "PARLOR", "PANTRY", "KETTLE", "GABLE", "DOORSTEP", "RAFTER",
   "ALCOVE", "BALCONY", "CLOSET", "CUPBOARD", "DEN", "GARRET", "HALLWAY", "INGLE",
   "LANDING", "LOFT", "MANTEL", "NURSERY", "OVEN", "SCULLERY", "STUDY", "WINDOW"},
  // Warded: newer locked networks, a careful new order
  {"SEAL", "GATE", "BASTION", "BULWARK", "SIGIL", "LOCK", "AEGIS", "KEEP",
   "VIGIL", "RAMPART", "CORDON", "LATTICE", "SENTRY", "CIRCLE", "BOLT", "WATCH",
   "ANCHOR", "BARRIER", "CAGE", "CHAIN", "CITADEL", "CLASP", "COVENANT", "FORT",
   "HASP", "HELM", "LINTEL", "MOAT", "OATH", "PALISADE", "REDOUBT", "SHIELD"},
  // Syndicate: company networks, corporate magi
  {"SPIRE", "VAULT", "EXCHANGE", "TOWER", "LEDGER", "ATRIUM", "BOURSE", "ANNEX",
   "DOME", "MINT", "PLAZA", "FOYER", "TRUST", "BUREAU", "ARCADE", "CONCORD",
   "AGENCY", "BOARD", "CHAMBER", "CONCERN", "DESK", "EMPORIUM", "ESTATE", "FIRM",
   "GUILD", "HOLDING", "INDEX", "LOBBY", "OFFICE", "RESERVE", "TICKER", "VENTURE"},
};

inline void nameOf(uint32_t seed, Faction f, char out[17]) {
  const char* a = Adjectives[seed & 63];
  const char* n = Nouns[(int)f][(seed >> 6) & 31];
  size_t la = strlen(a), ln = strlen(n);
  memcpy(out, a, la);
  out[la] = ' ';
  memcpy(out + la + 1, n, ln);
  out[la + 1 + ln] = 0;
}

} // namespace WellRules
