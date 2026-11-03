// Draws every screen with made-up wells and saves each as a PPM image in
// shots/, so the screens can be checked without a Cardputer.
//   pio run -e render && .pio/build/render/program && python3 tools/png.py
#include <M5GFX.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "BootScreen.h"
#include "DowseScreen.h"
#include "Effects.h"
#include "GlassScreen.h"
#include "Motion.h"
#include "ScannerScreen.h"
#include "Sightings.h"
#include "Status.h"
#include "WalkScreen.h"
#include "WellScreen.h"

M5Canvas canvas;
static Sightings sky;
namespace Play { void run(); }

void saveShot(const char* name);
static void save(const char* name) { saveShot(name); }
void saveShot(const char* name) {
  char path[128];
  snprintf(path, sizeof(path), "shots/%s.ppm", name);
  FILE* f = fopen(path, "wb");
  fprintf(f, "P6\n%d %d\n255\n", UI::W, UI::H);
  for (int y = 0; y < UI::H; y++)
    for (int x = 0; x < UI::W; x++) {
      const auto c = canvas.readPixelRGB(x, y);
      const unsigned char px[3] = {c.R8(), c.G8(), c.B8()};
      fwrite(px, 1, 3, f);
    }
  fclose(f);
  printf("%s\n", path);
}

// A made-up street: `n` routers, some broadcasting more than one network.
struct FakeRouter {
  uint8_t bssid[6];
  int8_t rssi;
  uint8_t channel;
  Lock lock;
  bool hidden;
  bool guest;
};

static uint32_t rng = 12345;
static int rnd(int n) {
  rng = rng * 1103515245u + 12345u;
  return (int)((rng >> 16) % (uint32_t)n);
}

static void makeRouters(FakeRouter* r, int n) {
  static const uint8_t channels[] = {1, 1, 6, 6, 6, 11, 11, 3, 9, 13};
  static const Lock locks[] = {Lock::Wpa2, Lock::Wpa2, Lock::Wpa2, Lock::Wpa2, Lock::Wpa23,
                               Lock::Wpa3, Lock::Open, Lock::Enterprise, Lock::Wpa2};
  for (int i = 0; i < n; i++) {
    for (int b = 0; b < 6; b++) r[i].bssid[b] = (uint8_t)rnd(256);
    r[i].bssid[5] &= 0xF0;
    r[i].rssi = (int8_t)(-45 - rnd(45));
    r[i].channel = channels[rnd(10)];
    r[i].lock = locks[rnd(9)];
    r[i].hidden = rnd(10) == 0;
    r[i].guest = rnd(4) == 0;
  }
}

static int toNets(const FakeRouter* r, int n, Net* out, int jitter) {
  int k = 0;
  for (int i = 0; i < n; i++) {
    Net& a = out[k++];
    memcpy(a.bssid, r[i].bssid, 6);
    a.rssi = (int8_t)(r[i].rssi + (jitter ? rnd(2 * jitter + 1) - jitter : 0));
    a.channel = r[i].channel;
    a.lock = r[i].lock;
    a.hidden = r[i].hidden;
    a.nameKey = a.hidden ? 0 : 0x1000u + i;
    if (r[i].guest) {
      Net& gst = out[k++];
      gst = a;
      gst.bssid[0] |= 0x02;
      gst.bssid[5] |= 0x01;
      gst.lock = Lock::Open;
      gst.hidden = false;
      gst.rssi = (int8_t)(a.rssi - 2);
    }
  }
  return k;
}

static Status status() {
  Status st;
  st.place = WalkFormat::Place::Street;
  st.log = Status::Log::On;
  st.walk = 3;
  st.logBytes = 41 * 1024 + 300;
  st.battery = 87;
  st.listening = true;
  return st;
}

// Hold a Motion in a steady pose for a while, or shake it like a walk.
static void hold(Motion& m, float ax, float ay, float az, float secs) {
  const float a[3] = {ax, ay, az}, g[3] = {0, 0, 0};
  for (float t = 0; t < secs; t += 1 / 30.0f) m.update(a, g, 1 / 30.0f);
}
static void walk(Motion& m, float secs) {
  for (int i = 0; i < (int)(secs * 30); i++) {
    const float b = 0.25f * sinf(i / 30.0f * 12.57f);
    const float a[3] = {0.02f, 0.6f + 0.6f * b, 0.8f + 0.8f * b};
    const float g[3] = {0.12f * sinf(i * 0.9f), 0.1f * cosf(i * 0.5f), 0.09f * sinf(i * 1.3f)};
    m.update(a, g, 1 / 30.0f);
  }
}

static void motionShots() {
  // A quiet residential street: 14 routers, two of them hidden.
  FakeRouter r[14];
  Net nets[40];
  rng = 777;
  makeRouters(r, 14);
  r[2].hidden = true; r[2].rssi = -58; r[2].guest = false;
  r[9].hidden = true; r[9].rssi = -76; r[9].guest = false;
  sky.reset(0);
  uint32_t now = 0;
  for (int k = 0; k < 8; k++) { now += 2400; sky.ingest(nets, toNets(r, 14, nets, 2), now); }
  Status st = status();
  st.nowMs = now;
  st.gyro = true;

  // The list while walking: Hush names still veiled; a ward holding.
  st.hushReveal = 0.35f;
  st.wardLeft = 40;
  ScannerScreen::selectedId = 0; ScannerScreen::selIndex = 0; ScannerScreen::top = 0;
  Press p;
  ScannerScreen::update(p, sky);
  ScannerScreen::draw(canvas, sky, st, 0.4f);  save("scan_hush_veiled");
  st.wardLeft = 0;

  // The glass while walking.
  Motion m;
  hold(m, 0, 0.6f, 0.8f, 0.5f);
  walk(m, 3);
  st.hushReveal = 0;
  GlassScreen::selectedId = 0;
  GlassScreen::update(p, sky, st);
  GlassScreen::draw(canvas, sky, m, st, 2.0f);  save("glass_walking");

  // Standing still, tipped a little to the left: clear, Hush surfaced.
  Motion still;
  hold(still, 0.3f, 0.55f, 0.78f, 5);
  st.hushReveal = 1;
  GlassScreen::draw(canvas, sky, still, st, 4.0f);  save("glass_still");
  st.hushReveal = 0.6f;
  GlassScreen::draw(canvas, sky, still, st, 4.0f);  save("glass_clearing");

  // Dowse the strongest well. It lies 60 degrees to the right of north.
  const WellRec* list[Sightings::MaxWells];
  sky.listed(list, Sightings::MaxWells);
  const uint32_t id = list[0]->id;
  const float truth = 60 * Motion::DegToRad;
  DowseScreen::begin(id);
  Motion dm;
  hold(dm, 0, 0.6f, 0.8f, 1);
  srand(5);
  auto reading = [&](float h) {
    const float c = cosf(h - truth);
    const float shade = c > -0.2f ? 0 : -8 * (-0.2f - c) / 0.8f;
    return (int)lroundf(-60 + shade) + rand() % 5 - 2;
  };
  for (float h = 0; h < 3.4f; h += 0.09f) DowseScreen::sample(Motion::wrap(h), reading(h));
  dm.heading = Motion::wrap(3.4f);
  DowseScreen::heardAt = 0;
  DowseScreen::draw(canvas, sky, dm, st, 0.05f);  save("dowse_turning");
  for (float h = 3.4f; h < 6.6f; h += 0.09f) DowseScreen::sample(Motion::wrap(h), reading(h));
  dm.heading = Motion::wrap(0.0f);   // facing north again: the well is ahead-right
  DowseScreen::draw(canvas, sky, dm, st, 1.0f);  save("dowse_found");
  printf("dowsed %.0f deg (truth 60), pull %.1f dB\n", DowseScreen::dowse.bearing() / Motion::DegToRad,
         DowseScreen::dowse.contrast());
  dm.heading = Motion::wrap(55 * Motion::DegToRad);
  DowseScreen::draw(canvas, sky, dm, st, 1.0f);  save("dowse_ahead");
  Status noGyro = st;
  noGyro.gyro = false;
  DowseScreen::draw(canvas, sky, dm, noGyro, 1.0f);  save("dowse_nogyro");

  // The glass with that well pinned to its real direction, facing north.
  sky.setBearing(id, DowseScreen::dowse.bearing(), now);
  GlassScreen::selectedId = id;
  GlassScreen::draw(canvas, sky, still, st, 4.0f);  save("glass_dowsed");

  // The gesture effects over the list.
  st.hushReveal = 1;
  ScannerScreen::draw(canvas, sky, st, 0.4f);
  Effects::pulse(canvas, 0.3f);  save("fx_pulse");
  ScannerScreen::draw(canvas, sky, st, 0.4f);
  Effects::ward(canvas, 0.5f);  save("fx_ward");
  ScannerScreen::draw(canvas, sky, st, 0.4f);
  Effects::eye(canvas, 0.35f);  save("fx_eye");
}

// The Deck's own font and energy glyphs as images, for tools/manual.py to
// typeset the manual in exactly the app's lettering.
static void fontAtlas() {
  M5Canvas atlas;
  atlas.setColorDepth(16);
  atlas.createSprite(16 * 6, 6 * 8);
  atlas.fillSprite(TFT_BLACK);
  atlas.setTextSize(1);
  atlas.setTextColor(TFT_WHITE);
  for (int c = 32; c < 127; c++) {
    atlas.setCursor((c - 32) % 16 * 6, (c - 32) / 16 * 8);
    atlas.print((char)c);
  }
  M5Canvas glyphs;
  glyphs.setColorDepth(16);
  glyphs.createSprite(5 * 8, 8);
  glyphs.fillSprite(TFT_BLACK);
  for (int e = 0; e < 5; e++) UI::energyGlyph(glyphs, e * 8, 0, (Energy)e, TFT_WHITE);
  auto dump = [](M5Canvas& c, const char* path) {
    FILE* f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", c.width(), c.height());
    for (int y = 0; y < c.height(); y++)
      for (int x = 0; x < c.width(); x++) {
        const auto px = c.readPixelRGB(x, y);
        const unsigned char b[3] = {px.R8(), px.G8(), px.B8()};
        fwrite(b, 1, 3, f);
      }
    fclose(f);
  };
  dump(atlas, "shots/font.ppm");
  dump(glyphs, "shots/glyphs.ppm");
}

int main() {
  mkdir("shots", 0755);
  canvas.setColorDepth(16);
  canvas.createSprite(UI::W, UI::H);
  for (int i = 0; i < 16; i++) Keys::device[i] = (uint8_t)(i * 7 + 1);

  BootScreen::Info info{true, true, true};
  BootScreen::draw(canvas, 0.6f, info);  save("boot_1");
  BootScreen::draw(canvas, 1.3f, info);  save("boot_2");
  BootScreen::draw(canvas, 2.2f, info);  save("boot_3");
  info.card = false;
  BootScreen::draw(canvas, 2.2f, info);  save("boot_nocard");
  // Cards that mount nowhere but a launcher: each says what it is.
  info.cardWord = "EXFAT";
  BootScreen::draw(canvas, 2.2f, info);  save("boot_card_exfat");
  info.cardWord = "NO WRITE";
  BootScreen::draw(canvas, 2.2f, info);  save("boot_card_nowrite");
  info.cardWord = nullptr;
  // A first boot on a Cardputer without a motion sensor: new keys, no moves, neither a fault.
  info = BootScreen::Info{true, true, false};
  info.keysNew = true;
  BootScreen::draw(canvas, 2.2f, info);  save("boot_first_noimu");
  // New keys with a save to load: its wells won't be known.
  info = BootScreen::Info{true, true, true};
  info.keysNew = info.keysLost = true;
  BootScreen::draw(canvas, 2.2f, info);  save("boot_keys_lost");
  BootScreen::drawStop(canvas, "THIS SAVE IS FROM A NEWER TITHE", "INSTALL THE NEWER BUILD");  save("boot_stop_newer");
  BootScreen::drawStop(canvas, "NOT ENOUGH MEMORY TO LOAD THE SAVE", "SWITCH IT OFF AND ON AGAIN");  save("boot_stop_memory");

  Status st = status();
  sky.reset(0);
  ScannerScreen::draw(canvas, sky, st, 0.3f);  save("scan_listening");
  st.deaf = true;
  ScannerScreen::draw(canvas, sky, st, 0.3f);  save("scan_deaf");
  st.deaf = false;

  // A busy street: 16 routers, a few dropping out, two found late.
  FakeRouter r[48];
  Net nets[96];
  makeRouters(r, 18);
  uint32_t now = 0;
  for (int s = 0; s < 6; s++) {
    now += 2500;
    const int count = s < 4 ? 16 : 18;   // the last two routers show up late
    sky.ingest(nets, toNets(r, count, nets, 3), now);
  }
  // Two routers drop out of the latest scan.
  now += 2500;
  FakeRouter keep[18];
  memcpy(keep, r, sizeof(keep));
  keep[3] = keep[17];
  keep[5] = keep[16];
  sky.ingest(nets, toNets(keep, 16, nets, 3), now);
  st.nowMs = now;
  Press p;
  ScannerScreen::update(p, sky);
  p.down = true;
  ScannerScreen::update(p, sky);
  ScannerScreen::update(p, sky);
  ScannerScreen::draw(canvas, sky, st, 0.2f);  save("scan_ward");

  // Scroll to the bottom of the list.
  for (int i = 0; i < 20; i++) ScannerScreen::update(p, sky);
  ScannerScreen::draw(canvas, sky, st, 0.9f);  save("scan_ward_bottom");

  // Detail of the selected well, walking toward it.
  const uint32_t id = ScannerScreen::selectedId;
  for (int s = 0; s < 14; s++) {
    now += 2500;
    // Make the selected well's signal climb: find which fake router it is.
    for (int i = 0; i < 16; i++) {
      uint8_t rb[5];
      WellRules::routerBytes(keep[i].bssid, rb);
      if (Keys::deviceHash(rb, 5) == id && keep[i].rssi < -42) keep[i].rssi += 3;
    }
    sky.ingest(nets, toNets(keep, s == 9 ? 15 : 16, nets, 2), now);
  }
  st.nowMs = now + 4 * 60000;
  WellScreen::draw(canvas, sky, id, st, 0.0f);  save("well_warmer");

  // A Nexus: 34 routers.
  sky.reset(0);
  ScannerScreen::selectedId = 0;
  ScannerScreen::selIndex = 0;
  ScannerScreen::top = 0;
  makeRouters(r, 34);
  now = 0;
  for (int s = 0; s < 3; s++) {
    now += 2500;
    sky.ingest(nets, toNets(r, 34, nets, 3), now);
  }
  st.nowMs = now;
  st.place = WalkFormat::Place::Work;
  p = Press();
  ScannerScreen::update(p, sky);
  ScannerScreen::draw(canvas, sky, st, 0.5f);  save("scan_nexus");
  {   // the hint star on a well further down than the screen shows
    static const WellRec* list[Sightings::MaxWells];
    const int n = sky.listed(list, Sightings::MaxWells);
    st.starId = n > 12 ? list[12]->id : 0;
    ScannerScreen::draw(canvas, sky, st, 0.5f);  save("scan_star_below");
    st.starId = 0;
  }
  {   // the count says Quarter before two scans agree: the held ward, and where it's going
    Sightings s2;
    s2.reset(0);
    FakeRouter q[16];
    makeRouters(q, 16);
    uint32_t t = 0;
    for (int k = 0; k < 4; k++) { t += 2500; s2.ingest(nets, toNets(q, 6, nets, 0), t); }
    t += 2500;
    s2.ingest(nets, toNets(q, 14, nets, 0), t);
    Status s3 = status();
    s3.nowMs = t;
    ScannerScreen::draw(canvas, s2, s3, 0.5f);  save("scan_ward_pending");
    // and with everything else the header can hold
    s3.inRun = true; s3.wardens = true; s3.heat = 3.4; s3.wardLeft = 30; s3.adrift = true; s3.battery = 100;
    s3.tithe = 410; s3.total = 120; s3.visitsLeft = 7; s3.coin = 23;
    ScannerScreen::draw(canvas, s2, s3, 0.5f);  save("scan_ward_pending_full");
  }

  // A Hollow with two wells and no card.
  sky.reset(0);
  makeRouters(r, 2);
  r[0].rssi = -71;
  r[1].rssi = -84;
  r[1].hidden = true;
  r[1].guest = false;
  r[0].guest = false;
  sky.ingest(nets, toNets(r, 2, nets, 0), 2500);
  st.place = WalkFormat::Place::Park;
  st.log = Status::Log::NoCard;
  st.battery = 12;
  ScannerScreen::update(p, sky);
  ScannerScreen::draw(canvas, sky, st, 0.5f);  save("scan_hollow");

  // Nothing in range after a scan.
  sky.reset(0);
  sky.ingest(nets, 0, 2500);
  st.place = WalkFormat::Place::None;
  st.log = Status::Log::Off;
  st.battery = 64;
  ScannerScreen::update(p, sky);
  ScannerScreen::draw(canvas, sky, st, 1.1f);  save("scan_empty");

  // The walk summary after a mixed walk.
  sky.reset(0);
  now = 0;
  // A 12-minute stroll: the routers in range drift along a street, and
  // the street gets busier, then quieter.
  static FakeRouter street[160];
  makeRouters(street, 160);
  const int wardMix[] = {2, 7, 7, 7, 15, 15, 30, 7, 2, 2};
  for (int s = 0; s < 300; s++) {
    now += 2400;
    const int n = wardMix[(s / 30) % 10];
    const int from = s * 2 / 5;
    sky.ingest(nets, toNets(street + from, n, nets, 3), now);
  }
  st = status();
  st.nowMs = now;
  WalkScreen::draw(canvas, sky, st, 0.2f);  save("walk");

  // A sample walk log written by the firmware's own log code, so
  // tools/walk_report.py can be checked against the real format.
  mkdir("shots/sample", 0755);
  FILE* log = fopen("shots/sample/W0001.CSV", "w");
  char line[256];
  WalkFormat::header(line, sizeof(line), Config::Version, Keys::deckTag(), 1);
  fputs(line, log);
  makeRouters(r, 12);
  sky.reset(0);
  now = 0;
  WalkFormat::placeLine(line, sizeof(line), now, WalkFormat::Place::Home);
  fputs(line, log);
  int routerTotal = 0;
  for (int s = 1; s <= 100; s++) {
    now += 2400;
    const int n = toNets(r, 12, nets, 3);
    sky.ingest(nets, n, now);
    routerTotal += sky.heard();
    WalkFormat::scanLine(line, sizeof(line), now, s, WalkFormat::Place::Home, n, sky.heard(), 80);
    fputs(line, log);
    for (int i = 0; i < n; i++) {
      WalkFormat::netLine(line, sizeof(line), s, nets[i]);
      fputs(line, log);
    }
  }
  fclose(log);
  printf("sample walk: %d routers a scan, ward %s, energy", routerTotal / 100, wardName(sky.ward()));
  for (int e = 0; e < EnergyCount; e++) printf(" %s %d", energyName((Energy)e), sky.energyNear((Energy)e));
  printf("\n");
  motionShots();
  Play::run();
  fontAtlas();
  return 0;
}
