#pragma once
#include <stdint.h>

// Wells' tunes, and a small sequencer to play them without holding up the
// frame. Pure logic: the sequencer hands each note to a callback when it's
// due (App::beep, which respects mute and the veil), so tests can listen in.

struct Note {
  uint16_t hz;
  uint16_t ms;
  uint16_t at;   // ms after the tune starts
};

struct Tune {
  static constexpr int Max = 8;
  Note n[Max];
  uint8_t count;
  void add(int hz, int ms, int at) {
    if (count < Max) n[count++] = Note{(uint16_t)hz, (uint16_t)ms, (uint16_t)at};
  }
};

namespace Tunes {

// Each energy's own note: the one its wells play over and over, and the
// new-well chirp. Ember, Tide, Gale, Hush, Static.
constexpr uint16_t Root[5] = {880, 660, 1175, 523, 1397};

// The steps a tune moves in: a pentatonic scale around the root, so no two
// notes clash. Semitones -5 -3 0 2 4 7 9 12, as frequency ratios x1000.
constexpr uint16_t Ratio[8] = {749, 841, 1000, 1122, 1260, 1498, 1682, 2000};
constexpr int Home = 2, Octave = 7;   // the steps that sound like the root

// Note lengths by rhythm: even, long-short, quick-quick, dotted.
constexpr uint16_t Rhythm[4][3] = {{110, 110, 220}, {160, 80, 220}, {80, 80, 260}, {220, 110, 160}};
constexpr int Gap = 25;   // ms of quiet between notes

inline int pitch(int root, int step) { return root * Ratio[step] / 1000; }

// The well's two wandering steps, from its world-keyed seed (above the bits
// its name uses), so a tune is the same on every Deck.
inline void steps(uint32_t seed, int out[3]) {
  const uint32_t s = seed >> 12;
  out[0] = Home;
  out[1] = (int)(s & 7);
  out[2] = (int)((s >> 3) & 7);
  if (out[2] == Home || out[2] == Octave) out[2] = 4;   // only a kept well ends at home
}

// A well's tune: its own note, two more, and for a well you keep (offered at
// three times or more) a last note that comes home. Static crackles first.
inline Tune of(uint32_t seed, int energy, bool kept) {
  Tune t{};
  const int root = Root[energy];
  const uint32_t s = seed >> 12;
  int at = 0;
  if (energy == 4) {
    t.add(2600 + (int)(s & 7) * 150, 6, 0);
    t.add(3400 - (int)((s >> 3) & 7) * 120, 6, 9);
    at = 24;
  }
  int st[3];
  steps(seed, st);
  const uint16_t* len = Rhythm[(s >> 6) & 3];
  for (int i = 0; i < 3; i++) {
    t.add(pitch(root, st[i]), len[i], at);
    at += len[i] + Gap;
  }
  if (kept) t.add(root, 300, at);
  return t;
}

// An offering that rings: the well's notes all at once, with the note of the
// energy you brought. Starts just after the ring.
inline Tune chord(uint32_t seed, int energy, bool kept, int brought) {
  Tune t{};
  constexpr int At = 140, Len = 450;
  const int root = Root[energy];
  int st[3];
  steps(seed, st);
  for (int i = 0; i < 3; i++) t.add(pitch(root, st[i]), Len, At);
  if (kept) t.add(pitch(root, Octave), Len, At);
  if (brought != energy) t.add(Root[brought], Len, At);
  return t;
}

// Wells of all five energies within reach on one walk: their notes, rising,
// held together.
inline Tune five() {
  Tune t{};
  static const int up[5] = {3, 1, 0, 2, 4};   // Hush, Tide, Ember, Gale, Static: low to high
  for (int i = 0; i < 5; i++) t.add(Root[up[i]], 1000 - i * 90, i * 90);
  return t;
}

// Stopping after a walk: a soft falling pair.
inline Tune settle() {
  Tune t{};
  t.add(784, 110, 0);
  t.add(587, 220, 140);
  return t;
}

// A drifting glyph turned up: three slow steps up by a semitone, something
// drifting in. Nothing else in the Deck moves by semitones, so it can't be
// taken for the stall's bright pair.
inline Tune adrift() {
  Tune t{};
  t.add(587, 90, 0);
  t.add(622, 90, 120);
  t.add(659, 220, 240);
  return t;
}

// A street written in: a low pair, up a fourth, one after the other (the
// Tithe's chord is a higher pair sounded together).
inline Tune written() {
  Tune t{};
  t.add(659, 60, 0);
  t.add(880, 140, 80);
  return t;
}

// A drifting glyph's own rhythm, by shape (Cat::Glyph order): Minted a thud,
// Ringing one strike, Echoing two matching notes, Deep a falling note, Bound
// one long note, Beacon four ticks.
inline Tune glyph(int shape) {
  Tune t{};
  switch (shape) {
    case 0: t.add(330, 90, 0); break;
    case 1: t.add(1568, 50, 0); break;
    case 2: t.add(1175, 70, 0); t.add(1175, 70, 200); break;
    case 3: t.add(988, 90, 0); t.add(740, 90, 110); t.add(554, 180, 220); break;
    case 4: t.add(784, 650, 0); break;
    default: for (int i = 0; i < 4; i++) t.add(2093, 18, i * 120); break;
  }
  return t;
}

// A well with the true name of one you've worked: its note, twice, the same.
inline Tune namesake(int energy) {
  Tune t{};
  t.add(Root[energy], 90, 0);
  t.add(Root[energy], 90, 230);
  return t;
}

// A kind of well met for the first time: two quick taps on one high note,
// then a longer one a fourth up. A spell is a pair sounded together; no
// well's tune repeats a note.
inline Tune guide() {
  Tune t{};
  t.add(1568, 40, 0);
  t.add(1568, 40, 70);
  t.add(2093, 180, 150);
  return t;
}

// A new well that isn't in reach: just its energy's note.
inline Tune chirp(int energy) {
  Tune t{};
  t.add(Root[energy], 60, 0);
  return t;
}

} // namespace Tunes

// Plays one tune at a time. Notes start at their own times; notes that start
// together sound together (the speaker gives each its own channel).
class Sequencer {
 public:
  static constexpr int Max = Tune::Max;

  // Start `t` now, in place of whatever was still to come.
  void play(const Tune& t, uint32_t nowMs) {
    n_ = 0;
    for (int i = 0; i < t.count && n_ < Max; i++) q_[n_++] = Due{t.n[i].hz, t.n[i].ms, nowMs + t.n[i].at};
  }

  // Sound every note that's due: out(hz, ms).
  template <class F>
  void tick(uint32_t nowMs, F out) {
    int k = 0;
    for (int i = 0; i < n_; i++) {
      if ((int32_t)(nowMs - q_[i].due) >= 0) out(q_[i].hz, q_[i].ms);
      else q_[k++] = q_[i];
    }
    n_ = k;
  }

  bool busy() const { return n_ > 0; }

 private:
  struct Due {
    uint16_t hz, ms;
    uint32_t due;
  };
  Due q_[Max];
  int n_ = 0;
};
