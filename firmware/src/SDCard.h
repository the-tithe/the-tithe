#pragma once
#include <SD.h>
#include <SPI.h>
#include <errno.h>
#include "Storage.h"
#include "ff.h"
#include "diskio.h"
#include "sd_diskio.h"

// microSD bring-up, adapted from Hazke (MIT, REZOR). The Deck keeps
// everything under /deck on the card.
//
// The Cardputer's microSD slot is on its own SPI bus:
//   CLK 40 · MISO 39 · MOSI 14 · CS 12
//
// A card counts as ready only once a test file has been written, read back and
// removed: some cards mount and list fine but fail longer writes at full speed,
// so each mount tries 25, then 10, then 4 MHz. A card that won't mount at all
// is read raw to say why (exFAT, GPT, NTFS...).

// In the SD library but not its header: wakes the card for raw reads.
DSTATUS ff_sd_initialize(uint8_t pdrv);

namespace SDCard {

constexpr int PinSCK  = 40;
constexpr int PinMISO = 39;
constexpr int PinMOSI = 14;
constexpr int PinCS   = 12;

using CardState = CardWords::CardState;

inline bool ready = false;
inline CardState health = CardState::None;
inline CardFormat::Kind kind = CardFormat::Kind::Unknown;
inline uint32_t hz = 0;           // the speed it mounted at
inline bool wasReady = false;     // it was ready at some point this session
inline uint32_t lostCount = 0;    // times it failed after being ready
inline uint32_t mounts = 0;       // times it became ready, so a save can tell a card that came back
inline void (*onLost)() = nullptr;   // the walk log closes its file before the card goes (WalkLog.h)
inline void (*onBack)() = nullptr;   // ...and starts a new walk when a lost card mounts again

// What failed last, for /deck/memory.log once the card is back: what was being
// done, and errno.
inline const char* lastFailure = nullptr;
inline int lastErrno = 0;
inline void noteFailure(const char* what) {
  lastFailure = what;
  lastErrno = errno;
}

// At most 3 files open at once: the walk log, a save or log being written, and
// one more for the console. Each open file costs 4 KB while the card is
// mounted, so the default 5 would hold 8 KB for nothing.
constexpr uint8_t MaxFiles = 3;
constexpr uint32_t Speeds[] = {25000000, 10000000, 4000000};
constexpr const char* Probe = "/deck/probe.tmp";

// One sector's room, shared by the card test, the raw read and SaveStore's
// check for an erased flash area (none of them runs during another).
inline uint8_t scratch[512];

// Writes 8 KB, reads it back and removes it: as long a write as a save makes.
inline bool probe() {
  uint8_t* buf = scratch;
  File f = SD.open(Probe, FILE_WRITE);
  if (!f) { noteFailure("test file open"); return false; }
  bool ok = true;
  for (int b = 0; b < 16 && ok; b++) {
    for (int i = 0; i < 512; i++) buf[i] = (uint8_t)(i * 7 + b * 13);
    ok = f.write(buf, 512) == 512;
  }
  f.close();
  if (!ok) { noteFailure("test file write"); SD.remove(Probe); return false; }
  f = SD.open(Probe, FILE_READ);
  if (!f || f.size() != 16 * 512) { noteFailure("test file reread"); if (f) f.close(); SD.remove(Probe); return false; }
  for (int b = 0; b < 16 && ok; b++) {
    ok = f.read(buf, 512) == 512;
    for (int i = 0; ok && i < 512; i++) ok = buf[i] == (uint8_t)(i * 7 + b * 13);
  }
  f.close();
  if (!ok) noteFailure("test file came back wrong");
  SD.remove(Probe);
  return ok;
}

// Reads the first sector or two of a card that wouldn't mount, to say what it
// is. Sets `health` to None when nothing answered, else Format and `kind`.
inline void identify() {
  uint8_t* s = scratch;
  health = CardState::None;
  const uint8_t pdrv = sdcard_init(PinCS, &SPI, 4000000);
  if (pdrv == 0xFF) return;
  const bool answered = (ff_sd_initialize(pdrv) & STA_NOINIT) == 0;
  kind = CardFormat::Kind::Unknown;
  uint32_t next = 0;
  if (answered && sd_read_raw(pdrv, s, 0)) kind = CardFormat::fromCard(s, next);
  if (answered && next && sd_read_raw(pdrv, s, next)) kind = CardFormat::fromVolume(s);
  sdcard_uninit(pdrv);
  if (answered) health = CardState::Format;
}

// Mount the card and check it takes a write. True once it's ready; `health`
// says what's wrong otherwise.
inline bool ensure() {
  if (ready) return true;
  SPI.begin(PinSCK, PinMISO, PinMOSI, PinCS);
  bool mounted = false;
  for (uint32_t f : Speeds) {
    if (!SD.begin(PinCS, SPI, f, "/sd", MaxFiles)) {
      // Waking the card runs at 400 kHz whatever the speed, so if nothing
      // answered the first time there's no card to try slower.
      if (f == Speeds[0]) {
        identify();
        if (health == CardState::None) return false;
      }
      continue;
    }
    mounted = true;
    SD.mkdir("/deck");
    SD.mkdir("/deck/walks");
    if (probe()) {
      const bool back = wasReady;
      hz = f;
      ready = wasReady = true;
      health = CardState::Ok;
      mounts++;
      if (back && onBack) onBack();
      return true;
    }
    SD.end();
  }
  if (mounted) health = CardState::NoWrite;
  else identify();
  return false;
}

// After a failed write: the card may have been pulled, so the next
// ensure() starts over. Whatever has a file open there closes it first.
inline void markFailed(const char* what = nullptr) {
  if (!ready) return;
  if (what) noteFailure(what);
  ready = false;
  lostCount++;
  if (onLost) onLost();
  SD.end();
}

// A card that worked this session and then failed is tried again now and then
// (a knock in a pocket, a write that timed out), so backups and the walk log
// pick up again by themselves. Each try takes a moment, so not often.
constexpr uint32_t RetryEveryMs = 30000;
inline uint32_t retryAt = 0;
inline bool retry(uint32_t nowMs) {
  if (ready) return true;
  if (!wasReady || (int32_t)(nowMs - retryAt) < 0) return false;
  retryAt = nowMs + RetryEveryMs;
  return ensure();
}

// The boot screen's word for the card, and what to do when it's the problem.
inline const char* word() { return CardWords::word(health, kind); }
inline const char* fix() { return CardWords::fix(health, kind); }

} // namespace SDCard
