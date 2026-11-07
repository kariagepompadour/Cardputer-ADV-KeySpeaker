/*
  AuxFrame.h — KeySpeaker: the frames the Pico 2 AUX unit sends over the Grove UART (docs/AUX_PICO2_DESIGN.md, chapter 6).

  The same file sits in AuxPico2/ and KeySpeaker_S0/: an Arduino sketch cannot include a file outside its folder. Plain
  C++ with no Arduino dependencies, so it can be tested on a computer.

  Frame: A5 5A, type, sequence, dial count (int16), flags, 0, sample count n (uint16), n samples (int16), CRC-16.
  All numbers are little-endian. The CRC (CCITT, start FFFF) covers everything after the two sync bytes.
*/
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace auxlink {

constexpr uint32_t BAUD = 2000000;
constexpr uint8_t SYNC0 = 0xA5, SYNC1 = 0x5A;
constexpr uint8_t TYPE_SOUND = 1, TYPE_DIAL = 2;
constexpr uint8_t FLAG_DIAL = 1, FLAG_BUTTON = 2;  // a dial is attached; its button is down
constexpr size_t SOUND_N = 240;                     // samples in a sound frame: 5 ms at 48 kHz
constexpr size_t HEAD = 10;
constexpr size_t MAX_FRAME = HEAD + SOUND_N * 2 + 2;

inline uint16_t crc16(const uint8_t* p, size_t n) {
  uint16_t c = 0xFFFF;
  while (n--) {
    c ^= (uint16_t)(*p++) << 8;
    for (int i = 0; i < 8; i++) c = c & 0x8000 ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
  }
  return c;
}

// Writes a frame into out (at least MAX_FRAME bytes) and returns its length.
inline size_t build(uint8_t* out, uint8_t type, uint8_t seq, int16_t dialCount, uint8_t flags, const int16_t* samples, size_t n) {
  out[0] = SYNC0;
  out[1] = SYNC1;
  out[2] = type;
  out[3] = seq;
  out[4] = (uint8_t)dialCount;
  out[5] = (uint8_t)((uint16_t)dialCount >> 8);
  out[6] = flags;
  out[7] = 0;
  out[8] = (uint8_t)n;
  out[9] = (uint8_t)(n >> 8);
  if (n) memcpy(out + HEAD, samples, n * 2);
  size_t end = HEAD + n * 2;
  uint16_t c = crc16(out + 2, end - 2);
  out[end] = (uint8_t)c;
  out[end + 1] = (uint8_t)(c >> 8);
  return end + 2;
}

// Finds frames in the received bytes. feed() returns true when a whole, correct frame has arrived.
struct Parser {
  uint8_t buf[MAX_FRAME];
  size_t pos = 0, want = HEAD;
  uint32_t crcBad = 0;  // frames dropped for a wrong CRC or an impossible header

  void reset() {
    pos = 0;
    want = HEAD;
  }
  bool feed(uint8_t b) {
    if (pos == 0 && b != SYNC0) return false;
    if (pos == 1 && b != SYNC1) {
      pos = b == SYNC0 ? 1 : 0;
      return false;
    }
    buf[pos++] = b;
    if (pos == HEAD) {
      size_t n = buf[8] | (size_t)buf[9] << 8;
      if ((buf[2] != TYPE_SOUND && buf[2] != TYPE_DIAL) || n > SOUND_N) {
        crcBad++;
        reset();
        return false;
      }
      want = HEAD + n * 2 + 2;
    }
    if (pos < want) return false;
    size_t end = want - 2;
    uint16_t got = buf[end] | (uint16_t)buf[end + 1] << 8;
    bool ok = crc16(buf + 2, end - 2) == got;
    if (!ok) crcBad++;
    reset();
    return ok;
  }
  uint8_t type() const { return buf[2]; }
  uint8_t seq() const { return buf[3]; }
  int16_t dialCount() const { return (int16_t)(buf[4] | (uint16_t)buf[5] << 8); }
  uint8_t flags() const { return buf[6]; }
  size_t sampleCount() const { return buf[8] | (size_t)buf[9] << 8; }
  void samples(int16_t* out) const { memcpy(out, buf + HEAD, sampleCount() * 2); }
};

}  // namespace auxlink
