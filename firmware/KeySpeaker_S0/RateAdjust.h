/*
  RateAdjust.h — KeySpeaker (USB Speaker & BLE Keyboard): clock-drift correction for the USB speaker (stage S1, docs/DESIGN_JA.md 4.2)

  The computer sends 48000 samples per second by its own clock and the speaker plays 48000 per second by the
  Cardputer's clock. The two differ by a few tens of ppm (S0's 6-hour test: the computer about 10-17 ppm slower), so
  without correction the buffer slowly drains or fills and the sound breaks every hour or two.

  playTask asks for one 10 ms chunk at a time. Instead of reading exactly 480 samples from the ring for it, it reads
  480 * (1 + ppm / 1e6) on average and resamples them to 480 (4-point Hermite interpolation). The Controller picks the
  ppm from the buffer level: a buffer above the target plays a little faster, below it a little slower. The change is
  at most +-200 ppm (0.35 cent of pitch), far below what anyone can hear. At 0 ppm the samples pass through unchanged.

  Plain C++ with no Arduino dependencies, so it can be tested on a computer.
*/
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

namespace rateadj {

constexpr float MAX_PPM = 200.0f;   // largest correction either way
constexpr float FULL_ERR = 960.0f;  // buffer error (samples, 20 ms) that asks for the largest correction
constexpr float SMOOTH = 0.0033f;   // per chunk (10 ms): a low-pass of about 3 s over the jittery buffer level
// The buffer then settles in about 100 s (1 / (MAX_PPM / FULL_ERR * 1e-6 * 48000)), and a constant drift of d ppm
// leaves it d / MAX_PPM * 20 ms away from the target: 1.7 ms for the measured 17 ppm.

// Turns the buffer level into a correction in ppm. Positive: the computer is faster, so play faster.
struct Controller {
  float avg = 0;
  float ppm = 0;
  bool init = false;
  void reset() {
    init = false;
    ppm = 0;
  }
  // Once per chunk, with the buffer level (samples) measured just before reading the chunk.
  float update(float fill, float target) {
    if (!init) {
      avg = fill;
      init = true;
    }
    avg += (fill - avg) * SMOOTH;
    float p = (avg - target) * (MAX_PPM / FULL_ERR);
    ppm = p > MAX_PPM ? MAX_PPM : p < -MAX_PPM ? -MAX_PPM : p;
    return ppm;
  }
};

// Makes OUT output samples per call from OUT * (1 + ppm / 1e6) input samples on average.
template <size_t OUT>
struct Resampler {
  int16_t hist[OUT + 16];  // input not yet used up; hist[pos - 1] .. hist[pos + 2] around each output sample
  size_t have = 0;         // samples in hist
  size_t pos = 1;          // index of the input sample at or before the next output sample
  float frac = 0;          // and how far past it (0 .. 1)

  void reset() {
    hist[0] = 0;  // one silent sample before the first one, for the interpolation
    have = 1;
    pos = 1;
    frac = 0;
  }
  // Input samples the next run() needs beyond what hist already holds.
  size_t need(float ppm) const {
    float d = ppm * 1e-6f;
    size_t last = pos + (OUT - 1) + (size_t)floorf(frac + (OUT - 1) * d) + 2;  // highest index run() reads
    return last + 1 > have ? last + 1 - have : 0;
  }
  // Where to copy n more input samples (n from need(), at most OUT + 4).
  int16_t* writePtr() { return hist + have; }
  void wrote(size_t n) { have += n; }

  void run(int16_t* out, float ppm) {
    float d = ppm * 1e-6f;
    for (size_t n = 0; n < OUT; n++) {
      float q = frac + n * d;  // offset from sample pos + n, kept small so float stays exact enough
      int k = (int)floorf(q);
      float f = q - k;
      const int16_t* x = hist + pos + n + k;
      float xm1 = x[-1], x0 = x[0], x1 = x[1], x2 = x[2];
      float c1 = 0.5f * (x1 - xm1);
      float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
      float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
      float y = ((c3 * f + c2) * f + c1) * f + x0;
      out[n] = y > 32767.0f ? 32767 : y < -32768.0f ? -32768 : (int16_t)lrintf(y);
    }
    float q = frac + OUT * d;
    int k = (int)floorf(q);
    frac = q - k;
    pos += OUT + k;
    // Keep hist[pos - 1] and after, moved to the front.
    size_t drop = pos - 1;
    have -= drop;
    memmove(hist, hist + drop, have * sizeof(int16_t));
    pos = 1;
  }
};

}  // namespace rateadj
