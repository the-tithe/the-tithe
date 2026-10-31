#pragma once
#include <LittleFS.h>
#include <SD.h>
#include <stdlib.h>
#include "SDCard.h"
#include "game/Save.h"

// Where the game is kept: the Deck's own flash, with a backup copy on the
// SD card. Each save goes to a temporary file that then replaces the old
// one, so a flat battery mid-save leaves the previous save whole. A damaged
// save fails its checksum on load, and the backup is used instead. A save
// that can't be loaded (damaged, or from different firmware) is set aside as
// deck.bad rather than written over, so a firmware update never quietly
// erases a career.

namespace SaveStore {

inline bool flashOk = false;
inline uint32_t lastBackupMs = 0;
constexpr const char* Path = "/deck.sav";
constexpr const char* Tmp = "/deck.tmp";
constexpr const char* SdPath = "/deck/deck.sav";
constexpr const char* SdTmp = "/deck/deck.tmp";
constexpr const char* Bad = "/deck.bad";
constexpr const char* SdBad = "/deck/deck.bad";

inline bool begin() {
  flashOk = LittleFS.begin(true);   // formats the data partition the first time
  return flashOk;
}

// `flash`: LittleFS's rename replaces the old file in one step. FAT (the SD
// card) can't rename over a file, so there the old one is removed first.
inline bool writeTo(fs::FS& fs, const char* path, const char* tmp, const Game::Saved& s, bool flash) {
  const Game::SaveFormat::Header h = Game::SaveFormat::headerFor(s);
  File f = fs.open(tmp, FILE_WRITE);
  if (!f) return false;
  const bool ok = f.write((const uint8_t*)&h, sizeof(h)) == sizeof(h) &&
                  f.write((const uint8_t*)&s, sizeof(s)) == sizeof(s);
  f.close();
  if (!ok) { fs.remove(tmp); return false; }
  if (!flash) fs.remove(path);
  return fs.rename(tmp, path);
}

// Save to flash, and to the SD card at most once a minute (or when `backup`).
inline bool save(const Game::Saved& s, uint32_t nowMs, bool backup) {
  bool ok = flashOk && writeTo(LittleFS, Path, Tmp, s, true);
  // Without a flash data area (some launchers' layouts have none), the SD
  // card is the only copy, so it gets every save.
  if ((backup || !flashOk || nowMs - lastBackupMs > 60000) && SDCard::ready) {
    if (writeTo(SD, SdPath, SdTmp, s, false)) lastBackupMs = nowMs;
    else SDCard::markFailed();
  }
  return ok;
}

inline Game::SaveFormat::Load readFrom(fs::FS& fs, const char* path, Game::Saved& out) {
  File f = fs.open(path, FILE_READ);
  if (!f) return Game::SaveFormat::Load::Empty;
  const size_t n = f.size();
  uint8_t* buf = (uint8_t*)malloc(n ? n : 1);
  if (!buf) { f.close(); return Game::SaveFormat::Load::WrongSize; }
  const size_t got = f.read(buf, n);
  f.close();
  const Game::SaveFormat::Load l = Game::SaveFormat::decode(buf, got, out);
  free(buf);
  return l;
}

// A save that won't load moves aside, so nothing saved later writes over it.
inline void setAside(fs::FS& fs, const char* path, const char* bad) {
  fs.remove(bad);
  fs.rename(path, bad);
}

// Read `path`, or the temporary file if a save was cut off between writing
// it and renaming it.
inline Game::SaveFormat::Load readEither(fs::FS& fs, const char* path, const char* tmp, Game::Saved& out) {
  Game::SaveFormat::Load l = readFrom(fs, path, out);
  if (l == Game::SaveFormat::Load::Empty) {
    l = readFrom(fs, tmp, out);
    if (l == Game::SaveFormat::Load::Ok) fs.rename(tmp, path);
  }
  return l;
}

// Load the game: flash first, then the SD backup. Returns where it came from.
inline const char* load(Game::Saved& out, Game::SaveFormat::Load& why) {
  using L = Game::SaveFormat::Load;
  why = flashOk ? readEither(LittleFS, Path, Tmp, out) : L::Empty;
  if (why == L::Ok) return "flash";
  if (flashOk && why != L::Empty) setAside(LittleFS, Path, Bad);
  if (SDCard::ready) {
    const L sd = readEither(SD, SdPath, SdTmp, out);
    if (sd == L::Ok) { why = sd; return "sd backup"; }
    if (sd != L::Empty) setAside(SD, SdPath, SdBad);
  }
  return nullptr;
}

} // namespace SaveStore
