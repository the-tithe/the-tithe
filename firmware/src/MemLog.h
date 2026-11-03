#pragma once
#include <SD.h>
#include <esp_heap_caps.h>
#include "SDCard.h"

// How much memory the Deck has, kept on the card in /deck/memory.log so a
// shortage shows up before it stops anything: a line at each boot with the free
// heap and the largest free block after each step of starting up, and a line
// whenever a session's lowest free heap sets a new low. Past 32 KB the log
// moves to memory.old (one old file is kept).

namespace MemLog {

constexpr const char* Path = "/deck/memory.log";
constexpr const char* Old = "/deck/memory.old";

struct Mark {
  const char* at;
  uint32_t free, largest;
};
inline Mark marks[8];
inline int nMarks = 0;
inline uint32_t lowest = 0;   // the lowest free heap logged this session, 0 for none yet

// Borrows that failed anywhere (any task), counted as they happen and logged by check().
inline volatile uint32_t failedAllocs = 0;
inline volatile uint32_t failedSize = 0;
inline uint32_t failedLogged = 0;
inline void onFailedAlloc(size_t size, uint32_t caps, const char* fn) {
  (void)caps;
  (void)fn;
  failedAllocs = failedAllocs + 1;
  failedSize = (uint32_t)size;
}
inline void watchFailures() { heap_caps_register_failed_alloc_callback(onFailedAlloc); }

inline uint32_t freeNow() { return heap_caps_get_free_size(MALLOC_CAP_8BIT); }
inline uint32_t largestNow() { return heap_caps_get_largest_free_block(MALLOC_CAP_8BIT); }

// A step of starting up, `at` a string that lives for good.
inline void mark(const char* at) {
  if (nMarks < 8) marks[nMarks++] = Mark{at, freeNow(), largestNow()};
}

inline File open() {
  if (!SDCard::ready) return File();
  File f = SD.open(Path, FILE_READ);
  const size_t size = f ? f.size() : 0;
  if (f) f.close();
  if (size > 32768) {
    SD.remove(Old);
    SD.rename(Path, Old);
  }
  return SD.open(Path, FILE_APPEND);
}

// The boot's line: each step's free heap / largest block, in KB, and what the save did.
// Then where the game is kept: the flash area (`flash`, nullptr for none) and
// the card's speed, with what failed on the way if it took a slower one.
inline void boot(const char* version, const char* save, const char* flash, bool keyKept) {
  File f = open();
  if (!f) return;
  f.printf("fw %s boot:", version);
  for (int i = 0; i < nMarks; i++)
    f.printf(" %s %lu/%lu", marks[i].at, (unsigned long)(marks[i].free / 1024), (unsigned long)(marks[i].largest / 1024));
  f.printf(" KB, save %s, flash %s, card %lu MHz", save, flash ? flash : "none", (unsigned long)(SDCard::hz / 1000000));
  if (SDCard::hz != SDCard::Speeds[0] && SDCard::lastFailure)
    f.printf(" (faster: %s failed, errno %d)", SDCard::lastFailure, SDCard::lastErrno);
  if (!keyKept) f.print(", the key couldn't be stored");
  if (failedAllocs) f.printf(", %lu borrows failed (last %lu bytes)", (unsigned long)failedAllocs, (unsigned long)failedSize);
  f.print("\n");
  failedLogged = failedAllocs;
  f.close();
  lowest = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
}

// Once a minute or so: a new low since boot (2 KB or more below the last one
// logged), or borrows that failed since the last line. `waited`: scans put off
// for want of memory so far (Scanner::waited).
inline void check(const char* version, uint32_t playSecs, uint32_t waited) {
  const uint32_t low = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
  const uint32_t failed = failedAllocs;
  if (lowest && low + 2048 > lowest && failed == failedLogged) return;
  if (!lowest || low + 2048 <= lowest) lowest = low;
  File f = open();
  if (!f) return;
  f.printf("fw %s at play %lus: lowest free heap %lu KB, now %lu KB, largest block %lu KB", version,
           (unsigned long)playSecs, (unsigned long)(low / 1024), (unsigned long)(freeNow() / 1024),
           (unsigned long)(largestNow() / 1024));
  if (failed) f.printf(", %lu borrows failed (last %lu bytes)", (unsigned long)failed, (unsigned long)failedSize);
  if (waited) f.printf(", %lu scans waited for memory", (unsigned long)waited);
  f.print("\n");
  f.close();
  failedLogged = failed;
}

// A card that failed and has mounted again: what failed, and the speed it's back at.
inline uint32_t cardLogged = 0;   // SDCard::lostCount logged so far
inline void cardBack(const char* version, uint32_t playSecs) {
  if (SDCard::lostCount == cardLogged) return;
  cardLogged = SDCard::lostCount;
  File f = open();
  if (!f) return;
  f.printf("fw %s at play %lus: card failed (%s, errno %d), mounted again at %lu MHz\n", version,
           (unsigned long)playSecs, SDCard::lastFailure ? SDCard::lastFailure : "?", SDCard::lastErrno,
           (unsigned long)(SDCard::hz / 1000000));
  f.close();
}

} // namespace MemLog
