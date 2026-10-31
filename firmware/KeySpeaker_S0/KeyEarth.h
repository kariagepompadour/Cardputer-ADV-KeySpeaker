/*
  KeyEarth.h — KeySpeaker (USB Speaker & BLE Keyboard): the slowly spinning Earth of the keyboard screen (S2-11)

  Design 8.3: 12 x 24 blocks of 6 x 3 px every 8 px across and 4 px down (the graphic EQ's and the terrain's blocks),
  about 96 px square at the top left. Sea blue, land green, polar regions white, darker toward the rim, seen from 20
  degrees north. The land flows from left to right, one turn a minute. Which latitude and longitude each block falls
  on is worked out once (begin()); a frame only shifts the longitude and looks the land up. "KEY BOARD" sits in white
  over the middle, with the Earth just around the letters at a third of its brightness. A star sky like the game screen's
  (mostly white, some yellow and pink, a few white ones twinkling) surrounds it; none behind the Earth.
  The land map is the mockup's coarse hand-made one, in 10-degree latitude bands.
*/
#pragma once
#include <math.h>
#include <stdint.h>

namespace KeyEarth {

constexpr int NX = 12, NY = 24, PX = 8, PY = 4, BW = 6, BH = 3;  // blocks, their pitch and size
constexpr int OX = 2, OY = 2;                                     // top left of the first block
constexpr int CX = 50, CY = 50;                                   // the Earth's centre on the screen
constexpr float TILT_DEG = 20.0f;                                 // seen from this latitude
constexpr uint32_t TURN_MS = 60000;                               // one turn a minute
constexpr int16_t LON_START = 135;                                // Japan in front at start
// The areas dimmed behind the letters (text size 2: 12 x 16 px a letter), 3 px around each line: a narrower one for
// "KEY" over a wider one for "BOARD", so the corners beside "KEY" stay bright (S2-12, the user's idea). Whole blocks
// are dimmed, those touching these areas, and the block at each outer corner (above "KEY", below "BOARD") is left
// out, so the dark part follows the blocks and its corners are rounded off (S2.5-5, the user's idea).
// While G0 holds the air mouse on, "MOUSE" takes their place on one line, dimmed the same way with all four corner
// blocks left out (S2.5-7, the user's idea).
struct Rect {
  int x0, y0, x1, y1;
  bool roundTop, roundBottom;  // leave out the top or the bottom corner blocks
  bool mouse;                  // behind "MOUSE", else behind "KEY BOARD"
};
// "BOARD" ends at CX + 30; its area stops short of the next block (from CX + 32), which held no letter (the user's eye).
static const Rect DIM[3] = {{CX - 21, CY - 20, CX + 21, CY, true, false, false},
                            {CX - 33, CY, CX + 32, CY + 20, false, true, false},
                            {CX - 33, CY - 11, CX + 32, CY + 9, true, true, true}};
constexpr int NSTARS = 16;  // the corners around the disc are small

// Land by latitude band (its centre, 85 to -85): longitude ranges, west to east.
struct LandRange {
  int8_t band;
  int16_t lon0, lon1;
};
static const LandRange LAND[] = {
    {85, -60, -20},
    {75, -120, -70}, {75, -60, -20}, {75, 15, 25}, {75, 50, 60}, {75, 80, 140},
    {65, -168, -60}, {65, -50, -25}, {65, -22, -14}, {65, 5, 180},
    {55, -135, -95}, {55, -80, -58}, {55, -8, -2}, {55, 8, 140}, {55, 155, 162},
    {45, -125, -60}, {45, -5, 145},
    {35, -120, -77}, {35, -10, 10}, {35, 35, 122}, {35, 130, 140},
    {25, -110, -97}, {25, -15, 57}, {25, 68, 120},
    {15, -92, -84}, {15, -17, 50}, {15, 74, 80}, {15, 98, 108},
    {5, -77, -50}, {5, -8, 48}, {5, 100, 118},
    {-5, -80, -35}, {-5, 10, 40}, {-5, 105, 115}, {-5, 120, 125}, {-5, 135, 150},
    {-15, -75, -40}, {-15, 12, 40}, {-15, 44, 50}, {-15, 125, 145},
    {-25, -70, -48}, {-25, 15, 33}, {-25, 44, 47}, {-25, 114, 153},
    {-35, -72, -57}, {-35, 18, 25}, {-35, 116, 140}, {-35, 172, 178},
    {-45, -75, -65}, {-45, 167, 172},
    {-55, -72, -66},
    {-65, -70, -55}, {-65, 40, 160},
    {-75, -150, -60}, {-75, -30, 180},
    {-85, -180, 180},
};

struct Cell {
  bool on = false;    // inside the disc
  bool polar = false;
  int8_t band = 0;    // the latitude band's centre
  int16_t lon = 0;    // longitude from the centre of the view, in degrees
  uint8_t shade = 0;  // brightness, 0 to 255
  bool dim = false;   // behind "KEY BOARD"
  bool dimM = false;  // behind "MOUSE"
};
static Cell cell[NY][NX];

struct Star {
  uint8_t x, y, kind;  // kind: 0 dim white, 1 white, 2 yellow, 3 pink
  bool near;           // near stars are 2 x 2 px
};
static Star star[NSTARS];
static int starCount = 0;

static inline uint16_t rgb565(int r, int g, int b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static bool isLand(int band, int lon) {
  for (const LandRange& r : LAND)
    if (r.band == band && r.lon0 <= lon && lon <= r.lon1) return true;
  return false;
}

static void begin() {
  const float c = cosf(TILT_DEG * (float)M_PI / 180.0f), s = sinf(TILT_DEG * (float)M_PI / 180.0f);
  for (int j = 0; j < NY; j++)
    for (int i = 0; i < NX; i++) {
      float x = (i + 0.5f - NX / 2.0f) / (NX / 2.0f), y = (NY / 2.0f - j - 0.5f) / (NY / 2.0f);
      float d = x * x + y * y;
      Cell& e = cell[j][i];
      if (d > 1) continue;
      float z = sqrtf(1 - d);
      float yy = y * c + z * s, zz = -y * s + z * c;  // tilted toward the viewer
      float lat = asinf(yy) * 180.0f / (float)M_PI;
      int band = (int)floorf(lat / 10.0f) * 10 + 5;
      e.on = true;
      e.polar = fabsf(lat) > 68.0f;
      e.band = (int8_t)(band > 85 ? 85 : band < -85 ? -85 : band);
      e.lon = (int16_t)lroundf(atan2f(x, zz) * 180.0f / (float)M_PI);
      e.shade = (uint8_t)((0.45f + 0.55f * z) * 255.0f);
    }
  // The blocks behind the letters: each one touching a dimmed area, less the outer corner blocks.
  for (const Rect& d : DIM) {
    int jTop = NY, jBottom = -1, iMin = NX, iMax = -1;
    for (int j = 0; j < NY; j++)
      for (int i = 0; i < NX; i++) {
        int x0 = OX + i * PX, y0 = OY + j * PY;
        if (x0 + BW <= d.x0 || x0 >= d.x1 || y0 + BH <= d.y0 || y0 >= d.y1) continue;
        (d.mouse ? cell[j][i].dimM : cell[j][i].dim) = true;
        if (j < jTop) jTop = j;
        if (j > jBottom) jBottom = j;
        if (i < iMin) iMin = i;
        if (i > iMax) iMax = i;
      }
    if (iMax < 0) continue;
    for (int j : {d.roundTop ? jTop : -1, d.roundBottom ? jBottom : -1}) {
      if (j < 0) continue;
      (d.mouse ? cell[j][iMin].dimM : cell[j][iMin].dim) = false;
      (d.mouse ? cell[j][iMax].dimM : cell[j][iMax].dim) = false;
    }
  }

  // Stars around the Earth only: in its square, off the disc (plus a margin).
  uint32_t rng = 3;
  starCount = 0;
  for (int tries = 0; tries < 400 && starCount < NSTARS; tries++) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    int x = (int)(rng % 101), y = (int)((rng >> 8) % 97), k = (int)((rng >> 16) % 10);
    bool near = (rng >> 20) % 4 == 0;
    if ((x - CX) * (x - CX) + (y - CY) * (y - CY) < 52 * 52) continue;
    star[starCount++] = {(uint8_t)x, (uint8_t)y, (uint8_t)(k < 4 ? 0 : k < 7 ? 1 : k < 9 ? 2 : 3), near};
  }
}

// Draws the stars, the Earth at the time nowMs, and "KEY BOARD" over it, or "MOUSE" while the air mouse is on.
template <typename Canvas>
static void draw(Canvas& g, uint32_t nowMs, uint16_t letters, bool mouse = false) {
  static const uint16_t STAR_C[4] = {rgb565(110, 110, 130), rgb565(235, 235, 255), rgb565(255, 220, 60),
                                     rgb565(255, 110, 200)};
  for (int n = 0; n < starCount; n++) {
    const Star& st = star[n];
    if (st.kind == 1 && ((nowMs / 250 + n * 7) % 23) == 0) continue;  // a short blink now and then
    g.fillRect(st.x, st.y, st.near ? 2 : 1, st.near ? 2 : 1, STAR_C[st.kind]);
  }

  // The view's centre moves west, so the land flows from left to right.
  int lon0 = LON_START - (int)((uint64_t)(nowMs % TURN_MS) * 360 / TURN_MS);
  for (int j = 0; j < NY; j++)
    for (int i = 0; i < NX; i++) {
      const Cell& e = cell[j][i];
      if (!e.on) continue;
      int r, gr, b;
      int lon = ((e.lon + lon0) % 360 + 540) % 360 - 180;
      if (e.polar) r = 230, gr = 240, b = 255;
      else if (isLand(e.band, lon)) r = 40, gr = 230, b = 70;
      else r = 20, gr = 90, b = 255;
      r = r * e.shade / 255, gr = gr * e.shade / 255, b = b * e.shade / 255;
      if (mouse ? e.dimM : e.dim) r /= 3, gr /= 3, b /= 3;  // behind the letters, a third of the brightness
      g.fillRect(OX + i * PX, OY + j * PY, BW, BH, rgb565(r, gr, b));
    }

  g.setTextSize(2);
  g.setTextColor(letters);
  if (mouse) {
    g.drawString("MOUSE", CX - 5 * 6, CY - 8);
  } else {
    g.drawString("KEY", CX - 3 * 6, CY - 17);
    g.drawString("BOARD", CX - 5 * 6, CY + 1);
  }
  g.setTextSize(1);
}

}  // namespace KeyEarth
