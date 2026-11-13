/*
  Mp3Source.h — (S16, MP3 input) one MP3 file from the SD card -> 48 kHz mono 16-bit blocks.

  The decoder is minimp3 (https://github.com/lieff/minimp3, CC0 1.0, public domain), one header, layer 3 only.
  It replaces the Helix decoder of the first S16-1 builds (RealNetworks RPSL), see docs/MP3_LICENSE.md.

  Memory is taken when the MP3 input is switched on (begin) and given back when it is switched off (end); nothing stays
  allocated otherwise, because the BLE keyboard needs that heap when there is no USB. The buffers live in an arena the caller
  lends (the ring buffer's storage, which an MP3 input does not need in full); only the decoder's state (about 6 KB) comes
  from the heap. minimp3 keeps its per-frame scratch (about 14 KB) on the stack, so the caller's task stack must be large.
  A file is opened and closed inside the input (open / close) while the buffers and the resampler's table stay.
  No threads in here: the caller's task asks for the next block and decides how fast.
*/
#pragma once
#include <Arduino.h>
#include <FS.h>
#include <esp_heap_caps.h>
#include <new>
#define MINIMP3_ONLY_MP3
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "Sinc48.h"

namespace mp3src {

constexpr size_t align16(size_t n) { return (n + 15) & ~(size_t)15; }

class Source {
 public:
  static constexpr size_t INBUF = 2048;  // room for a whole frame (at most 1441 bytes) plus the next frame header
  static constexpr size_t OUT_MAX = 1728;  // 1152 samples at 32 kHz become 1728 at 48 kHz (the worst case)

  // Bytes the buffers take from the arena (16-byte aligned pieces).
  static constexpr size_t ARENA_NEED = align16(INBUF) + align16(1152 * 2 * sizeof(int16_t)) + align16(sizeof(sinc48::Resampler));

  // The buffers come from a block the caller gives (the ring buffer's spare storage, so the heap is not asked for them);
  // only the decoder's state (about 6 KB) comes from the heap. False when the arena is too small or the heap cannot give the state.
  bool begin(uint8_t* arena, size_t size) {
    if (!arena || size < ARENA_NEED) return false;
    in_ = arena;
    arena += align16(INBUF);
    pcm_ = (int16_t*)arena;  // stereo out, then reused for the 48 kHz mono
    arena += align16(1152 * 2 * sizeof(int16_t));
    rs_ = new (arena) sinc48::Resampler();
    dec_ = (mp3dec_t*)heap_caps_malloc(sizeof(mp3dec_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!dec_) {
      end();
      return false;
    }
    mp3dec_init(dec_);
    return true;
  }

  void end() {
    close();
    if (dec_) heap_caps_free(dec_);
    dec_ = nullptr;
    in_ = nullptr;
    pcm_ = nullptr;
    rs_ = nullptr;
  }

  bool open(File f) {
    close();
    f_ = f;
    if (!f_) return false;
    uint8_t h[10];
    if (f_.read(h, 10) == 10 && !memcmp(h, "ID3", 3))
      f_.seek(10 + (((h[6] & 0x7f) << 21) | ((h[7] & 0x7f) << 14) | ((h[8] & 0x7f) << 7) | (h[9] & 0x7f)));
    else
      f_.seek(0);
    have_ = 0;
    eof_ = false;
    rate_ = 0;
    mp3dec_init(dec_);  // forget the previous file's bit reservoir
    return dec_ != nullptr;
  }

  void close() {
    if (f_) f_.close();
    have_ = 0;
  }

  // Decodes the next frame. Returns the number of 48 kHz mono samples (in *out), 0 at the end of the file. Bad frames are skipped.
  size_t next(int16_t** out) {
    while (!eof_ || have_ > 0) {
      if (have_ < (int)INBUF - 512 && !eof_) {
        int n = f_.read(in_ + have_, INBUF - have_);
        if (n <= 0) eof_ = true; else have_ += n;
      }
      mp3dec_frame_info_t fi;
      int ns = mp3dec_decode_frame(dec_, in_, have_, pcm_, &fi);  // ns: samples per channel
      int used = fi.frame_bytes;
      if (used > have_) used = have_;
      if (used <= 0) {
        if (eof_) break;  // nothing more that looks like a frame
        if (have_ >= (int)INBUF) used = 1;  // a full buffer without a frame: slide on
      }
      if (used > 0) {
        memmove(in_, in_ + used, have_ - used);
        have_ -= used;
      }
      if (ns <= 0) continue;
      if (fi.hz < 16000 || fi.hz > 48000 || fi.channels < 1 || fi.channels > 2 || ns > 1152) continue;
      if (fi.hz != rate_) {
        rs_->begin(fi.hz);
        rate_ = fi.hz;
      }
      int16_t* x = rs_->in();
      if (fi.channels == 2)
        for (int i = 0; i < ns; i++) x[i] = (int16_t)(((int32_t)pcm_[2 * i] + pcm_[2 * i + 1]) >> 1);
      else
        memcpy(x, pcm_, ns * sizeof(int16_t));
      size_t m = rs_->process(ns, pcm_, 1152 * 2);
      if (m == 0) continue;
      *out = pcm_;
      return m;
    }
    return 0;
  }

 private:
  mp3dec_t* dec_ = nullptr;
  uint8_t* in_ = nullptr;
  int16_t* pcm_ = nullptr;
  sinc48::Resampler* rs_ = nullptr;
  File f_;
  int have_ = 0;
  bool eof_ = false;
  int rate_ = 0;
};

}  // namespace mp3src
