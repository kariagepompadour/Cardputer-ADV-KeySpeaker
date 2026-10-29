/*
  FlightViz — the level visualizer with a small fighter flying over it (screen-saver style, not a game)

  The level history is the terrain, drawn like an LED level meter: one bar per 267 ms, 6 px wide with a 2 px gap,
  newest on the right, scrolling left at 30 px/s. Each bar is a stack of 3 px segments with 1 px gaps, colored by
  height from blue at the bottom through green, yellow and orange to red at the top. The sky is black with
  slowly drifting stars, mostly white with a few yellow and pink ones, slower than the terrain for depth. A
  fighter (original pixel art in big 3 px dots, a sleek ship in the style of early-80s American arcade games) keeps to the left, drifts
  slowly forward and back and gently above the coming terrain, fires a laser beam straight ahead now and then, and drops bombs that fall in an arc
  and burst where they meet the terrain, carving a small crater into it. Beats in the music bring in small harmless
  targets in six kinds (walkers materialize out of dots drawn in from all around, the others fly in from the right) (original pixel art, each in the role of a classic arcade alien: a pink walker, a quick saucer,
  a red zigzagging mutant, a slow blue bomber that leaves blinking mines, and a spiky pod that splits into three fast
  swarmers when hit); the fighter darts forward, lines up and shoots them, and they pop in colored sparks that fly out to the screen edges. They shoot back
  now and then with slow bullets aimed at the fighter; the fighter weaves out of the way, and a bullet that still
  hits only lights up its shield for a moment. No score, nothing to lose.

  Plain C++ on a 240 x 135 RGB565 frame buffer laid out like M5Canvas's (16-bit pixels, high byte first), so the same
  code also runs on a computer for previews.
*/
#pragma once
#include <stdint.h>
#include <math.h>
#include <stdlib.h>

namespace flightviz {

constexpr int W = 240, H = 135;
constexpr int BAR_W = 6, PITCH = 8;
constexpr int COLS = W / PITCH + 2;     // +2: the partly visible columns at both edges
constexpr uint32_t COL_MS = 267;        // one new column every 267 ms: 30 px/s
constexpr int SEG_H = 3, SEG_PITCH = 4; // LED segments: 3 px lit, 1 px gap
constexpr float SCROLL_PX_S = PITCH * 1000.0f / COL_MS;  // 30 px/s
constexpr int TOP = 12;                 // the status line above is left free
constexpr int NSEG = 20;
constexpr int MAX_BAR = NSEG * SEG_PITCH;  // tallest bar (80 px); leaves the sky to fly and drop bombs in
constexpr int GROUND = SEG_PITCH;       // silence still lights the blue bottom row: the ground
// The bars show each column's level against the music's own recent average (avgDb, over about 3 s), on a narrow
// scale: most music changes only a few dB, so this is what makes hills and valleys. The average level sits a little
// below half height; SPAN_DB above or below it fills or empties the bar. Any volume and any source look alike.
constexpr float SPAN_DB = 8.0f;
constexpr float MID = 0.45f;            // bar height (share of full) at the average level
constexpr float AVG_RATE = 0.08f;       // share of the gap the average closes per column (about 3.3 s)
constexpr float DB_SILENT = -60.0f;     // below this (or muted) a column is just the ground
constexpr int DOT = 3;                  // the ship, targets, bombs and bursts use big 3 x 3 px dots, like the terrain
constexpr int SHIP_DW = 9, SHIP_DH = 4;  // ship size in dots
constexpr int SHIP_W = SHIP_DW * DOT, SHIP_H = SHIP_DH * DOT;
constexpr float SHIP_X_MID = 40.0f, SHIP_X_SWING = 24.0f;  // drifts slowly forward and back between 16 and 64 px
constexpr float SHIP_EASE_S = 0.9f;     // how slowly the fighter follows the terrain
constexpr float SHIP_CLEAR = 26.0f;     // gap kept above the terrain ahead: room for the bombs' arc
constexpr uint32_t FIRE_MS_MIN = 2400, FIRE_MS_MAX = 4000;  // a laser shot straight ahead, now and then
// Laser beam: the head streaks to the screen edge (or the first target in its way), the beam holds a moment, then
// the tail runs after the head until the beam is gone. Colored in bands, the tail end broken into dashes.
constexpr float LASER_PX_S = 900.0f, LASER_HOLD_S = 0.12f, LASER_TAIL_PX_S = 650.0f;
constexpr int LASER_H = 2;
constexpr int MAX_MISSILES = 6;
constexpr uint32_t BOMB_MS_MIN = 3500, BOMB_MS_MAX = 6000;  // a bomb, falling in an arc
constexpr float BOMB_VX = 50.0f, BOMB_G = 140.0f;  // px/s forward when dropped, px/s^2 down
constexpr int MAX_BOMBS = 4;
constexpr float BURST_S = 0.45f;        // how long a burst lasts
constexpr int MAX_BURSTS = 6;
constexpr float POP_S = 1.1f;           // a target's pop lasts longer: its sparks fly out to the screen edges
constexpr int POP_SPARKS = 16;
constexpr int NSTARS = 40;
constexpr float STAR_FAR_PX_S = 5.0f, STAR_NEAR_PX_S = 11.0f;  // small far stars and bigger near ones
constexpr int CRATER_SEG = 3;           // a bomb knocks this many segments off the bar it hits (1 off each neighbor)
// Targets: a column well above the running average and clearly louder than the one before is a beat, and a beat
// brings in a target from the right (not more often than SPAWN_MS_MIN). Music with few clear beats still brings one
// every SPAWN_MS_MAX; silence brings none.
constexpr float BEAT_DB = 1.5f, BEAT_RISE_DB = 1.0f;
constexpr uint32_t SPAWN_MS_MIN = 1600, SPAWN_MS_MAX = 4500;
constexpr float MAT_S = 0.7f, MAT_R = 46.0f;  // a target materializes from dots drawn in from this far, this long
constexpr int MAX_TARGETS = 3;          // on screen at once, not counting swarmers
constexpr int MAX_ENEMIES = 6;          // room for a pod's three swarmers
constexpr int SWARM_N = 3;
constexpr float MINE_MS = 800, MINE_S = 3.0f;  // a bomber leaves a mine this often; it blinks this long
constexpr int MAX_MINES = 8;
// Return fire: each target (not pods or swarmers) shoots every 2.2 to 4 s while it is ahead of the fighter.
constexpr uint32_t EFIRE_MS_MIN = 2200, EFIRE_MS_MAX = 4000;
constexpr float EBULLET_PX_S = 65.0f;
constexpr int MAX_EBULLETS = 6;
constexpr float DODGE_GAP = 3.0f;       // the fighter steps aside to clear a bullet about to hit by this much
constexpr float DODGE_LOOK_S = 0.9f;    // when a bullet will reach it within this time
constexpr float DODGE_EASE_S = 0.18f;
constexpr uint32_t SHIELD_MS = 350;     // how long the shield shows after a hit
constexpr float CHASE_EASE_S = 0.35f;   // the fighter lines up with a target faster than it follows the terrain
constexpr float CHASE_CLEAR = 6.0f;     // and may fly closer to the terrain while it does
constexpr float LUNGE_PX = 18.0f;       // and darts this far forward
constexpr float LUNGE_EASE_S = 0.6f;
constexpr uint32_t AIM_FIRE_MS = 450;   // shortest time between shots at a target
constexpr int ENGAGE_X = W - 80;        // the fighter holds fire until a target is this far in: time for a firefight

// Original 9 x 4 dot ship facing right: tail fin, canopy, long pointed nose.
// W hull, B hull shade, M magenta trim, C canopy.
static const char* const SHIP[SHIP_DH] = {
  "M........",
  "MWWCC....",
  "WWWWWWWWW",
  ".BMMMMB..",
};

// Original targets, 2 animation frames each (up to 4 rows). Colors by letter: P pink, E/W white, G green, C cyan,
// O orange, Y light yellow, R red, M magenta, Q blue, S spike yellow, V violet.
enum Kind : uint8_t { WALKER, SAUCER, MUTANT, BOMBER, POD, SWARMER, NKINDS };
struct KindInfo {
  uint8_t dw, dh;
  float pxS;             // leftward speed on screen
  float wave, waveHz;    // up-and-down wobble (px, rad/s)
  const char* art[2][4];
};
static const KindInfo KINDS[NKINDS] = {
  {5, 3, 18, 5, 2.2f, {{".PPP.", "PEPEP", "G.G.G"}, {".PPP.", "PEPEP", ".G.G."}}},
  {7, 2, 42, 2, 5.0f, {{"..CCC..", "OYOOOYO"}, {"..CCC..", "OOOYOOO"}}},
  {5, 4, 30, 10, 3.0f, {{"R...R", ".RRR.", "RYRYR", ".M.M."}, {".R.R.", ".RRR.", "RYRYR", "M.M.M"}}},
  {3, 3, 12, 8, 1.2f, {{"QQQ", "QYQ", "QQQ"}, {"QQQ", "QWQ", "QQQ"}}},
  {3, 3, 15, 4, 1.5f, {{".S.", "SVS", ".S."}, {"S.S", ".V.", "S.S"}}},
  {2, 1, 55, 4, 8.0f, {{"OR"}, {"RO"}}},
};

constexpr uint16_t rgb(int r, int g, int b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }
constexpr uint16_t swap(uint16_t c) { return (uint16_t)((c << 8) | (c >> 8)); }

struct Missile {  // a laser shot: from x (where it was fired) to head; the tail follows after LASER_HOLD_S
  float x = 0;
  int y = 0;
  bool on = false;
  float head = 0, age = 0;
  bool stopped = false;  // the head reached the edge or a target
};

struct Bomb {
  float x = 0, y = 0, vx = 0, vy = 0;
  bool on = false;
};

struct Burst {
  float x = 0, y = 0, age = 0;  // x moves with the terrain
  bool on = false;
  bool pop = false;             // a target popping in the air (stays put, colored sparks)
};

struct Enemy {
  float x = 0, y = 0, baseY = 0, phase = 0, mineMs = 0;
  uint8_t kind = WALKER;
  bool on = false;
  float fireMs = 0;  // time to its next shot
  float born = MAT_S;  // time since it began to materialize; below MAT_S it is still forming
};
static inline bool formed(const Enemy& e) { return e.born >= MAT_S; }

struct EBullet {  // a target's shot, flying straight at where the fighter was
  float x = 0, y = 0, vx = 0, vy = 0;
  bool on = false;
};
static inline int enemyW(const Enemy& e) { return KINDS[e.kind].dw * DOT; }
static inline int enemyH(const Enemy& e) { return KINDS[e.kind].dh * DOT; }

struct Mine {  // left behind by a bomber: a blinking dot that drifts with the terrain, harmless
  float x = 0, y = 0, age = 0;
  bool on = false;
};

struct Star {
  float x = 0;
  uint8_t y = 0, kind = 0;  // kind: 0 dim white, 1 white, 2 yellow, 3 pink
  bool near = false;        // near stars are 2 x 2 px and move faster
};

struct State {
  uint8_t h[COLS] = {};      // column heights; h[0] is the newest
  uint32_t lastColMs = 0;    // when h[0] was added
  uint32_t lastFrameMs = 0, nextFireMs = 0, nextBombMs = 0;
  uint32_t rng = 1;          // for the irregular firing intervals
  float shipY = H - GROUND - SHIP_H - SHIP_CLEAR;
  float avgDb = 0, prevDb = DB_SILENT;
  bool avgSet = false, beat = false;  // beat: set by push, used by update
  uint32_t nextSpawnMs = 0, nextAimMs = 0;
  float lunge = 0;           // how far the fighter has darted forward
  Enemy e[MAX_ENEMIES];
  Mine mine[MAX_MINES];
  EBullet eb[MAX_EBULLETS];
  uint32_t shieldUntil = 0;  // the shield shows until then
  bool dodging = false;      // stepping aside to dodgeTo until the bullet has passed
  float dodgeTo = 0;
  Missile m[MAX_MISSILES];
  Bomb b[MAX_BOMBS];
  Burst x[MAX_BURSTS];
  Star star[NSTARS];
  uint16_t lit[MAX_BAR];     // segment colors by height from the bottom (swapped for the buffer); 0 in the gaps
  bool ready = false;
};

// xorshift; nothing here needs good randomness.
static inline uint32_t nextRandom(State& s) {
  s.rng ^= s.rng << 13;
  s.rng ^= s.rng >> 17;
  s.rng ^= s.rng << 5;
  return s.rng;
}

// A new star at x: random height in the sky, mostly white, some yellow or pink, one in four near.
static inline void placeStar(State& s, Star& st, float x) {
  uint32_t r = nextRandom(s);
  st.x = x;
  st.y = (uint8_t)(TOP + r % (H - TOP - GROUND));
  uint32_t k = (r >> 8) % 10;
  st.kind = k < 4 ? 0 : k < 7 ? 1 : k < 9 ? 2 : 3;
  if ((r >> 16) % 10 == 0) st.kind = 3;  // pink a little more often than one in ten
  st.near = (r >> 20) % 4 == 0;
}

// Segment colors like an LED meter: blue, green, yellow, orange, red from the bottom, blended segment by segment.
static void init(State& s, uint32_t nowMs) {
  static const float STOP[5] = {0.0f, 0.3f, 0.6f, 0.8f, 1.0f};  // where each color sits (0 bottom .. 1 top)
  static const int COLOR[5][3] = {{40, 100, 255}, {40, 235, 90}, {245, 230, 40}, {255, 140, 0}, {255, 36, 30}};
  for (int y = 0; y < MAX_BAR; y++) {
    float t = (float)(y / SEG_PITCH) / (NSEG - 1);
    int i = 0;
    while (i < 3 && t > STOP[i + 1]) i++;
    float u = (t - STOP[i]) / (STOP[i + 1] - STOP[i]);
    int r = (int)(COLOR[i][0] + (COLOR[i + 1][0] - COLOR[i][0]) * u);
    int g = (int)(COLOR[i][1] + (COLOR[i + 1][1] - COLOR[i][1]) * u);
    int b = (int)(COLOR[i][2] + (COLOR[i + 1][2] - COLOR[i][2]) * u);
    bool gap = y % SEG_PITCH >= SEG_H;
    s.lit[y] = gap ? 0 : swap(rgb(r, g, b));
  }
  for (auto& v : s.h) v = GROUND;
  s.lastColMs = s.lastFrameMs = nowMs;
  s.rng = nowMs | 1;
  s.nextFireMs = nowMs + FIRE_MS_MIN;
  s.avgSet = false;
  s.nextBombMs = nowMs + BOMB_MS_MIN / 2;
  for (auto& m : s.m) m.on = false;
  for (auto& b : s.b) b.on = false;
  for (auto& x : s.x) x.on = false;
  for (auto& e : s.e) e.on = false;
  for (auto& m : s.mine) m.on = false;
  for (auto& b : s.eb) b.on = false;
  s.shieldUntil = nowMs;
  s.dodging = false;
  s.beat = false;
  s.lunge = 0;
  s.nextSpawnMs = s.nextAimMs = nowMs;
  for (auto& st : s.star) placeStar(s, st, (float)(nextRandom(s) % W));
  s.ready = true;
}

// Adds the newest column. rms is the average level of the last column (0 .. 32767).
static void push(State& s, float rms) {
  for (int i = COLS - 1; i > 0; i--) s.h[i] = s.h[i - 1];
  float db = rms > 1 ? 20.0f * log10f(rms / 32767.0f) : DB_SILENT;
  if (db <= DB_SILENT) {
    s.h[0] = GROUND;
    s.prevDb = DB_SILENT;
    return;
  }
  if (!s.avgSet) {
    s.avgDb = db;
    s.avgSet = true;
  }
  if (db - s.avgDb > BEAT_DB && db - s.prevDb > BEAT_RISE_DB) s.beat = true;
  s.prevDb = db;
  float t = MID + (db - s.avgDb) / SPAN_DB;
  s.avgDb += (db - s.avgDb) * AVG_RATE;
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  int n = 1 + (int)lrintf(t * (NSEG - 1));  // lit segments, at least the bottom row
  s.h[0] = (uint8_t)(n * SEG_PITCH);
}

// Left edge of column i: column 0 comes in from the right edge as the scroll advances.
static inline int colX(int i, int offset) { return W - offset - i * PITCH; }
static inline int scrollOffset(const State& s, uint32_t nowMs) {
  uint32_t d = nowMs - s.lastColMs;
  if (d > COL_MS) d = COL_MS;
  return (int)(d * PITCH / COL_MS);
}
// Column under screen x (the gap pixels count with their bar); -1 right of the newest column.
static inline int colAt(int x, int offset) {
  int d = W - offset - x;
  return d < 0 ? -1 : (d + PITCH - 1) / PITCH;
}
// Terrain height under screen column x.
static inline int heightAt(const State& s, int x, int offset) {
  int i = colAt(x, offset);
  if (i < 0) return 0;
  return i < COLS ? s.h[i] : GROUND;
}
// Knocks n segments off column i, keeping the ground row.
static inline void carve(State& s, int i, int n) {
  if (i < 0 || i >= COLS) return;
  int h = s.h[i] - n * SEG_PITCH;
  s.h[i] = (uint8_t)(h < GROUND ? GROUND : h);
}

// A random interval between lo and hi ms.
static inline uint32_t interval(State& s, uint32_t lo, uint32_t hi) { return lo + nextRandom(s) % (hi - lo + 1); }

// Slow drift forward and back: two slow waves, so the path does not repeat in an obvious way.
static inline int shipX(uint32_t nowMs) {
  float t = nowMs * 0.001f * 2.0f * (float)M_PI;
  return (int)lrintf(SHIP_X_MID + SHIP_X_SWING * (0.7f * sinf(t / 17.0f) + 0.3f * sinf(t / 6.3f)));
}

// Gentle up-and-down of the fighter, 1.5 px every 3.2 s.
static inline float bobY(uint32_t nowMs) { return 1.5f * sinf(nowMs * (2.0f * (float)M_PI / 3200.0f)); }

// A laser shot from the nose (the middle of dot row 2) of the fighter at sx, sy.
static inline void fireLaser(State& s, int sx, int sy) {
  for (auto& m : s.m) {
    if (m.on) continue;
    m = {(float)(sx + SHIP_W), sy + 2 * DOT + (DOT - LASER_H) / 2, true};
    m.head = m.x;
    break;
  }
}

// Moves the fighter, laser shots, targets, bombs and bursts to nowMs.
static void update(State& s, uint32_t nowMs) {
  float dt = (nowMs - s.lastFrameMs) * 0.001f;
  if (dt > 0.2f) dt = 0.2f;
  s.lastFrameMs = nowMs;
  int off = scrollOffset(s, nowMs);

  // The nearest target still ahead of the fighter, if any: the fighter darts forward and lines up with it.
  const Enemy* aim = nullptr;
  int baseX = shipX(nowMs);
  for (const auto& e : s.e)
    if (e.on && e.x > baseX + s.lunge + SHIP_W && e.x < W && (!aim || e.x < aim->x)) aim = &e;
  s.lunge += ((aim ? LUNGE_PX : 0.0f) - s.lunge) * (1.0f - expf(-dt / LUNGE_EASE_S));
  int sx = baseX + (int)lrintf(s.lunge);

  // Look at the terrain from the fighter to 60 px ahead, so it rises before a loud passage reaches it.
  int tallest = GROUND;
  for (int i = 0; i < COLS; i++) {
    int x = colX(i, off);
    if (x + BAR_W > sx && x < sx + SHIP_W + 60 && s.h[i] > tallest) tallest = s.h[i];
  }
  float target = H - tallest - SHIP_H - SHIP_CLEAR;
  if (target > H - GROUND - SHIP_H - SHIP_CLEAR) target = H - GROUND - SHIP_H - SHIP_CLEAR;
  float ease = SHIP_EASE_S;
  if (aim) {  // nose (dot row 2) to the target's middle, still clear of the terrain
    target = aim->y + enemyH(*aim) / 2 - (2 * DOT + DOT / 2);
    float low = H - tallest - SHIP_H - CHASE_CLEAR;
    if (target > low) target = low;
    ease = CHASE_EASE_S;
  }
  // A bullet about to hit: step aside, away from it, even off the line to the target.
  bool threat = false;
  for (const auto& b : s.eb) {
    if (!b.on || b.vx >= 0) continue;
    float t = (sx + SHIP_W - b.x) / b.vx;  // until it reaches the nose
    if ((sx - DOT - b.x) / b.vx < 0 || t > DODGE_LOOK_S) continue;  // gone past the tail, or still far off
    float y = b.y + b.vy * (t > 0 ? t : 0);
    if (!s.dodging && (y + DOT + 2 <= s.shipY || y - 2 >= s.shipY + SHIP_H)) continue;  // will miss anyway
    if (!s.dodging) {  // pick a side once (the nearer one with room), and keep to it until the bullet has passed
      float low = H - tallest - SHIP_H - CHASE_CLEAR;
      float above = y - SHIP_H - DODGE_GAP, below = y + DOT + DODGE_GAP;
      bool upOk = above >= TOP + 4, downOk = below <= low;
      bool down = downOk && (!upOk || below - s.shipY < s.shipY - above);
      s.dodgeTo = down ? below : upOk ? above : s.shipY;  // no room either way: take it on the shield
      s.dodging = true;
    }
    target = s.dodgeTo;
    ease = DODGE_EASE_S;
    threat = true;
    break;
  }
  if (!threat) s.dodging = false;
  if (target < TOP + 4) target = TOP + 4;
  s.shipY += (target - s.shipY) * (1.0f - expf(-dt / ease));
  int sy = (int)lrintf(s.shipY + bobY(nowMs));

  for (auto& m : s.m) {
    if (!m.on) continue;
    m.age += dt;
    float from = m.head;
    if (!m.stopped && (m.head += LASER_PX_S * dt) >= W) m.head = W, m.stopped = true;
    float tail = m.x + (m.age > LASER_HOLD_S ? (m.age - LASER_HOLD_S) * LASER_TAIL_PX_S : 0);
    if (tail >= m.head) {
      m.on = false;
      continue;
    }
    if (m.stopped && from >= m.head) continue;
    Enemy* hit = nullptr;  // the first formed target the head swept over this frame
    for (auto& e : s.e) {
      if (!e.on || !formed(e) || e.x + enemyW(e) <= from || e.x >= m.head) continue;
      if (m.y + LASER_H <= e.y || m.y >= e.y + enemyH(e)) continue;
      if (!hit || e.x < hit->x) hit = &e;
    }
    if (hit) {  // the beam ends at the target, which pops
      Enemy e = *hit;  // a copy: its slot may go to a swarmer below
      hit->on = false;
      m.head = e.x > m.x ? e.x : m.x + 1;
      m.stopped = true;
      for (auto& x : s.x) {
        if (x.on) continue;
        x = {e.x + enemyW(e) / 2, e.y + enemyH(e) / 2, 0, true, true};
        break;
      }
      if (e.kind == POD) {  // splits into swarmers, fanned out up and down
        int k = 0;
        for (auto& w : s.e) {
          if (w.on || k == SWARM_N) continue;
          w = {e.x, 0, e.y + (k - 1) * 12.0f, k * 0.7f, 0, SWARMER, true};
          w.y = w.baseY;
          k++;
        }
      }
    }
  }
  for (auto& e : s.e) {
    if (!e.on) continue;
    if (!formed(e)) {  // still materializing: stays put
      e.born += dt;
      continue;
    }
    const KindInfo& k = KINDS[e.kind];
    e.x -= k.pxS * dt;
    e.phase += dt;
    float w = sinf(e.phase * k.waveHz);
    if (e.kind == MUTANT) w = asinf(w) * (2.0f / (float)M_PI);  // zigzag
    e.y = e.baseY + k.wave * w;
    if (e.kind == BOMBER && (e.mineMs += dt * 1000.0f) >= MINE_MS) {
      e.mineMs = 0;
      for (auto& m : s.mine) {
        if (m.on) continue;
        m = {e.x + enemyW(e) / 2 - DOT / 2, e.y + enemyH(e), 0, true};
        break;
      }
    }
    float ground = H - heightAt(s, (int)e.x + enemyW(e) / 2, off);  // keep a little above the terrain
    if (e.y + enemyH(e) + 4 > ground) e.baseY -= e.y + enemyH(e) + 4 - ground, e.y = ground - enemyH(e) - 4;
    if (e.x < -enemyW(e)) e.on = false;
  }
  // Return fire, and the targets' bullets.
  for (auto& e : s.e) {
    if (!e.on || !formed(e) || e.kind == POD || e.kind == SWARMER || e.x > W - enemyW(e) || e.x < sx + SHIP_W + 30) continue;
    if ((e.fireMs -= dt * 1000.0f) > 0) continue;
    e.fireMs = (float)interval(s, EFIRE_MS_MIN, EFIRE_MS_MAX);
    float bx = e.x, by = e.y + enemyH(e) / 2;
    float dx = sx + SHIP_W / 2 - bx, dy = sy + SHIP_H / 2 - by, d = sqrtf(dx * dx + dy * dy);
    if (d < 1) continue;
    for (auto& b : s.eb) {
      if (b.on) continue;
      b = {bx, by, EBULLET_PX_S * dx / d, EBULLET_PX_S * dy / d, true};
      break;
    }
  }
  for (auto& b : s.eb) {
    if (!b.on) continue;
    b.x += b.vx * dt;
    b.y += b.vy * dt;
    if (b.x < -DOT || b.x >= W || b.y < TOP || b.y + DOT >= H - heightAt(s, (int)b.x, off)) {
      b.on = false;
    } else if (b.x + DOT > sx && b.x < sx + SHIP_W && b.y + DOT > sy && b.y < sy + SHIP_H) {
      b.on = false;  // a hit: the shield takes it
      s.shieldUntil = nowMs + SHIELD_MS;
    }
  }
  for (auto& m : s.mine) {
    if (!m.on) continue;
    m.age += dt;
    m.x -= SCROLL_PX_S * dt;
    if (m.age >= MINE_S || m.x < -DOT) m.on = false;
  }
  bool music = s.prevDb > DB_SILENT;
  if (s.beat || (music && (int32_t)(nowMs - s.nextSpawnMs) >= (int32_t)(SPAWN_MS_MAX - SPAWN_MS_MIN))) {
    s.beat = false;
    int targets = 0;
    for (const auto& e : s.e) targets += e.on && e.kind != SWARMER;
    if (targets < MAX_TARGETS && (int32_t)(nowMs - s.nextSpawnMs) >= 0) {
      for (auto& e : s.e) {
        if (e.on) continue;
        uint32_t r = nextRandom(s);
        uint32_t k = r % 20;  // walkers most often, then saucers; mutants, bombers and pods now and then
        e.kind = k < 7 ? WALKER : k < 11 ? SAUCER : k < 14 ? MUTANT : k < 17 ? BOMBER : POD;
        e.mineMs = 0;
        e.baseY = e.y = TOP + 4 + (float)((r >> 8) % 48);
        if (e.kind == WALKER) {  // walkers materialize in view; the others fly in from the right edge
          e.x = (float)(W - 30 - enemyW(e) - (int)((r >> 14) % 60));
          e.born = 0;
        } else {
          e.x = W;
          e.born = MAT_S;
        }
        e.phase = 0;
        e.fireMs = (float)interval(s, 400, 1200);
        e.on = true;
        s.nextSpawnMs = nowMs + SPAWN_MS_MIN;
        break;
      }
    }
  }
  for (auto& b : s.b) {
    if (!b.on) continue;
    b.x += b.vx * dt;
    b.y += b.vy * dt + 0.5f * BOMB_G * dt * dt;
    b.vy += BOMB_G * dt;
    int bx = (int)b.x;
    float ground = H - heightAt(s, bx, off);
    if (b.y + DOT - 1 >= ground) {  // burst on the terrain surface
      b.on = false;
      int i = colAt(bx + DOT / 2, off);
      carve(s, i, CRATER_SEG);
      carve(s, i - 1, 1);
      carve(s, i + 1, 1);
      for (auto& x : s.x) {
        if (x.on) continue;
        x = {(float)bx, ground - 1, 0, true};
        break;
      }
    } else if (bx >= W) {
      b.on = false;
    }
  }
  for (auto& x : s.x) {
    if (!x.on) continue;
    x.age += dt;
    if (!x.pop) x.x -= SCROLL_PX_S * dt;  // the burst stays where it hit
    if (x.age >= (x.pop ? POP_S : BURST_S) || x.x < -8) x.on = false;
  }
  for (auto& st : s.star) {
    st.x -= (st.near ? STAR_NEAR_PX_S : STAR_FAR_PX_S) * dt;
    if (st.x < -2) placeStar(s, st, W + 1);
  }

  if (aim) {  // lined up: shoot
    int nose = sy + 2 * DOT + DOT / 2, mid = (int)aim->y + enemyH(*aim) / 2;
    if (abs(nose - mid) <= DOT && aim->x < ENGAGE_X && formed(*aim) && (int32_t)(nowMs - s.nextAimMs) >= 0) {
      s.nextAimMs = nowMs + AIM_FIRE_MS;
      s.nextFireMs = nowMs + interval(s, FIRE_MS_MIN, FIRE_MS_MAX);
      fireLaser(s, sx, sy);
    }
  } else if ((int32_t)(nowMs - s.nextFireMs) >= 0) {
    s.nextFireMs = nowMs + interval(s, FIRE_MS_MIN, FIRE_MS_MAX);
    fireLaser(s, sx, sy);
  }
  if ((int32_t)(nowMs - s.nextBombMs) >= 0) {
    s.nextBombMs = nowMs + interval(s, BOMB_MS_MIN, BOMB_MS_MAX);
    for (auto& b : s.b) {
      if (b.on) continue;
      b = {(float)(sx + 3 * DOT), (float)(sy + SHIP_H), BOMB_VX, 0, true};  // from the belly
      break;
    }
  }
}

static inline void px(uint16_t* fb, int x, int y, uint16_t c) {
  if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) fb[y * W + x] = c;
}
// One big dot (DOT x DOT px) with its top-left corner at x, y.
static inline void dot(uint16_t* fb, int x, int y, uint16_t c) {
  for (int dy = 0; dy < DOT; dy++)
    for (int dx = 0; dx < DOT; dx++) px(fb, x + dx, y + dy, c);
}

// A small burst in big dots: a white flash, then sparks flying up and out, cooling from yellow to red.
static void drawPop(uint16_t* fb, const Burst& x);
static void drawBurst(uint16_t* fb, const Burst& x) {
  if (x.pop) return drawPop(fb, x);
  static const uint16_t WHITE = swap(rgb(255, 255, 255)), YELLOW = swap(rgb(255, 230, 64)),
                        ORANGE = swap(rgb(255, 140, 0)), RED = swap(rgb(220, 40, 24));
  static const int8_t DIR[5][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}};
  int cx = (int)lrintf(x.x) - DOT / 2, cy = (int)lrintf(x.y) - DOT + 1;  // the center dot sits on the surface
  float t = x.age / BURST_S;
  if (t < 0.25f) {
    dot(fb, cx, cy, WHITE);
    dot(fb, cx - DOT, cy, YELLOW);
    dot(fb, cx + DOT, cy, YELLOW);
    dot(fb, cx, cy - DOT, YELLOW);
  }
  uint16_t c = t < 0.35f ? YELLOW : t < 0.7f ? ORANGE : RED;
  int r = DOT + (int)(x.age * 30.0f);
  for (const auto& d : DIR) dot(fb, cx + d[0] * r, cy + d[1] * r, c);
}

// A target popping: a white flash, then sparks flying out in all directions to the screen edges, in party colors,
// big dots at first and smaller ones as they spread.
static void drawPop(uint16_t* fb, const Burst& x) {
  static const uint16_t WHITE = swap(rgb(255, 255, 255));
  static const uint16_t SPARK[4] = {swap(rgb(0, 224, 255)), swap(rgb(255, 80, 200)), swap(rgb(255, 230, 64)),
                                    swap(rgb(120, 255, 120))};
  int cx = (int)lrintf(x.x), cy = (int)lrintf(x.y);
  if (x.age < 0.12f) {
    dot(fb, cx - DOT / 2, cy - DOT / 2, WHITE);
    dot(fb, cx - DOT / 2 - DOT, cy - DOT / 2, WHITE);
    dot(fb, cx - DOT / 2 + DOT, cy - DOT / 2, WHITE);
    dot(fb, cx - DOT / 2, cy - DOT / 2 - DOT, WHITE);
    dot(fb, cx - DOT / 2, cy - DOT / 2 + DOT, WHITE);
  }
  int shift = (int)(x.age * 10.0f);
  for (int i = 0; i < POP_SPARKS; i++) {
    float a = i * (2.0f * (float)M_PI / POP_SPARKS) + ((i & 1) ? 0.2f : 0.0f);
    float r = DOT + x.age * ((i & 1) ? 230.0f : 150.0f);
    int px0 = cx + (int)(cosf(a) * r), py0 = cy + (int)(sinf(a) * r);
    uint16_t c = SPARK[(i + shift) & 3];
    if (x.age < 0.35f) {
      dot(fb, px0 - DOT / 2, py0 - DOT / 2, c);
    } else {
      px(fb, px0, py0, c);
      px(fb, px0 + 1, py0, c);
      px(fb, px0, py0 + 1, c);
      px(fb, px0 + 1, py0 + 1, c);
    }
  }
}

// Draws the whole frame (black sky with stars, terrain, bursts, targets, mines, their bullets, laser beams, bombs, fighter) into fb.
static void render(const State& s, uint16_t* fb, uint32_t nowMs) {
  for (int i = 0; i < W * H; i++) fb[i] = 0;
  int off = scrollOffset(s, nowMs);

  // Stars behind everything; a few of the white ones twinkle.
  static const uint16_t STAR_C[4] = {swap(rgb(110, 110, 130)), swap(rgb(235, 235, 255)), swap(rgb(255, 220, 60)),
                                     swap(rgb(255, 110, 200))};
  for (int i = 0; i < NSTARS; i++) {
    const Star& st = s.star[i];
    if (st.kind == 1 && ((nowMs / 150 + i * 7) % 23) == 0) continue;  // a short blink now and then
    int x = (int)st.x, y = st.y;
    px(fb, x, y, STAR_C[st.kind]);
    if (st.near) {
      px(fb, x + 1, y, STAR_C[st.kind]);
      px(fb, x, y + 1, STAR_C[st.kind]);
      px(fb, x + 1, y + 1, STAR_C[st.kind]);
    }
  }

  for (int i = 0; i < COLS; i++) {
    int x0 = colX(i, off), h = s.h[i];
    for (int dx = 0; dx < BAR_W; dx++) {
      int x = x0 + dx;
      if ((unsigned)x >= (unsigned)W) continue;
      for (int k = 0; k < h; k++) fb[(H - 1 - k) * W + x] = s.lit[k];  // the sky above stays black
    }
  }

  for (const auto& x : s.x)
    if (x.on) drawBurst(fb, x);

  static const uint16_t EP = swap(rgb(255, 96, 200)), EE = swap(rgb(255, 255, 255)), EG = swap(rgb(60, 220, 90)),
                        EC = swap(rgb(0, 224, 255)), EO = swap(rgb(255, 150, 30)), EY = swap(rgb(255, 250, 170));
  int frame = (nowMs / 250) & 1;
  static const uint16_t ER = swap(rgb(255, 50, 40)), EM = swap(rgb(224, 48, 200)), EQ = swap(rgb(70, 100, 255)),
                        ES = swap(rgb(255, 220, 40)), EV = swap(rgb(170, 80, 255));
  for (const auto& e : s.e) {
    if (!e.on) continue;
    const KindInfo& k = KINDS[e.kind];
    float u = formed(e) ? 1.0f : e.born / MAT_S;  // while forming, each dot flies in from its own direction
    float spread = MAT_R * (1.0f - u) * (1.0f - u);
    int idx = &e - s.e;
    for (int y = 0; y < k.dh; y++)
      for (int x = 0; x < k.dw; x++) {
        char c = k.art[frame][y][x];
        if (c == '.') continue;
        int ox = 0, oy = 0;
        if (spread > 0) {
          uint32_t h = (uint32_t)(idx * 97 + y * 31 + x * 17) * 2654435761u;
          float a = (h >> 8) * (2.0f * (float)M_PI / 16777216.0f);
          ox = (int)(cosf(a) * spread);
          oy = (int)(sinf(a) * spread);
        }
        uint16_t col = c == 'P' ? EP : c == 'E' || c == 'W' ? EE : c == 'G' ? EG : c == 'C' ? EC : c == 'O' ? EO
                     : c == 'R' ? ER : c == 'M' ? EM : c == 'Q' ? EQ : c == 'S' ? ES : c == 'V' ? EV : EY;
        dot(fb, (int)e.x + x * DOT + ox, (int)e.y + y * DOT + oy, col);
      }
  }
  for (int i = 0; i < MAX_MINES; i++) {
    const Mine& m = s.mine[i];
    if (m.on) dot(fb, (int)m.x, (int)m.y, ((nowMs / 120 + i) & 1) ? ER : EE);
  }

  static const uint16_t EB_A = swap(rgb(255, 120, 40)), EB_B = swap(rgb(255, 255, 200));
  for (int i = 0; i < MAX_EBULLETS; i++) {
    const EBullet& b = s.eb[i];
    if (b.on) dot(fb, (int)b.x, (int)b.y, ((nowMs / 80 + i) & 1) ? EB_A : EB_B);
  }

  // Laser beams: bands of color running along the beam, a white head, the last stretch before the tail in dashes.
  static const uint16_t BEAM[5] = {swap(rgb(255, 255, 255)), swap(rgb(0, 224, 255)), swap(rgb(255, 80, 200)),
                                   swap(rgb(255, 230, 64)), swap(rgb(120, 255, 120))};
  for (const auto& m : s.m) {
    if (!m.on) continue;
    int tail = (int)(m.x + (m.age > LASER_HOLD_S ? (m.age - LASER_HOLD_S) * LASER_TAIL_PX_S : 0));
    int head = (int)m.head, len = head - tail;
    for (int x = tail; x < head; x++) {
      int d = x - tail;
      if (d < len / 3 && ((x >> 2) & 1)) continue;  // dashes toward the tail
      uint16_t c = head - x < 6 ? BEAM[0] : BEAM[1 + (((x - (int)m.x) / 8 + (int)(m.age * 30.0f)) & 3)];
      for (int y = 0; y < LASER_H; y++) px(fb, x, m.y + y, c);
    }
  }

  static const uint16_t BOMB_BODY = swap(rgb(255, 255, 255));
  for (const auto& b : s.b)
    if (b.on) dot(fb, (int)b.x, (int)b.y, BOMB_BODY);

  static const uint16_t Wh = swap(rgb(236, 236, 236)), B = swap(rgb(120, 128, 160)), M = swap(rgb(224, 48, 200)),
                        C = swap(rgb(0, 224, 255)), F1 = swap(rgb(255, 120, 0)), F2 = swap(rgb(255, 240, 96));
  int sx = shipX(nowMs) + (int)lrintf(s.lunge), sy = (int)lrintf(s.shipY + bobY(nowMs));
  for (int y = 0; y < SHIP_DH; y++)
    for (int x = 0; x < SHIP_DW; x++) {
      char c = SHIP[y][x];
      if (c == '.') continue;
      dot(fb, sx + x * DOT, sy + y * DOT, c == 'W' ? Wh : c == 'B' ? B : c == 'M' ? M : C);
    }
  // Shield after a hit: a bright arc of dots around the nose, flickering.
  if ((int32_t)(s.shieldUntil - nowMs) > 0 && ((nowMs / 50) & 1)) {
    static const uint16_t SH = swap(rgb(120, 240, 255));
    int fx = sx + SHIP_W + DOT;
    for (int y = -1; y <= SHIP_DH; y++) dot(fb, fx + (y < 0 || y == SHIP_DH ? -DOT : 0), sy + y * DOT, SH);
    dot(fb, sx + SHIP_W / 2, sy - 2 * DOT, SH);
    dot(fb, sx + SHIP_W / 2, sy + SHIP_H + DOT, SH);
  }
  // Engine flame behind the hull (dot row 2), flickering between one and two dots.
  dot(fb, sx - DOT, sy + 2 * DOT, F2);
  if ((nowMs / 90) & 1) dot(fb, sx - 2 * DOT, sy + 2 * DOT, F1);
}

}  // namespace flightviz
