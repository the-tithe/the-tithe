#pragma once
#include <M5Cardputer.h>
#include "Press.h"

// Keyboard to Press, with edge detection. Adapted from Hazke's Input.h
// (MIT, REZOR), including keys held by the serial console's "keys"
// command: the characters the keyboard reports, plus T for Tab and N for
// Enter.

inline char injectedKeys[16] = "";

namespace Input {

inline bool prev[8] = {false};
inline char prevCh = 0, prevKey = 0;

inline Press poll() {
  bool now[8] = {false};   // up down left right enter back tab
  char ch = 0, key = 0;
  const bool pressed = M5Cardputer.Keyboard.isPressed();
  if (pressed || injectedKeys[0]) {
    auto st = pressed ? M5Cardputer.Keyboard.keysState() : Keyboard_Class::KeysState{};
    for (const char* k = injectedKeys; *k; k++) {
      if (*k == 'T') st.tab = true;
      else if (*k == 'N') st.enter = true;
      else st.word.push_back(*k);
    }
    now[4] = st.enter;
    now[7] = st.del;   // DEL is back too, so the right hand can play alone
    now[6] = st.tab;
    for (auto c : st.word) {
      switch (c) {
        case ';': now[0] = true; break;
        case '.': now[1] = true; break;
        case ',': now[2] = true; break;
        case '/': now[3] = true; break;
        case '`': now[5] = true; break;
        case '\t': now[6] = true; break;
        default:
          if (c >= 'a' && c <= 'z') { ch = c; key = (char)(c - 'a' + 'A'); }
          else if (c >= 'A' && c <= 'Z') { ch = (char)(c - 'A' + 'a'); key = c; }
          else if ((c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '\'') key = c;
          break;
      }
    }
  }
  Press p;
  bool* edge[7] = {&p.up, &p.down, &p.left, &p.right, &p.enter, &p.back, &p.tab};
  for (int i = 0; i < 7; i++) {
    *edge[i] = now[i] && !prev[i];
    prev[i] = now[i];
    p.any = p.any || *edge[i];
  }
  p.del = now[7] && !prev[7];
  prev[7] = now[7];
  if (p.del) { p.back = true; p.any = true; }
  if (ch && ch != prevCh) { p.ch = ch; p.any = true; }
  prevCh = ch;
  if (key && key != prevKey) { p.key = key; p.any = true; }
  prevKey = key;
  return p;
}

} // namespace Input
