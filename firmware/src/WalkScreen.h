#pragma once
#include "UI.h"
#include "Press.h"
#include "Sightings.h"
#include "Status.h"

// This walk so far: how long, how many wells, and how much of it was spent
// in each kind of ward.

namespace WalkScreen {

inline bool update(const Press& p) { return p.back || p.ch == 's'; }

inline void rowText(Canvas& g, int y, const char* label, const char* value, uint16_t c = UI::Text) {
  UI::text(g, 4, y, label, UI::Dim);
  UI::text(g, 64, y, value, c);
}

inline void draw(Canvas& g, const Sightings& sky, const Status& st, float t) {
  g.fillSprite(UI::Bg);
  char b[40];
  if (st.walk) snprintf(b, sizeof(b), "WALK %lu", (unsigned long)st.walk);
  else snprintf(b, sizeof(b), "THIS WALK");
  UI::text(g, 4, 4, b, UI::Text, 2);
  if (st.log == Status::Log::On) {
    if (fmodf(t, 1.0f) < 0.6f) g.fillCircle(UI::W - 34, 11, 3, UI::Rec);
    UI::textRight(g, UI::W - 4, 8, "REC", UI::Rec);
  }

  const uint32_t secs = (st.nowMs - sky.startMs()) / 1000;
  snprintf(b, sizeof(b), "%lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  rowText(g, 26, "TIME", b);
  const float mins = secs / 60.0f;
  if (mins >= 1) snprintf(b, sizeof(b), "%lu, %.1f NEW A MINUTE", (unsigned long)sky.found(), sky.found() / mins);
  else snprintf(b, sizeof(b), "%lu", (unsigned long)sky.found());
  rowText(g, 37, "WELLS", b);
  snprintf(b, sizeof(b), "%lu", (unsigned long)sky.scans());
  rowText(g, 48, "SCANS", b);
  rowText(g, 59, "PLACE", WalkFormat::placeLabel(st.place),
          st.place == WalkFormat::Place::None ? UI::Dim : UI::Accent);
  switch (st.log) {
    case Status::Log::On:
      snprintf(b, sizeof(b), "W%04lu.CSV, %lu KB", (unsigned long)st.walk, (unsigned long)(st.logBytes / 1024));
      rowText(g, 70, "FILE", b);
      break;
    case Status::Log::Off: rowText(g, 70, "FILE", "NOT RECORDING", UI::Dim); break;
    case Status::Log::NoCard: rowText(g, 70, "FILE", "NO SD CARD", UI::Dim); break;
    case Status::Log::Failed: rowText(g, 70, "FILE", "CARD ERROR", UI::Rec); break;
  }

  // Time in each ward, as one bar with the share under each part.
  uint32_t total = 0;
  for (int w = 0; w < WardCount; w++) total += sky.wardScans((WardKind)w);
  const int y = 86, x0 = 4, width = UI::W - 8;
  if (total == 0) {
    g.drawRect(x0, y, width, 8, UI::Faint);
  } else {
    uint32_t acc = 0;
    int x = x0;
    for (int w = 0; w < WardCount; w++) {
      const uint32_t c = sky.wardScans((WardKind)w);
      if (!c) continue;
      acc += c;
      const int x1 = x0 + (int)(width * acc / total);
      g.fillRect(x, y, x1 - x - (acc < total ? 1 : 0), 8, UI::wardColor((WardKind)w));
      x = x1;
    }
  }
  for (int w = 0; w < WardCount; w++) {   // legend, two by two
    const int lx = x0 + (w % 2) * (width / 2), ly = y + 13 + (w / 2) * 11;
    const uint32_t c = sky.wardScans((WardKind)w);
    snprintf(b, sizeof(b), "%s %lu%%", wardName((WardKind)w),
             (unsigned long)(total ? (c * 100 + total / 2) / total : 0));
    g.fillRect(lx, ly + 1, 3, 6, UI::wardColor((WardKind)w));
    UI::text(g, lx + 6, ly, b, total && c ? UI::Text : UI::Dim);
  }

  UI::footerLine(g);
  int x = UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
  x = UI::keyHint(g, x, "TAB", "PLACE", UI::Text);
  UI::keyHint(g, x, "R", st.log == Status::Log::On ? "STOP" : "RECORD", UI::Text);
  UI::battery(g, st.battery);
}

} // namespace WalkScreen
