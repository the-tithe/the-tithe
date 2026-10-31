// The Tithe: a roguelike for the Cardputer, played on the Deck, a street mage's
// terminal that hears the city's hidden currents.
// The WiFi routers around you are wells of energy; draw it, sell it, and pay
// each Night's Tithe. The game itself is in App.h and game/; this file owns
// the hardware: screen, keys, radio, motion sensor, sound, SD card and flash.
//
// Keys:
//   ; .      move up / down          , /   left / right
//   ENTER    open, choose            ESC (`)  back, and the menu
//   TAB      set where you are (for the walk log)
//   R        start / stop recording the walk to the SD card
//   G        the scrying glass       D     dowse the open well
//   B        your build              C     cast a spell
//   M        sound on / off
//
// Moves (Cardputer ADV): hold still to clear the glass and surface Hush wells;
// snap the wrist for a pulse; twist and back for a ward against the Wardens;
// lay it face-down to veil it; pick it up to wake it.
//
// USB serial console (115200): type "help". tools/pull_walks.py copies the
// walk logs over USB; tools/capture.py takes screenshots.

#include <M5Cardputer.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include "Config.h"
#include "App.h"
#include "Beacons.h"
#include "Capture.h"
#include "Effects.h"
#include "Imu.h"
#include "Input.h"
#include "Mic.h"
#include "Persist.h"
#include "SaveStore.h"
#include "Scanner.h"
#include "SDCard.h"
#include "Sightings.h"
#include "Status.h"
#include "WalkLog.h"

using WalkFormat::Place;

static M5Canvas canvas(&M5Cardputer.Display);
static Sightings sky;
static Status st;
static float clock_ = 0;
static uint32_t lastFrameUs = 0;
static uint32_t lastKeyMs = 0;
static bool dimmed = false;
static bool veiled = false;
static float pulseT = -1, wardT = -1, eyeT = -1;   // seconds since each effect started
static float ringLeft = 0;                         // a pulse makes Hush wells ring out
static bool saveDue = false;
static uint32_t saveAt = 0;
static Game::ClockNote bootNote{};   // routers' timers from before the Deck was switched off
static bool bootCheck = false;       // still to compare them with what's heard now
static uint32_t bootPlayMs = 0;      // the play clock the save came back with
static uint32_t nextNoteAt = 0;
constexpr uint32_t NoteEveryMs = 60000;

constexpr uint8_t Bright = 150;
constexpr uint8_t DimBright = 25;
constexpr uint32_t DimAfterMs = 90000;   // dim after 90 s without a key
constexpr float StowedAfter = 5.0f;      // or 5 s after it's put away
constexpr float VeilAfter = 1.0f;        // face-down this long veils it

static App::Ctx ctx() { return App::Ctx{sky, Imu::motion, st, Imu::available}; }

// ---- Hooks the game app calls -----------------------------------------------------

static void hookTone(int hz, int ms) {
  if (!Persist::muted && !veiled) M5Cardputer.Speaker.tone(hz, ms);
}

static void writeSave(bool backup) {
  SaveStore::save(App::game, millis(), backup);
  saveDue = false;
}

static void hookSave(bool now) {
  if (now) writeSave(App::game.phase != Game::Phase::Night);
  else if (!saveDue) { saveDue = true; saveAt = millis() + 3000; }
}

static void hookFocus(int ch) { Scanner::focusChannel = (uint8_t)ch; }
static void hookHurry() { Scanner::hurry(millis()); }

static void setPlace(Place p) {
  st.place = p;
  WalkLog::place(millis(), p);
}
static void hookPlace(int p) { setPlace((Place)p); }

static void startWalk() {
  WalkLog::start();
  char b[24];
  if (WalkLog::state == Status::Log::On) {
    snprintf(b, sizeof(b), "RECORDING W%lu", (unsigned long)WalkLog::walk);
    App::toast(b, UI::Rec);
  } else {
    App::toast("NO SD CARD", UI::Rec);
  }
}
static void stopWalk() {
  WalkLog::stop();
  App::toast("STOPPED RECORDING");
}
static void hookRecording() {
  if (WalkLog::state == Status::Log::On) stopWalk();
  else startWalk();
}
static void hookMuted(bool m) { Persist::setMuted(m); }
static uint32_t hookSeed() { return esp_random(); }

// ---- Scans ------------------------------------------------------------------------

static void onScan(uint32_t now) {
  if (Scanner::lastFocused) {   // a quick one-channel scan, for dowsing
    if (App::mode != App::Mode::Dowse) return;
    int best = -128;
    for (int i = 0; i < Scanner::count; i++) {
      uint8_t rb[5];
      WellRules::routerBytes(Scanner::nets[i].bssid, rb);
      if (Keys::deviceHash(rb, 5) == DowseScreen::target && Scanner::nets[i].rssi > best) best = Scanner::nets[i].rssi;
    }
    if (best > -128) {
      App::dowseSample(sky, Imu::motion.heading, best, now);
      if (!Persist::muted && !veiled) M5Cardputer.Speaker.tone(300 + (best + 95) * 18, 18);   // higher when stronger
    }
    return;
  }
  sky.ingest(Scanner::nets, Scanner::count, now);
  WalkLog::scan(now, sky.scans(), st.place, Scanner::nets, Scanner::count, sky.heard(), st.battery);
  App::afterScan(ctx());   // what the Deck remembers, and the wells' tunes
}

// ---- USB serial console ---------------------------------------------------------

static void printNet(const Net& n) {
  char b[96];
  WalkFormat::netLine(b, sizeof(b), sky.scans(), n);
  Serial.print(b);
}

static void runCommand(char* line) {
  char* cmd = strtok(line, " \t");
  if (!cmd) return;
  char* arg = strtok(nullptr, " \t");
  if (strcmp(cmd, "status") == 0) {
    Serial.printf("ok deck %04x fw %s walk %lu log %d scans %lu heard %d near %d ward %s place %s"
                  " battery %d heap %u scan_fails %lu mode %d\n",
                  Keys::deckTag(), Config::Version, (unsigned long)WalkLog::walk, (int)WalkLog::state,
                  (unsigned long)sky.scans(), sky.heard(), sky.near(), wardName(sky.ward()),
                  WalkFormat::placeName(st.place), st.battery, (unsigned)ESP.getFreeHeap(),
                  (unsigned long)Scanner::failures, (int)App::mode);
  } else if (strcmp(cmd, "game") == 0) {
    const Game::Run& r = App::game.run;
    Serial.printf("ok game runs %lu wins %lu active %d night %d resonance %.0f tithe %.0f visits %d/%d coin %.0f"
                  " relics %d heat %.1f phase %d atlas %u\n",
                  (unsigned long)App::game.career.runs, (unsigned long)App::game.career.wins, r.active, r.night + 1,
                  r.n.total, r.n.tithe, r.n.used, r.n.visits, r.coin, r.nRelics, r.heat, (int)App::game.phase,
                  (unsigned)App::game.atlas.count);
  } else if (strcmp(cmd, "scan") == 0) {
    for (int i = 0; i < Scanner::count; i++) printNet(Scanner::nets[i]);
    Serial.printf("ok scan %d networks\n", Scanner::count);
  } else if (strcmp(cmd, "wells") == 0) {
    static const WellRec* list[Sightings::MaxWells];
    const int n = sky.listed(list, Sightings::MaxWells);
    for (int i = 0; i < n; i++) {
      const WellRec& w = *list[i];
      Serial.printf("%08lx %-16s %-6s %-9s ch%-2u %4.0f dBm nets %u heard %u age %lu\n", (unsigned long)w.id,
                    w.name, energyName(w.energy), factionName(w.faction), w.channel, w.sig, w.nets, w.seen,
                    (unsigned long)sky.age(w));
    }
    Serial.printf("ok wells %d\n", n);
  } else if (strcmp(cmd, "place") == 0) {
    for (int p = 0; p < (int)Place::Count; p++) {
      if (arg && strcmp(arg, WalkFormat::placeName((Place)p)) == 0) {
        setPlace((Place)p);
        Serial.printf("ok place %s\n", arg);
        return;
      }
    }
    Serial.println("err places: none home street transit shop work park");
  } else if (strcmp(cmd, "log") == 0) {
    if (arg && strcmp(arg, "on") == 0) startWalk();
    else if (arg && strcmp(arg, "off") == 0) stopWalk();
    Serial.printf("ok log %d walk %lu\n", (int)WalkLog::state, (unsigned long)WalkLog::walk);
  } else if (strcmp(cmd, "sound") == 0) {
    Persist::setMuted(!(arg && strcmp(arg, "on") == 0));
    st.muted = Persist::muted;
    Serial.printf("ok sound %s\n", Persist::muted ? "off" : "on");
  } else if (strcmp(cmd, "files") == 0) {
    if (!SDCard::ensure()) { Serial.println("err no card"); return; }
    int n = 0;
    File dir = SD.open("/deck/walks");
    if (dir) {
      for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        const char* base = strrchr(f.name(), '/');
        Serial.printf("%s %lu\n", base ? base + 1 : f.name(), (unsigned long)f.size());
        f.close();
        n++;
      }
      dir.close();
    }
    Serial.printf("ok files %d\n", n);
  } else if (strcmp(cmd, "send") == 0) {
    // "FILE <name> <bytes>", then exactly that many bytes, then "ok send".
    unsigned long w = 0;
    char tail = 0;
    if (!arg || sscanf(arg, "W%lu.CSV%c", &w, &tail) != 1) { Serial.println("err usage: send W0001.CSV"); return; }
    if (!SDCard::ensure()) { Serial.println("err no card"); return; }
    char path[40];
    snprintf(path, sizeof(path), "/deck/walks/%s", arg);
    File f = SD.open(path, FILE_READ);
    if (!f) { Serial.println("err no such file"); return; }
    unsigned long left = f.size();
    Serial.printf("FILE %s %lu\n", arg, left);
    static uint8_t buf[1024];
    bool shortRead = false;
    while (left) {
      int n = f.read(buf, left < sizeof(buf) ? left : sizeof(buf));
      if (n <= 0) {   // keep the byte count promised, so the reader stays in step
        shortRead = true;
        memset(buf, '\n', sizeof(buf));
        n = left < sizeof(buf) ? left : sizeof(buf);
      }
      Serial.write(buf, n);
      left -= n;
    }
    f.close();
    Serial.flush();
    Serial.println(shortRead ? "\nerr short read" : "\nok send");
  } else if (strcmp(cmd, "imu") == 0) {
    if (!Imu::available) { Serial.println("err no motion sensor"); return; }
    const Motion& m = Imu::motion;
    Serial.printf("ok imu up %.2f %.2f %.2f heading %.0f yaw_rate %.0f still %.1f s steadiness %.2f"
                  " face_down %.1f s away %.1f s viewing %d tilt %.2f %.2f\n",
                  m.up[0], m.up[1], m.up[2], m.heading / Motion::DegToRad, m.yawRate / Motion::DegToRad,
                  m.stillFor, m.steadiness, m.faceDownFor, m.awayFor, m.viewing(), m.tiltX(), m.tiltY());
  } else if (strcmp(cmd, "cap") == 0) {
    Capture::lockstep = arg && strcmp(arg, "on") == 0;
    Capture::pending = 0;
    if (!Capture::lockstep) injectedKeys[0] = '\0';
    Serial.printf("ok cap %s\n", Capture::lockstep ? "on" : "off");
  } else if (strcmp(cmd, "keys") == 0) {
    strncpy(injectedKeys, arg ? arg : "", sizeof(injectedKeys) - 1);
    injectedKeys[sizeof(injectedKeys) - 1] = '\0';
    Serial.printf("ok keys %s\n", injectedKeys);
  } else if (strcmp(cmd, "step") == 0 || strcmp(cmd, "rec") == 0) {
    const int n = arg ? atoi(arg) : 1;
    if (!Capture::lockstep || n < 1) { Serial.println("err usage: cap on, then step|rec <frames>"); return; }
    Capture::pending = n;
    Capture::recording = cmd[0] == 'r';
  } else if (strcmp(cmd, "shot") == 0) {
    if (Capture::lastFrame) Capture::sendFrame(*Capture::lastFrame);
    Serial.println("ok shot");
  } else {
    Serial.println("err commands: status | game | scan | wells | place <where> | log on|off | sound on|off"
                   " | files | send <file> | imu | cap on|off | keys <chars> | step|rec <n> | shot");
  }
}

static void pollConsole() {
  static char line[64];
  static int len = 0;
  // In lockstep, stop reading once a step is queued so the next command
  // waits until those frames have run.
  while (Serial.available() > 0 && !(Capture::lockstep && Capture::pending > 0)) {
    const int c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (len == 0) continue;
      line[len] = '\0';
      len = 0;
      runCommand(line);
    } else if (len < (int)sizeof(line) - 1) {
      line[len++] = (char)c;
    }
  }
}

// ---- Routers' timers: how long the Deck was off (game/RealClock.h) ------------------

static bool clockDue(uint32_t now) {
  if (Scanner::running || Scanner::focusChannel) return false;
  if (bootCheck) return true;
  return (int32_t)(now - nextNoteAt) >= 0 && sky.scans() > 0 && !Scanner::lastFocused && Scanner::count > 0;
}

static void startClockListen(uint32_t now) {
  uint8_t ch[Game::ClockNote::Max + Scanner::MaxNets];
  int n = 0;
  if (bootCheck) {
    for (int i = 0; i < bootNote.n; i++) ch[n++] = bootNote.m[i].channel;
  } else {   // the channels of the strongest routers in the last scan
    int order[Scanner::MaxNets];
    for (int i = 0; i < Scanner::count; i++) order[i] = i;
    for (int a = 1; a < Scanner::count; a++)
      for (int b = a; b > 0 && Scanner::nets[order[b]].rssi > Scanner::nets[order[b - 1]].rssi; b--) {
        const int t = order[b]; order[b] = order[b - 1]; order[b - 1] = t;
      }
    for (int i = 0; i < Scanner::count; i++) ch[n++] = Scanner::nets[order[i]].channel;
  }
  if (!Beacons::start(ch, n, now)) {
    bootCheck = false;
    nextNoteAt = now + NoteEveryMs;
  }
}

static void logClock(const Game::ClockMark* m, int n, int64_t secs, uint32_t off, int settled) {
  if (!SDCard::ready) return;
  File f = SD.open("/deck/clock.log", FILE_APPEND);
  if (!f) return;
  f.printf("fw %s: note at play %lus, save at %lus, heard %d, off %lu min, settled %d, median %lds:", Config::Version,
           (unsigned long)(bootNote.playMs / 1000), (unsigned long)(bootPlayMs / 1000), n, (unsigned long)off, settled,
           (long)secs);
  for (int i = 0; i < bootNote.n; i++)
    for (int j = 0; j < n; j++)
      if (m[j].id == bootNote.m[i].id)
        f.printf(" %lds", m[j].tsf >= bootNote.m[i].tsf ? (long)((m[j].tsf - bootNote.m[i].tsf) / 1000000ULL) : -1L);
  f.print("\n");
  f.close();
}

static void onBeacons(uint32_t now) {
  Game::ClockMark m[Beacons::Max];
  const int n = Beacons::marks(m, Beacons::Max);
  if (bootCheck) {
    bootCheck = false;
    const int64_t secs = Game::secondsSince(bootNote, m, n);
    const uint32_t off = Game::offMinutes(bootNote, bootPlayMs, secs);
    const int settled = off ? Game::settleFor(App::game.atlas, off, App::game.playMs / 60000) : 0;
    logClock(m, n, secs, off, settled);
    if (off) {
      char b[32];
      if (off >= 60) snprintf(b, sizeof(b), "OFF FOR %luH %02luM", (unsigned long)(off / 60), (unsigned long)(off % 60));
      else snprintf(b, sizeof(b), "OFF FOR %lu MIN", (unsigned long)off);
      App::toast(b, 0, 3.0f);
      App::noteOff(off);
      if (settled) writeSave(false);
    }
  }
  if (n > 0) {
    Game::ClockNote note{};
    note.n = (uint8_t)(n < Game::ClockNote::Max ? n : Game::ClockNote::Max);
    for (int i = 0; i < note.n; i++) note.m[i] = m[i];
    note.playMs = App::game.playMs;
    note.seal();
    Persist::setClockNote(note);
  }
  nextNoteAt = now + NoteEveryMs;
}

// ---- Setup and the frame loop ---------------------------------------------------

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  Serial.setTxBufferSize(1024);   // room for a whole console reply
  M5Cardputer.begin(cfg, true);
  // Wipe the panel's whole memory once, not just the 240x135 we draw into:
  // rows just outside it can show whatever the launcher left there.
  if (auto* panel = M5Cardputer.Display.getPanel()) {
    const auto saved = panel->config();
    auto all = saved;
    all.offset_x = 0;
    all.offset_y = 0;
    all.panel_width = saved.memory_width;
    all.panel_height = saved.memory_height;
    panel->config(all);
    M5Cardputer.Display.setRotation(0);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    panel->config(saved);
  }
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(Bright);
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  canvas.setColorDepth(16);
  if (!canvas.createSprite(Config::ScreenW, Config::ScreenH)) Serial.println("[tithe] no memory for the screen");
  M5Cardputer.Speaker.setVolume(110);

  App::bootInfo.ears = Scanner::begin();
  Imu::begin();
  App::bootInfo.balance = Imu::available;
  Persist::begin();
  App::bootInfo.card = SDCard::ensure();
  if (App::bootInfo.card) WalkLog::start();
  else WalkLog::state = Status::Log::NoCard;
  sky.reset(millis());
  st.muted = Persist::muted;

  // The game: from flash, else the SD backup, else a new career.
  SaveStore::begin();
  Game::SaveFormat::Load why;
  const char* from = SaveStore::load(App::game, why);
  if (!from) App::newGame();
  bootPlayMs = App::game.playMs;
  bootCheck = Persist::clockNote(bootNote) && bootNote.n > 0;
  App::hooks.tone = hookTone;
  App::hooks.save = hookSave;
  App::hooks.listen = Mic::listen;
  App::hooks.focusChannel = hookFocus;
  App::hooks.hurry = hookHurry;
  App::hooks.setPlace = hookPlace;
  App::hooks.toggleRecording = hookRecording;
  App::hooks.setMuted = hookMuted;
  App::randomSeed = hookSeed;
  App::go(App::Mode::Boot);
  lastKeyMs = millis();
  lastFrameUs = micros();
  Serial.printf("[tithe] fw %s, deck %04x, radio %s, motion %s, card %s, walk %lu, game %s (%s), free heap %u\n",
                Config::Version, Keys::deckTag(), App::bootInfo.ears ? "ok" : "FAILED",
                Imu::available ? "ok" : "none", App::bootInfo.card ? "ok" : "none", (unsigned long)WalkLog::walk,
                from ? from : "new", Game::SaveFormat::loadText(why), (unsigned)ESP.getFreeHeap());
}

static void handleMotion(Press& p, uint32_t now) {
  const Motion& mo = Imu::motion;
  auto wake = [&](bool withEye) {
    dimmed = veiled = false;
    M5Cardputer.Display.setBrightness(Bright);
    lastKeyMs = now;
    if (withEye) eyeT = 0;
  };
  if (p.any) {
    lastKeyMs = now;
    if (dimmed || veiled) {   // the first key only wakes the screen
      wake(false);
      p = Press();
    }
  }
  if (Imu::available) {
    if (!veiled && mo.faceDownFor > VeilAfter) {   // face-down: veiled, still listening
      veiled = true;
      M5Cardputer.Display.setBrightness(0);
    } else if (veiled && !mo.faceDown()) {
      wake(true);
    } else if (dimmed && mo.lifted) {
      wake(true);
    } else if (!dimmed && !veiled && App::mode != App::Mode::Dowse && App::mode != App::Mode::Drawing &&
               mo.awayFor > StowedAfter && now - lastKeyMs > 5000) {
      dimmed = true;   // put away: dim at once
      M5Cardputer.Display.setBrightness(DimBright);
    }
    if (!veiled && !dimmed && App::mode != App::Mode::Boot) {
      if (mo.flick) {   // a pulse: listen again, and the Hush wells ring out
        pulseT = 0;
        ringLeft = 4;
        Scanner::hurry(now);
        hookTone(1568, 40);
        App::overhear(ctx(), esp_random(), true);
      }
      if (mo.twist) {   // a ward
        wardT = 0;
        hookTone(392, 120);
        App::twist(ctx());
      }
    }
  }
  if (!dimmed && !veiled && now - lastKeyMs > DimAfterMs && (!Imu::available || mo.steadiness > 0.8f)) {
    dimmed = true;
    M5Cardputer.Display.setBrightness(DimBright);
  }
}

void loop() {
  M5Cardputer.update();
  const uint32_t frameUs = micros();
  if (Capture::lockstep) {
    pollConsole();
    if (Capture::pending == 0) { delay(1); return; }
  }
  float dt = (frameUs - lastFrameUs) / 1e6f;
  lastFrameUs = frameUs;
  if (dt > 0.1f) dt = 0.1f;
  if (Capture::lockstep) dt = Capture::StepDt;
  clock_ += dt;

  const uint32_t now = millis();
  Imu::update(dt);
  if (Beacons::listening) {
    if (Beacons::poll(now)) onBeacons(now);
  } else if (clockDue(now)) {
    startClockListen(now);
  } else if (Scanner::poll(now)) {
    onScan(now);
  }
  if (!Capture::lockstep) pollConsole();

  static uint32_t batteryAt = 0;
  if (st.battery < 0 || now - batteryAt > 10000) {
    st.battery = M5Cardputer.Power.getBatteryLevel();
    batteryAt = now;
  }

  Press p = Input::poll();
  handleMotion(p, now);
  if (pulseT >= 0) pulseT += dt;
  if (wardT >= 0) wardT += dt;
  if (eyeT >= 0) eyeT += dt;
  if (ringLeft > 0) ringLeft -= dt;

  st.log = WalkLog::state;
  st.walk = WalkLog::walk;
  st.logBytes = WalkLog::bytes;
  st.listening = Scanner::running;
  st.nowMs = now;
  st.gyro = Imu::available;
  // Hush wells surface after a second of stillness, fully after three, or
  // for a few seconds after a pulse. Once surfaced they linger, fading over
  // HushLinger seconds, so pressing keys to pick one doesn't wipe it away.
  const Motion& mo = Imu::motion;
  constexpr float HushLinger = 8.0f;
  if (!Imu::available) {
    st.hushReveal = 1;
  } else {
    const float target = ringLeft > 0 ? 1 : mo.stillFor < 1 ? 0 : mo.stillFor > 3 ? 1 : (mo.stillFor - 1) / 2;
    const float faded = st.hushReveal - dt / HushLinger;
    st.hushReveal = target > faded ? target : faded < 0 ? 0 : faded;
  }

  const App::Ctx c = ctx();
  App::update(c, p, dt);
  App::draw(canvas, c, clock_);
  if (App::mode != App::Mode::Boot) {
    if (pulseT >= 0 && pulseT < Effects::PulseLen) Effects::pulse(canvas, pulseT);
    if (wardT >= 0 && wardT < Effects::WardLen) Effects::ward(canvas, wardT);
    if (eyeT >= 0 && eyeT < Effects::EyeLen) Effects::eye(canvas, eyeT);
  }
  canvas.pushSprite(0, 0);
  Capture::lastFrame = &canvas;

  if (saveDue && (int32_t)(now - saveAt) >= 0) writeSave(false);

  if (Capture::lockstep) {
    if (Capture::recording) Capture::sendFrame(canvas);
    if (--Capture::pending == 0) Serial.println("ok step");
    return;
  }
  const uint32_t spent = micros() - frameUs;
  if (spent < Config::FrameUs) delay((Config::FrameUs - spent) / 1000);
}
