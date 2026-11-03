#pragma once
#include <stdint.h>
#include <string.h>

// Rules for where the game is kept, apart from the hardware so they can be
// tested on a computer (SDCard.h and SaveStore.h do the reading and writing).
//
// CardFormat: what's on a card that wouldn't mount, read from its first sector (and, on a
// partitioned card, the first partition's). The Deck reads FAT16 and FAT32 on
// a card with an MBR partition table, or none. Cards of 64 GB and up come
// formatted exFAT, and a Mac's Disk Utility can lay a card out with GPT
// ("GUID Partition Map"): neither mounts here, so the Deck says which it is.

namespace CardFormat {

enum class Kind : uint8_t {
  Unknown,   // nothing the Deck recognises
  Fat,       // FAT, but it wouldn't mount: damaged
  ExFat,
  Ntfs,
  Gpt,
  Blank,     // never formatted
};

inline uint32_t le32(const uint8_t* p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

// A volume's own first sector.
inline Kind fromVolume(const uint8_t* s) {
  if (!memcmp(s + 3, "EXFAT   ", 8)) return Kind::ExFat;
  if (!memcmp(s + 3, "NTFS    ", 8)) return Kind::Ntfs;
  if (!memcmp(s + 0x36, "FAT", 3) || !memcmp(s + 0x52, "FAT", 3)) return Kind::Fat;
  return Kind::Unknown;
}

// The card's sector 0. When it's a partition table pointing at a volume, the
// answer is Unknown and `next` is the sector to read and pass to fromVolume.
inline Kind fromCard(const uint8_t* s0, uint32_t& next) {
  next = 0;
  bool zero = true, ones = true;
  for (int i = 0; i < 512; i++) {
    zero = zero && s0[i] == 0x00;
    ones = ones && s0[i] == 0xFF;
  }
  if (zero || ones) return Kind::Blank;
  const Kind k = fromVolume(s0);
  if (k != Kind::Unknown) return k;
  if (s0[510] != 0x55 || s0[511] != 0xAA) return Kind::Unknown;
  const uint8_t* p = s0 + 0x1BE;   // the first of the four partitions
  if (p[4] == 0xEE) return Kind::Gpt;
  if (p[4] == 0x00) return Kind::Blank;   // a table with nothing in it
  next = le32(p + 8);
  return Kind::Unknown;
}

} // namespace CardFormat

// The Deck's own flash. Launchers lay flash out differently: a full image of
// the Tithe asks for a data area of its own named "tithe" (partitions.csv);
// M5Launcher before 2.6 gives every app a shared one named "spiffs"; installed
// from the game file alone, M5Launcher 2.6 and later give it none, and then the
// card is the only copy.
namespace FlashArea {

// May a data area that wouldn't mount be formatted? An erased one, always: it
// holds nobody's data. One named for the Tithe, unless this Deck has mounted
// that same area before: then the save may be in it, and a mount can fail for
// a moment, so it's left alone and the card's copy carries the game. A shared
// "spiffs" area that isn't erased may hold another app's data: never.
inline bool mayFormat(bool own, bool erased, bool mountedHereBefore) {
  if (erased) return true;
  return own && !mountedHereBefore;
}

} // namespace FlashArea

// What the card is doing, and what the Deck says about it.
namespace CardWords {

enum class CardState : uint8_t {
  None,      // no card answered
  Ok,        // mounted, and a test write came back whole
  NoWrite,   // mounted, but nothing could be written (full, locked or failing)
  Format,    // a card that won't mount: see the CardFormat::Kind
};

// The boot screen's word for the card (8 letters at most).
inline const char* word(CardState h, CardFormat::Kind k) {
  using K = CardFormat::Kind;
  switch (h) {
    case CardState::Ok: return "SD OK";
    case CardState::NoWrite: return "NO WRITE";
    case CardState::Format:
      switch (k) {
        case K::ExFat: return "EXFAT";
        case K::Gpt: return "GPT";
        case K::Ntfs: return "NTFS";
        default: return "UNREAD";
      }
    default: return "NO CARD";
  }
}

// Said after the boot when the card is the problem, or nullptr: what to do about it.
inline const char* fix(CardState h, CardFormat::Kind k) {
  using K = CardFormat::Kind;
  switch (h) {
    case CardState::NoWrite: return "CARD IS FULL OR WON'T WRITE";
    case CardState::Format:
      switch (k) {
        case K::ExFat: return "CARD IS EXFAT: FORMAT IT FAT32";
        case K::Gpt: return "CARD IS GPT: FORMAT IT FAT32, MBR";
        case K::Ntfs: return "CARD IS NTFS: FORMAT IT FAT32";
        default: return "CAN'T READ THE CARD: FORMAT FAT32";
      }
    default: return nullptr;
  }
}

} // namespace CardWords
