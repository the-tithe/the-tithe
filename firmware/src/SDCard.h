#pragma once
#include <SD.h>
#include <SPI.h>

// microSD bring-up, adapted from Hazke (MIT, REZOR). The Deck keeps
// everything under /deck on the card.
//
// The Cardputer's microSD slot is on its own SPI bus:
//   CLK 40 · MISO 39 · MOSI 14 · CS 12

namespace SDCard {

constexpr int PinSCK  = 40;
constexpr int PinMISO = 39;
constexpr int PinMOSI = 14;
constexpr int PinCS   = 12;

inline bool ready = false;

inline bool ensure() {
  if (ready) return true;
  SPI.begin(PinSCK, PinMISO, PinMOSI, PinCS);
  if (!SD.begin(PinCS, SPI, 25000000)) return false;
  SD.mkdir("/deck");
  SD.mkdir("/deck/walks");
  ready = true;
  return true;
}

// After a failed write: the card may have been pulled, so the next
// ensure() starts over.
inline void markFailed() {
  ready = false;
  SD.end();
}

} // namespace SDCard
