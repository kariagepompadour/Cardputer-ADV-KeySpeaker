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

  Now and then a graphic equalizer like an old hi-fi's joins in front of the terrain, which keeps flowing behind
  it: 30 bars for 30 frequency bands, bass on the left, up to half the terrain's height, jumping with the music at
  the frame rate, with white peak dots that hold and fall slowly. Three patterns take turns every minute, and when
  the music comes back after a gap between songs: the terrain alone, the equalizer in the aqua of an old hi-fi's
  fluorescent display, and the equalizer in KariPom's colors (each bar its own color, bass to treble). The
  equalizer rises from, and sinks into, the ground. The spectrum comes from a 512-point FFT of the last samples, done here at drawing time; the audio side only
  copies samples into a small ring.

  A space monster (the beast from the Space Invaders cabinet art, black and red, lit from the left, with glowing
  yellow eyes) visits now and then, only while music plays: it walks in from the right with the terrain, behind the
  bars, reaches for the fighter with a clawed hand that is drawn in front of everything, misses as the fighter slips
  away, and walks off (S9-1). Each reach comes from a different side (from the right, over the top, from below, or
  swinging past the fighter and hooking back), the other arm helps with a pose of its own, and about four visits in ten let
  the fighter pass first and then reach back after it with the far arm (S9-2). In S9-3 every visit plays out
  differently: it may reach one to three times, may first make itself big (arms up, or one up and one down), now and then only
  bluffs, and after a miss it waits a second or more and only sometimes does more; it comes in six colours (red, blue, green,
  purple, gold, ghost white), about half as often as before, and its arms are drawn in the same 3 px dots as the body.

  A meteor zone comes every few minutes while music plays (S13-1): the targets, the monsters and the searchlights fall behind, the
  terrain sinks low but keeps moving with the music, and meteors stream from right to left while the fighter weaves through them
  without firing. The louder the music, the more meteors, and a beat brings a big one. Each meteor is placed to miss the fighter's
  path, so the fighter always slips past.

  A damaged city comes now and then (S14-1): three to five buildings side by side flow by with the terrain (they do not react to the
  sound), small people wait on the roofs and lean out of windows, waving for help, and the fighter flies to the nearest one and saves
  them by overlapping them (a small light, no score). A few people run out of the doorways after the fighter and wave to it. The
  buildings are in front of the monsters' arms, the fighter in front of the buildings, the arms in front of the fighter, and all of it
  behind the equalizer. The searchlight's base is a T of four cells with a yellow lamp, and once in twenty visits a monster comes in
  flat shocking pink. Tried in the web demo first.

  S15-1, three things from the web demo. A third monster, an octopus (a tall egg of a head, two small slanted eyes, eight tentacles that lie
  far out along the ground), so each visit is the ape, the squid or the octopus, a third each. A building with somebody at a window has nobody
  on its roof, and one to three people run out at once. And a rare boss of the meteor zone, a wedge-shaped capital ship as long as the screen is
  wide (about one zone in five, by lot): it crosses the sky slowly during the whole zone, behind the meteors and the terrain, fires volleys of
  two or three green bolts straight at the fighter (which slips away from them; a hit only lights its shield), and when big meteors strike
  it, it smokes and finally burns, but flies on. Not while a monster is there. S15-2: about halfway across, its underside glows red, then a barrage of twelve
  missiles leaves it in a fan; they curve toward the fighter trailing smoke (once in a pass; the ordinary volleys pause meanwhile).

  A desert zone comes rarely while music plays (S17-1): the terrain sinks to low dunes the colour of sand (the equalizer turns to sand
  too), SOUND TERRARIUM's moon face rises, and pyramids and a sphinx flow by with the ground under the searchlights, which come more
  often. The pyramids breathe with the music, each a little after the one before; a beat lights their golden capstones and the
  sphinx's eyes. Somebody by the pyramids waves to the fighter. No monsters, no city, no bombs. Designed in the web demo first.

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
// Graphic-equalizer terrain. 512-point FFT at 48 kHz: 93.75 Hz per bin; bands log spaced from bin 1 to bin 170
// (about 94 Hz to 16 kHz). Each band is drawn against its own average of the last few seconds, like the terrain
// against the music's: the average sits a little below the middle, EQ_SPAN_DB above or below it fills or empties
// the bar. So every band dances, whatever the music's tone balance.
constexpr int FFT_N = 512, FFT_LOG2 = 9;
constexpr int EQ_BANDS = W / PITCH;     // 30
constexpr int EQ_BIN_LO = 1, EQ_BIN_HI = 170;
constexpr float EQ_SPAN_DB = 12.0f, EQ_MID = 0.4f, EQ_AVG_S = 2.5f;
constexpr int EQ_NSEG = NSEG / 2;       // equalizer bars reach half the terrain's full height
constexpr int EQ_AQUA_R = 60, EQ_AQUA_G = 255, EQ_AQUA_B = 210;  // a fluorescent display's aqua
constexpr float EQ_FALL_SEG_S = 28.0f;  // bars jump up at once and fall at this speed
constexpr float PEAK_HOLD_S = 0.5f, PEAK_FALL_SEG_S = 9.0f;
constexpr float EQ_SILENT_RMS = 33.0f;  // below about -60 dBFS the bars rest on the ground
constexpr uint32_t MODE_MS = 60000, MODE_MIN_MS = 15000;  // turns: every minute, or after a gap if 15 s have passed
constexpr uint32_t GAP_MS = 1500;       // silence at least this long counts as a gap between songs
constexpr float FADE_S = 0.45f;         // the equalizer rises from, or sinks into, the ground in this long
constexpr int CRATER_SEG = 3;           // a bomb knocks this many segments off the bar it hits (1 off each neighbor)
// Targets: a column well above the running average and clearly louder than the one before is a beat, and a beat
// brings in a target from the right (not more often than SPAWN_MS_MIN). Music with few clear beats still brings one
// every SPAWN_MS_MAX; silence one every SPAWN_MS_QUIET, so the screen still plays like a game's demo (S5-15).
constexpr float BEAT_DB = 1.5f, BEAT_RISE_DB = 1.0f;
constexpr uint32_t SPAWN_MS_MIN = 1600, SPAWN_MS_MAX = 4500, SPAWN_MS_QUIET = 6000;
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

// The space monster (S9-1): the beast from the Space Invaders cabinet art, 69 x 90 px, feet on the bottom edge. It walks
// in from the right with the terrain, only while music plays, reaches for the fighter, misses (the fighter slips away),
// draws its arm back and walks off. The body is behind the terrain, both arms in front of everything.
constexpr int KONG_W = 23, KONG_H = 30, KW = KONG_W * DOT, KH = KONG_H * DOT;
enum KongPhase : uint8_t { K_IDLE, K_WALK, K_REACH, K_GRAB, K_MISS, K_LEAVE, K_THREAT };
constexpr float K_REACH_S = 0.8f, K_GRAB_S = 0.22f, K_MISS_S = 0.45f, K_RETRACT_S = 0.5f;
constexpr float K_THREAT_S = 1.3f;      // making itself big (S9-3): arms up, or one up and one down
constexpr float K_GAP_MIN_S = 1.0f, K_GAP_VAR_S = 1.2f;  // after a miss it waits this long (1.0 to 2.2 s) before it maybe does more
constexpr float K_RETRY_R = 55.0f;      // and goes for the fighter again only if it is still this close to a shoulder
constexpr int K_NOTICE_DX = 84;         // it notices the fighter, and may make itself big, this far ahead
constexpr float K_ARM = 26.0f;          // two links of this length: a short, thick arm (reach about 51 px)
constexpr float K_HANG = 44.0f;         // where the hand hangs below the shoulder
constexpr int K_START_DX = 44;          // it reaches when the fighter is this close
constexpr int K_CHASE_DX = 36;          // a chasing monster reaches back while the fighter is this close behind its far shoulder
constexpr uint32_t K_CHASE_PERCENT = 40;  // how many visits are chases
constexpr float K_EVADE_PX = 17.0f;     // how far the fighter slips away as the fist closes
constexpr uint32_t KONG_MS_MIN = 24000, KONG_MS_MAX = 56000;  // between two visits: about half as often as in S9-2
constexpr uint32_t KONG_FIRST_MS = 10000;    // the first visit, after the start
constexpr uint32_t KONG_MUSIC_MS = 3000;     // it appears only after this long of music,
constexpr uint32_t KONG_GAP_MS = 2000;       // and a gap in the music up to this long does not count as silence
// The silhouette: per row, up to two spans of columns {from, to, from, to} (-1: none). Horn ears, a small head sunk
// between high shoulders, a broad chest, bow legs with big feet.
static const int8_t KONG_SPAN[KONG_H][4] = {
  {6, 6, 16, 16}, {6, 7, 15, 16}, {6, 16, -1, -1}, {7, 15, -1, -1}, {7, 15, -1, -1},
  {2, 20, -1, -1}, {1, 21, -1, -1}, {0, 22, -1, -1}, {0, 22, -1, -1}, {0, 22, -1, -1}, {0, 22, -1, -1},
  {1, 21, -1, -1}, {1, 21, -1, -1}, {2, 20, -1, -1}, {3, 19, -1, -1}, {4, 18, -1, -1},
  {5, 17, -1, -1}, {5, 17, -1, -1}, {5, 17, -1, -1}, {5, 17, -1, -1},
  {4, 10, 12, 18}, {3, 10, 12, 19}, {2, 9, 13, 20}, {2, 9, 13, 20}, {2, 9, 13, 20},
  {3, 9, 13, 19}, {3, 9, 13, 19}, {3, 9, 13, 19}, {2, 9, 13, 20}, {1, 9, 13, 21},
};

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

// Searchlights (S11-2): a city on alert. They stand on the ground, each with a small base, and flow from right to left with the terrain,
// sweeping the sky with a slim beam that widens and fades toward the top. Added to the light already there; in front of the monsters'
// bodies and the terrain, behind the equalizer. A beam stops at the monster's centre line and on the fighter. Only while music plays
// (the ones already out carry on). Designed in the web demo first.
constexpr int MAX_LIGHTS = 3;
constexpr uint32_t LIGHT_MS_MIN = 10000, LIGHT_MS_MAX = 35000;  // between two new ones
constexpr uint32_t LIGHT_FIRST_MS = 6000;
constexpr int LIGHT_LEN = 150;          // px
struct Light {
  bool on = false;
  float x = 0;                          // the base's middle, px
  float base = 0, amp = 0, w = 0, ph = 0, a = 0;  // the beam's angle from straight up (rad): base + amp * sin(t * w + ph), now a
};

// The meteor zone (S13-1). See the top of this file.
constexpr float MZ_IN_S = 1.5f, MZ_RUN_S = 17.0f, MZ_OUT_S = 2.0f;
constexpr uint32_t MZ_FIRST_MS = 90000, MZ_MS_MIN = 120000, MZ_MS_MAX = 240000;
constexpr int MZ_SEG = 7;               // the tallest the terrain gets in the zone, in segments
constexpr float MZ_WARP_PX_S = 260.0f;  // what was on screen falls behind this fast
constexpr int MZ_MAX = 40, MZ_DUST_MAX = 18;
constexpr float MZ_RATE_MIN = 3.0f, MZ_RATE_MAX = 12.0f, MZ_DUST_RATE = 14.0f;  // per second, quiet to loud
constexpr float MZ_GAP = 8.0f;          // px kept between a meteor and the fighter's path
struct MeteorSize { int r, vMin, vMax, tail; };  // radius px, speed px/s, tail in dots
static const MeteorSize MZ_SIZE[3] = {{2, 170, 240, 6}, {4, 220, 300, 10}, {6, 280, 360, 15}};
struct Meteor {
  bool on = false, warm = false;
  float x = 0, y = 0, v = 0, vy = 0;
  uint8_t size = 0;
};
struct Dust {  // thin streaks far behind, only for the sense of speed
  bool on = false;
  float x = 0, y = 0, v = 0;
  uint8_t len = 0;
};
struct MeteorZone {
  uint8_t ph = 0;                       // 0 none, 1 coming in, 2 running, 3 going out
  float t = 0, u = 0, acc = 0, dustAcc = 0, ph0 = 0;  // u: 0 normal world ... 1 full zone
  uint32_t nextMs = 0, startMs = 0;
};

// The desert zone (S17-1). See the top of this file.
constexpr float DZ_IN_S = 2.0f, DZ_OUT_S = 2.0f, DZ_EASE_S = 0.18f;
constexpr uint32_t DZ_FIRST_MS = 300000, DZ_MS_MIN = 480000, DZ_MS_MAX = 900000;
constexpr int DZ_SEG = 4;               // the dunes' tallest, in segments
// The sphinx, lying with its paws to the left and its face turned toward us: L moonlit stone, S shade, D dark lines, E the two eyes,
// N the striped headdress falling on both sides of the face (gold and blue rows).
constexpr int SPHINX_W = 31, SPHINX_H = 13;
static const char* const SPHINX[SPHINX_H] = {
  ".....NNNNN.....................",
  "....NNNNNNN....................",
  "...NNNLLLNNN...................",
  "...NNLLLLLNN...................",
  "..NNLELLLELNN..................",
  "..NNLLLDLLLNN..................",
  "..NNLLDDDLLNN..................",
  "..NNNLLLLLNNNSSSSSSSSSSS.......",
  "..NNNLLLLLNNNLLLLLLLLLLLLSS....",
  "...NNLLLLLNNLLLLLLLLLLLLLLLS...",
  "....SLLLLLLLLLLLLLLLLLLLLLLLLS.",
  "LLLLLLLLLLLLSLLLLLLLLLLLLLLLLS.",
  "LDLDLLLLLLLLSLLLLLLLLLLLLLLLLLS"};
// The monuments, left to right: ox px from where the first one enters; kind 0 a pyramid (max: its tallest, in dots), 1 the sphinx,
// 2 somebody waving to the fighter.
struct DzItem { int16_t ox; uint8_t kind, max; };
constexpr int DZ_N = 7;
static const DzItem DZ_LAYOUT[DZ_N] = {{0, 0, 13}, {64, 0, 19}, {120, 2, 0}, {156, 0, 10}, {250, 1, 0}, {440, 0, 12}, {494, 0, 8}};
constexpr int DZ_LEN = 494 + 8 * DOT + 4;   // px from the first monument's middle to past the last one's right edge
struct DesertZone {
  uint8_t ph = 0;                       // 0 none, 1 coming in, 2 running, 3 going out
  float t = 0, u = 0, x = 0, glow = 0;  // u: 0 normal world ... 1 full zone; x: where the layout starts (px); glow: a beat, fading
  float hd[DZ_N] = {};                  // each pyramid's height now (dots)
  uint32_t nextMs = 0;
};

// The cruiser (S15-1): a wedge-shaped capital ship as long as the screen is wide, a rare boss of the meteor zone. See the top of this file.
constexpr int CR_W = 80, CR_H = 25, CR_Y = 8, CR_TRAIL = 34;           // dots, dots, px from the top, px of engine trail behind it
constexpr float CR_SPEED = (W + 2 + CR_W * 3 + CR_TRAIL) / (MZ_IN_S + MZ_RUN_S + MZ_OUT_S);   // px/s: it takes the whole zone to cross
constexpr int CR_ZONE_PERCENT = 20;                                    // about 1 meteor zone in 5 brings it, by lot
constexpr int CR_MAX_BOLTS = 10, CR_MAX_PUFFS = 40;
// The barrage (S15-2): once in a pass, about halfway across (the cruiser's left edge at CR_BARRAGE_X px), its underside glows red for CR_WARN_S, then it
// launches CR_MISSILES missiles from the bay along its underside, in a fan from down-left to up-left (the terrain is low in the zone, so a downward fan
// would hit the ground at once). They speed up, curve toward the fighter and wobble a little, trailing smoke.
constexpr int CR_MISSILES = 12, CR_TRAIL_N = 9, CR_BARRAGE_X = 50, CR_BAY0 = 26, CR_BAY1 = 60;
constexpr float CR_WARN_S = 0.9f, CR_MISSILE_GAP_S = 0.08f, CR_MISSILE_LIFE = 3.4f, CR_MISSILE_TURN = 1.8f, CR_MISSILE_V0 = 60.0f, CR_MISSILE_VMAX = 115.0f;
static inline float crBot(float x) { return 17.0f + 7.0f * x / (CR_W - 1); }  // the underside's row at column x
static const uint8_t CR_ENGINES[3] = {16, 19, 22};                     // the rows of the three nozzles at the stern
static const uint8_t CR_GUNS[3][2] = {{28, 21}, {40, 22}, {52, 23}};   // where the bolts leave when the fighter is below it (dots)
static const uint8_t CR_TOPGUNS[5][2] = {{32, 12}, {42, 11}, {48, 11}, {58, 8}, {60, 8}};   // and when it is above it: the mounts on top
static const uint8_t CR_DAMAGE[3][2] = {{56, 12}, {36, 15}, {20, 16}};  // where it smokes, in the order the damage builds up (dots)
// The hull, one character per dot (made by the web demo's builder): S lit plane, H bright edge, M mid tone, D dark, K very dark, L lit face,
// W small light, G globe, C cyan light, R maroon, E engine glow.
static const char* const CR_ART[CR_H] = {
    "..............................................................GS.............GS.",
    "..............................................................LM.............LM.",
    ".............................................................HHRRRRRRRRRRRRRRHH.",
    ".............................................................WLWLWLWLWLWLWLWLWL.",
    ".............................................................DDDDDSLLLDDDKDDDDD.",
    ".................................................................SLCCLDDDK......",
    ".................................................................SLLLLDDDDK.....",
    "................................................................SLCCLLDDDDK.....",
    "..........................................................M.M...SLLLLLDDDDDK....",
    "..........................................................KKKK.SLLLLLLDDDDDK....",
    "........................................................DHHHHHHHHHHHHHHHHHHHHHHH",
    "..........................................M.....M.......DSRRRRRRRRRRRRRRRRRRRRSS",
    "................................M.........KK....KK......DDDDDDDDDDDDDDDDDDDDDDDD",
    "................................KK............HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH",
    ".................................HHHHHHHHHHHHHSSSSSMSSSSSSSMSSSSSSSMSSSSSSSMSSSS",
    "....................HHHHHHHHHHHHHSSMSSSSSSSMSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSKEEE",
    ".......HHHHHHHHHHHHHSSSSSSSMSSSSSSSSSSSSSSSSSSMCDMMMDMMMDMMMDMMMDMMMDMMMDCMMKEEE",
    "HHHHHHHSSSSMSSSSSSSMSSSSSSSSSSSSSMMMDMMMDCMMDMKMDMKMDMKMDMKMDMKMDMKCDMKMDMKMKEEE",
    "......SSSSSSSSSSSSSSMMMMMMMMMMMMMSSSSSSSKDKDKDDDKDDDKDDDKDDDKCDMDMMMDMMMDMCMKEEE",
    ".................SSSSSSSSSSSSMMMMKKKKKWKSSSSSSSSSSSDKDDCKDDDKDDDKDDDKDDDKDDDKEEE",
    ".............................SSSSKKKKKKKKKKKWKKKKKWSSSSSSSSSSSSDKDDDKDDDKDDCKEEE",
    "........................................KKKKKKKKKKKKKKKKWKKKKKWSSSSSSSSSSSDDKEEE",
    "...................................................KKKKKKKKKKKKKKKKKWKKKKKSSKEEE",
    "...............................................................KKKKKKKKKKKWKKEEE",
    "..........................................................................KKKKKK",
};
struct Cruiser {
  bool on = false;
  float x = 0, puff = 0;                // the left edge, px; time to the next puff of smoke
  uint8_t hits = 0;                     // big meteors that have struck it
  uint8_t volley = 0;                   // bolts still to fire in this volley
  uint32_t nextFire = 0, volleyAt = 0;
  uint8_t barrage = 0, fired = 0;       // 0 waiting, 1 warning, 2 launching, 3 done; missiles launched so far
  float bt = 0;                         // time in the warning
  uint32_t nextMissile = 0;
};
struct CrBolt {                         // a bolt, flying straight at where the fighter was
  bool on = false;
  float x = 0, y = 0, ox = 0, oy = 0, vx = 0, vy = 0, age = 0;
};
struct CrMissile {                      // a missile of the barrage: it turns toward the fighter and leaves a smoke trail
  bool on = false;
  uint8_t seed = 0, nt = 0;             // its wobble's phase; how many trail points it has
  float x = 0, y = 0, th = 0, v = 0, vx = 0, vy = 0, age = 0, tt = 0;
  float tx[CR_TRAIL_N] = {}, ty[CR_TRAIL_N] = {};
};
struct CrPuff {                         // smoke (or fire) left behind by a place that was hit
  bool on = false, fire = false;
  float x = 0, y = 0, age = 0;
};

// The city (S14-1). See the top of this file.
constexpr int CITY_W = 10, CITY_FLOOR_MIN = 3, CITY_FLOOR_MAX = 8, CITY_N_MIN = 3, CITY_N_MAX = 5;  // a building: 10 dots wide, 3 to 8 floors of 2 dots
constexpr int CITY_ROWS = 1 + 2 * CITY_FLOOR_MAX;   // the tallest building's dot rows
constexpr uint32_t CITY_MS_MIN = 12000, CITY_MS_MAX = 25000, CITY_FIRST_MS = 8000;
constexpr int PERSON_W = 10, PERSON_H = 11;
constexpr int MAX_CIT = 8, MAX_SPARKS = 6;
struct Building {
  int ox = 0;                           // px from the row's left end
  int h = 0;                            // px
  int8_t f = 0;                         // floors
  bool collapsed = false, hasWin = false;
  int8_t fire = -1;                     // the column that burns, or -1
  int8_t winFl = 0, winPh = 0;          // a figure leans out of the window on this floor
  uint8_t lit[CITY_FLOOR_MAX * 3] = {}; // windows: 0 dark, 1 lit, 2 burning
  uint8_t top[CITY_W] = {};             // rows broken off the top of each column
  uint8_t gone[CITY_ROWS * CITY_W] = {};  // 1: bitten away
};
struct Person { int8_t bi = 0; int ox = 0; uint8_t ph = 0; bool saved = false; };
struct City {
  bool on = false;
  float x = 0;                          // the left end, px
  int n = 0;
  Building b[CITY_N_MAX];
  Person p[CITY_N_MAX];
  int np = 0;
};
struct Citizen {                        // somebody who came out of a doorway
  bool on = false;
  uint8_t mode = 0;                     // 0 waits to start, 1 runs left after the fighter, 2 runs back right, 3 stands and waves
  bool back = false;                    // after the ground has carried it off to the left it runs back to the middle and waves again
  float x = 0, t = 0;
  uint8_t ph = 0;
  int stop = 0, backTo = 0;
};
struct Spark { bool on = false; float x = 0, y = 0, t = 0; };

struct Kong {
  uint8_t ph = K_IDLE;
  float t = 0, x = W;        // time in this phase; the left edge of the body
  uint32_t nextMs = 0;       // the next visit, at the earliest
  float tx = 0, ty = 0;      // where the fighter was when it reached
  bool musicOn = false;
  uint32_t musicSince = 0, lastMusic = 0;
  uint8_t arm = 0;           // the arm that goes for the fighter: 0 the near one, 1 the far one (S9-2)
  uint8_t style = 0;         // the way in: 0 from the right, 1 over the top, 2 from below, 3 past the fighter and hooking back
  bool chase = false;        // this visit it lets the fighter pass first, then reaches back after it with the far arm
  uint8_t pal = 0;           // this visit's colours (S9-3): 0 red, 1 blue, 2 green, 3 purple, 4 gold, 5 ghost white
  uint8_t tries = 0, maxTries = 1;  // how many times it has gone for the fighter this visit, and how many it may
  bool noReach = false;      // this visit it only bluffs: no reach at all
  bool doThreat = false, threatDone = false;  // whether it makes itself big when it notices the fighter, and whether it has
  uint8_t threat = 0;        // the pose: 0 both arms up, 1 the near arm up and the far one down, 2 the other way round
  uint8_t nextAct = 0;       // after a miss: 0 walk on, 1 reach again, 2 make itself big (done when nextAt has passed)
  float nextAt = 0;
  uint8_t kind = 0;          // which monster this visit: 0 the ape, 1 the squid (S11-1), 2 the octopus (S15-1)
  bool doPre = false, doPost = false, preDone = false, postDone = false;  // the squid: a reach before the fighter passes, and one after
  uint8_t sc = 0, flip = 0;  // the squid's reach: 0 up its side (fighter above), 1 ahead, 2 behind; flip: 1 up its right side
  uint8_t legs = 0;          // bit i: tentacle i is stretching out (7 tentacles for the squid, 8 for the octopus)
  float rise = 0;            // how far the squid has risen (0 .. 1): up while the fighter is ahead of it, down once it has passed
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
  bool eq = false;           // the graphic equalizer is showing (or rising or sinking)
  float scale = 1;           // equalizer height share while it rises or sinks
  int8_t fade = 0;           // -1 sinking, +1 rising, 0 steady
  uint32_t modeSinceMs = 0, silentSinceMs = 0;
  bool wasSilent = false;
  float eqDb[EQ_BANDS] = {};  // latest band levels from analyze()
  bool eqFresh = false, eqSilent = true;
  float eqH[EQ_BANDS] = {}, eqPeak[EQ_BANDS] = {}, eqHold[EQ_BANDS] = {};  // in segments (0 .. EQ_NSEG)
  float eqAvg[EQ_BANDS] = {};
  bool eqAvgSet = false;
  uint16_t lit[MAX_BAR];     // segment colors by height from the bottom (swapped for the buffer); 0 in the gaps
  uint16_t eqCol[2][EQ_BANDS];  // equalizer bar colors, bass to treble: aqua, KariPom
  uint8_t pattern = 0;       // 0 terrain alone, 1 aqua equalizer, 2 KariPom equalizer
  uint8_t next = 0;          // the pattern a sinking equalizer gives way to
  Light lt[MAX_LIGHTS];
  uint32_t nextLightMs = 0;
  City city;
  uint32_t nextCityMs = 0;
  Citizen cit[MAX_CIT];
  Spark spark[MAX_SPARKS];
  float citAcc = 0;
  bool cityForce = false, pinkForce = false;  // for previews on a computer: a city at once, every monster pink
  MeteorZone mz;
  DesertZone dz;
  bool desertForce = false;  // for previews on a computer: one soon after the start, then every 30 s
  uint16_t sand[MAX_BAR];    // the dunes' colours by height (swapped), like lit
  Meteor met[MZ_MAX];
  Dust dust[MZ_DUST_MAX];
  Cruiser cr;
  CrBolt crb[CR_MAX_BOLTS];
  CrMissile ms[CR_MISSILES];
  CrPuff puff[CR_MAX_PUFFS];
  bool cruiserForce = false;  // for previews on a computer: the cruiser comes with every meteor zone
  Kong k;
  int8_t kongForce = -1;     // for previews on a computer: 0 only the ape, 1 only the squid, 2 only the octopus, -1 by lot
  float evadeCur = 0;        // how far the fighter has slipped away from the monster's hand
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
  // Each equalizer bar has its own color, bass to treble, as in KariPom's EQ Classic: blue, cyan, green,
  // yellow-green, yellow, orange, red, magenta, blended across the 30 bars.
  static const uint8_t KP[8][3] = {{0, 0, 255},   {0, 255, 255}, {0, 255, 0}, {173, 255, 41},
                                   {255, 255, 0}, {255, 165, 0}, {255, 0, 0}, {255, 0, 255}};
  for (int j = 0; j < EQ_BANDS; j++) {
    float t = (float)j * 7 / (EQ_BANDS - 1);
    int a = t >= 7 ? 6 : (int)t;
    float u = t - a;
    s.eqCol[0][j] = swap(rgb(EQ_AQUA_R, EQ_AQUA_G, EQ_AQUA_B));
    s.eqCol[1][j] = swap(rgb((int)(KP[a][0] + (KP[a + 1][0] - KP[a][0]) * u), (int)(KP[a][1] + (KP[a + 1][1] - KP[a][1]) * u),
                          (int)(KP[a][2] + (KP[a + 1][2] - KP[a][2]) * u)));
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
  s.eq = false;
  s.pattern = s.next = 0;
  s.scale = 0;
  s.fade = 0;
  s.modeSinceMs = s.silentSinceMs = nowMs;
  s.wasSilent = false;
  s.eqFresh = false;
  s.eqAvgSet = false;
  for (int j = 0; j < EQ_BANDS; j++) s.eqH[j] = s.eqPeak[j] = 0, s.eqHold[j] = 0;
  s.k = Kong();
  s.k.nextMs = nowMs + KONG_FIRST_MS;
  for (auto& l : s.lt) l = Light();
  s.nextLightMs = nowMs + LIGHT_FIRST_MS;
  s.city = City();
  s.nextCityMs = 0;
  for (auto& c : s.cit) c = Citizen();
  for (auto& k : s.spark) k = Spark();
  s.citAcc = 0;
  s.mz = MeteorZone();
  s.mz.nextMs = nowMs + MZ_FIRST_MS;
  s.dz = DesertZone();
  s.dz.nextMs = nowMs + (s.desertForce ? 1500 : DZ_FIRST_MS);
  for (int y = 0; y < MAX_BAR; y++) {  // dark sand at the bottom, pale moonlit sand on top
    float t = (float)(y / SEG_PITCH) * SEG_PITCH / ((DZ_SEG - 1) * SEG_PITCH);
    if (t > 1) t = 1;
    s.sand[y] = y % SEG_PITCH >= SEG_H ? 0 : swap(rgb((int)lrintf(150 + 96 * t), (int)lrintf(100 + 110 * t), (int)lrintf(52 + 96 * t)));
  }
  s.cr = Cruiser();
  for (auto& q : s.crb) q = CrBolt();
  for (auto& m : s.ms) m = CrMissile();
  for (auto& p : s.puff) p = CrPuff();
  for (auto& m : s.met) m = Meteor();
  for (auto& d : s.dust) d = Dust();
  s.evadeCur = 0;
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
// A bar's height in the meteor zone: lower, in whole segments, still moving with the music.
// In the desert zone: the dunes, lower still.
static inline int mzHeight(const State& s, int h) {
  float u = s.mz.u;
  int seg = MZ_SEG;
  if (s.dz.u > u) u = s.dz.u, seg = DZ_SEG;
  if (u <= 0) return h;
  int n = (int)lrintf(h * (1.0f - u * (1.0f - (float)seg / NSEG)) / SEG_PITCH) * SEG_PITCH;
  return n < GROUND ? GROUND : n;
}
// Graphic-equalizer bar j in px (0 while it is not showing), scaled while it rises or sinks.
static inline int eqHeight(const State& s, int j) {
  return s.eq ? mzHeight(s, (int)lrintf(s.eqH[j] * s.scale) * SEG_PITCH) : 0;
}
// Height of the ground under screen column x: the terrain, or the equalizer where it stands taller.
static inline int heightAt(const State& s, int x, int offset) {
  int i = colAt(x, offset);
  int h = i < 0 ? 0 : i < COLS ? mzHeight(s, s.h[i]) : GROUND;
  if (s.eq && x >= 0 && x < EQ_BANDS * PITCH) {
    int e = eqHeight(s, x / PITCH);
    if (e > h) h = e;
  }
  if (s.dz.ph) {  // the pyramids and the sphinx, so the fighter flies over them
    for (int i = 0; i < DZ_N; i++) {
      const DzItem& d = DZ_LAYOUT[i];
      int cx = DOT * (int)lrintf((s.dz.x + d.ox) / DOT), m = 0;
      if (d.kind == 1) m = x >= cx && x < cx + SPHINX_W * DOT ? GROUND + SPHINX_H * DOT : 0;
      else if (d.kind == 0) {
        int rows = (int)lrintf(s.dz.hd[i]) - abs(x - cx - 1) / DOT;
        if (rows > 0) m = GROUND + rows * DOT;
      }
      if (m > h) h = m;
    }
  }
  return h;
}
// Knocks n segments off column i, keeping the ground row.
static inline void carve(State& s, int i, int n) {
  if (i < 0 || i >= COLS) return;
  int h = s.h[i] - n * SEG_PITCH;
  s.h[i] = (uint8_t)(h < GROUND ? GROUND : h);
}

// The spectrum for the graphic-equalizer terrain: a 512-point FFT of the newest samples in ring (mono, mask + 1
// long, pos is the next write position), summed into the bands. Only needed while s.eq is set.
static float fftRe[FFT_N], fftIm[FFT_N], hann[FFT_N], twC[FFT_N / 2], twS[FFT_N / 2];
static uint16_t bandBin[EQ_BANDS + 1];
static bool fftReady = false;
static void analyze(State& s, const int16_t* ring, uint32_t mask, uint32_t pos) {
  if (!fftReady) {
    for (int i = 0; i < FFT_N; i++) hann[i] = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * i / FFT_N);
    for (int i = 0; i < FFT_N / 2; i++) twC[i] = cosf(2.0f * (float)M_PI * i / FFT_N), twS[i] = -sinf(2.0f * (float)M_PI * i / FFT_N);
    int last = EQ_BIN_LO - 1;
    for (int j = 0; j <= EQ_BANDS; j++) {  // log spaced edges, each band at least one bin wide
      int b = (int)lrintf(EQ_BIN_LO * powf((float)EQ_BIN_HI / EQ_BIN_LO, (float)j / EQ_BANDS));
      if (b <= last) b = last + 1;
      bandBin[j] = (uint16_t)b;
      last = b;
    }
    fftReady = true;
  }
  float sq = 0;
  for (int i = 0; i < FFT_N; i++) {  // windowed, in bit-reversed order
    float v = ring[(pos - FFT_N + i) & mask];
    sq += v * v;
    int r = 0;
    for (int b = 0; b < FFT_LOG2; b++) r |= ((i >> b) & 1) << (FFT_LOG2 - 1 - b);
    fftRe[r] = v * hann[i];
    fftIm[r] = 0;
  }
  s.eqSilent = sqrtf(sq / FFT_N) < EQ_SILENT_RMS;
  for (int len = 2; len <= FFT_N; len <<= 1) {
    int half = len / 2, step = FFT_N / len;
    for (int i = 0; i < FFT_N; i += len)
      for (int k = 0; k < half; k++) {
        float c = twC[k * step], sn = twS[k * step];
        int a = i + k, b = a + half;
        float xr = fftRe[b] * c - fftIm[b] * sn, xi = fftRe[b] * sn + fftIm[b] * c;
        fftRe[b] = fftRe[a] - xr, fftIm[b] = fftIm[a] - xi;
        fftRe[a] += xr, fftIm[a] += xi;
      }
  }
  for (int j = 0; j < EQ_BANDS; j++) {
    float p = 1e-3f;
    for (int k = bandBin[j]; k < bandBin[j + 1]; k++) p += fftRe[k] * fftRe[k] + fftIm[k] * fftIm[k];
    s.eqDb[j] = 10.0f * log10f(p);
  }
  s.eqFresh = true;
}

// Graphic-equalizer bars and peak dots, from the latest spectrum.
static void updateEq(State& s, float dt) {
  if (s.eqFresh) {
    s.eqFresh = false;
    if (!s.eqSilent && !s.eqAvgSet) {
      for (int j = 0; j < EQ_BANDS; j++) s.eqAvg[j] = s.eqDb[j];
      s.eqAvgSet = true;
    }
    float k = 1.0f - expf(-dt / EQ_AVG_S);
    for (int j = 0; j < EQ_BANDS; j++) {
      float t = 0;
      if (!s.eqSilent) {
        t = EQ_MID + (s.eqDb[j] - s.eqAvg[j]) / EQ_SPAN_DB;
        s.eqAvg[j] += (s.eqDb[j] - s.eqAvg[j]) * k;
      }
      if (t < 0) t = 0;
      if (t > 1) t = 1;
      float n = t * EQ_NSEG;
      if (n > s.eqH[j]) s.eqH[j] = n;
    }
  }
  for (int j = 0; j < EQ_BANDS; j++) {
    s.eqH[j] -= EQ_FALL_SEG_S * dt;
    if (s.eqH[j] < 0) s.eqH[j] = 0;
    if (s.eqH[j] >= s.eqPeak[j]) {
      s.eqPeak[j] = s.eqH[j];
      s.eqHold[j] = PEAK_HOLD_S;
    } else if ((s.eqHold[j] -= dt) < 0) {
      s.eqPeak[j] -= PEAK_FALL_SEG_S * dt;
      if (s.eqPeak[j] < s.eqH[j]) s.eqPeak[j] = s.eqH[j];
    }
  }
}

// The equalizer comes and goes: every MODE_MS, or when the music comes back after a gap between songs.
static void updateTurns(State& s, uint32_t nowMs, float dt) {
  bool silent = s.prevDb <= DB_SILENT;
  bool turn = nowMs - s.modeSinceMs >= MODE_MS;
  if (silent && !s.wasSilent) s.silentSinceMs = nowMs;
  if (!silent && s.wasSilent && nowMs - s.silentSinceMs >= GAP_MS && nowMs - s.modeSinceMs >= MODE_MIN_MS) turn = true;
  s.wasSilent = silent;
  bool rise = false;
  if (turn && s.fade == 0) {  // on to the next pattern: none, aqua, KariPom
    s.modeSinceMs = nowMs;
    s.next = (uint8_t)((s.pattern + 1) % 3);
    if (s.eq) s.fade = -1;  // sink first; the next equalizer, if any, rises after
    else rise = true;
  }
  if (s.fade < 0 && (s.scale -= dt / FADE_S) <= 0) {
    s.scale = 0;
    s.eq = false;
    s.fade = 0;
    rise = true;
  }
  if (rise) {
    s.pattern = s.next;
    if (s.pattern) {
      s.eq = true;
      s.fade = 1;
      s.scale = 0;
      s.eqAvgSet = false;  // the band averages start over from the music now playing
      for (int j = 0; j < EQ_BANDS; j++) s.eqH[j] = s.eqPeak[j] = 0, s.eqHold[j] = 0;
    }
  } else if (s.fade > 0 && (s.scale += dt / FADE_S) >= 1) {
    s.scale = 1;
    s.fade = 0;
  }
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

// Which span (0 or 1) of row r holds column c, or -1.
static inline int kongSeg(int r, int c) {
  if (r < 0 || r >= KONG_H || c < 0 || c >= KONG_W) return -1;
  const int8_t* sp = KONG_SPAN[r];
  if (c >= sp[0] && c <= sp[1]) return 0;
  if (sp[2] >= 0 && c >= sp[2] && c <= sp[3]) return 1;
  return -1;
}
static inline float ease3(float u) {
  if (u < 0) u = 0;
  if (u > 1) u = 1;
  return u * u * (3.0f - 2.0f * u);
}
static inline float kongSpeed(uint8_t ph) {  // it slows down while it reaches, and walks on at the terrain's pace
  switch (ph) {
    case K_REACH: return 0.6f;
    case K_GRAB: case K_MISS: return 0.3f;
    case K_THREAT: return 0.4f;
    default: return 1.0f;
  }
}
// The shoulder of the arm toward the fighter (left), for a body at screen x bx.
static inline void kongShoulder(int bx, float& sx, float& sy) { sx = (float)(bx + 7); sy = (float)(H - KH + 7 * DOT + 2); }
// The far arm's shoulder (right).
static inline void kongShoulder2(int bx, float& sx, float& sy) { sx = (float)(bx + 21 * DOT); sy = (float)(H - KH + 7 * DOT + 2); }

// The monster's colours (S9-3): six palettes of three colours (the lit edge, the dark body, the eyes), each visit draws one by lot.
// The eyes are chosen to stand out from the body in every colour. Nothing new is drawn, so this costs a few numbers.
static const uint16_t KPAL[7][3] = {
  {swap(rgb(212, 38, 46)), swap(rgb(40, 20, 24)), swap(rgb(255, 226, 64))},    // 0 red, the original
  {swap(rgb(70, 120, 235)), swap(rgb(16, 22, 46)), swap(rgb(120, 255, 255))},  // 1 blue
  {swap(rgb(60, 190, 84)), swap(rgb(14, 34, 20)), swap(rgb(255, 90, 60))},     // 2 green
  {swap(rgb(176, 76, 224)), swap(rgb(34, 16, 44)), swap(rgb(120, 255, 160))},  // 3 purple
  {swap(rgb(240, 192, 56)), swap(rgb(54, 38, 10)), swap(rgb(255, 255, 255))},  // 4 gold, rare
  {swap(rgb(222, 232, 255)), swap(rgb(38, 42, 62)), swap(rgb(255, 60, 90))},   // 5 ghost white
  {swap(rgb(255, 142, 222)), swap(rgb(255, 40, 170)), swap(rgb(255, 226, 64))},  // 6 shocking pink all over, lighter where the light falls, yellow eyes (S14-1)
};
static const uint8_t KPAL_PERCENT[7] = {28, 15, 15, 15, 10, 12, 5};  // how often each comes (sums to 100); the pink one: one visit in twenty
static inline uint8_t pickKongPal(State& s) {
  uint32_t r = nextRandom(s) % 100;
  uint8_t i = 0;
  while (i < 6 && r >= KPAL_PERCENT[i]) r -= KPAL_PERCENT[i++];
  return i;
}


// ---- The second big monster (S11-1): a squid from the Space Invaders cabinet art, 23 x 30 dots like the ape. A pointed bell of a head with
// slanted eyes and a small grumpy mouth, over seven tentacles that hang side by side from a narrow base, flare out like a trumpet and lie
// along the ground. It floats in with the terrain. Now and then, before the fighter passes and again after it has passed, the two tentacles
// on the fighter's side stretch out for it (sometimes the two on the other side too) and the fighter slips away. Each visit is the ape or
// the squid, by lot, half and half.
constexpr int SQ_NECK_X = 11, SQ_NECK_Y = 15;       // where the tentacles leave the head, in dots from the body's top left
constexpr float SQ_REACH_DOTS = 34.0f;              // it goes for the fighter when it is this close (dots) to the neck
constexpr float SQ_RISE_PX = 6.0f;                  // two dots
static const uint8_t SQ_WID[15] = {0, 0, 3, 5, 7, 9, 11, 13, 15, 17, 19, 19, 17, 13, 11};  // the head and its base: width of each row, centred
static const float SQ_ROOT[7] = {-3, -2, -1, 0, 1, 2, 3};     // where each tentacle starts, in dots from the neck's middle
static const float SQ_FLARE[7] = {15, 11, 7, 3, 7, 11, 15};   // how high (px) above the ground it starts to bend outward
static const float SQ_ENDX[7] = {-14, -10, -6, 2, 6, 10, 14};  // and where it ends on the ground (dots from the root, sideways) ...
static const float SQ_ENDY[7] = {-1, 2, -2, 2, -2, 2, -1};     // ... and (dots) toward or away from us, as if lying around it like rays
static const uint8_t SQ_PAL_PERCENT[7] = {9, 10, 24, 9, 34, 9, 5};  // yellow and green most often, the pink one visit in twenty (sums to 100)
// The octopus (S15-1), the third monster, from the author's sketch: a tall egg of a head with two small slanted eyes low on it, a narrow neck, and
// eight tentacles that hang packed together under it and then lie far out along the ground to both sides, the outer ones with their tips curled up.
// It plays like the squid (the same reach, the same colours); only the shape differs, through SqSpec. pad: how far (px) the tentacles lie out
// beyond the body, so it comes in and goes out without a pop.
static const uint8_t OC_WID[20] = {5, 9, 11, 13, 15, 15, 17, 17, 17, 17, 17, 15, 15, 13, 13, 11, 9, 9, 7, 7};
static const float OC_ROOT[8] = {-3.5f, -2.5f, -1.5f, -0.5f, 0.5f, 1.5f, 2.5f, 3.5f};
static const float OC_FLARE[8] = {14, 11, 8, 5, 5, 8, 11, 14};
static const float OC_ENDX[8] = {-22, -13, -8, -4, 4, 8, 13, 22};
static const float OC_ENDY[8] = {-3, 1, -1, 2, 2, -1, 1, -3};
struct SqSpec { uint8_t rows, n; int neckX, neckY, pad; const uint8_t* wid; const float *root, *flare, *endx, *endy; };
static const SqSpec SQ_SPEC[3] = {
  {0, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr},                                          // 0: the ape, not used here
  {15, 7, SQ_NECK_X, SQ_NECK_Y, 0, SQ_WID, SQ_ROOT, SQ_FLARE, SQ_ENDX, SQ_ENDY},                        // 1: the squid
  {20, 8, 11, 20, 40, OC_WID, OC_ROOT, OC_FLARE, OC_ENDX, OC_ENDY},                                      // 2: the octopus
};
static inline const SqSpec& sqSpec(uint8_t kind) { return SQ_SPEC[kind < 3 && kind ? kind : 1]; }
// The part of the head at row r, column c of the 23 x 30 grid: 0 none, 1 body, 2 the lit edge, 3 an eye, 4 the squid's mouth.
static inline uint8_t sqCell(uint8_t kind, int r, int c) {
  const SqSpec& sp = sqSpec(kind);
  if (r < 0 || r >= sp.rows) return 0;
  int w = sp.wid[r];
  if (!w) return 0;
  int m = (KONG_W - w) / 2;
  if (c < m || c >= m + w) return 0;
  if (kind == 1) {
    if ((r == 9 && (c == 7 || c == 15)) || (r == 10 && (c == 8 || c == 9 || c == 13 || c == 14))) return 3;
    if ((r == 12 && c >= 9 && c <= 13) || (r == 13 && (c == 8 || c == 14))) return 4;
  } else if ((r == 14 && (c == 8 || c == 14)) || (r == 15 && (c == 9 || c == 13))) {
    return 3;
  }
  return c - m < 2 ? 2 : 1;
}
// A tentacle that stretches: its first control point's offset from the neck (d1), its second one's from the tip (d2), and where the tip
// lands relative to the fighter (o), all in dots. Four per scenario: the two on the fighter's side first, then the other two.
struct SqReach { int8_t leg; float d1x, d1y, d2x, d2y, ox, oy; };
static const SqReach SQ_SC[4][4] = {
  {{0, -20, -2, -6, 4, -6, -3}, {1, -16, 2, -6, 5, -5, 3}, {6, 20, -2, 6, 4, 7, 2}, {5, 16, 2, 6, 5, 6, 8}},   // fighter above, on its left
  {{0, -8, 5, 6, 0, -2, 0}, {1, -8, 9, 6, -2, -3, 3}, {6, 2, 12, 8, 2, -1, 6}, {5, 0, 14, 8, 4, -2, 9}},         // ahead (left)
  {{6, 8, 5, -6, 0, 2, 0}, {5, 8, 9, -6, -2, 3, 3}, {0, -2, 12, -8, 2, 1, 6}, {1, 0, 14, -8, 4, 2, 9}},         // behind (right)
  {{6, 20, -2, 6, 4, 6, -3}, {5, 16, 2, 6, 5, 5, 3}, {0, -20, -2, -6, 4, -7, 2}, {1, -16, 2, -6, 5, -6, 8}},   // fighter above, on its right
};
static inline uint8_t pickSquidPal(State& s) {
  uint32_t r = nextRandom(s) % 100;
  uint8_t i = 0;
  while (i < 6 && r >= SQ_PAL_PERCENT[i]) r -= SQ_PAL_PERCENT[i++];
  return i;
}
static inline int squidTop(const Kong& k) { return H - KH - 2 - (int)lrintf(SQ_RISE_PX * k.rise); }
static inline void squidNeck(const Kong& k, float& nx, float& ny) { const SqSpec& sp = sqSpec(k.kind); nx = (float)((int)lrintf(k.x) + sp.neckX * DOT + 1); ny = (float)(squidTop(k) + sp.neckY * DOT); }

static inline bool squidNear(const Kong& k, float dxd, float dyd) {
  return sqrtf(dxd * dxd + dyd * dyd) <= SQ_REACH_DOTS && fabsf(dxd) <= SQ_REACH_DOTS && k.x < W - KW - 6;
}
static void squidStart(State& s, float shipCx, float shipCy, float nx, float ny) {
  Kong& k = s.k;
  k.ph = K_REACH, k.t = 0, k.tx = shipCx, k.ty = shipCy, k.tries++;
  float dxd = (shipCx - nx) / DOT, dyd = (shipCy - ny) / DOT;
  k.sc = (dyd < -13 && fabsf(dxd) <= 12) ? 0 : dxd < 0 ? 1 : 2;   // above it, ahead of it, or behind it
  k.flip = fabsf(dxd) < 4 ? (nextRandom(s) % 2) : dxd < 0 ? 0 : 1;  // above it: up the side the fighter is on
  bool aimLeft = k.sc == 1 || (k.sc == 0 && !k.flip);
  uint8_t right = (uint8_t)(3u << (sqSpec(k.kind).n - 2));  // the left pair is tentacles 0 and 1, the right pair the last two (5 and 6 of the squid's seven, 6 and 7 of the octopus's eight)
  uint8_t mine = aimLeft ? 0x03 : right, other = aimLeft ? right : 0x03;
  k.legs = (nextRandom(s) % 100 < 50) ? (mine | other) : mine;   // half of the reaches use both sides, half only the fighter's side
}

// Moves the squid. Returns how far (px) the fighter should slip away right now, as for the ape.
static float updateSquid(State& s, uint32_t nowMs, float dt, float shipCx, float shipCy) {
  Kong& k = s.k;
  float evade = 0;
  k.x -= SCROLL_PX_S * kongSpeed(k.ph) * dt;
  float nx, ny;
  squidNeck(k, nx, ny);
  k.rise += ((shipCx < nx ? 1.0f : 0.0f) - k.rise) * (1.0f - expf(-dt));   // slowly up while the fighter is to its left, slowly down once it has passed
  squidNeck(k, nx, ny);
  float dxd = (shipCx - nx) / DOT, dyd = (shipCy - ny) / DOT;
  if (k.ph == K_WALK || (k.ph == K_LEAVE && k.t >= 0.6f)) {  // (once the tentacles are back from a reach it may reach again)
    if (squidNear(k, dxd, dyd)) {
      if (dxd < 4 && k.doPre && !k.preDone) k.preDone = true, squidStart(s, shipCx, shipCy, nx, ny);        // the fighter is still ahead
      else if (dxd >= 4 && k.doPost && !k.postDone) k.postDone = true, squidStart(s, shipCx, shipCy, nx, ny);  // the fighter has passed
    }
  } else if (k.ph == K_REACH) {
    if (k.t >= K_REACH_S) k.ph = K_GRAB, k.t = 0;
  } else if (k.ph == K_GRAB) {
    evade = shipCy > 52 ? -K_EVADE_PX : K_EVADE_PX;  // the fighter slips away
    if (k.t >= K_GRAB_S) k.ph = K_MISS, k.t = 0;
  } else if (k.ph == K_MISS) {
    evade = shipCy > 52 ? -K_EVADE_PX : K_EVADE_PX;
    if (k.t >= K_MISS_S) k.ph = K_LEAVE, k.t = 0;  // the tentacles are drawn back
  }
  if (k.x < -KW - 4 - sqSpec(k.kind).pad) {
    k.ph = K_IDLE;
    k.nextMs = nowMs + KONG_MS_MIN + nextRandom(s) % (KONG_MS_MAX - KONG_MS_MIN);
  }
  return evade;
}

// Moves the monster. Returns how far (px, up or down) the fighter should slip away right now: 0 except while the fist closes.
// Every visit plays out differently (S9-3): how many times it may reach (1 to 3), whether it first makes itself big, now and then
// it only bluffs, and after a miss it waits a second or more and only sometimes does something more.
static float updateKong(State& s, uint32_t nowMs, float dt, float shipCx, float shipCy) {
  Kong& k = s.k;
  k.t += dt;
  // It appears only while music plays: 3 s of sound, and a gap of up to 2 s between notes or songs is not silence.
  if (s.prevDb > DB_SILENT) {
    if (!k.musicOn) k.musicOn = true, k.musicSince = nowMs;
    k.lastMusic = nowMs;
  } else if (k.musicOn && nowMs - k.lastMusic > KONG_GAP_MS) {
    k.musicOn = false;
  }
  bool musicOk = k.musicOn && nowMs - k.musicSince >= KONG_MUSIC_MS;
  if (k.ph == K_IDLE) {
    if (musicOk && !s.cr.on && (int32_t)(nowMs - k.nextMs) >= 0) {
      k.ph = K_WALK, k.t = 0, k.x = (float)(W + 4);
      k.chase = nextRandom(s) % 100 < K_CHASE_PERCENT;
      k.pal = pickKongPal(s);
      k.tries = 0, k.nextAct = 0, k.threatDone = false;
      uint32_t r = nextRandom(s) % 100;
      k.maxTries = r < 35 ? 1 : r < 75 ? 2 : 3;
      k.doThreat = nextRandom(s) % 100 < 55;
      k.noReach = nextRandom(s) % 100 < 12;
      { uint32_t q = nextRandom(s) % 100; k.kind = s.kongForce >= 0 ? (uint8_t)s.kongForce : (uint8_t)(q < 34 ? 0 : q < 67 ? 1 : 2); }  // this visit: the ape, the squid or the octopus, a third each
      if (s.pinkForce && k.kind == 0) k.pal = 6;
      if (k.kind >= 1) {
        k.x += (float)sqSpec(k.kind).pad;
        k.doThreat = false, k.chase = false, k.noReach = false;
        k.pal = s.pinkForce ? 6 : pickSquidPal(s);
        k.doPre = nextRandom(s) % 100 < 60, k.doPost = nextRandom(s) % 100 < 50, k.preDone = k.postDone = false;
        k.sc = 0, k.flip = 0, k.legs = 0, k.rise = 0;
      }
    }
    return 0;
  }
  if (k.kind >= 1) return updateSquid(s, nowMs, dt, shipCx, shipCy);
  k.x -= SCROLL_PX_S * kongSpeed(k.ph) * dt;
  float shx, shy, s2x, s2y;
  kongShoulder((int)lrintf(k.x), shx, shy);
  kongShoulder2((int)lrintf(k.x), s2x, s2y);
  float evade = 0;
  if (k.ph == K_WALK) {
    float dx = shx - shipCx, dy = shy - shipCy, d2x = s2x - shipCx, d2y = s2y - shipCy;
    if (k.doThreat && !k.threatDone && k.x < W - KW - 6 && dx <= K_NOTICE_DX && dx > -10) {
      // It has noticed the fighter: before anything else it makes itself look big, once a visit.
      k.ph = K_THREAT, k.t = 0, k.threatDone = true, k.threat = nextRandom(s) % 3;
    } else if (!k.noReach && k.tries < k.maxTries && !k.chase && k.x < W - KW - 6 && dx <= K_START_DX && dx > -10 &&
               sqrtf(dx * dx + dy * dy) <= 2 * K_ARM * 1.25f) {  // close enough, and within reach: lock on and reach
      k.ph = K_REACH, k.t = 0, k.tx = shipCx, k.ty = shipCy, k.arm = 0, k.style = nextRandom(s) % 4, k.tries++;
    } else if (!k.noReach && k.tries < k.maxTries && k.chase && d2x <= 4 && d2x >= -K_CHASE_DX &&
               sqrtf(d2x * d2x + d2y * d2y) <= 2 * K_ARM - 1) {  // it let the fighter pass: now it reaches back after it
      k.ph = K_REACH, k.t = 0, k.tx = shipCx, k.ty = shipCy, k.arm = 1, k.style = nextRandom(s) % 4, k.tries++;
    }
  } else if (k.ph == K_THREAT) {
    if (k.t >= K_THREAT_S) k.ph = K_WALK, k.t = 0;
  } else if (k.ph == K_REACH) {
    if (k.t >= K_REACH_S) k.ph = K_GRAB, k.t = 0;
  } else if (k.ph == K_GRAB) {
    evade = shipCy > 52 ? -K_EVADE_PX : K_EVADE_PX;  // the fighter slips away as the fist closes
    if (k.t >= K_GRAB_S) k.ph = K_MISS, k.t = 0;
  } else if (k.ph == K_MISS) {
    evade = shipCy > 52 ? -K_EVADE_PX : K_EVADE_PX;
    if (k.t >= K_MISS_S) {
      k.ph = K_LEAVE, k.t = 0;  // no tantrum: it draws its arm back, and then, maybe: 45 % reach again, 20 % make itself big, 35 % walk on
      uint32_t r = nextRandom(s) % 100;
      k.nextAct = k.tries >= k.maxTries ? (r < 20 ? 2 : 0) : r < 45 ? 1 : r < 65 ? 2 : 0;
      k.nextAt = K_GAP_MIN_S + (nextRandom(s) % 1000) / 1000.0f * K_GAP_VAR_S;
    }
  } else if (k.ph == K_LEAVE && k.nextAct && k.t >= k.nextAt) {
    // After the pause: reach again if the fighter is still close (with the nearer arm, the far one if it has slipped past on
    // the right), or make itself big once more. Often it does neither and just walks on.
    float d1 = sqrtf((shx - shipCx) * (shx - shipCx) + (shy - shipCy) * (shy - shipCy));
    float d2 = sqrtf((s2x - shipCx) * (s2x - shipCx) + (s2y - shipCy) * (s2y - shipCy));
    uint8_t act = k.nextAct;
    k.nextAct = 0;
    if (act == 1 && fminf(d1, d2) <= K_RETRY_R && k.x < W - KW - 6) {
      k.ph = K_REACH, k.t = 0, k.tx = shipCx, k.ty = shipCy, k.style = nextRandom(s) % 4, k.arm = d2 < d1 ? 1 : 0, k.tries++;
    } else if (act == 2) {
      k.ph = K_THREAT, k.t = 0, k.threat = nextRandom(s) % 3;
    }
  }
  if (k.x < -KW - 4) {
    k.ph = K_IDLE;
    k.nextMs = nowMs + KONG_MS_MIN + nextRandom(s) % (KONG_MS_MAX - KONG_MS_MIN);
  }
  return evade;
}

// Moves the searchlights and brings in a new one now and then (only while music plays).
static void updateLights(State& s, uint32_t nowMs, float dt) {
  int n = 0;
  for (auto& l : s.lt) {
    if (!l.on) continue;
    l.x -= SCROLL_PX_S * dt;
    l.a = l.base + l.amp * sinf(nowMs * 0.001f * l.w + l.ph);
    if (l.x <= -LIGHT_LEN) l.on = false;
    else n++;
  }
  if (!s.mz.ph && s.prevDb > DB_SILENT && n < MAX_LIGHTS && (int32_t)(nowMs - s.nextLightMs) >= 0) {
    const float d = (float)M_PI / 180.0f;
    for (auto& l : s.lt) {
      if (l.on) continue;
      l.on = true, l.x = (float)(W + 24);
      l.base = ((int)(nextRandom(s) % 21) - 10) * d, l.amp = (20 + nextRandom(s) % 22) * d;
      l.w = 0.45f + (nextRandom(s) % 40) / 100.0f, l.ph = (nextRandom(s) % 628) / 100.0f;
      l.a = l.base;
      break;
    }
    s.nextLightMs = nowMs + (s.dz.ph ? 2500 + nextRandom(s) % 4000 : LIGHT_MS_MIN + nextRandom(s) % (LIGHT_MS_MAX - LIGHT_MS_MIN));  // more often in the desert
  }
}

// The fighter's way through the zone: its top edge at nowMs (which may be ahead of now).
static inline float mzPath(const State& s, uint32_t ms) {
  float t = (int32_t)(ms - s.mz.startMs) * 0.001f + s.mz.ph0;
  const float top = TOP + 6.0f, bottom = (float)(H - MZ_SEG * SEG_PITCH - SHIP_H - 6);
  return (top + bottom) / 2 + (bottom - top) / 2 * (0.62f * sinf(t * 1.1f) + 0.38f * sinf(t * 2.3f + 1.0f));
}
// A new meteor, placed so that it and its tail miss the fighter's path; none if no place was found.
static void spawnMeteor(State& s, uint32_t nowMs, int sx, uint8_t size) {
  Meteor* m = nullptr;
  for (auto& c : s.met)
    if (!c.on) { m = &c; break; }
  if (!m) return;
  const MeteorSize& z = MZ_SIZE[size];
  float v = (float)(z.vMin + nextRandom(s) % (z.vMax - z.vMin));
  float vy = v * (0.04f + (nextRandom(s) % 12) / 100.0f), tail = (float)(z.tail * DOT);
  float arrive = (W + z.r - sx - SHIP_W) / v, leave = (W + z.r + tail - sx) / v, clear = SHIP_H / 2.0f + z.r + MZ_GAP;
  for (int tries = 0; tries < 8; tries++) {
    float y = (float)(TOP + nextRandom(s) % (H - MZ_SEG * SEG_PITCH - TOP - 6));
    bool ok = true;
    for (int i = 0; i <= 5 && ok; i++) {
      float t = arrive + (leave - arrive) * i / 5;
      ok = fabsf(y + vy * t - (mzPath(s, nowMs + (uint32_t)(t * 1000)) + SHIP_H / 2.0f)) >= clear;
    }
    if (!ok) continue;
    m->on = true, m->x = (float)(W + z.r), m->y = y, m->v = v, m->vy = vy, m->size = size;
    m->warm = size == 2 || nextRandom(s) % 100 < 55;
    return;
  }
}
// A burst of sparks at x, y (the first free one).
static inline void freeBurst(State& s, float x, float y) {
  for (auto& b : s.x)
    if (!b.on) { b = {x, y, 0, true, false}; return; }
}
// Is there a part of the cruiser at the screen point x, y?
static inline bool cruiserAt(const State& s, float x, float y) {
  if (!s.cr.on) return false;
  int c = (int)floorf((x - lrintf(s.cr.x)) / DOT), r = (int)floorf((y - CR_Y) / DOT);
  return r >= 0 && r < CR_H && c >= 0 && c < CR_W && CR_ART[r][c] != '.';
}
static void updateMeteorZone(State& s, uint32_t nowMs, float dt, int sx) {
  MeteorZone& z = s.mz;
  Kong& k = s.k;
  if (z.ph == 0) {
    if ((int32_t)(nowMs - z.nextMs) < 0 || s.dz.ph || k.ph != K_IDLE || !k.musicOn || nowMs - k.musicSince < KONG_MUSIC_MS) return;
    z.ph = 1, z.t = 0, z.acc = z.dustAcc = 0, z.ph0 = (nextRandom(s) % 628) / 100.0f, z.startMs = nowMs;
    if (s.cruiserForce || nextRandom(s) % 100 < CR_ZONE_PERCENT) {  // the cruiser comes in with the zone
      s.cr = Cruiser();
      s.cr.on = true, s.cr.x = (float)(W + 2), s.cr.nextFire = nowMs + 2500;
    }
    uint32_t after = nowMs + (uint32_t)((MZ_IN_S + MZ_RUN_S + MZ_OUT_S) * 1000) + 8000;
    if ((int32_t)(after - k.nextMs) > 0) k.nextMs = after;
  }
  z.t += dt;
  int meteors = 0;
  for (const auto& m : s.met) meteors += m.on;
  if (z.ph == 1) {
    z.u = z.t / MZ_IN_S;
    if (z.u >= 1) z.u = 1, z.ph = 2, z.t = 0;
  } else if (z.ph == 2 && z.t >= MZ_RUN_S) {
    z.ph = 3, z.t = 0;
  } else if (z.ph == 3) {
    z.u = 1.0f - z.t / MZ_OUT_S;
    if (z.u < 0) z.u = 0;
    if (z.t >= MZ_OUT_S && meteors == 0 && !s.cr.on) {
      z.ph = 0, z.u = 0;
      uint32_t light = nowMs + 6000;
      if ((int32_t)(light - s.nextLightMs) > 0) s.nextLightMs = light;
      z.nextMs = nowMs + MZ_MS_MIN + nextRandom(s) % (MZ_MS_MAX - MZ_MS_MIN);
    }
  }
  if (z.ph == 2) {
    float level = s.prevDb <= DB_SILENT ? 0.0f : (s.prevDb - s.avgDb) / SPAN_DB + 0.5f;
    level = level < 0 ? 0 : level > 1 ? 1 : level;
    z.acc += dt * (MZ_RATE_MIN + (MZ_RATE_MAX - MZ_RATE_MIN) * level);
    while (z.acc >= 1) {
      z.acc -= 1;
      spawnMeteor(s, nowMs, sx, nextRandom(s) % 100 < 65 ? 0 : 1);
    }
    if (s.beat) spawnMeteor(s, nowMs, sx, 2);
  }
  s.beat = false;
  z.dustAcc += dt * (z.ph == 3 ? 0.0f : MZ_DUST_RATE * z.u);
  while (z.dustAcc >= 1) {
    z.dustAcc -= 1;
    for (auto& d : s.dust) {
      if (d.on) continue;
      d.on = true, d.x = (float)(W + 8), d.y = (float)(TOP + nextRandom(s) % (H - TOP - MZ_SEG * SEG_PITCH));
      d.v = (float)(340 + nextRandom(s) % 200), d.len = (uint8_t)(10 + nextRandom(s) % 18);
      break;
    }
  }
  for (auto& m : s.met) {
    if (!m.on) continue;
    m.x -= m.v * dt, m.y += m.vy * dt;
    if (s.cr.on && m.size == 2 && cruiserAt(s, m.x, m.y)) {  // a big meteor strikes the cruiser: sparks, and the damage adds up
      m.on = false, s.cr.hits++;
      freeBurst(s, m.x, m.y);
      continue;
    }
    if (m.x <= -MZ_SIZE[m.size].tail * DOT - 8) m.on = false;
  }
  for (auto& d : s.dust) {
    if (!d.on) continue;
    d.x -= d.v * dt;
    if (d.x <= -d.len) d.on = false;
  }
  float w = MZ_WARP_PX_S * z.u * dt;  // whatever was on screen falls behind
  for (auto& e : s.e) if (e.on) e.x -= w;
  for (auto& b : s.eb) if (b.on) b.x -= w;
  for (auto& m : s.mine) if (m.on) m.x -= w;
  for (auto& l : s.lt) if (l.on) l.x -= w;
}

// The desert zone: comes in, flows by with the ground, goes. The pyramids breathe with the music.
static void updateDesert(State& s, uint32_t nowMs, float dt) {
  DesertZone& z = s.dz;
  Kong& k = s.k;
  if (z.ph == 0) {
    if ((int32_t)(nowMs - z.nextMs) < 0 || s.mz.ph || k.ph != K_IDLE || s.city.on || s.cr.on) return;
    if (!s.desertForce && (!k.musicOn || nowMs - k.musicSince < KONG_MUSIC_MS)) return;
    z.ph = 1, z.t = 0, z.x = (float)(W + 40), z.glow = 0;
    for (int i = 0; i < DZ_N; i++) z.hd[i] = DZ_LAYOUT[i].max * 0.6f;
    uint32_t dur = (uint32_t)((DZ_IN_S + DZ_OUT_S) * 1000 + (W + 40 + DZ_LEN) / SCROLL_PX_S * 1000);
    uint32_t a = nowMs + dur + 8000, b = nowMs + dur + 30000;
    if ((int32_t)(a - k.nextMs) > 0) k.nextMs = a;              // no monster while it lasts
    if ((int32_t)(b - s.mz.nextMs) > 0) s.mz.nextMs = b;         // nor a meteor zone right after it
    if ((int32_t)(a - s.nextCityMs) > 0) s.nextCityMs = a;
    uint32_t l = nowMs + 2000;
    if ((int32_t)(s.nextLightMs - l) > 0) s.nextLightMs = l;     // the searchlights come sooner
    return;
  }
  z.t += dt;
  z.x -= SCROLL_PX_S * dt;
  if (z.ph == 1) {
    z.u = z.t / DZ_IN_S;
    if (z.u >= 1) z.u = 1, z.ph = 2, z.t = 0;
  } else if (z.ph == 2) {
    if (z.x + DZ_LEN < 0) z.ph = 3, z.t = 0;
  } else {
    z.u = 1.0f - z.t / DZ_OUT_S;
    if (z.u < 0) z.u = 0;
    if (z.t >= DZ_OUT_S) {
      z.ph = 0, z.u = 0;
      z.nextMs = nowMs + (s.desertForce ? 30000 : DZ_MS_MIN + nextRandom(s) % (DZ_MS_MAX - DZ_MS_MIN));
      return;
    }
  }
  if (s.beat) z.glow = 1;  // read, not cleared: the targets use it after this
  z.glow -= dt * 2.5f;
  if (z.glow < 0) z.glow = 0;
  int n = 0;
  for (int i = 0; i < DZ_N; i++) {
    if (DZ_LAYOUT[i].kind != 0) continue;
    int h = s.h[2 * n < COLS ? 2 * n : COLS - 1];  // each pyramid hears the music a little later than the one before
    n++;
    float t = (float)(h - GROUND) / (MAX_BAR - GROUND);
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    z.hd[i] += (DZ_LAYOUT[i].max * (0.45f + 0.55f * t) - z.hd[i]) * (1.0f - expf(-dt / DZ_EASE_S));
  }
}

// The fighter slips up or down away from a bolt or a missile that is about to reach it.
static inline float dodgeFrom(float qx, float qy, float qvx, float qvy, int sx, float sy) {
  if (fabsf(qvx) < 1) return 0;
  float t = (sx + SHIP_W / 2.0f - qx) / qvx;  // when it gets to the fighter's x
  if (t < 0.05f || t > 0.7f) return 0;
  float y = qy + qvy * t;
  if (fabsf(y - (sy + SHIP_H / 2.0f)) < SHIP_H / 2.0f + 6) return y > sy + SHIP_H / 2.0f ? -K_EVADE_PX : K_EVADE_PX;
  return 0;
}
static float cruiserEvade(const State& s, int sx, float sy) {
  for (const auto& q : s.crb)
    if (q.on) { float e = dodgeFrom(q.x, q.y, q.vx, q.vy, sx, sy); if (e != 0) return e; }
  for (const auto& m : s.ms)
    if (m.on) { float e = dodgeFrom(m.x, m.y, m.vx, m.vy, sx, sy); if (e != 0) return e; }
  return 0;
}
// Moves the cruiser, its volleys of bolts (two or three at a time, straight at the fighter, from the guns that face it) and its smoke.
static void updateCruiser(State& s, uint32_t nowMs, float dt, int sx, int sy) {
  Cruiser& b = s.cr;
  if (b.on) {
    b.x -= CR_SPEED * dt;
    if (b.barrage == 0 && b.x <= CR_BARRAGE_X && b.volley == 0) b.barrage = 1, b.bt = 0;  // about halfway across: the warning
    if (b.barrage == 1) {
      b.bt += dt;
      if (b.bt >= CR_WARN_S) b.barrage = 2, b.fired = 0, b.nextMissile = nowMs;
    } else if (b.barrage == 2 && (int32_t)(nowMs - b.nextMissile) >= 0) {  // then the missiles, a moment apart, in a fan
      int i = b.fired++;
      float f = i / (float)(CR_MISSILES - 1), xd = CR_BAY0 + (CR_BAY1 - CR_BAY0) * f;  // the left ones leave from the left end of the bay
      for (auto& m : s.ms) {
        if (m.on) continue;
        m = CrMissile();
        m.on = true, m.seed = (uint8_t)i, m.v = CR_MISSILE_V0;
        m.x = (float)lrintf(b.x) + xd * DOT, m.y = CR_Y + (crBot(xd) + 1.5f) * DOT;
        m.th = (float)M_PI + (-70.0f + 140.0f * f) * (float)M_PI / 180.0f;
        break;
      }
      b.nextMissile = nowMs + (uint32_t)(CR_MISSILE_GAP_S * 1000);
      if (b.fired >= CR_MISSILES) b.barrage = 3, b.nextFire = nowMs + 2500;  // done: the ordinary volleys come back after a pause
    }
    if (b.barrage != 1 && b.barrage != 2 && (int32_t)(nowMs - b.nextFire) >= 0) {
      b.nextFire = nowMs + 1100 + nextRandom(s) % 900;
      b.volley = (uint8_t)(2 + nextRandom(s) % 2), b.volleyAt = nowMs;
    }
    if (b.volley > 0 && (int32_t)(nowMs - b.volleyAt) >= 0) {
      b.volley--, b.volleyAt = nowMs + 90;
      float fcx = sx + SHIP_W / 2.0f, fcy = sy + SHIP_H / 2.0f;
      bool below = fcy > CR_Y + 21 * DOT;  // the fighter is under the hull: the underside guns; otherwise the mounts on top
      const uint8_t* gun = below ? CR_GUNS[nextRandom(s) % 3] : CR_TOPGUNS[nextRandom(s) % 5];
      float gx = (float)(lrintf(b.x) + gun[0] * DOT + 1), gy = (float)(CR_Y + gun[1] * DOT);
      if (gx > 6 && gx < W - 4) {
        for (auto& q : s.crb) {
          if (q.on) continue;
          float tx = fcx + (int)(nextRandom(s) % 21) - 10, ty = fcy + (int)(nextRandom(s) % 17) - 8;  // straight at the fighter, a little off
          float d = sqrtf((tx - gx) * (tx - gx) + (ty - gy) * (ty - gy));
          if (d < 1) d = 1;
          q.on = true, q.x = q.ox = gx, q.y = q.oy = gy, q.vx = (tx - gx) / d * 130.0f, q.vy = (ty - gy) / d * 130.0f, q.age = 0;
          break;
        }
      }
    }
    b.puff -= dt;  // smoke from the places that have been hit: more hits, more places, the last one burns
    if (b.puff <= 0 && b.hits >= 2) {
      b.puff = 0.1f;
      int n = b.hits >= 6 ? 3 : b.hits >= 4 ? 2 : 1;
      for (int i = 0; i < n; i++)
        for (auto& p : s.puff) {
          if (p.on) continue;
          p.on = true, p.fire = i == 2, p.age = 0, p.x = (float)(lrintf(b.x) + CR_DAMAGE[i][0] * DOT), p.y = (float)(CR_Y + CR_DAMAGE[i][1] * DOT);
          break;
        }
    }
    if (b.x < -CR_W * DOT - CR_TRAIL - 4) b.on = false;
  }
  int off = scrollOffset(s, nowMs);
  for (auto& q : s.crb) {  // the bolts fly straight and are not swept away with the rest of the world
    if (!q.on) continue;
    q.x += q.vx * dt, q.y += q.vy * dt, q.age += dt;
    int bx = (int)q.x;
    int ground = H - heightAt(s, bx, off);
    if (q.x > sx - DOT && q.x < sx + SHIP_W && q.y + DOT > sy && q.y < sy + SHIP_H) {  // on the fighter: the shield takes it, with a bigger flash
      q.on = false;
      s.shieldUntil = nowMs + SHIELD_MS;
      freeBurst(s, (float)bx, q.y);
      freeBurst(s, (float)(bx - 6), q.y + 3);
    } else if (q.y + DOT >= ground) {
      q.on = false;
      carve(s, colAt(bx + 1, off), 2);
      freeBurst(s, (float)bx, (float)(ground - 1));
    } else if (bx < -DOT || bx > W + DOT || q.y < -DOT) {
      q.on = false;
    }
  }
  for (auto& m : s.ms) {  // the missiles: they speed up, turn toward the fighter and wobble a little
    if (!m.on) continue;
    m.age += dt;
    float fx = sx + SHIP_W / 2.0f, fy = sy + SHIP_H / 2.0f;
    float d = atan2f(fy - m.y, fx - m.x) - m.th;
    while (d > (float)M_PI) d -= 2.0f * (float)M_PI;
    while (d < -(float)M_PI) d += 2.0f * (float)M_PI;
    float turn = CR_MISSILE_TURN * dt;
    m.th += fmaxf(-turn, fminf(turn, d)) + sinf(m.age * 9.0f + m.seed) * 0.4f * dt;
    m.v = fminf(CR_MISSILE_VMAX, CR_MISSILE_V0 + 60.0f * m.age);
    m.vx = cosf(m.th) * m.v, m.vy = sinf(m.th) * m.v;
    m.x += m.vx * dt, m.y += m.vy * dt;
    m.tt -= dt;
    if (m.tt <= 0) {  // a new trail point, the older ones move along
      m.tt = 0.035f;
      for (int i = CR_TRAIL_N - 1; i > 0; i--) m.tx[i] = m.tx[i - 1], m.ty[i] = m.ty[i - 1];
      m.tx[0] = m.x, m.ty[0] = m.y;
      if (m.nt < CR_TRAIL_N) m.nt++;
    }
    int bx = (int)m.x;
    int ground = H - heightAt(s, bx, off);
    if (m.x > sx - DOT && m.x < sx + SHIP_W && m.y + DOT > sy && m.y < sy + SHIP_H) {  // on the fighter: the shield takes it, with a bigger flash
      m.on = false;
      s.shieldUntil = nowMs + SHIELD_MS;
      freeBurst(s, (float)bx, m.y);
      freeBurst(s, (float)(bx - 6), m.y + 3);
      freeBurst(s, (float)(bx + 5), m.y - 4);
    } else if (m.y + DOT >= ground) {
      m.on = false;
      carve(s, colAt(bx + 1, off), 2);
      freeBurst(s, (float)bx, (float)(ground - 1));
    } else if (m.age >= CR_MISSILE_LIFE) {  // it burns out in a burst
      m.on = false;
      freeBurst(s, m.x, m.y);
    } else if (bx < -8 || bx > W + 8 || m.y < -8) {
      m.on = false;
    }
  }
  for (auto& p : s.puff) {
    if (!p.on) continue;
    p.age += dt, p.y -= 9.0f * dt;
    if (p.age >= 1.3f) p.on = false;
  }
}

// ---- The city (S14-1): where the people are, who is saved, who comes out of a doorway. Designed in the web demo first. ----
static void newCity(State& s) {
  City& g = s.city;
  g = City();
  g.on = true;
  g.x = (float)(W + 4);
  g.n = CITY_N_MIN + (int)(nextRandom(s) % (CITY_N_MAX - CITY_N_MIN + 1));
  for (int i = 0; i < g.n; i++) {
    Building& b = g.b[i];
    bool collapsed = nextRandom(s) % 100 < 5;
    int f = collapsed ? 1 + (int)(nextRandom(s) % 2) : CITY_FLOOR_MIN + (int)(nextRandom(s) % (CITY_FLOOR_MAX - CITY_FLOOR_MIN + 1));
    int R = 1 + 2 * f;
    b.ox = i * CITY_W * DOT, b.f = (int8_t)f, b.h = R * DOT, b.collapsed = collapsed;
    for (int r = 0; r < f * 3; r++) { uint32_t q = nextRandom(s) % 100; b.lit[r] = q < 20 ? 1 : q < 26 ? 2 : 0; }
    // a little like a Space Invaders defence shield, but only at the corners: chamfered top corners, a small arch at the bottom
    // (the door), and a rough bite out of a corner now and then
    if (collapsed) for (int c = 0; c < CITY_W; c++) b.top[c] = (uint8_t)(nextRandom(s) % (2 * f));
    else {
      b.top[0] = b.top[9] = (uint8_t)(1 + nextRandom(s) % 2);
      b.top[1] = b.top[8] = (uint8_t)(nextRandom(s) % 2);
    }
    if (f >= 2) for (int r = R - 2; r < R; r++) for (int c = 4; c <= 5; c++) b.gone[r * CITY_W + c] = 1;
    const int CORN[4][2] = {{2, 0}, {2, CITY_W - 1}, {R - 3, 0}, {R - 3, CITY_W - 1}};
    for (const auto& cn : CORN) {
      if (collapsed || nextRandom(s) % 100 >= 34) continue;
      float rr = 0.9f + (nextRandom(s) % 100) / 100.0f * 0.8f;
      for (int dr = -2; dr <= 2; dr++)
        for (int dc = -2; dc <= 2; dc++) {
          int r = cn[0] + dr, c = cn[1] + dc;
          if (r < 2 || r >= R || c < 0 || c >= CITY_W || (r < 2 && c >= 3 && c <= 6)) continue;
          float d = hypotf((float)dr, (float)dc);
          if (d <= rr + 0.5f && (float)(nextRandom(s) % 100) < 100 * (1 - d / (rr + 1.2f)) + 25) b.gone[r * CITY_W + c] = 1;
        }
    }
    if (!collapsed && nextRandom(s) % 100 < 18) b.fire = (int8_t)(nextRandom(s) % 2 ? 9 : 0);
    if (!collapsed && f >= 3 && nextRandom(s) % 100 < 40) {  // somebody leans out of a lit window, waving
      b.hasWin = true, b.winFl = (int8_t)(nextRandom(s) % (f - 1)), b.lit[b.winFl * 3 + 1] = 1, b.winPh = (int8_t)(nextRandom(s) % 7);
    }
    if (!collapsed && !b.hasWin && nextRandom(s) % 100 < 45) {  // a building with somebody at a window has nobody on its roof: one waving person per building
      Person& p = g.p[g.np++];
      p.bi = (int8_t)i, p.ox = 2 + (int)(nextRandom(s) % (CITY_W * DOT - PERSON_W - 3)), p.ph = (uint8_t)(nextRandom(s) % 7), p.saved = false;
    }
  }
}
// where a person stands on the screen (px): the top left; the size is PERSON_W x PERSON_H
static inline void personAt(const City& g, const Person& p, int& x, int& y) {
  const Building& b = g.b[p.bi];
  x = (int)lrintf(g.x) + b.ox + p.ox, y = H - GROUND - b.h - PERSON_H;
}
// The nearest one still waiting ahead of the fighter: its middle height, or false.
static bool nearestWaiting(const State& s, int sx, float& midY) {
  const City& g = s.city;
  if (!g.on) return false;
  int best = 1 << 20;
  bool found = false;
  for (int i = 0; i < g.np; i++) {
    const Person& p = g.p[i];
    if (p.saved) continue;
    int x, y;
    personAt(g, p, x, y);
    if (x > sx + SHIP_W + 6 && x < W - 30 && x < best) best = x, midY = y + PERSON_H / 2.0f, found = true;
  }
  return found;
}
static void updateCity(State& s, uint32_t nowMs, float dt, int sx, int sy) {
  if (s.nextCityMs == 0) s.nextCityMs = nowMs + (s.cityForce ? 300 : CITY_FIRST_MS);
  City& g = s.city;
  if (g.on) {
    g.x -= SCROLL_PX_S * dt;
    for (int i = 0; i < g.np; i++) {
      Person& p = g.p[i];
      if (p.saved) continue;
      int x, y;
      personAt(g, p, x, y);
      if (fabsf(x + PERSON_W / 2.0f - (sx + SHIP_W / 2.0f)) < (SHIP_W + PERSON_W) / 2.0f && fabsf(y + PERSON_H / 2.0f - (sy + SHIP_H / 2.0f)) < (SHIP_H + PERSON_H) / 2.0f) {
        p.saved = true;  // saved: a small light
        for (auto& k : s.spark)
          if (!k.on) { k = {true, x + PERSON_W / 2.0f, y + PERSON_H / 2.0f, 0}; break; }
      }
    }
    if (g.x < -g.n * CITY_W * DOT) {
      g.on = false;
      s.nextCityMs = nowMs + CITY_MS_MIN + nextRandom(s) % (CITY_MS_MAX - CITY_MS_MIN);
    }
  } else if (!s.mz.ph && !s.dz.ph && (s.prevDb > DB_SILENT || s.cityForce) && (int32_t)(nowMs - s.nextCityMs) >= 0) {
    newCity(s);
  }
  // now and then a few people come out of a doorway all at once and run after the fighter; after a while each stops and waves
  if (g.on) {
    s.citAcc -= dt;
    if (s.citAcc <= 0) {
      int ok[CITY_N_MAX], nok = 0, used = 0;
      for (int i = 0; i < g.n; i++) {
        int dx = (int)lrintf(g.x) + g.b[i].ox + 5 * DOT;
        if (!g.b[i].collapsed && g.b[i].f >= 2 && dx > 110 && dx < W - 20) ok[nok++] = i;
      }
      for (const auto& c : s.cit) used += c.on;
      if (nok && used < 4) {
        const Building& b = g.b[ok[nextRandom(s) % nok]];
        int n = 1 + (int)(nextRandom(s) % 3);  // one to three people run out at once
        float x0 = (float)((int)lrintf(g.x) + b.ox + 5 * DOT - PERSON_W / 2);
        for (int k = 0; k < n; k++) {
          Citizen* c = nullptr;
          for (auto& q : s.cit) if (!q.on) { c = &q; break; }
          if (!c) break;
          *c = Citizen();
          c->on = true, c->x = x0, c->mode = 0, c->t = -k * 0.13f, c->ph = (uint8_t)(nextRandom(s) % 7);
          c->stop = 22 + (int)(nextRandom(s) % 70), c->back = nextRandom(s) % 100 < 35, c->backTo = 100 + (int)(nextRandom(s) % 60);
        }
      }
      s.citAcc = 5.0f + (nextRandom(s) % 50) / 10.0f;
    }
  }
  float fc = sx + SHIP_W / 2.0f;
  for (auto& c : s.cit) {
    if (!c.on) continue;
    c.t += dt;
    if (c.mode == 0) {
      c.x -= SCROLL_PX_S * dt;
      if (c.t >= 0) c.mode = 1;
    } else if (c.mode == 1) {
      c.x -= (SCROLL_PX_S + 42) * dt;
      if (c.x + PERSON_W / 2.0f - fc <= c.stop) c.mode = 3, c.t = 0;
    } else if (c.mode == 2) {  // running back to the right, against the ground
      c.x += (72 - SCROLL_PX_S) * dt;
      if (c.x + PERSON_W / 2.0f >= c.backTo) c.mode = 3, c.t = 0, c.back = false;
    } else {  // standing and waving, carried along by the ground
      c.x -= SCROLL_PX_S * dt;
      if (c.back && c.x + PERSON_W / 2.0f < 60) c.mode = 2;  // carried past the fighter: some turn round
    }
    if (c.x <= -PERSON_W - 2) c.on = false;
  }
  for (auto& k : s.spark) {
    if (!k.on) continue;
    k.t += dt;
    if (k.t >= 0.5f) k.on = false;
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

  updateTurns(s, nowMs, dt);
  if (s.eq) updateEq(s, dt);
  updateMeteorZone(s, nowMs, dt, sx);
  updateDesert(s, nowMs, dt);

  // Look at the terrain from the fighter to 60 px ahead, so it rises before a loud passage reaches it.
  int tallest = GROUND;
  for (int x = sx; x < sx + SHIP_W + 60; x += 2) {
    int h = heightAt(s, x, off);
    if (h > tallest) tallest = h;
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
  float waitY;
  if (!s.mz.ph && !threat && nearestWaiting(s, sx, waitY)) target = waitY - SHIP_H / 2.0f, ease = CHASE_EASE_S;  // somebody waits ahead: the fighter flies to them
  if (target < TOP + 4) target = TOP + 4;
  if (s.mz.u > 0) {  // in the zone the fighter keeps to its way through the meteors
    target += (mzPath(s, nowMs) - target) * s.mz.u;
    ease = 0.05f + (ease - 0.05f) * (1.0f - s.mz.u);
  }
  s.shipY += (target - s.shipY) * (1.0f - expf(-dt / ease));
  updateLights(s, nowMs, dt);
  float evadeTgt = updateKong(s, nowMs, dt, sx + SHIP_W / 2.0f, s.shipY + bobY(nowMs) + SHIP_H / 2.0f);
  if (evadeTgt == 0) evadeTgt = cruiserEvade(s, sx, s.shipY + bobY(nowMs));  // or from a bolt of the cruiser
  s.evadeCur += (evadeTgt - s.evadeCur) * (1.0f - expf(-dt / (evadeTgt != 0 ? 0.07f : 0.5f)));
  int sy = (int)lrintf(s.shipY + bobY(nowMs) + s.evadeCur);
  updateCity(s, nowMs, dt, sx, sy);
  updateCruiser(s, nowMs, dt, sx, sy);

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
  if (!s.mz.ph && (s.beat || (int32_t)(nowMs - s.nextSpawnMs) >= (int32_t)((music ? SPAWN_MS_MAX : SPAWN_MS_QUIET) - SPAWN_MS_MIN))) {
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
    st.x -= (st.near ? STAR_NEAR_PX_S : STAR_FAR_PX_S) * (1.0f + 9.0f * s.mz.u) * dt;
    if (st.x < -2) placeStar(s, st, W + 1);
  }

  if (s.mz.ph) {  // the fighter only weaves through
  } else if (aim) {  // lined up: shoot
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
  if (!s.mz.ph && !s.dz.ph && (int32_t)(nowMs - s.nextBombMs) >= 0) {  // no bombs on the pyramids
    s.nextBombMs = nowMs + interval(s, BOMB_MS_MIN, BOMB_MS_MAX);
    for (auto& b : s.b) {
      if (b.on) continue;
      b = {(float)(sx + 3 * DOT), (float)(sy + SHIP_H), BOMB_VX, 0, true};  // from the belly
      break;
    }
  }
}

// Nothing is drawn outside this rectangle (the whole screen unless a few things are redrawn over the fighter only).
static int clipX0 = 0, clipY0 = 0, clipX1 = W, clipY1 = H;
static inline void px(uint16_t* fb, int x, int y, uint16_t c) {
  if (x >= clipX0 && x < clipX1 && y >= clipY0 && y < clipY1) fb[y * W + x] = c;
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

static inline void fillRect(uint16_t* fb, int x, int y, int w, int h, uint16_t c) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) px(fb, xx, yy, c);
}

// A colour between a and b (both byte-swapped RGB565, as in the frame buffer): t 0 is a, 1 is b.
static inline uint16_t mix565(uint16_t a, uint16_t b, float t) {
  a = swap(a), b = swap(b);
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31, br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  int r = (int)lrintf(ar + (br - ar) * t), g = (int)lrintf(ag + (bg - ag) * t), bl = (int)lrintf(ab + (bb - ab) * t);
  return swap((uint16_t)((r << 11) | (g << 5) | bl));
}
struct Pt { float x, y; };
static inline Pt bez3(const Pt* p, float u) {  // a point on a cubic Bezier curve
  float v = 1 - u, a = v * v * v, b = 3 * u * v * v, c = 3 * u * u * v, d = u * u * u;
  return {a * p[0].x + b * p[1].x + c * p[2].x + d * p[3].x, a * p[0].y + b * p[1].y + c * p[2].y + d * p[3].y};
}
static inline int roundi(float v) { return (int)floorf(v + 0.5f); }

// One tentacle along a curve, on the body's own 3 px dot grid (gx, gy). Resting ones are three dots thick where they hang side by side
// (shadow in straight vertical lines, one in three columns) and two along the ground, ending in a one dot tip; stretched ones are four
// dots thick, then three, then two. A lit edge, a half tone and the dark body colour, and a fleck of shadow here and there.
static void drawSquidLeg(uint16_t* fb, const Pt* p, int gx, int gy, bool rest, const uint16_t* P, int neckX) {
  const uint16_t lit = P[0], dark = P[1], mid = mix565(lit, dark, 0.55f);
  float len = 0;
  for (int i = 0; i < 3; i++) len += sqrtf((p[i + 1].x - p[i].x) * (p[i + 1].x - p[i].x) + (p[i + 1].y - p[i].y) * (p[i + 1].y - p[i].y));
  int steps = (int)ceilf(len / DOT * 1.6f);
  if (steps < 8) steps = 8;
  for (int i = 0; i <= steps; i++) {
    float u = (float)i / steps;
    Pt q = bez3(p, u);
    int w = rest ? (u < 0.88f ? 3 : u < 0.96f ? 2 : 1) : (u < 0.3f ? 4 : u < 0.5f ? 3 : u < 0.8f ? 2 : 1);
    Pt q1 = bez3(p, fminf(1.0f, u + 0.02f)), q0 = bez3(p, fmaxf(0.0f, u - 0.02f));
    bool flat = fabsf(q1.x - q0.x) > fabsf(q1.y - q0.y);  // going sideways: the thickness is across, in y
    int cx = gx + DOT * roundi((q.x - gx) / DOT), cy = gy + DOT * roundi((q.y - gy) / DOT);
    for (int j = w - 1; j >= 0; j--) {
      int o = (j - (w >> 1)) * DOT, ox = flat ? cx : cx + o, oy = flat ? cy + o : cy;
      uint16_t col = j == 0 ? lit : (j == 1 && w > 2) ? mid : dark;
      int cc = (ox - gx) / DOT, rr = (oy - gy) / DOT;
      uint32_t h = ((uint32_t)(int32_t)cc * 73856093u) ^ ((uint32_t)(int32_t)rr * 19349663u);
      if (rest && !flat) {
        int m = ((cc - neckX) % 3 + 3) % 3;
        col = m == 0 ? dark : m == 1 ? mid : lit;
      } else if (h % 5 == 0 && col != dark) {
        col = col == lit ? mid : dark;
      }
      dot(fb, ox, oy, col);
    }
  }
}

// The squid. back pass (front false): the body and the tentacles that rest, behind the terrain; front pass: the tentacles that stretch,
// in front of everything. The body is a pointed bell of a head (widths in SQ_WID, a lit band two dots wide on its left edge), two
// glowing slanted eyes (one dot above and two below on each side, like the ape's) and a small mouth, 5 dots with its corners turned down,
// in the eyes' colour toned down. All in this visit's colours (KPAL).
static void drawSquid(const State& s, uint16_t* fb, uint32_t nowMs, bool front) {
  const Kong& k = s.k;
  if (k.ph == K_IDLE) return;
  const uint16_t* P = KPAL[k.pal < 7 ? k.pal : 0];
  int bx = (int)lrintf(k.x), by = squidTop(k);
  const SqSpec& sp = sqSpec(k.kind);
  if (!front) {
    const uint16_t mouth = mix565(P[2], P[1], 0.4f);
    for (int r = 0; r < sp.rows; r++)
      for (int c = 0; c < KONG_W; c++) {
        uint8_t t = sqCell(k.kind, r, c);
        if (t) dot(fb, bx + c * DOT, by + r * DOT, t == 3 ? P[2] : t == 4 ? mouth : t == 2 ? P[0] : P[1]);
      }
  }
  float nx = (float)(bx + sp.neckX * DOT + 1), ny = (float)(by + sp.neckY * DOT);
  bool reaching = k.ph == K_REACH || k.ph == K_GRAB || k.ph == K_MISS;
  float w = k.ph == K_REACH ? ease3(k.t / K_REACH_S) : reaching ? 1.0f : k.ph == K_LEAVE ? 1.0f - ease3(k.t / K_RETRACT_S) : 0.0f;  // 0 at rest ... 1 stretched out
  const SqReach* tab = SQ_SC[(k.sc == 0 && k.flip) ? 3 : k.sc];
  float gy = (float)(H - 8);
  for (int i = 0; i < sp.n; i++) {
    bool moving = w > 0 && ((k.legs >> i) & 1);
    if (moving != front) continue;  // resting tentacles are drawn with the body (behind), stretching ones in front
    Pt R0 = {nx + sp.root[i] * DOT, ny};
    // at rest: a packed bundle hanging down, flaring out like a trumpet (the outer ones start higher) and lying along the ground
    Pt p[4] = {R0, {R0.x, gy - sp.flare[i]}, {R0.x + sp.endx[i] * 0.2f * DOT, gy}, {R0.x + sp.endx[i] * DOT, gy + sp.endy[i] * DOT}};
    if (moving) {
      const SqReach* sc = nullptr;
      int want = i == 0 ? 0 : i == 1 ? 1 : i == sp.n - 2 ? 5 : 6;  // the scenarios name the squid's tentacles 0, 1, 5 and 6; the octopus's right pair is its last two
      for (int j = 0; j < 4; j++)
        if (tab[j].leg == want) sc = &tab[j];
      if (sc) {
        Pt q2 = {k.tx + sc->ox * DOT, k.ty + sc->oy * DOT};
        float wob = sinf(nowMs * 0.009f + i) * 0.8f * DOT * w;
        Pt r[4] = {R0, {nx + sc->d1x * DOT, ny + sc->d1y * DOT}, {q2.x + sc->d2x * DOT + wob, q2.y + sc->d2y * DOT}, q2};
        for (int n = 0; n < 4; n++) p[n].x += (r[n].x - p[n].x) * w, p[n].y += (r[n].y - p[n].y) * w;
      }
    }
    drawSquidLeg(fb, p, bx, by, !moving, P, sp.neckX);
  }
}

// A 3 x 3 px dot whose light is added to what is behind it (the stars show through). r, g, b: how much to add, 0 .. 255.
static void addDot(uint16_t* fb, int x, int y, int r, int g, int b) {
  int ar = r * 31 / 255, ag = g * 63 / 255, ab = b * 31 / 255;
  for (int dy = 0; dy < DOT; dy++)
    for (int dx = 0; dx < DOT; dx++) {
      int xx = x + dx, yy = y + dy;
      if ((unsigned)xx >= (unsigned)W || (unsigned)yy >= (unsigned)H) continue;
      uint16_t c = swap(fb[yy * W + xx]);
      int cr = ((c >> 11) & 31) + ar, cg = ((c >> 5) & 63) + ag, cb = (c & 31) + ab;
      if (cr > 31) cr = 31;
      if (cg > 63) cg = 63;
      if (cb > 31) cb = 31;
      fb[yy * W + xx] = swap((uint16_t)((cr << 11) | (cg << 5) | cb));
    }
}
// The meteors, their tails added to what is behind them, and the thin streaks far behind.
static void drawMeteors(const State& s, uint16_t* fb) {
  for (const auto& d : s.dust) {
    if (!d.on) continue;
    int x = (int)lrintf(d.x), y = (int)lrintf(d.y);
    for (int i = 0; i < d.len; i += 2) {
      float f = 1.0f - (float)i / d.len;
      px(fb, x + i, y, swap(rgb((int)(90 * f), (int)(110 * f), (int)(150 * f))));
    }
  }
  static const uint16_t CORE = swap(rgb(255, 255, 235)), HOT_W = swap(rgb(255, 214, 110)), HOT_C = swap(rgb(150, 220, 255));
  static const int8_t BIG[13][3] = {{0, 0, 1}, {-1, 0, 1}, {1, 0, 1}, {0, -1, 1}, {0, 1, 1}, {-1, -1, 0}, {1, -1, 0},
                                    {-1, 1, 0}, {1, 1, 0}, {-2, 0, 0}, {2, 0, 0}, {0, -2, 0}, {0, 2, 0}};
  for (const auto& m : s.met) {
    if (!m.on) continue;
    const MeteorSize& z = MZ_SIZE[m.size];
    int gx = DOT * (int)lrintf(m.x / DOT), gy = DOT * (int)lrintf(m.y / DOT);
    for (int i = z.tail; i >= 1; i--) {
      float f = 1.0f - (float)i / (z.tail + 1);
      int slope = (int)lrintf(-m.vy / m.v * i);
      int r = (int)((m.warm ? 255 : 70) * f), g = (int)((m.warm ? 70 + 150 * f : 150 + 80 * f) * f), b = (int)((m.warm ? 10 + 40 * f : 255) * f);
      addDot(fb, gx + i * DOT, gy + slope * DOT, r, g, b);
      if (m.size == 2 && i < z.tail * 0.6f) {
        addDot(fb, gx + i * DOT, gy + (slope - 1) * DOT, r / 2, g / 2, b / 2);
        addDot(fb, gx + i * DOT, gy + (slope + 1) * DOT, r / 2, g / 2, b / 2);
      }
    }
    uint16_t hot = m.warm ? HOT_W : HOT_C;
    if (m.size == 0) {
      dot(fb, gx, gy, CORE);
    } else if (m.size == 1) {
      dot(fb, gx, gy, CORE);
      dot(fb, gx + DOT, gy, hot);
      dot(fb, gx, gy - DOT, hot);
      dot(fb, gx, gy + DOT, hot);
    } else {
      for (const auto& d : BIG) dot(fb, gx + d[0] * DOT, gy + d[1] * DOT, d[2] ? CORE : hot);
    }
  }
}

// Is there a part of the monster at the screen point x, y? The ape's silhouette, or the squid's head (its tentacles let light through).
static bool monsterAt(const Kong& k, float x, float y) {
  if (k.ph == K_IDLE) return false;
  int bx = (int)lrintf(k.x), by = k.kind >= 1 ? squidTop(k) : H - KH;
  int c = (int)floorf((x - bx) / DOT), r = (int)floorf((y - by) / DOT);
  if (c < 0 || c >= KONG_W || r < 0) return false;
  if (k.kind >= 1) return sqCell(k.kind, r, c) != 0;   // the head only; the tentacles let light through
  return kongSeg(r, c) >= 0;
}
// The beams, in front of the monsters' bodies and the terrain (the monster's arms and the equalizer are drawn over them).
static void drawSearchlights(const State& s, uint16_t* fb, uint32_t nowMs) {
  const Kong& k = s.k;
  float midX = (float)((int)lrintf(k.x) + KONG_W * DOT / 2);  // the monster's centre line
  int fx = shipX(nowMs) + (int)lrintf(s.lunge), fy = (int)lrintf(s.shipY + bobY(nowMs) + s.evadeCur);  // where the fighter is drawn
  for (const auto& l : s.lt) {
    if (!l.on) continue;
    float sn = sinf(l.a), cs = cosf(l.a);
    int side = l.x < midX ? -1 : 1;  // which side of the monster the light stands on
    for (int t = 0; t < LIGHT_LEN; t += DOT) {
      float fade = 1.0f - (float)t / LIGHT_LEN, cx = l.x + sn * t, y = (float)(H - GROUND - 4) - cs * t;
      int hw = 1 + t / 42;  // the beam widens: dots either side of its middle
      if (monsterAt(k, cx, y) && (cx - midX) * side <= 0) break;  // it runs on over the monster up to its centre line, and stops there
      if (cx >= fx && cx < fx + SHIP_W && y >= fy && y < fy + SHIP_H) break;  // and on the fighter
      float f = powf(fade, 0.6f) * 0.95f;
      for (int j = -hw; j <= hw; j++) {
        float kf = f * (abs(j) == hw ? 0.5f : 1.0f);
        addDot(fb, DOT * roundi((cx + j * DOT) / DOT), DOT * roundi(y / DOT), (int)lrintf(150 * kf), (int)lrintf(190 * kf), (int)lrintf(255 * kf));
      }
    }
  }
}
// The searchlights' bases: a T of four cells, like a Tetris piece: three light grey cells and, on top, the yellow lamp. They stand on the
// terrain's second row (the bottom row stays colourful), where each beam starts.
static void drawLightBases(const State& s, uint16_t* fb) {
  static const uint16_t GREY = swap(rgb(206, 208, 214)), LAMP = swap(rgb(255, 226, 90));
  for (const auto& l : s.lt) {
    if (!l.on) continue;
    int gx = (int)lrintf(l.x) - 4, gy = H - GROUND - 2 * DOT;
    for (int i = 0; i < 3; i++) dot(fb, gx + i * DOT, gy + DOT, GREY);
    dot(fb, gx + DOT, gy, LAMP);
  }
}

// The monster's body, behind the terrain: a flat dark silhouette with a band of light along each part's left edge (wide on the
// shoulders and chest, thin on the waist) and two glowing slanted eyes in the small head, in this visit's colours (KPAL).
// Everything is drawn in the same 3 x 3 px dots as the other characters; the arms below follow the body's dot grid.
static void drawKongBody(const State& s, uint16_t* fb, uint32_t nowMs) {
  const Kong& k = s.k;
  if (k.ph == K_IDLE) return;
  if (k.kind >= 1) return drawSquid(s, fb, nowMs, false);  // the squid (S11-1) and the octopus (S15-1)
  const uint16_t* P = KPAL[k.pal < 7 ? k.pal : 0];
  int bx = (int)lrintf(k.x), by = H - KH;
  bool walking = k.ph == K_WALK || k.ph == K_LEAVE;
  int frame = (nowMs / 330) & 1;
  for (int r = 0; r < KONG_H; r++)
    for (int c = 0; c < KONG_W; c++) {
      int i = kongSeg(r, c);
      if (i < 0) continue;
      if (r == KONG_H - 1 && walking && (frame ? c >= 11 : c <= 10)) continue;  // the lifted foot of the walk
      int start = KONG_SPAN[r][i * 2];
      int w = r <= 1 ? 1 : r <= 15 ? 3 : r <= 18 ? 1 : i == 1 ? (r >= 27 ? 4 : 2) : 1;  // width of the lit band
      bool face = r >= 3 && r <= 6 && c >= 8 && c <= 14;
      bool eye = face && ((r == 4 && (c == 8 || c == 14)) || (r == 5 && (c == 9 || c == 10 || c == 12 || c == 13)));
      bool lit = !face && c - start < w;
      dot(fb, bx + c * DOT, by + r * DOT, eye ? P[2] : lit ? P[0] : P[1]);
    }
}

// A block of n x n dots whose middle dot sits on the body's dot grid point nearest to x, y (gx, gy: where the grid starts);
// dxDots shifts it right by that many dots.
static void kongBlk(uint16_t* fb, int gx, int gy, float x, float y, int n, uint16_t c, int dxDots = 0) {
  int o = DOT * (n >> 1);
  fillRect(fb, gx + DOT * (int)lrintf((x - gx) / DOT) - o + dxDots * DOT, gy + DOT * (int)lrintf((y - gy) / DOT) - o, n * DOT, n * DOT, c);
}
// A thick limb as a chain of such blocks.
static void kongLimb(uint16_t* fb, int gx, int gy, float x0, float y0, float x1, float y1, uint16_t c, int n, int dxDots = 0) {
  float len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
  int steps = (int)ceilf(len / DOT);
  if (steps < 1) steps = 1;
  for (int i = 0; i <= steps; i++) kongBlk(fb, gx, gy, x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps, n, c, dxDots);
}

// A two-link arm from the shoulder to the hand (clamped to its reach), the elbow bent out and down, never toward the body
// (side -1: the arm toward the fighter, +1: the far arm), all in 3 x 3 px dots on the body's grid. Lit colour first, then a dark
// core shifted right so the light shows on the left; the shoulder of the far arm stays dark. Then the hand: a dark palm of
// 3 x 3 dots and three claws of one or two dots along the arm, spread wide when the hand is open.
static void drawKongArm(uint16_t* fb, float sx, float sy, float hx, float hy, int side, bool open, int gx, int gy, const uint16_t* P) {
  const uint16_t lit = P[0], body = P[1];
  float dx = hx - sx, dy = hy - sy, d = sqrtf(dx * dx + dy * dy);
  if (d > 2 * K_ARM - 1) hx = sx + dx * (2 * K_ARM - 1) / d, hy = sy + dy * (2 * K_ARM - 1) / d, dx = hx - sx, dy = hy - sy, d = 2 * K_ARM - 1;
  if (d < 1) d = 1;
  float ux = dx / d, uy = dy / d, h = sqrtf(fmaxf(0.0f, K_ARM * K_ARM - d * d / 4));
  float nx = -uy, ny = ux;
  if (nx * side * 0.7f + ny * 0.7f < 0) nx = -nx, ny = -ny;
  float ex = sx + ux * d / 2 + nx * h, ey = sy + uy * d / 2 + ny * h;
  kongLimb(fb, gx, gy, sx, sy, ex, ey, side > 0 ? body : lit, 3);
  kongLimb(fb, gx, gy, ex, ey, hx, hy, lit, 4);  // the forearm is the thicker one
  kongLimb(fb, gx, gy, sx, sy, ex, ey, body, 2, 1);
  kongLimb(fb, gx, gy, ex, ey, hx, hy, body, 3);
  kongBlk(fb, gx, gy, hx, hy, 3, body);
  float spread = open ? 0.8f : 0.42f, a0 = atan2f(uy, ux);
  for (int f = -1; f <= 1; f++) {
    float a = a0 + f * spread;
    for (int t = 7; t <= (open ? 10 : 7); t += 3) {
      float bend = f * 0.05f * (t - 4);
      kongBlk(fb, gx, gy, hx + cosf(a + bend) * t, hy + sinf(a + bend) * t, 1, lit);
    }
  }
}

// Both arms, in front of everything. While it waits they hang still. Each reach goes for the fighter with one arm (k.arm: 0 the
// near one, 1 the far one, which catches the fighter as the monster walks past it) from the side drawn by lot (k.style: 0 from the
// right, straight in; 1 over the top, a smash; 2 from below, a scoop; 3 past the fighter and hooking back). The other arm helps
// with a pose of its own (st): 1 claws too, 2 pounds its chest, 3 goes high and smashes down, 4 stays down. While it makes itself
// big (K_THREAT) both arms go up, or one up and one down (k.threat) (S9-2, S9-3).
static inline float kongMix(float a, float b, float u) { return a + (b - a) * u; }
static void drawKongArms(const State& s, uint16_t* fb, uint32_t nowMs) {
  const Kong& k = s.k;
  if (k.ph == K_IDLE) return;
  if (k.kind >= 1) return drawSquid(s, fb, nowMs, true);  // the stretching tentacles
  const uint16_t* P = KPAL[k.pal < 7 ? k.pal : 0];
  int bx = (int)lrintf(k.x), by = H - KH;
  const float sy0 = (float)(by + 7 * DOT + 2);
  const float sx[2] = {(float)(bx + 7), (float)(bx + 21 * DOT)}, sy[2] = {sy0, sy0}, ds[2] = {-1.0f, 1.0f};
  const bool reaching = k.ph == K_REACH || k.ph == K_GRAB || k.ph == K_MISS;
  const float pound = (sinf(nowMs * 0.02f) + 1) * 0.5f;
  static const uint8_t ST[4] = {1, 2, 4, 3};
  const int st = ST[k.style & 3];
  const bool wp = k.style != 0;  // the way in: where the hand goes first (relative to the fighter) before it snaps onto it
  const float wpx = k.style == 1 ? k.tx + 6 : k.style == 2 ? k.tx + 4 : k.tx - 8, wpy = k.style == 1 ? k.ty - 36 : k.style == 2 ? k.ty + 36 : k.ty - 6;
  const float uR = k.ph == K_REACH ? ease3(k.t / K_REACH_S) : 1.0f;
  const float snap = k.ph == K_GRAB ? ease3(k.t / K_GRAB_S) : k.ph == K_MISS ? 1.0f : 0.0f;
  const bool fistOpen = k.ph == K_REACH || (k.ph == K_GRAB && k.t < K_GRAB_S * 0.5f);  // the fingers snap shut
  float hx[2], hy[2];
  bool open[2] = {false, false};
  for (int i = 0; i < 2; i++) {
    const float d = ds[i], gx = sx[i] + 4 * d, gy = sy[i] + K_HANG;  // where the hand hangs
    hx[i] = gx, hy[i] = gy;
    if (k.ph == K_THREAT) {  // making itself big
      float e = ease3(fminf(1.0f, fminf(k.t / 0.3f, (K_THREAT_S - k.t) / 0.3f)));
      bool up = k.threat == 0 || (k.threat == 1 ? i == 0 : i == 1);
      float goalx = up ? sx[i] + 10 * d : sx[i] + 16 * d, goaly = (up ? sy[i] - 46 : sy[i] + 34) + 2.5f * sinf(nowMs * 0.012f + i * 1.7f);
      hx[i] = kongMix(gx, goalx, e), hy[i] = kongMix(gy, goaly, e);
      open[i] = true;
      continue;
    }
    if (i == k.arm) {  // the arm that goes for the fighter
      if (reaching) {
        if (k.ph == K_REACH) {
          hx[i] = wp ? kongMix(gx, wpx, uR) : kongMix(sx[i], k.tx, uR);
          hy[i] = wp ? kongMix(gy, wpy, uR) : kongMix(sy[i], k.ty, uR);
        } else if (k.ph == K_GRAB && wp) {
          hx[i] = kongMix(wpx, k.tx, snap), hy[i] = kongMix(wpy, k.ty, snap);
        } else {
          hx[i] = k.tx, hy[i] = k.ty;
        }
        open[i] = fistOpen;
      } else if (k.ph == K_LEAVE && k.t < K_RETRACT_S && k.tx != 0) {  // the arm is drawn back
        float u = ease3(k.t / K_RETRACT_S);
        hx[i] = kongMix(k.tx, gx, u), hy[i] = kongMix(k.ty, gy, u);
      }
    } else {  // the other arm
      const float fx = k.tx + 10 * d, fy = k.ty - 8, upx = sx[i] + 2 * d, upy = sy[i] - 34;
      if (reaching) {
        if (st == 1) {
          float u = k.ph == K_REACH ? ease3((k.t - 0.12f) / (K_REACH_S - 0.12f)) : 1.0f;
          hx[i] = kongMix(gx, fx, u), hy[i] = kongMix(gy, fy, u);
          open[i] = fistOpen;
        } else if (st == 2) {
          float cx = sx[i] - d * (16 + 6 * pound), cy = sy[i] + 12 + 5 * pound;
          hx[i] = kongMix(gx, cx, uR), hy[i] = kongMix(gy, cy, uR);
        } else if (st == 3) {
          if (k.ph == K_REACH) hx[i] = kongMix(gx, upx, uR), hy[i] = kongMix(gy, upy, uR), open[i] = true;
          else hx[i] = kongMix(upx, fx, snap), hy[i] = kongMix(upy, fy, snap);
        }
      } else if (k.ph == K_LEAVE && k.t < K_RETRACT_S && k.tx != 0 && st != 4) {
        float u = ease3(k.t / K_RETRACT_S);
        float fromx = (st == 1 || st == 3) ? fx : sx[i] - d * 19, fromy = (st == 1 || st == 3) ? fy : sy[i] + 14;
        hx[i] = kongMix(fromx, gx, u), hy[i] = kongMix(fromy, gy, u);
      }
    }
  }
  for (int i = 1; i >= 0; i--) drawKongArm(fb, sx[i], sy[i], hx[i], hy[i], (int)ds[i], open[i], bx, by, P);
}

// ---- The city and its people (S14-1) ----
// SOUND TERRARIUM's waving runner (its wave frames, played in the order 0, 1, 2, 1) and its running frames (it runs to the right): w white,
// o orange, b blue.
static const char* const WAVER[3][11] = {
  {"....ww.w..", "...wwwww..", "...wwww...", "....ww....", "...wwww...", "..wwww....", "....ww....", "....ww....", "...wwww...", "...w..w...", "..ww..ww.."},
  {"....ww..w.", "...wwww.w.", "...wwww.w.", "....ww.w..", "...wwww...", "..wwww....", "....ww....", "....ww....", "...wwww...", "...w..w...", "..ww..ww.."},
  {"....ww....", "...wwww...", "...wwww...", "....ww....", "...wwwwwww", "..wwww....", "....ww....", "....ww....", "...wwww...", "...w..w...", "..ww..ww.."}};
static const char* const RUNNER[3][11] = {
  {"......b...", ".....www..", ".....www..", "....www...", "..wwowww..", ".wwbww..ww", "....ww....", "....www...", ".wwwwbww..", "......ww..", "......ww.."},
  {"......b...", ".....www..", ".....www..", ".....ww...", "....www...", "...wwwww..", "...wwwwbww", ".....www..", "....wwww..", "...www....", "....ww...."},
  {"......b...", ".....www..", ".....www..", ".....ww...", "...wwwwbb.", "..wwowww..", ".....ww...", "....wwww..", "...ww..ww.", "..ww...ww.", "..ww......"}};
static const uint8_t WAVE_SEQ[4] = {0, 1, 2, 1};
// The figure with a black outline, as in SOUND TERRARIUM; hh rows of it.
static void drawFigure(uint16_t* fb, const char* const* rows, int hh, int x, int y, bool mirror) {
  static const uint16_t FW = swap(rgb(255, 255, 255)), FO = swap(rgb(255, 140, 32)), FB = swap(rgb(40, 168, 255));
  auto at = [&](int yy, int xx) -> char { return (yy < 0 || yy >= hh || xx < 0 || xx >= PERSON_W) ? '.' : rows[yy][mirror ? PERSON_W - 1 - xx : xx]; };
  for (int yy = -1; yy <= hh; yy++)
    for (int xx = -1; xx <= PERSON_W; xx++) {
      if (at(yy, xx) != '.') continue;
      bool near = false;
      for (int oy = -1; oy <= 1 && !near; oy++)
        for (int ox = -1; ox <= 1; ox++)
          if (at(yy + oy, xx + ox) != '.') { near = true; break; }
      if (near) px(fb, x + xx, y + yy, 0);
    }
  for (int yy = 0; yy < hh; yy++)
    for (int xx = 0; xx < PERSON_W; xx++) {
      char ch = at(yy, xx);
      if (ch != '.') px(fb, x + xx, y + yy, ch == 'w' ? FW : ch == 'o' ? FO : FB);
    }
}
static void drawCity(const State& s, uint16_t* fb, uint32_t nowMs) {
  static const uint16_t BODY = swap(rgb(52, 62, 94)), EDGE = swap(rgb(116, 134, 186)), ROOF = swap(rgb(158, 172, 210)), OFF = swap(rgb(16, 20, 34)),
                        ON = swap(rgb(255, 214, 110)), FIRE1 = swap(rgb(255, 150, 40)), FIRE2 = swap(rgb(255, 70, 20));
  const City& g = s.city;
  if (g.on) {
    int blink = (int)(nowMs / 140);
    for (int bi = 0; bi < g.n; bi++) {
      const Building& b = g.b[bi];
      int bx = (int)lrintf(g.x) + b.ox, by = H - GROUND - b.h, R = b.h / DOT;
      if (bx > W || bx < -CITY_W * DOT) continue;
      for (int r = 0; r < R; r++)
        for (int c = 0; c < CITY_W; c++) {
          if (b.f >= 2 && r >= R - 2 && c >= 4 && c <= 5) { dot(fb, bx + c * DOT, by + r * DOT, 0); continue; }  // the doorway: black
          if (r < b.top[c] || b.gone[r * CITY_W + c]) continue;
          bool open = r == b.top[c] || (r > 0 && b.gone[(r - 1) * CITY_W + c]);  // nothing above: a lit edge
          uint16_t col = BODY;
          if (open) col = ROOF;
          else if (c == 0) col = EDGE;
          else if ((r - 1) % 2 == 0 && c % 3 != 0) {  // windows: 2 x 1 dots, like one block of the terrain
            int w = b.lit[((r - 1) / 2) * 3 + (c - 1) / 3];
            col = w == 1 ? ON : w == 2 ? (((blink + bi) & 1) ? FIRE1 : FIRE2) : OFF;
          }
          dot(fb, bx + c * DOT, by + r * DOT, col);
        }
      if (b.fire >= 0) {  // a fire on the broken corner, flickering, and its smoke rising and thinning
        int fx = bx + b.fire * DOT, fy = by + b.top[(int)b.fire] * DOT, f1 = (blink + bi) & 1;
        dot(fb, fx, fy - DOT, f1 ? FIRE1 : FIRE2);
        dot(fb, fx, fy - 2 * DOT, f1 ? FIRE2 : FIRE1);
        if (((blink + bi) & 3) != 0) dot(fb, fx + (f1 ? DOT : -DOT), fy - DOT, FIRE2);
        for (int k = 0; k < 4; k++) {
          float u = fmodf(nowMs / 1100.0f + k / 4.0f + bi * 0.37f, 1.0f);
          int sx = fx + roundi(sinf(u * 5 + k * 1.7f) * 4) - 1, sy = fy - 3 * DOT - roundi(u * 26), v = roundi(150 * (1 - u)) + 30;
          dot(fb, sx, sy, swap(rgb(v, v, v + 12)));
        }
      }
      if (b.hasWin) {  // head and a waving hand out of the window: the upper body of the waver, its bottom row on the sill
        int wx = bx + (1 + 1 * 3) * DOT + DOT - 5, wy = by + (1 + 2 * b.winFl) * DOT + DOT - 6;
        drawFigure(fb, WAVER[WAVE_SEQ[(nowMs / 200 + b.winPh) % 4]], 6, wx, wy, false);
      }
    }
    for (int i = 0; i < g.np; i++) {
      const Person& p = g.p[i];
      if (p.saved) continue;
      int x, y;
      personAt(g, p, x, y);
      drawFigure(fb, WAVER[WAVE_SEQ[(nowMs / 230 + p.ph) % 4]], PERSON_H, x, y, false);
    }
  }
  for (const auto& c : s.cit) {  // the people who came out of the buildings
    if (!c.on || c.mode == 0) continue;
    bool run = c.mode == 1 || c.mode == 2;
    int gy = H - GROUND - PERSON_H;
    drawFigure(fb, run ? RUNNER[(nowMs / 110) % 3] : WAVER[WAVE_SEQ[(nowMs / 230 + c.ph) % 4]], PERSON_H, (int)lrintf(c.x), gy, c.mode == 1);  // running left: mirrored
  }
  for (const auto& k : s.spark) {  // a small light where somebody was saved: a plus that opens, then fades
    if (!k.on) continue;
    float u = k.t / 0.5f, a = 1 - u;
    int r = 1 + roundi(u * 2), cr = (int)lrintf(255 * a + 40), cb = (int)lrintf(180 * a + 20);
    if (cr > 255) cr = 255;
    uint16_t c = swap(rgb(cr, cr, cb));
    const int D[5][2] = {{0, 0}, {r, 0}, {-r, 0}, {0, r}, {0, -r}};
    for (const auto& d : D) dot(fb, (int)lrintf(k.x - 1) + d[0] * DOT, (int)lrintf(k.y - 1) + d[1] * DOT, c);
  }
}

// Draws the whole frame (black sky with stars, terrain, bursts, targets, mines, their bullets, laser beams, bombs, fighter) into fb.
// The cruiser: the hull from CR_ART, the engines' glow and exhaust, and the smoke or fire from the places that were hit.
static uint16_t crColor(char ch) {
  static const uint16_t S = swap(rgb(206, 212, 222)), H = swap(rgb(242, 246, 252)), M = swap(rgb(138, 146, 162)), D = swap(rgb(80, 88, 106)),
                        K = swap(rgb(34, 38, 52)), L = swap(rgb(184, 192, 206)), W = swap(rgb(255, 222, 150)), G = swap(rgb(246, 249, 255)),
                        C = swap(rgb(110, 226, 226)), R = swap(rgb(150, 50, 62));
  switch (ch) {
    case 'S': return S;
    case 'H': return H;
    case 'M': return M;
    case 'D': return D;
    case 'K': return K;
    case 'L': return L;
    case 'W': return W;
    case 'G': return G;
    case 'C': return C;
    default: return R;
  }
}
static void drawCruiser(const State& s, uint16_t* fb, uint32_t nowMs) {
  const Cruiser& b = s.cr;
  if (!b.on) return;
  int bx = (int)lrintf(b.x);
  uint32_t fr = nowMs / 70;
  static const uint16_t E1 = swap(rgb(120, 176, 255)), E2 = swap(rgb(180, 220, 255));
  for (int r = 0; r < CR_H; r++)
    for (int c = 0; c < CR_W; c++) {
      int x = bx + c * DOT;
      if (x + DOT <= 0 || x >= W) continue;
      char ch = CR_ART[r][c];
      if (ch == '.') continue;
      dot(fb, x, CR_Y + r * DOT, ch == 'E' ? (((fr + r) & 1) ? E1 : E2) : crColor(ch));
    }
  if (b.barrage == 1) {  // the warning: the underside glows red, flickering faster toward the launch
    float u = b.bt / CR_WARN_S;
    bool on = ((nowMs / (uint32_t)(170.0f - 110.0f * u)) & 1) == 0;
    for (int xd = CR_BAY0 - 2; xd <= CR_BAY1 + 2; xd++) {
      int x = bx + xd * DOT;
      if (x + DOT <= 0 || x >= W) continue;
      dot(fb, x, CR_Y + ((int)floorf(crBot((float)xd) + 0.5f) + 1) * DOT, on ? swap(rgb(255, 70, 40)) : swap(rgb(140, 28, 22)));
    }
  }
  for (int e = 0; e < 3; e++) {  // the engines' exhaust: a flickering streak of blue behind each
    int ey = CR_ENGINES[e];
    int n = 5 + (int)((fr * 2654435761u + (uint32_t)ey * 7u) % 5u);
    for (int k = 0; k < n; k++) {
      int x = bx + (CR_W + k) * DOT;
      if (x + DOT <= 0 || x >= W) continue;
      float f = 1.0f - k / 10.0f;
      dot(fb, x, CR_Y + ey * DOT, swap(rgb((int)(40 + 110 * f), (int)(80 + 120 * f), (int)(150 + 105 * f))));
    }
  }
  for (const auto& p : s.puff) {
    if (!p.on) continue;
    float f = fmaxf(0.0f, 1.0f - p.age / 1.3f);
    int v = (int)lrintf(40 + 110 * f);
    uint16_t col = (p.fire && p.age < 0.5f) ? (((nowMs / 70) & 1) ? swap(rgb(255, 150, 40)) : swap(rgb(255, 80, 20))) : swap(rgb(v, v, v + 4));
    dot(fb, (int)lrintf(p.x + p.age * 6.0f), (int)lrintf(p.y), col);
  }
}
// The barrage's missiles: a smoke trail (older points greyer and fainter), a flame, a bright head and a flash at the launch.
static void drawCruiserMissiles(const State& s, uint16_t* fb, uint32_t nowMs) {
  for (const auto& m : s.ms) {
    if (!m.on) continue;
    for (int i = 0; i < m.nt; i++) {
      float f = 1.0f - i / (float)m.nt;
      int v = (int)lrintf(60 + 110 * f);
      fillRect(fb, roundi(m.tx[i]) - 1, roundi(m.ty[i]) - 1, 2, 2, swap(rgb(v, v > 12 ? v - 12 : 0, v > 32 ? v - 32 : 0)));
    }
    float ux = cosf(m.th), uy = sinf(m.th);
    fillRect(fb, roundi(m.x - ux * 4) - 1, roundi(m.y - uy * 4) - 1, 2, 2, swap(rgb(255, 200, 80)));  // the flame
    fillRect(fb, roundi(m.x - ux * 7) - 1, roundi(m.y - uy * 7) - 1, 2, 2, (((nowMs / 60) + m.seed) & 1) ? swap(rgb(255, 110, 30)) : swap(rgb(255, 150, 50)));
    dot(fb, (int)m.x - 1, (int)m.y - 1, swap(rgb(255, 240, 205)));  // the head
    if (m.age < 0.1f) {
      static const int8_t F[5][2] = {{0, 0}, {-DOT, 0}, {DOT, 0}, {0, -DOT}, {0, DOT}};
      for (const auto& d : F) dot(fb, roundi(m.x) - 1 + d[0], roundi(m.y) - 1 + d[1], (d[0] || d[1]) ? swap(rgb(255, 150, 50)) : swap(rgb(255, 255, 255)));
    }
  }
}
// The cruiser's bolts: a long tail fading into green, a white-green head, and a muzzle flash at the gun for a moment.
static void drawCruiserBolts(const State& s, uint16_t* fb) {
  for (const auto& q : s.crb) {
    if (!q.on) continue;
    float l = sqrtf(q.vx * q.vx + q.vy * q.vy);
    if (l < 1) l = 1;
    float ux = q.vx / l, uy = q.vy / l;
    for (int i = 7; i >= 1; i--) {
      float f = 1.0f - i / 8.0f;
      int w = i < 3 ? 3 : 2;
      fillRect(fb, (int)lrintf(q.x - ux * i * 3.5f) - 1, (int)lrintf(q.y - uy * i * 3.5f) - 1, w, w, swap(rgb((int)(30 + 110 * f), (int)(120 + 135 * f), (int)(50 + 130 * f))));
    }
    dot(fb, (int)q.x - 1, (int)q.y - 1, swap(rgb(230, 255, 235)));
    if (q.age < 0.12f) {
      static const int8_t F[5][2] = {{0, 0}, {-DOT, 0}, {DOT, 0}, {0, -DOT}, {0, DOT}};
      for (const auto& d : F) dot(fb, (int)lrintf(q.ox) - 1 + d[0], (int)lrintf(q.oy) - 1 + d[1], (d[0] || d[1]) ? swap(rgb(120, 255, 150)) : swap(rgb(255, 255, 255)));
    }
  }
}

// ---- The desert zone (S17-1) ----
// The moon: SOUND TERRARIUM's moon face (the same face as its sun, 20 px across: brows, eyes, nose, lips and chin in fine lines), full,
// rising a little and brightening as the zone comes in.
static const int8_t MOON_FACE[16][4] = {{-6, -4, -4, -5}, {-4, -5, -2, -4}, {2, -4, 4, -5}, {4, -5, 6, -4},   // brows
                                        {-6, -2, -4, -3}, {-4, -3, -2, -2}, {2, -2, 4, -3}, {4, -3, 6, -2},   // eyes
                                        {0, -1, -1, 2},   {-1, 2, 0, 3},    {0, 3, 2, 2},                     // nose
                                        {-2, 5, 0, 4},    {0, 4, 2, 5},     {-2, 6, 0, 7},  {0, 7, 2, 6},     // lips
                                        {-1, 8, 1, 8}};                                                       // chin
static const int8_t MOON_DOTS[3][2] = {{-4, -2}, {4, -2}, {0, 4}};  // the pupils and the middle of the lips
static void drawMoon(const State& s, uint16_t* fb) {
  float u = s.dz.u;
  if (u <= 0) return;
  const int cx = 196, cy = 30 - (int)lrintf(8 * (1 - u)), R = 10;
  uint16_t face = mix565(0, swap(rgb(247, 210, 74)), u), ink = mix565(0, swap(rgb(66, 65, 33)), u);
  for (int yy = -R; yy <= R; yy++)
    for (int xx = -R; xx <= R; xx++)
      if (xx * xx + yy * yy <= R * R) px(fb, cx + xx, cy + yy, face);
  for (int a = 0; a < 64; a++) px(fb, cx + (int)lrintf(R * cosf(a * (float)M_PI / 32)), cy + (int)lrintf(R * sinf(a * (float)M_PI / 32)), ink);  // the rim
  for (const auto& l : MOON_FACE) {
    int n = abs(l[2] - l[0]) > abs(l[3] - l[1]) ? abs(l[2] - l[0]) : abs(l[3] - l[1]);
    for (int i = 0; i <= n; i++) px(fb, cx + (int)lrintf(l[0] + (l[2] - l[0]) * (float)i / n), cy + (int)lrintf(l[1] + (l[3] - l[1]) * (float)i / n), ink);
  }
  for (const auto& d : MOON_DOTS) px(fb, cx + d[0], cy + d[1], ink);
}
// The pyramids, behind the dunes: lit on the moon's side, in courses of stone, with a golden capstone that a beat lights up.
static void drawPyramids(const State& s, uint16_t* fb) {
  const DesertZone& z = s.dz;
  if (!z.ph) return;
  static const uint16_t LIT = swap(rgb(232, 194, 124)), LIT2 = swap(rgb(214, 174, 106)), SHADE = swap(rgb(132, 94, 54)),
                        SHADE2 = swap(rgb(116, 82, 46)), EDGE = swap(rgb(252, 226, 168));
  uint16_t cap = mix565(swap(rgb(220, 170, 40)), swap(rgb(255, 250, 190)), z.glow);
  const int gy = H - GROUND;
  for (int i = 0; i < DZ_N; i++) {
    const DzItem& d = DZ_LAYOUT[i];
    if (d.kind != 0) continue;
    int cx = DOT * (int)lrintf((z.x + d.ox) / DOT), hd = (int)lrintf(z.hd[i]);
    if (cx - hd * DOT > W || cx + hd * DOT < 0) continue;
    for (int r = 0; r < hd; r++) {
      int y = gy - (hd - r) * DOT;
      bool course = r & 1;
      for (int c = -r; c <= r; c++) {
        uint16_t col = r == 0 ? cap : c == -r ? EDGE : c < 0 ? (course ? LIT2 : LIT) : c == 0 ? LIT2 : (course ? SHADE2 : SHADE);
        dot(fb, cx + c * DOT, y, col);
      }
    }
    if (z.glow > 0.05f) {  // a beat: a glint above the tip
      int a = (int)lrintf(120 * z.glow);
      addDot(fb, cx, gy - (hd + 1) * DOT, a, a, (int)lrintf(a * 0.4f));
    }
  }
}
// In front of the dunes: the sphinx (its eyes glow on a beat) and somebody waving to the fighter.
static void drawSphinx(const State& s, uint16_t* fb, uint32_t nowMs) {
  const DesertZone& z = s.dz;
  if (!z.ph) return;
  static const uint16_t L = swap(rgb(232, 194, 124)), S = swap(rgb(132, 94, 54)), D = swap(rgb(70, 48, 30)),
                        GOLD = swap(rgb(226, 182, 60)), BLUE = swap(rgb(52, 84, 170));
  uint16_t eye = mix565(swap(rgb(40, 28, 20)), swap(rgb(120, 255, 255)), z.glow);
  const int gy = H - GROUND;
  for (int i = 0; i < DZ_N; i++) {
    const DzItem& d = DZ_LAYOUT[i];
    int cx = DOT * (int)lrintf((z.x + d.ox) / DOT);
    if (d.kind == 2) {
      if (cx > -PERSON_W && cx < W) drawFigure(fb, WAVER[WAVE_SEQ[(nowMs / 230) % 4]], 11, cx, gy - PERSON_H, false);
      continue;
    }
    if (d.kind != 1 || cx > W || cx < -SPHINX_W * DOT) continue;
    for (int r = 0; r < SPHINX_H; r++)
      for (int c = 0; c < SPHINX_W; c++) {
        char ch = SPHINX[r][c];
        if (ch == '.') continue;
        uint16_t col = ch == 'N' ? ((r & 1) ? BLUE : GOLD) : ch == 'L' ? L : ch == 'S' ? S : ch == 'D' ? D : eye;
        dot(fb, cx + c * DOT, gy - (SPHINX_H - r) * DOT, col);
      }
  }
}

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
    for (int t = (int)lrintf(s.mz.u * (st.near ? 10 : 4)); t > 0; t--) px(fb, x + 1 + t, y, STAR_C[st.kind]);  // streaks in the zone
  }

  drawMoon(s, fb);             // the desert's moon
  drawCruiser(s, fb, nowMs);   // the cruiser: in the sky, behind everything but the stars
  drawKongBody(s, fb, nowMs);  // behind the terrain
  drawPyramids(s, fb);         // behind the dunes

  // In the desert the terrain (and the equalizer) turns to sand.
  float du = s.dz.u;
  uint16_t litNow[MAX_BAR];
  const uint16_t* lit = s.lit;
  if (du > 0) {
    for (int k = 0; k < MAX_BAR; k++) litNow[k] = s.lit[k] ? mix565(s.lit[k], s.sand[k], du) : 0;
    lit = litNow;
  }
  for (int i = 0; i < COLS; i++) {
    int x0 = colX(i, off), h = mzHeight(s, s.h[i]);
    for (int dx = 0; dx < BAR_W; dx++) {
      int x = x0 + dx;
      if ((unsigned)x >= (unsigned)W) continue;
      for (int k = 0; k < h; k++) fb[(H - 1 - k) * W + x] = lit[k];  // the sky above stays black
    }
  }
  drawSphinx(s, fb, nowMs);        // in front of the dunes
  drawSearchlights(s, fb, nowMs);  // in front of the terrain and the monster's body, behind the equalizer
  drawKongArms(s, fb, nowMs);      // both arms: in front of the terrain and the beams, behind the city
  drawCity(s, fb, nowMs);          // the city and its people: in front of the arms, behind the equalizer
  drawLightBases(s, fb);           // the searchlights' bases, on the ground, in front of the city
  {  // the fighter: in front of the city, behind the equalizer. The arms are drawn once more over it, but only where it is, so a hand
     // or a tentacle closes in front of the fighter while the buildings stay in front of the arms.
    static const uint16_t Wh = swap(rgb(236, 236, 236)), B = swap(rgb(120, 128, 160)), M = swap(rgb(224, 48, 200)),
                          C = swap(rgb(0, 224, 255)), F1 = swap(rgb(255, 120, 0)), F2 = swap(rgb(255, 240, 96));
    int sx = shipX(nowMs) + (int)lrintf(s.lunge), sy = (int)lrintf(s.shipY + bobY(nowMs) + s.evadeCur);
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
    if (s.mz.u > 0.5f) {  // a longer flame at the zone's speed
      dot(fb, sx - 2 * DOT, sy + 2 * DOT, F1);
      if ((nowMs / 60) & 1) dot(fb, sx - 3 * DOT, sy + 2 * DOT, F2);
    }
    clipX0 = sx - 3 * DOT, clipY0 = sy - 2 * DOT, clipX1 = sx + SHIP_W + 2 * DOT, clipY1 = sy + SHIP_H + 2 * DOT;
    drawKongArms(s, fb, nowMs);
    clipX0 = 0, clipY0 = 0, clipX1 = W, clipY1 = H;
  }
  if (s.eq) {  // the equalizer in front: its own gap rows black, so it reads as LEDs over the terrain
    static const uint16_t PEAK = swap(rgb(255, 255, 255));
    for (int j = 0; j < EQ_BANDS; j++) {
      int x0 = j * PITCH + (PITCH - BAR_W) / 2, h = eqHeight(s, j);
      int peak = s.scale >= 1 && s.mz.u <= 0 && s.dz.u <= 0 ? ((int)lrintf(s.eqPeak[j]) - 1) * SEG_PITCH : -1;  // peak dot: its top segment
      if (peak < h) peak = -1;
      for (int dx = 0; dx < BAR_W; dx++) {
        int x = x0 + dx;
        uint16_t col = du > 0 ? mix565(s.eqCol[s.pattern - 1][j], s.sand[0], du) : s.eqCol[s.pattern - 1][j];
        for (int k = 0; k < h; k++) fb[(H - 1 - k) * W + x] = k % SEG_PITCH < SEG_H ? (du > 0 ? mix565(s.eqCol[s.pattern - 1][j], s.sand[k], du) : col) : 0;
        if (peak >= 0)
          for (int k = peak; k < peak + SEG_H; k++) fb[(H - 1 - k) * W + x] = PEAK;
      }
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
  drawCruiserBolts(s, fb);
  drawCruiserMissiles(s, fb, nowMs);
  drawMeteors(s, fb);

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

}

}  // namespace flightviz
