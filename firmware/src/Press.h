#pragma once

// Keys pressed this frame (edges, not held state). Input.h fills it from
// the keyboard; the screen renderer fills it by hand.
//
// Cardputer arrow keys are the printed arrows on ; . , /
// and ` (top left, marked ESC) is back. DEL is back too, and also sets `del`,
// so a screen with typing can erase a letter instead.
struct Press {
  bool up = false, down = false, left = false, right = false;
  bool enter = false, back = false, tab = false;
  bool del = false;
  char ch = 0;   // one letter pressed this frame, lowercase
  char key = 0;  // for typing: a letter (uppercase), digit, space, - or ' pressed this frame
  bool any = false;
};
