#pragma once
#include <stdint.h>
#include <string.h>
#include "Well.h"

// Every well the Deck has heard this walk, updated after each scan.
//
// Signal strength jumps around from scan to scan, so the list uses a
// smoothed value. A well that drops out of a scan still counts for one more
// scan, then shows as LOST for one scan before it goes, so the list doesn't
// flicker.

struct WellRec {
  uint32_t id;        // device-keyed router hash, the ID the log uses
  uint32_t seed;      // world-keyed router hash, for the name
  uint32_t nameKey;   // device-keyed hash of its strongest named network, 0 if none (for mesh groups)
  char name[17];
  Energy energy;
  Faction faction;
  uint8_t channel;
  uint8_t nets;       // networks this router broadcast in its last scan
  bool anyOpen;
  bool sawGuest;      // ever broadcast a guest or free network
  int8_t maxGuard;    // its most guarded lock seen (WellRules::guardRank)
  uint8_t maxNets;    // the most networks it ran at once
  bool sawVisible;    // ever broadcast a name (otherwise it's a Hush well)
  bool phoneLike;     // ever broadcast a name like a phone's hotspot
  bool localOnly;     // every hardware ID so far was made up (locally administered), as phones' are
  bool travelling;    // stayed with you while the street around you changed (fixedish below)
  bool neverHeard;    // this Deck had never heard it before this walk (set by the app)
  bool tuned;         // its tune has played this walk
  bool judged;        // came into reach this walk, and what it might be was decided (app)
  bool namesake;      // decided then: it has the true name of a well you've worked (app)
  float sig;          // smoothed dBm
  int8_t last;        // last raw dBm
  int8_t peak;        // loudest raw dBm heard
  uint16_t seen;      // scans it appeared in
  uint32_t firstScan, lastScan;
  uint32_t firstMs, lastMs;
  static constexpr int Hist = 48;
  int8_t hist[Hist];  // dBm in each recent scan, by scan number mod Hist
  bool dowsed;        // a dowsing found which way it lies
  float bearing;      // that direction, in Motion::heading's frame
  uint32_t dowsedMs;
  uint32_t goneBefore;   // wells that had come and gone before this one was first heard
};

namespace WellRules {
// A well that may be in someone's pocket, or that travels with you, is never
// a find, never starts a moment and is never named on a card. A stranger's
// hotspot walking past still looks like a router, so everything a find does
// is also safe on one: decided once, at first sight, and never dowsable.
inline bool fixedish(const WellRec& w) { return !w.phoneLike && !w.travelling; }

// One of the old radios on the lampposts that never stopped talking: a lone
// open network on channel 11, named, never a guest network, and (by its world
// key, so the same for every Deck) one of the half that are that old.
inline bool isOldWell(const WellRec& w) {
  return w.maxGuard == 0 && !w.sawGuest && w.maxNets == 1 && w.channel == 11 && w.sawVisible &&
         ((w.seed >> 20) & 1) == 0;
}
} // namespace WellRules

class Sightings {
 public:
  static constexpr int MaxWells = 320;
  static constexpr int MaxGroups = 96;   // routers in one scan
  static constexpr int8_t Absent = -128;
  static constexpr int ShowScans = 3;    // listed until missed this many scans
  // Travelling with you: heard in nearly every scan for ten minutes while
  // twenty other wells came and went (a companion's phone, a bus, a train).
  // One network on made-up hardware IDs, the way phone hotspots look, is
  // suspected sooner. Only wells heard for a few scans count as coming and
  // going, so cars and passers-by at home don't.
  static constexpr uint32_t TravelMs = 600000, TravelMsHint = 300000;
  static constexpr uint32_t TravelGone = 20, TravelGoneHint = 10;
  static constexpr uint16_t GoneAfter = 3;   // scans heard before leaving counts

  void reset(uint32_t nowMs) {
    count_ = 0;
    scanNo_ = 0;
    near_ = 0;
    heard_ = 0;
    newThisScan_ = 0;
    evicted_ = 0;
    gone_ = 0;
    startMs_ = nowMs;
    lastScanMs_ = nowMs;
    memset(wardScans_, 0, sizeof(wardScans_));
    ward_ = pending_ = WardKind::Hollow;
    pendingScans_ = 0;
    memset(energyNear_, 0, sizeof(energyNear_));
  }

  // Fold one scan's networks into the table.
  void ingest(const Net* nets, int n, uint32_t nowMs) {
    scanNo_++;
    lastScanMs_ = nowMs;
    newThisScan_ = 0;

    Group g[MaxGroups];
    int ng = 0;
    for (int i = 0; i < n; i++) {
      uint8_t rb[5];
      WellRules::routerBytes(nets[i].bssid, rb);
      const uint32_t id = Keys::deviceHash(rb, 5);
      int k = 0;
      while (k < ng && g[k].id != id) k++;
      if (k == ng) {
        if (ng == MaxGroups) continue;
        g[ng] = Group{};
        g[ng].id = id;
        memcpy(g[ng].rb, rb, 5);
        g[ng].rssi = -127;
        g[ng].visRssi = -127;
        g[ng].guard = -1;
        g[ng].allLocal = true;
        ng++;
      }
      Group& G = g[k];
      G.nets++;
      if (nets[i].rssi > G.rssi) { G.rssi = nets[i].rssi; G.channel = nets[i].channel; }
      const Faction f = WellRules::factionOf(nets[i].lock);
      if (WellRules::guardRank(f) > G.guard) G.guard = WellRules::guardRank(f);
      if (f == Faction::Hedge) G.anyOpen = true;
      if (nets[i].guest) G.guest = true;
      if (nets[i].phone) G.phone = true;
      if (!(nets[i].bssid[0] & 0x02)) G.allLocal = false;
      if (!nets[i].hidden) {
        G.visible = true;
        if (nets[i].rssi > G.visRssi) { G.visRssi = nets[i].rssi; G.nameKey = nets[i].nameKey; }
      }
    }

    const int slot = (int)(scanNo_ % WellRec::Hist);
    for (int i = 0; i < count_; i++) wells_[i].hist[slot] = Absent;

    heard_ = ng;
    for (int k = 0; k < ng; k++) {
      WellRec* w = find(g[k].id);
      if (!w) {
        w = add(g[k], nowMs);
        newThisScan_++;
      }
      const int gap = (int)(scanNo_ - w->lastScan);
      w->sig = (w->seen == 0 || gap > ShowScans) ? g[k].rssi : w->sig * 0.6f + g[k].rssi * 0.4f;
      w->last = g[k].rssi;
      if (w->seen == 0 || g[k].rssi > w->peak) w->peak = g[k].rssi;
      w->seen++;
      w->lastScan = scanNo_;
      w->lastMs = nowMs;
      w->channel = g[k].channel;
      w->nets = g[k].nets;
      w->anyOpen = w->anyOpen || g[k].anyOpen;
      if (g[k].nameKey) w->nameKey = g[k].nameKey;
      w->sawVisible = w->sawVisible || g[k].visible;
      // A router's faction comes from everything it has ever broadcast, so a
      // missed scan of one of its networks can't flip it back and forth.
      if (w->seen == 1 || g[k].guard > w->maxGuard) w->maxGuard = g[k].guard;
      if (g[k].nets > w->maxNets) w->maxNets = g[k].nets;
      w->sawGuest = w->sawGuest || g[k].guest;
      w->phoneLike = w->phoneLike || g[k].phone;
      w->localOnly = (w->seen == 1 || w->localOnly) && g[k].allLocal;
      const Faction f = WellRules::routerFaction(w->maxGuard, w->sawGuest, w->maxNets);
      if (w->seen == 1 || f != w->faction) {
        w->faction = f;
        WellRules::nameOf(w->seed, f, w->name);
      }
      w->energy = WellRules::energyOf(!w->sawVisible, w->faction, w->channel);
      w->hist[slot] = g[k].rssi;
    }

    for (int i = 0; i < count_; i++) {
      WellRec& w = wells_[i];
      if (scanNo_ - w.lastScan == (uint32_t)ShowScans && w.seen >= GoneAfter) gone_++;   // it just left
      if (w.lastScan == scanNo_ && !w.travelling) w.travelling = travelsWithYou(w);
    }

    // Wells heard in this scan or the one before, and not faint, set the
    // ward, so one missed scan doesn't change it. The ward itself moves only
    // once two scans in a row agree: faint wells flicker, and a street that
    // sits on a line would flip every scan.
    near_ = 0;
    memset(energyNear_, 0, sizeof(energyNear_));
    for (int i = 0; i < count_; i++) {
      if (counts(wells_[i])) {
        near_++;
        energyNear_[(int)wells_[i].energy]++;
      }
    }
    const WardKind now = WellRules::wardOf(near_);
    if (scanNo_ == 1) ward_ = pending_ = now;
    else if (now == pending_) { if (++pendingScans_ >= 2) ward_ = now; }
    else { pending_ = now; pendingScans_ = 1; }
    wardScans_[(int)ward()]++;
  }

  // Wells to list, strongest first. Returns how many were written.
  int listed(const WellRec** out, int cap) const {
    int n = 0;
    for (int i = 0; i < count_ && n < cap; i++) {
      if (scanNo_ - wells_[i].lastScan < (uint32_t)ShowScans) out[n++] = &wells_[i];
    }
    for (int i = 1; i < n; i++) {   // insertion sort, n is small
      const WellRec* x = out[i];
      int j = i - 1;
      while (j >= 0 && out[j]->sig < x->sig) { out[j + 1] = out[j]; j--; }
      out[j + 1] = x;
    }
    return n;
  }

  // Dowsing found which way this well lies.
  void setBearing(uint32_t id, float bearing, uint32_t nowMs) {
    WellRec* w = find(id);
    if (!w) return;
    w->dowsed = true;
    w->bearing = bearing;
    w->dowsedMs = nowMs;
  }

  // Routers in range sharing this well's network name (a mesh or a chain's
  // hotspots), counting itself. 0 when it's alone or has no name.
  int meshOf(const WellRec& w) const {
    if (!w.nameKey) return 0;
    int n = 0;
    for (int i = 0; i < count_; i++) n += counts(wells_[i]) && wells_[i].nameKey == w.nameKey;
    return n >= 2 ? n : 0;
  }
  int onChannel(int ch) const {
    int n = 0;
    for (int i = 0; i < count_; i++) n += counts(wells_[i]) && wells_[i].channel == ch;
    return n;
  }

  // Every well heard this walk, listed or not.
  int size() const { return count_; }
  const WellRec& at(int i) const { return wells_[i]; }

  const WellRec* byId(uint32_t id) const {
    for (int i = 0; i < count_; i++) if (wells_[i].id == id) return &wells_[i];
    return nullptr;
  }
  // For the app's marks on a well: its tune played, never heard before, or
  // known to travel (from an earlier walk).
  WellRec* edit(uint32_t id) { return find(id); }

  // Scans since this well was last heard (0 = in the latest scan).
  uint32_t age(const WellRec& w) const { return scanNo_ - w.lastScan; }
  // Heard in this scan or the one before: counts toward the ward. Older
  // wells stay listed one more scan, marked lost.
  bool inRange(const WellRec& w) const { return age(w) <= 1; }
  // In range and not faint: what the ward and "wells in range" count.
  bool counts(const WellRec& w) const { return inRange(w) && w.sig >= WellRules::FaintDbm; }
  bool isFresh(const WellRec& w) const { return scanNo_ - w.firstScan < 3 && w.firstScan > 1; }

  uint32_t scans() const { return scanNo_; }
  int near() const { return near_; }
  int heard() const { return heard_; }   // routers in the latest scan
  WardKind ward() const { return ward_; }
  int energyNear(Energy e) const { return energyNear_[(int)e]; }
  int newThisScan() const { return newThisScan_; }
  uint32_t found() const { return count_ + evicted_; }
  uint32_t wardScans(WardKind w) const { return wardScans_[(int)w]; }
  uint32_t startMs() const { return startMs_; }
  uint32_t lastScanMs() const { return lastScanMs_; }

 private:
  struct Group {
    uint32_t id;
    uint8_t rb[5];
    int8_t rssi, visRssi;
    uint32_t nameKey;
    uint8_t channel;
    int8_t guard;
    uint8_t nets;
    bool anyOpen;
    bool guest;
    bool visible;
    bool phone;
    bool allLocal;
  };

  bool travelsWithYou(const WellRec& w) const {
    const bool hint = w.localOnly && w.maxNets == 1;
    const uint32_t span = scanNo_ - w.firstScan + 1;
    return w.lastMs - w.firstMs >= (hint ? TravelMsHint : TravelMs) && w.seen * 5u >= span * 4u &&
           gone_ - w.goneBefore >= (hint ? TravelGoneHint : TravelGone);
  }

  WellRec* find(uint32_t id) {
    for (int i = 0; i < count_; i++) if (wells_[i].id == id) return &wells_[i];
    return nullptr;
  }

  WellRec* add(const Group& g, uint32_t nowMs) {
    WellRec* w;
    if (count_ < MaxWells) {
      w = &wells_[count_++];
    } else {
      // Full: reuse the well heard longest ago.
      w = &wells_[0];
      for (int i = 1; i < count_; i++) if (wells_[i].lastScan < w->lastScan) w = &wells_[i];
      evicted_++;
    }
    memset(w, 0, sizeof(*w));
    w->id = g.id;
    w->seed = Keys::worldHash(g.rb, 5);
    w->firstScan = scanNo_;
    w->firstMs = nowMs;
    w->goneBefore = gone_;
    for (int i = 0; i < WellRec::Hist; i++) w->hist[i] = Absent;
    return w;
  }

  WellRec wells_[MaxWells];
  int count_ = 0;
  uint32_t scanNo_ = 0;
  int near_ = 0;
  int heard_ = 0;
  int newThisScan_ = 0;
  uint32_t evicted_ = 0;
  uint32_t gone_ = 0;   // wells that have come and gone this walk
  uint32_t startMs_ = 0, lastScanMs_ = 0;
  uint32_t wardScans_[WardCount] = {0};
  int energyNear_[EnergyCount] = {0};
  WardKind ward_ = WardKind::Hollow, pending_ = WardKind::Hollow;   // the held ward, and the one waiting to replace it
  int pendingScans_ = 0;
};
