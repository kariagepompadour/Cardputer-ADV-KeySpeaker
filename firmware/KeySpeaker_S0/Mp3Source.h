/*
  Mp3Source.h — (S16, MP3 input) one MP3 file from the SD card -> 48 kHz mono 16-bit blocks.

  The decoder is the Helix MP3 decoder (chmorgan/esp-libhelix-mp3, RealNetworks RPSL / RCSL) that the m5stack:esp32 SDK
  already ships prebuilt. Its license is still to be decided before a release (docs/MP3_DESIGN.md).

  Memory is taken when the MP3 input is switched on (begin) and given back when it is switched off (end); nothing stays
  allocated otherwise, because the BLE keyboard needs that heap when there is no USB. A file is opened and closed
  inside the input (open / close) while the buffers and the resampler's table stay.
  No threads in here: the caller's task asks for the next block and decides how fast.
*/
#pragma once
#include <Arduino.h>
#include <FS.h>
#include <esp_heap_caps.h>
#include <mp3dec.h>
#include "Sinc48.h"

namespace mp3src {

class Source {
 public:
  static constexpr size_t INBUF = 2048;  // Helix needs MAINBUF_SIZE (1940) plus the next frame's header
  static constexpr size_t OUT_MAX = 1728;  // 1152 samples at 32 kHz become 1728 at 48 kHz (the worst case)

  // Allocates everything; false when the heap cannot give it (then nothing stays allocated).
  bool begin() {
    constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    dec_ = MP3InitDecoder();
    in_ = (uint8_t*)heap_caps_malloc(INBUF, caps);
    pcm_ = (int16_t*)heap_caps_malloc(1152 * 2 * sizeof(int16_t), caps);  // stereo out, then reused for the 48 kHz mono
    rs_ = (sinc48::Resampler*)heap_caps_calloc(1, sizeof(sinc48::Resampler), caps);
    if (!dec_ || !in_ || !pcm_ || !rs_) {
      end();
      return false;
    }
    return true;
  }

  void end() {
    close();
    if (dec_) MP3FreeDecoder(dec_);
    heap_caps_free(in_);
    heap_caps_free(pcm_);
    heap_caps_free(rs_);
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
    MP3FreeDecoder(dec_);  // a fresh decoder for each file (this SDK's Helix has no flush call)
    dec_ = MP3InitDecoder();
    return dec_ != nullptr;
  }

  void close() {
    if (f_) f_.close();
    have_ = 0;
  }

  // Decodes the next frame. Returns the number of 48 kHz mono samples (in *out), 0 at the end of the file. Bad frames are skipped.
  size_t next(const int16_t** out) {
    while (!eof_ || have_ > 0) {
      if (have_ < (int)INBUF - 512 && !eof_) {
        int n = f_.read(in_ + have_, INBUF - have_);
        if (n <= 0) eof_ = true; else have_ += n;
      }
      int off = MP3FindSyncWord(in_, have_);
      if (off < 0) {
        have_ = 0;
        if (eof_) break;
        continue;
      }
      unsigned char* p = in_ + off;
      int left = have_ - off;
      int err = MP3Decode(dec_, &p, &left, pcm_, 0);
      memmove(in_, p, left);
      have_ = left;
      if (err == ERR_MP3_INDATA_UNDERFLOW && eof_) break;
      if (err) continue;
      MP3FrameInfo fi;
      MP3GetLastFrameInfo(dec_, &fi);
      if (fi.samprate < 16000 || fi.samprate > 48000 || fi.nChans < 1 || fi.nChans > 2) continue;
      if (fi.samprate != rate_) {
        rs_->begin(fi.samprate);
        rate_ = fi.samprate;
      }
      int n = fi.outputSamps / fi.nChans;
      int16_t* x = rs_->in();
      if (fi.nChans == 2)
        for (int i = 0; i < n; i++) x[i] = (int16_t)(((int32_t)pcm_[2 * i] + pcm_[2 * i + 1]) >> 1);
      else
        memcpy(x, pcm_, n * sizeof(int16_t));
      size_t m = rs_->process(n, pcm_, 1152 * 2);
      if (m == 0) continue;
      *out = pcm_;
      return m;
    }
    return 0;
  }

 private:
  HMP3Decoder dec_ = nullptr;
  uint8_t* in_ = nullptr;
  int16_t* pcm_ = nullptr;
  sinc48::Resampler* rs_ = nullptr;
  File f_;
  int have_ = 0;
  bool eof_ = false;
  int rate_ = 0;
};

}  // namespace mp3src
