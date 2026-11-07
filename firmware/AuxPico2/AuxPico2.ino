/*
  AuxPico2 — KeySpeaker's AUX unit: a Pico 2 H between a PCM1808 board and the Cardputer's Grove port
  (docs/AUX_PICO2_DESIGN.md).

  Core 0: receives I2S from the PCM1808 board (it is the master, 96 kHz), mixes left and right, and reduces to 48 kHz
          with a low-pass FIR. The I2S part comes from KariPom's karipom_pico2.ino.
  Core 1: talks to the Cardputer over UART0 and reads the jog dial, if there is one on I2C1.

  GP0 (TX) stays an input until the Cardputer asks: Launcher drives pins at start-up, and a unit that is quiet until
  asked cannot fight it. 500 ms after the last command GP0 is an input again.

  Pins: DATA GP2, BCLK GP3, LRCK GP4 (fixed order), UART TX GP0, RX GP1, dial SDA GP6, SCL GP7.
  Board: Raspberry Pi Pico 2 (rp2040:rp2040:rpipico2). USB serial prints a status line a second, at any speed.
*/
#include <I2S.h>
#include <Wire.h>
#include <math.h>
#include <atomic>
#include "AuxFrame.h"

#define BUILD_TAG "S12-1"

static constexpr int PIN_DIN = 2, PIN_TX = 0, PIN_RX = 1, PIN_SDA = 6, PIN_SCL = 7;
static constexpr long FS_HZ = 96000;
static constexpr int BITS = 32;
static constexpr uint32_t CMD_TIMEOUT_MS = 500, DIAL_FRAME_MS = 20, DIAL_MS = 20, DIAL_PROBE_MS = 1000;
static constexpr uint8_t DIAL_ADDR = 0x40, DIAL_REG_COUNT = 0x10, DIAL_REG_BUTTON = 0x20;

// ---- I2S (from karipom_pico2.ino)
static constexpr int ALIGN_MAX_ATTEMPTS = 25, PROBE_WORDS = 2048, PROBE_MIN_NONZERO = 16, BAD_PERMILLE = 20;
I2S i2s(INPUT);
enum FmtModel { MODEL_A, MODEL_B };
static FmtModel fmtModel = MODEL_A;
static bool i2sOk = false;

static inline int32_t toS24(int32_t raw) {
  if (fmtModel == MODEL_B) return (int32_t)((uint32_t)raw << 1) >> 8;
  return raw >> 8;
}
static inline bool fitsModel(uint32_t w, FmtModel m) {
  return m == MODEL_A ? (w & 0x000000FFu) == 0 : (w & 0x8000007Fu) == 0;
}

static int alignmentProbe() {  // 2: model A, 1: model B, 0: neither, -1: silence, -2: no clock
  uint32_t t0 = millis();
  int got = 0, nz = 0, fitA = 0, fitB = 0;
  while (got < PROBE_WORDS && millis() - t0 < 400) {
    if (i2s.available() >= 4) {
      int32_t w;
      if (i2s.read(&w, true) == 0) break;
      got++;
      if (w != 0) {
        nz++;
        if (fitsModel((uint32_t)w, MODEL_A)) fitA++;
        if (fitsModel((uint32_t)w, MODEL_B)) fitB++;
      }
    }
  }
  if (got & 1) {
    int32_t w;
    if (i2s.available() >= 4) i2s.read(&w, true);
  }
  if (got < PROBE_WORDS / 2) return -2;
  if (nz < PROBE_MIN_NONZERO) return -1;
  if ((nz - fitA) * 1000 <= nz * BAD_PERMILLE) return 2;
  if ((nz - fitB) * 1000 <= nz * BAD_PERMILLE) return 1;
  return 0;
}

static bool tryAlign() {
  for (int attempt = 1; attempt <= ALIGN_MAX_ATTEMPTS; attempt++) {
    uint32_t t0 = millis();
    while (millis() - t0 < 20) {
      while (i2s.available() >= 4) {
        int32_t w;
        i2s.read(&w, true);
      }
    }
    int r = alignmentProbe();
    if (r == 2 || r == -1) {
      fmtModel = MODEL_A;  // silence says nothing: A for now, and loop() looks again if the samples do not fit
      return true;
    }
    if (r == 1) {
      fmtModel = MODEL_B;
      return true;
    }
    if (r == -2) {
      Serial.println("[FAIL] no I2S clock: check the wiring and the board's power");
      return false;
    }
    i2s.end();
    delayMicroseconds(700 + (micros() % 1300));
    if (!i2s.begin(FS_HZ)) return false;
  }
  Serial.println("[FAIL] I2S did not line up");
  return false;
}

// ---- 96 kHz -> 48 kHz: a windowed-sinc low-pass at 24 kHz (Blackman), then every second sample
static constexpr int FIR_N = 63;
static float firCoef[FIR_N];
static float firBuf[2 * FIR_N];  // written twice, so a run of FIR_N samples is always contiguous
static int firPos = 0;
static bool firOdd = false;

static void firInit() {
  double sum = 0;
  double c[FIR_N];
  for (int i = 0; i < FIR_N; i++) {
    double x = i - (FIR_N - 1) / 2.0;
    double s = x == 0 ? 0.5 : sin(M_PI * 0.5 * x) / (M_PI * x);
    double w = 0.42 - 0.5 * cos(2 * M_PI * i / (FIR_N - 1)) + 0.08 * cos(4 * M_PI * i / (FIR_N - 1));
    c[i] = s * w;
    sum += c[i];
  }
  for (int i = 0; i < FIR_N; i++) firCoef[i] = (float)(c[i] / sum);
}

// One 96 kHz sample in. Returns true when a 48 kHz sample comes out.
static inline bool firPush(float in, float* out) {
  firBuf[firPos] = firBuf[firPos + FIR_N] = in;
  if (++firPos == FIR_N) firPos = 0;
  firOdd = !firOdd;
  if (firOdd) return false;
  const float* p = firBuf + firPos;  // the oldest sample first
  float a = 0;
  for (int i = 0; i < FIR_N; i++) a += firCoef[i] * p[i];
  *out = a;
  return true;
}

// ---- Core 0 -> core 1: blocks of samples, one writer and one reader
static constexpr uint32_t PCM_SLOTS = 8;  // 40 ms
static int16_t pcmSlot[PCM_SLOTS][auxlink::SOUND_N];
static std::atomic<uint32_t> pcmHead{0}, pcmTail{0};
static std::atomic<bool> soundOn{false};   // the Cardputer asked for sound
static std::atomic<uint32_t> pcmDropped{0}, framesSent{0}, cmdSeen{0};
static std::atomic<bool> linkUp{false}, dialOn{false};

static uint32_t sampleCount = 0, misCount = 0, overflowCount = 0;
static float rmsAcc = 0;
static uint32_t rmsN = 0;

void setup() {
  Serial.begin(115200);
  firInit();
  bool ok = i2s.setSlave() && i2s.setDATA(PIN_DIN) && i2s.setBitsPerSample(BITS) && i2s.setBuffers(8, 256);
  i2sOk = ok && i2s.begin(FS_HZ) && tryAlign();
}

void loop() {
  if (!i2sOk) {
    static uint32_t last = 0;
    if (millis() - last >= 2000) {
      last = millis();
      Serial.println("[FAIL] I2S not running");
    }
    return;
  }
  static size_t fill = 0;
  static int16_t block[auxlink::SOUND_N];
  while (i2s.available() >= 8) {
    int32_t rawL, rawR;
    if (!i2s.read32(&rawL, &rawR)) break;
    if (rawL != 0 && !fitsModel((uint32_t)rawL, fmtModel)) misCount++;
    if (rawR != 0 && !fitsModel((uint32_t)rawR, fmtModel)) misCount++;
    sampleCount++;
    float mono = (float)((toS24(rawL) >> 1) + (toS24(rawR) >> 1)) * (1.0f / 256.0f);  // 24 bits down to 16
    float s;
    if (!firPush(mono, &s)) continue;
    rmsAcc += s * s;
    rmsN++;
    if (!soundOn.load(std::memory_order_relaxed)) {
      fill = 0;
      continue;
    }
    block[fill++] = s > 32767.0f ? 32767 : s < -32768.0f ? -32768 : (int16_t)lrintf(s);
    if (fill == auxlink::SOUND_N) {
      fill = 0;
      uint32_t h = pcmHead.load(std::memory_order_relaxed);
      if (h - pcmTail.load(std::memory_order_acquire) >= PCM_SLOTS) {
        pcmDropped++;
      } else {
        memcpy(pcmSlot[h % PCM_SLOTS], block, sizeof block);
        pcmHead.store(h + 1, std::memory_order_release);
      }
    }
  }
  static bool prevOvf = false;
  bool ovf = i2s.getOverflow();
  if (ovf && !prevOvf) overflowCount++;
  prevOvf = ovf;

  static uint32_t lastReport = 0;
  uint32_t now = millis();
  if (now - lastReport >= 1000) {
    lastReport = now;
    float db = rmsN && rmsAcc > 0 ? 10.0f * log10f(rmsAcc / rmsN / (32768.0f * 32768.0f)) : -120.0f;
    Serial.printf("ST> %s n=%lu mis=%lu ovf=%lu rmsDb=%.1f link=%d sound=%d sent=%lu drop=%lu cmd=%lu dial=%d\n", BUILD_TAG,
                  (unsigned long)sampleCount, (unsigned long)misCount, (unsigned long)overflowCount, db, (int)linkUp.load(),
                  (int)soundOn.load(), (unsigned long)framesSent.load(), (unsigned long)pcmDropped.load(),
                  (unsigned long)cmdSeen.load(), (int)dialOn.load());
    bool lineUp = sampleCount && misCount > sampleCount / 50;  // the samples do not fit the model picked in silence
    sampleCount = misCount = 0;
    rmsAcc = 0;
    rmsN = 0;
    if (lineUp) {
      Serial.println("[I2S] lining up again");
      i2sOk = tryAlign();
    }
  }
}

// ---- Core 1: the Cardputer and the dial
static void txPin(bool on) {
  if (on) {
    gpio_set_function(PIN_TX, GPIO_FUNC_UART);
  } else {
    gpio_set_function(PIN_TX, GPIO_FUNC_SIO);
    gpio_set_dir(PIN_TX, GPIO_IN);
  }
}

static bool dialRead(uint8_t reg, uint8_t* buf, uint8_t len) {
  Wire1.beginTransmission(DIAL_ADDR);
  Wire1.write(reg);
  if (Wire1.endTransmission() != 0) return false;
  if (Wire1.requestFrom(DIAL_ADDR, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) {
    int b = Wire1.read();
    if (b < 0) return false;
    buf[i] = (uint8_t)b;
  }
  return true;
}

static int16_t dialCount = 0;
static bool dialDown = false;

static void dialService(uint32_t now) {
  static uint32_t last = 0;
  if (now - last < (dialOn ? DIAL_MS : DIAL_PROBE_MS)) return;
  last = now;
  uint8_t d[2], b = 1;
  if (!dialRead(DIAL_REG_COUNT, d, 2)) {
    dialOn = false;
    return;
  }
  dialCount = (int16_t)(d[0] | (d[1] << 8));
  dialDown = dialRead(DIAL_REG_BUTTON, &b, 1) && b == 0;
  dialOn = true;
}

static void commandService(uint32_t now) {
  static uint8_t got = 0;  // bytes of "KS?\n" matched so far
  static char cmd = 0;
  static uint32_t lastCmd = 0;
  while (Serial1.available()) {
    int c = Serial1.read();
    if (got == 0) got = c == 'K' ? 1 : 0;
    else if (got == 1) got = c == 'S' ? 2 : c == 'K' ? 1 : 0;
    else if (got == 2) {
      cmd = (char)c;
      got = c == '?' || c == '+' || c == '-' ? 3 : 0;
    } else {
      got = 0;
      if (c != '\n') continue;
      lastCmd = now;
      cmdSeen++;
      if (!linkUp) {
        txPin(true);
        linkUp = true;
      }
      bool sound = cmd == '+';
      if (sound && !soundOn) pcmTail.store(pcmHead.load(std::memory_order_acquire), std::memory_order_release);
      soundOn = sound;
    }
  }
  if (linkUp && now - lastCmd >= CMD_TIMEOUT_MS) {
    soundOn = false;
    linkUp = false;
    txPin(false);
  }
}

void setup1() {
  Serial1.setTX(PIN_TX);
  Serial1.setRX(PIN_RX);
  Serial1.setFIFOSize(256);
  Serial1.begin(auxlink::BAUD);
  txPin(false);
  Wire1.setSDA(PIN_SDA);
  Wire1.setSCL(PIN_SCL);
  Wire1.setClock(100000);
  Wire1.begin();
  Wire1.setTimeout(5);
}

void loop1() {
  static uint8_t frame[auxlink::MAX_FRAME];
  static uint8_t seq = 0;
  static uint32_t lastFrame = 0;
  uint32_t now = millis();
  commandService(now);
  dialService(now);
  if (!linkUp) return;
  uint8_t flags = (dialOn ? auxlink::FLAG_DIAL : 0) | (dialDown ? auxlink::FLAG_BUTTON : 0);
  uint32_t t = pcmTail.load(std::memory_order_relaxed);
  if (t != pcmHead.load(std::memory_order_acquire)) {
    size_t len = auxlink::build(frame, auxlink::TYPE_SOUND, seq++, dialCount, flags, pcmSlot[t % PCM_SLOTS], auxlink::SOUND_N);
    pcmTail.store(t + 1, std::memory_order_release);
    Serial1.write(frame, len);
    framesSent++;
    lastFrame = now;
  } else if (now - lastFrame >= DIAL_FRAME_MS) {
    size_t len = auxlink::build(frame, auxlink::TYPE_DIAL, seq++, dialCount, flags, nullptr, 0);
    Serial1.write(frame, len);
    framesSent++;
    lastFrame = now;
  }
}
