#pragma once
#include <stdint.h>
#include <string.h>

// The week's Nights and the career's records, for THE WEEK tab and the
// Grimoire's RECORDS. Saved at the end of the game block (Save.h version 2).

namespace Game {

struct NightRow {
  float total;     // Resonance made
  float tithe;     // what the Night asked
  int8_t spare;    // visits left when it closed
  uint8_t how;     // NightPaid, NightLoan or NightBroke
  uint8_t pad[2];
};
enum : uint8_t { NightNone, NightPaid, NightLoan, NightBroke };

// A best, and where it happened. Week is the Binding's number.
struct Record {
  double value;
  uint32_t wellId, wellSeed;   // for an offering: the well (its true name comes from the seed)
  uint16_t week;
  uint8_t night;               // 0-based
  uint8_t faction;
};

struct Log {
  static constexpr int MaxNights = 14;   // the week and seven Overnight Nights
  uint32_t runNo;                        // the week the rows below belong to
  NightRow nights[MaxNights];
  Record loudest;        // the biggest single offering
  Record biggestNight;   // the most Resonance in one Night
  Record quickest;       // the fewest visits to pay a Tithe (value = visits; 0 none yet)
  uint8_t nNights;
  uint8_t pad[7];

  // A Night that just closed (paid, paid by loan, or the one a Binding broke on).
  void night(uint32_t run, int nightNo, double total, double tithe, int spare, uint8_t how) {
    if (runNo != run) { memset(nights, 0, sizeof(nights)); nNights = 0; runNo = run; }
    if (nightNo < 0 || nightNo >= MaxNights) return;
    NightRow& row = nights[nightNo];
    row.total = (float)total;
    row.tithe = (float)tithe;
    row.spare = (int8_t)(spare < 0 ? 0 : spare > 99 ? 99 : spare);
    row.how = how;
    if (nightNo + 1 > nNights) nNights = (uint8_t)(nightNo + 1);
  }
};

} // namespace Game
