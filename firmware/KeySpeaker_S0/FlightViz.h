/*
  FlightViz — the level visualizer with a small fighter flying over it (screen-saver style, not a game)

  The level history is the terrain, drawn like an LED level meter: one bar per 267 ms, 6 px wide with a 2 px gap,
  newest on the right, scrolling left at 30 px/s. Each bar is a stack of 3 px segments with 1 px gaps, colored by
  height from blue at the bottom through green, yellow and orange to red at the top. The sky is black with
  slowly drifting stars, mostly white with a few yellow and pink ones, slower than the terrain for depth. A
  fighter (original pixel art in big 3 px dots, a sleek ship in the style of early-80s American arcade games) keeps to the left, drifts
  slowly forward and back and gently above the coming terrain, fires a missile straight ahead now and then, and drops bombs that fall in an arc
  and burst where they meet the terrain, carving a small crater into it. No enemies, no score, nothing to lose.

  Plain C++ on a 240 x 135 RGB565 frame buffer laid out like M5Canvas's (16-bit pixels, high byte first), so the same
  code also runs on a computer for previews.
*/
#pragma once
#include <stdint.h>
#include <math.h>

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
constexpr int DOT = 3;                  // the ship, missiles, bombs and bursts use big 3 x 3 px dots, like the terrain
constexpr int SHIP_DW = 9, SHIP_DH = 4;  // ship size in dots
constexpr int SHIP_W = SHIP_DW * DOT, SHIP_H = SHIP_DH * DOT;
constexpr float SHIP_X_MID = 40.0f, SHIP_X_SWING = 24.0f;  // drifts slowly forward and back between 16 and 64 px
constexpr float SHIP_EASE_S = 0.9f;     // how slowly the fighter follows the terrain
constexpr float SHIP_CLEAR = 26.0f;     // gap kept above the terrain ahead: room for the bombs' arc
constexpr uint32_t FIRE_MS_MIN = 2400, FIRE_MS_MAX = 4000;  // a missile straight ahead, now and then
constexpr float MISSILE_PX_S = 150.0f;
constexpr int MAX_MISSILES = 6;
constexpr uint32_t BOMB_MS_MIN = 3500, BOMB_MS_MAX = 6000;  // a bomb, falling in an arc
constexpr float BOMB_VX = 50.0f, BOMB_G = 140.0f;  // px/s forward when dropped, px/s^2 down
constexpr int MAX_BOMBS = 4;
constexpr float BURST_S = 0.45f;        // how long a burst lasts
constexpr int MAX_BURSTS = 4;
constexpr int NSTARS = 40;
constexpr float STAR_FAR_PX_S = 5.0f, STAR_NEAR_PX_S = 11.0f;  // small far stars and bigger near ones
constexpr int CRATER_SEG = 3;           // a bomb knocks this many segments off the bar it hits (1 off each neighbor)

// Original 9 x 4 dot ship facing right: tail fin, canopy, long pointed nose.
// W hull, B hull shade, M magenta trim, C canopy.
static const char* const SHIP[SHIP_DH] = {
  "M........",
  "MWWCC....",
  "WWWWWWWWW",
  ".BMMMMB..",
};

constexpr uint16_t rgb(int r, int g, int b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }
constexpr uint16_t swap(uint16_t c) { return (uint16_t)((c << 8) | (c >> 8)); }

struct Missile {
  float x = 0;
  int y = 0;
  bool on = false;
};

struct Bomb {
  float x = 0, y = 0, vx = 0, vy = 0;
  bool on = false;
};

struct Burst {
  float x = 0, y = 0, age = 0;  // x moves with the terrain
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
  float avgDb = 0;
  bool avgSet = false;
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
  for (auto& st : s.star) placeStar(s, st, (float)(nextRandom(s) % W));
  s.ready = true;
}

// Adds the newest column. rms is the average level of the last column (0 .. 32767).
static void push(State& s, float rms) {
  for (int i = COLS - 1; i > 0; i--) s.h[i] = s.h[i - 1];
  float db = rms > 1 ? 20.0f * log10f(rms / 32767.0f) : DB_SILENT;
  if (db <= DB_SILENT) {
    s.h[0] = GROUND;
    return;
  }
  if (!s.avgSet) {
    s.avgDb = db;
    s.avgSet = true;
  }
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

// Moves the fighter, missiles, bombs and bursts to nowMs.
static void update(State& s, uint32_t nowMs) {
  float dt = (nowMs - s.lastFrameMs) * 0.001f;
  if (dt > 0.2f) dt = 0.2f;
  s.lastFrameMs = nowMs;
  int off = scrollOffset(s, nowMs);
  int sx = shipX(nowMs);

  // Look at the terrain from the fighter to 60 px ahead, so it rises before a loud passage reaches it.
  int tallest = GROUND;
  for (int i = 0; i < COLS; i++) {
    int x = colX(i, off);
    if (x + BAR_W > sx && x < sx + SHIP_W + 60 && s.h[i] > tallest) tallest = s.h[i];
  }
  float target = H - tallest - SHIP_H - SHIP_CLEAR;
  if (target < TOP + 4) target = TOP + 4;
  if (target > H - GROUND - SHIP_H - SHIP_CLEAR) target = H - GROUND - SHIP_H - SHIP_CLEAR;
  s.shipY += (target - s.shipY) * (1.0f - expf(-dt / SHIP_EASE_S));
  int sy = (int)lrintf(s.shipY + bobY(nowMs));

  for (auto& m : s.m) {
    if (!m.on) continue;
    m.x += MISSILE_PX_S * dt;
    if (m.x >= W) m.on = false;
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
    x.x -= SCROLL_PX_S * dt;  // the burst stays where it hit
    if (x.age >= BURST_S || x.x < -8) x.on = false;
  }
  for (auto& st : s.star) {
    st.x -= (st.near ? STAR_NEAR_PX_S : STAR_FAR_PX_S) * dt;
    if (st.x < -2) placeStar(s, st, W + 1);
  }

  if ((int32_t)(nowMs - s.nextFireMs) >= 0) {
    s.nextFireMs = nowMs + interval(s, FIRE_MS_MIN, FIRE_MS_MAX);
    for (auto& m : s.m) {
      if (m.on) continue;
      m = {(float)(sx + SHIP_W), sy + 2 * DOT, true};  // from the nose (dot row 2)
      break;
    }
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
static void drawBurst(uint16_t* fb, const Burst& x) {
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

// Draws the whole frame (black sky with stars, terrain, bursts, missiles, bombs, fighter) into fb.
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

  static const uint16_t MIS_BODY = swap(rgb(255, 216, 0)), MIS_HEAD = swap(rgb(255, 255, 255));
  for (const auto& m : s.m) {
    if (!m.on) continue;
    int x = (int)m.x;
    dot(fb, x, m.y, MIS_BODY);
    dot(fb, x + DOT, m.y, MIS_HEAD);
  }

  static const uint16_t BOMB_BODY = swap(rgb(255, 255, 255));
  for (const auto& b : s.b)
    if (b.on) dot(fb, (int)b.x, (int)b.y, BOMB_BODY);

  static const uint16_t Wh = swap(rgb(236, 236, 236)), B = swap(rgb(120, 128, 160)), M = swap(rgb(224, 48, 200)),
                        C = swap(rgb(0, 224, 255)), F1 = swap(rgb(255, 120, 0)), F2 = swap(rgb(255, 240, 96));
  int sx = shipX(nowMs), sy = (int)lrintf(s.shipY + bobY(nowMs));
  for (int y = 0; y < SHIP_DH; y++)
    for (int x = 0; x < SHIP_DW; x++) {
      char c = SHIP[y][x];
      if (c == '.') continue;
      dot(fb, sx + x * DOT, sy + y * DOT, c == 'W' ? Wh : c == 'B' ? B : c == 'M' ? M : C);
    }
  // Engine flame behind the hull (dot row 2), flickering between one and two dots.
  dot(fb, sx - DOT, sy + 2 * DOT, F2);
  if ((nowMs / 90) & 1) dot(fb, sx - 2 * DOT, sy + 2 * DOT, F1);
}

}  // namespace flightviz
