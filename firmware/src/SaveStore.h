#pragma once
#include <LittleFS.h>
#include <esp_partition.h>
#include <SD.h>
#include <stdio.h>
#include <stdlib.h>
#include "Persist.h"
#include "SDCard.h"
#include "Storage.h"
#include "game/Save.h"

// Where the game is kept: the Deck's own flash, with a backup copy on the
// SD card. Each save goes to a temporary file that then replaces the old
// one, so a flat battery mid-save leaves the previous save whole. On boot the
// newer of the two copies loads (Game::SaveFormat::chooseBoot), so a damaged
// flash save, or flash saves that have been failing, fall back to the card.
// A save that can't be loaded (damaged, or from different firmware) is set
// aside as deck.bad, deck.bad2 and so on, never written over. A save from a
// newer Tithe, or one there was no memory to read, is left exactly where it
// is, and nothing is saved until the Deck restarts. Loading reads straight into
// the game, so it needs no spare memory (only a 72-relic save borrows some).

namespace SaveStore {

inline bool flashOk = false;
inline bool locked = false;   // the save there mustn't be written over: nothing is saved
inline uint32_t lastBackupMs = 0;
inline bool cardSeen = false;     // the card's copy was weighed at boot, or checked since
inline bool cardKept = false;     // the card holds a newer save than this session's: it isn't written
inline uint16_t deckTag = 0;
inline uint32_t cardMount = 0;   // the card mount (SDCard::mounts) cardSeen and cardKept are about
inline bool lastKept = true;     // the last save reached flash or the card

// Whether the game can be kept at all right now: a save would reach flash or
// the card. False means progress ends when the Deck is switched off.
inline bool keeping() { return !locked && lastKept && (flashOk || SDCard::ready); }
constexpr const char* Path = "/deck.sav";
constexpr const char* Tmp = "/deck.tmp";
constexpr const char* SdPath = "/deck/deck.sav";
constexpr const char* SdTmp = "/deck/deck.tmp";
constexpr const char* Bad = "/deck.bad";
constexpr const char* SdBad = "/deck/deck.bad";

// Where the save is kept in flash: "tithe" (the area a full image of the
// Tithe asks for), else "spiffs" (the one older launchers share between apps),
// else nowhere and the card is the only copy. See Storage.h for when an area
// that won't mount may be formatted. Call after Persist::begin().
inline const char* flashArea = nullptr;

// Nothing but 0xFF in its first 16 KB: never written.
inline bool erased(const esp_partition_t* p) {
  uint8_t* b = SDCard::scratch;
  const size_t n = p->size < 16384 ? p->size : 16384;
  for (size_t at = 0; at < n; at += sizeof(SDCard::scratch)) {
    if (esp_partition_read(p, at, b, sizeof(SDCard::scratch)) != ESP_OK) return false;
    for (size_t i = 0; i < sizeof(SDCard::scratch); i++)
      if (b[i] != 0xFF) return false;
  }
  return true;
}

inline bool mountArea(const char* label, bool own) {
  const esp_partition_t* p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, label);
  if (!p) return false;
  bool ok = LittleFS.begin(false, "/littlefs", 10, label) || LittleFS.begin(false, "/littlefs", 10, label);
  if (!ok && FlashArea::mayFormat(own, erased(p), own && Persist::titheAt == p->address))
    ok = LittleFS.begin(true, "/littlefs", 10, label);
  if (ok && own) Persist::noteTitheMounted(p->address);
  return ok;
}

inline bool begin() {
  flashArea = mountArea("tithe", true) ? "tithe" : mountArea("spiffs", false) ? "spiffs" : nullptr;
  flashOk = flashArea != nullptr;
  return flashOk;
}

// Before the first card write of a session whose boot didn't weigh the card's
// copy (the card went in later): a newer save there is kept as it is, and only
// flash gets this session's saves, so the next boot weighs both and loads the
// newer. Another Deck's save there moves aside (deck.bad).
inline void checkCard(const Game::Saved& s);

// `flash`: LittleFS's rename replaces the old file in one step. FAT (the SD
// card) can't rename over a file, so there the old one is removed first.
inline bool writeTo(fs::FS& fs, const char* path, const char* tmp, const Game::Saved& s, bool flash) {
  const Game::SaveFormat::Header h = Game::SaveFormat::headerFor(s);
  auto failed = [flash](const char* what) {
    if (!flash) SDCard::noteFailure(what);
    return false;
  };
  File f = fs.open(tmp, FILE_WRITE);
  if (!f) return failed("save open");
  const bool ok = f.write((const uint8_t*)&h, sizeof(h)) == sizeof(h) &&
                  f.write((const uint8_t*)&s, sizeof(s)) == sizeof(s);
  f.close();
  if (!ok) {
    failed("save write");
    fs.remove(tmp);
    return false;
  }
  if (!flash) fs.remove(path);
  return fs.rename(tmp, path) || failed("save rename");
}

// Where a save went.
struct Wrote {
  bool flash;        // into flash
  bool card;         // onto the card
  bool cardFailed;   // the card was tried and failed (backups stop until it's mounted again)
  bool cardKept;     // the card holds a newer save, so it wasn't written (see checkCard)
};

// Save to flash, and to the SD card at most once a minute (or when `backup`).
inline Wrote save(const Game::Saved& s, uint32_t nowMs, bool backup) {
  Wrote w{false, false, false, false};
  if (locked) return w;
  w.flash = flashOk && writeTo(LittleFS, Path, Tmp, s, true);
  // Without a flash data area (some launchers' layouts have none), or while
  // saving there fails, the card is the only copy, so it gets every save.
  if ((backup || !w.flash || nowMs - lastBackupMs > 60000) && SDCard::ready) {
    if (cardMount != SDCard::mounts) {   // a card that mounted again may not be the same card
      cardMount = SDCard::mounts;
      cardSeen = cardKept = false;
    }
    if (!cardSeen) checkCard(s);
    if (cardKept) {
      w.cardKept = true;
    } else {
      w.card = writeTo(SD, SdPath, SdTmp, s, false);
      if (w.card) {
        lastBackupMs = nowMs;
      } else {
        SDCard::markFailed();
        w.cardFailed = true;
      }
    }
  }
  // Kept somewhere: flash took it, or the card did.
  lastKept = w.flash || w.card;
  return w;
}

// Read the save at `path` straight into `out`, borrowing no memory (see
// Game::SaveFormat::decodeInto). On anything but Ok, `out` holds rubbish.
inline Game::SaveFormat::Load readFrom(fs::FS& fs, const char* path, Game::Saved& out) {
  File f = fs.open(path, FILE_READ);
  if (!f) return Game::SaveFormat::Load::Empty;
  auto read = [&f](size_t at, uint8_t* buf, size_t len) -> size_t { return f.seek(at) ? f.read(buf, len) : 0; };
  const Game::SaveFormat::Load l = Game::SaveFormat::decodeInto(f.size(), read, out);
  f.close();
  return l;
}

// What the file at `path` holds, read a piece at a time.
inline Game::SaveFormat::Copy peekFile(fs::FS& fs, const char* path) {
  File f = fs.open(path, FILE_READ);
  if (!f) return Game::SaveFormat::Copy{Game::SaveFormat::Load::Empty, 0, 0};
  auto read = [&f](size_t at, uint8_t* buf, size_t len) -> size_t { return f.seek(at) ? f.read(buf, len) : 0; };
  const Game::SaveFormat::Copy c = Game::SaveFormat::peek(f.size(), read);
  f.close();
  return c;
}

// `path`, or the temporary file if a save was cut off between writing it and
// renaming it. `from` says which file it was.
inline Game::SaveFormat::Copy peekEither(fs::FS& fs, const char* path, const char* tmp, const char*& from) {
  using L = Game::SaveFormat::Load;
  from = path;
  const Game::SaveFormat::Copy c = peekFile(fs, path);
  if (c.load != L::Empty) return c;
  const Game::SaveFormat::Copy t = peekFile(fs, tmp);
  if (t.load == L::Ok) fs.rename(tmp, path);
  else if (t.load != L::Empty) from = tmp;
  return t;
}

// A save that won't load moves aside, so nothing saved later writes over it.
// Each gets a name of its own (deck.bad, deck.bad2, ...): one set aside earlier
// is never replaced.
inline void setAside(fs::FS& fs, const char* path, const char* bad) {
  char name[32];
  for (int i = 1; i < 100; i++) {
    if (i == 1) snprintf(name, sizeof(name), "%s", bad);
    else snprintf(name, sizeof(name), "%s%d", bad, i);
    if (!fs.exists(name)) { fs.rename(path, name); return; }
  }
}

// Load the game into `out`: the newer of the flash copy and the card's (`deck`
// is this Deck's tag). Broken copies are set aside. `why` says what was wrong
// with the copy that didn't load, for the console. `out` is the room each copy
// is read into, so unless a copy loads, the caller starts a new career in it.
inline Game::SaveFormat::Boot load(Game::Saved& out, uint16_t deck, Game::SaveFormat::Load& why) {
  using namespace Game::SaveFormat;
  deckTag = deck;
  cardSeen = SDCard::ready;
  cardMount = SDCard::mounts;
  const char* fPath = Path;
  const char* cPath = SdPath;
  Copy f = flashOk ? peekEither(LittleFS, Path, Tmp, fPath) : Copy{Load::Empty, 0, 0};
  Copy c = SDCard::ready ? peekEither(SD, SdPath, SdTmp, cPath) : Copy{Load::Empty, 0, 0};
  why = f.load != Load::Empty ? f.load : c.load;
  for (;;) {
    const Boot b = chooseBoot(f, c, deck);
    if (b == Boot::Flash || fromCard(b)) {
      const bool card = fromCard(b);
      const Load l = readFrom(card ? (fs::FS&)SD : (fs::FS&)LittleFS, card ? cPath : fPath, out);
      if (l == Load::Ok) {
        if (b == Boot::CardDamaged) setAside(LittleFS, fPath, Bad);
        // Another Deck's save on the card moves aside, so this Deck's backups never write over it.
        // (After a lost key every save of this Deck reads as another's: then nothing moves.)
        if (b == Boot::Flash && c.load == Load::Ok && c.tag && c.tag != deck && !Persist::keyNew) setAside(SD, cPath, SdBad);
        return b;
      }
      why = l;
      (card ? c : f).load = l;   // it looked whole but won't load: choose again without it
      continue;
    }
    if (b == Boot::SetAside) {
      if (broken(f.load)) setAside(LittleFS, fPath, Bad);
      if (broken(c.load)) setAside(SD, cPath, SdBad);
    }
    if (b == Boot::Newer || b == Boot::NoMemory) locked = true;
    return b;
  }
}

inline void checkCard(const Game::Saved& s) {
  cardSeen = true;
  const char* from = SdPath;
  const Game::SaveFormat::Copy c = peekEither(SD, SdPath, SdTmp, from);
  if (c.load != Game::SaveFormat::Load::Ok) return;   // nothing there worth keeping: a broken one moves aside at the next boot
  if (c.tag && c.tag != deckTag && !Persist::keyNew) setAside(SD, from, SdBad);   // another Deck's: kept, out of the way
  else cardKept = c.playMs > s.playMs;
}

} // namespace SaveStore
