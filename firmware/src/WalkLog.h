#pragma once
#include <SD.h>
#include <stdio.h>
#include <unistd.h>
#include "Config.h"
#include "Persist.h"
#include "SDCard.h"
#include "Status.h"
#include "WalkFormat.h"

// Writes the walk log (WalkFormat.h) to /deck/walks/W0001.CSV and so on.
// Each walk gets its own file. The file is created at the first scan, so
// switching recording on and off without walking leaves no empty files.
//
// Writing uses plain C files on the card's mount point (/sd), because
// they report a failed flush; Arduino's File hides it, so a pulled card
// would go unnoticed.

namespace WalkLog {

inline FILE* file = nullptr;
inline Status::Log state = Status::Log::Off;
inline uint32_t walk = 0;
inline uint32_t bytes = 0;
inline bool pendingOpen = false;

// The next walk number: one past both the Deck's counter and any file
// already on the card, so a card moved between Decks never gets overwritten.
inline uint32_t nextWalkNumber() {
  uint32_t best = Persist::lastWalk;
  File dir = SD.open("/deck/walks");
  if (dir) {
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      const char* n = f.name();
      const char* base = strrchr(n, '/');
      base = base ? base + 1 : n;
      unsigned long w = 0;
      if (sscanf(base, "W%lu.CSV", &w) == 1 && w > best) best = (uint32_t)w;
      f.close();
    }
    dir.close();
  }
  return best + 1;
}

// Starts a new walk. It opens its file at the first scan.
inline void start() {
  if (!SDCard::ensure()) { state = Status::Log::NoCard; return; }
  walk = nextWalkNumber();
  Persist::setLastWalk(walk);
  bytes = 0;
  pendingOpen = true;
  state = Status::Log::On;
}

inline void closeFile() {
  if (file) fclose(file);
  file = nullptr;
}

inline void stop() {
  closeFile();
  pendingOpen = false;
  if (state == Status::Log::On) state = Status::Log::Off;
}

inline void fail() {
  closeFile();
  SDCard::markFailed();
  pendingOpen = false;
  state = Status::Log::Failed;
}

inline bool put(const char* s, int n) {
  if (n <= 0) return true;
  if (fwrite(s, 1, (size_t)n, file) != (size_t)n) { fail(); return false; }
  bytes += (uint32_t)n;
  return true;
}

// Pushes everything written so far onto the card. False if the card failed.
inline bool sync() {
  if (fflush(file) != 0 || fsync(fileno(file)) != 0) { fail(); return false; }
  return true;
}

inline bool ensureOpen() {
  if (!pendingOpen) return file != nullptr;
  char path[40];
  snprintf(path, sizeof(path), "/sd/deck/walks/W%04lu.CSV", (unsigned long)walk);
  file = fopen(path, "w");
  pendingOpen = false;
  if (!file) { fail(); return false; }
  char buf[256];
  return put(buf, WalkFormat::header(buf, sizeof(buf), Config::Version, Keys::deckTag(), walk));
}

inline void scan(uint32_t ms, uint32_t scanNo, WalkFormat::Place place, const Net* nets, int n, int routers,
                 int battery) {
  if (state != Status::Log::On || !ensureOpen()) return;
  char buf[96];
  if (!put(buf, WalkFormat::scanLine(buf, sizeof(buf), ms, scanNo, place, n, routers, battery))) return;
  for (int i = 0; i < n; i++) {
    if (!put(buf, WalkFormat::netLine(buf, sizeof(buf), scanNo, nets[i]))) return;
  }
  sync();   // a pulled card or flat battery loses one scan at most
}

inline void place(uint32_t ms, WalkFormat::Place p) {
  if (state != Status::Log::On || !ensureOpen()) return;
  char buf[48];
  if (put(buf, WalkFormat::placeLine(buf, sizeof(buf), ms, p))) sync();
}

} // namespace WalkLog
