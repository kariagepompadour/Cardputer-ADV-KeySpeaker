/*
  Sinc48.h — KeySpeaker MP3 feasibility spike: streaming mono resampler from any rate up to 48 kHz to 48 kHz

  RateAdjust.h's 4-point Hermite is right for +-200 ppm but not for a fixed 44.1 -> 48 kHz conversion: it passes the
  tone's level but adds images, so a 10 kHz tone comes out only 24 dB above its own distortion (host test, 2026-11-13).
  This one is a 16-tap Kaiser-windowed sinc (beta 8) with 64 phases and linear interpolation between neighbouring
  phases (float, 65 x 16 table = 4,160 bytes): 80 dB or better up to 15 kHz at 44.1 kHz in the same host test.

  Plain C++ with no Arduino dependencies, so it can be tested on a computer. No allocation: the caller owns the table
  and the work buffer through this object, which is meant to be created once when the MP3 input is switched on.

  Use: fill in() with up to MAXIN mono samples, call process(n, out, maxOut), repeat. The last 16 input samples are kept
  between calls. The output is at 48 kHz; with fs = 48000 the samples pass through (delayed by 8).
*/
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

namespace sinc48 {

constexpr int TAPS = 16, PHASES = 64, HIST = TAPS;  // history samples kept in front of each block
constexpr size_t MAXIN = 1152;                       // one MPEG-1 Layer III granule pair (mono)

struct Resampler {
  float table[PHASES + 1][TAPS];
  int16_t work[HIST + MAXIN];
  bool built = false;  // the table is built once; a new file only sets the rate (the build took 246 ms on the device)
  uint64_t pos = 0;    // the next output instant, in input samples counted from work[0], Q32
  uint64_t step = 0;   // input samples per output sample, Q32
  size_t have = 0;     // valid samples in work (HIST + the block)

  static double bessel0(double x) {
    double s = 1, t = 1;
    for (int k = 1; k < 30; k++) {
      t *= (x / 2 / k) * (x / 2 / k);
      s += t;
    }
    return s;
  }

  // Sets the source rate (and builds the table the first time: about 250 ms on the ESP32-S3, measured).
  void begin(uint32_t fs) {
    if (!built) buildTable();
    memset(work, 0, sizeof work);
    have = HIST;
    step = (uint64_t)(((double)fs / 48000.0) * 4294967296.0 + 0.5);
    pos = (uint64_t)HIST << 32;  // the first output sits on the first real input sample
  }
  void buildTable() {
    built = true;
    for (int p = 0; p <= PHASES; p++) {
      double fr = (double)p / PHASES, sum = 0, h[TAPS];
      for (int k = 0; k < TAPS; k++) {
        double d = (k - (TAPS / 2 - 1)) - fr;  // distance of tap k from the output instant
        double r = d / (TAPS / 2.0);
        double w = fabs(r) >= 1 ? 0 : bessel0(8.0 * sqrt(1 - r * r)) / bessel0(8.0);
        double s = fabs(d) < 1e-12 ? 1 : sin(M_PI * d) / (M_PI * d);
        h[k] = s * w;
        sum += h[k];
      }
      for (int k = 0; k < TAPS; k++) table[p][k] = (float)(h[k] / sum);  // unity gain at DC for every phase
    }
  }
  void reset() {
    memset(work, 0, sizeof work);
    have = HIST;
    pos = (uint64_t)HIST << 32;
  }

  int16_t* in() { return work + HIST; }

  // n new samples are in in(). Returns the number of 48 kHz samples written to out (at most maxOut).
  size_t process(size_t n, int16_t* out, size_t maxOut) {
    have = HIST + n;
    size_t produced = 0;
    for (;;) {
      uint32_t i = (uint32_t)(pos >> 32);  // work[i + 1 - TAPS/2 ..] is the first tap
      if (i + (TAPS / 2) + 1 > have || produced >= maxOut) break;
      uint32_t frac = (uint32_t)pos;       // Q32
      uint32_t ph = frac >> 26;             // 0 .. 63
      float u = (float)(frac & 0x03FFFFFFu) * (1.0f / 67108864.0f);
      const float* a = table[ph];
      const float* b = table[ph + 1];
      const int16_t* x = work + i - (TAPS / 2 - 1);
      float ya = 0, yb = 0;
      for (int k = 0; k < TAPS; k++) {
        float v = (float)x[k];
        ya += v * a[k];
        yb += v * b[k];
      }
      float y = ya + (yb - ya) * u;
      out[produced++] = y > 32767.0f ? 32767 : y < -32768.0f ? -32768 : (int16_t)lrintf(y);
      pos += step;
    }
    // Keep the last HIST samples in front of the next block and move the position with them.
    size_t drop = have - HIST;
    memmove(work, work + drop, HIST * sizeof(int16_t));
    pos -= (uint64_t)drop << 32;
    have = HIST;
    return produced;
  }
};

}  // namespace sinc48
