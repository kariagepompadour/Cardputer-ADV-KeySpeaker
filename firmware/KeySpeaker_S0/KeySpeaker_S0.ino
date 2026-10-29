/*
  KeySpeaker_S0 — Cardputer ADV as a USB Audio Class 1 speaker (stage S0 probe)

  What this probe does (docs/DESIGN_JA.md, section 12):
    - Shows up on the computer as a USB speaker (UAC1, 48 kHz, 16 bit, stereo) next to the USB serial port (CDC).
    - Mixes left and right to mono and plays it on the built-in speaker (or the earphone jack) through M5.Speaker.
    - (S0-2) Makes it louder the way small speakers do: a bass cut (the 1 W speaker cannot play deep bass, which
      only wastes headroom), a boost, and a peak limiter so the boost never clips. M5.Speaker itself is set to a
      gain of 1: at its old level 192 the signal was already at full scale, so raising that level only distorted.
    - Does NOT correct the clock difference between the computer and the Cardputer yet (that is S1). Instead it
      measures it: once a second it reports the buffer fill, underruns, overruns and the rates in ppm, on the screen
      and on the USB serial port.
    - (S0-3) A second screen, the flight visualizer (FlightViz.h): the level history as scrolling terrain, with a
      small fighter flying over it. It measures the level of what the computer sends, before its volume, and does
      not touch the audio path. The measurement screen stays the default.
    - No keyboard to the computer, no Bluetooth, no Wi-Fi.

  Keys on the Cardputer (local only, never sent to the computer):
    -  /  =   boost down / up (0 .. +18 dB in 3 dB steps)
    b         bass cut on / off (to compare)
    r         reset the counters
    v         measurement screen / flight visualizer

  Required Arduino IDE settings (Tools menu), enforced by #error below:
    Board: M5Cardputer (m5stack:esp32 3.3.10)
    USB Mode: USB-OTG (TinyUSB)
    USB CDC On Boot: Enabled
  Install the app-only .ino.bin through a launcher.
*/

#include <atomic>
#include <M5Cardputer.h>
#include "USB.h"
#include "USBAudioCard.h"
#include "esp32-hal-tinyusb.h"
#include "soc/usb_struct.h"
#include <freertos/stream_buffer.h>
#include "FlightViz.h"

#define BUILD_TAG "S0-3"

#if !defined(ARDUINO_USB_MODE) || ARDUINO_USB_MODE != 0
#error "Tools > USB Mode must be 'USB-OTG (TinyUSB)'. In 'Hardware CDC and JTAG' mode there is no USB audio."
#endif
#if !defined(ARDUINO_USB_CDC_ON_BOOT) || ARDUINO_USB_CDC_ON_BOOT != 1
// With CDC On Boot disabled, Serial becomes UART0 (TX = GPIO43), which the
// Cardputer ADV uses for audio I2S. Keep Serial on USB CDC.
#error "Tools > USB CDC On Boot must be 'Enabled'."
#endif
#if !CONFIG_TINYUSB_AUDIO_ENABLED
#error "This core's TinyUSB has no audio class. Use m5stack:esp32 3.3.10 or later."
#endif

// ---- Audio format and buffer sizes
static constexpr uint32_t RATE = 48000;          // USB and speaker sample rate (Hz); no resampling in between
static constexpr size_t CHUNK = 480;             // samples handed to M5.Speaker at a time (10 ms)
static constexpr size_t RING_SAMPLES = 24000;    // mono ring buffer: 500 ms
static constexpr size_t START_FILL = 4800;       // (re)start playing when 100 ms are buffered
static constexpr int SPK_CH = 0;                 // M5.Speaker virtual channel
// M5.Speaker's gain for a mono 16-bit sound at channel volume 255 is 2 * 16 * master^2 * 255^2 / 2^36
// (Speaker_Class.inl, magnification 16 on the Cardputer). Master 181 gives 0.996: the samples reach the codec as
// they leave the limiter below, so the limiter alone decides the peak level.
static constexpr uint8_t SPK_MASTER_UNITY = 181;

// ---- Loudness processing (S0-2), run by playTask on each 10 ms chunk
static constexpr float HPF_HZ = 150.0f;          // bass cut: 2nd-order Butterworth high-pass
static constexpr float LIMIT = 0.89f * 32767;    // peak ceiling, -1 dBFS
static constexpr float RELEASE_MS = 80.0f;       // how fast the limiter lets go after a peak
static constexpr int BOOST_MAX_DB = 18, BOOST_STEP_DB = 3, BOOST_DEFAULT_DB = 9;

// The descriptor is registered by this constructor. It runs before the core starts USB (CDC On Boot), so the
// computer sees the speaker from the first enumeration. Speaker only: no microphone interface.
USBAudioCard uac(RATE, UAC_BPS_16, UAC_SPK_STEREO, UAC_MIC_NONE);

static StreamBufferHandle_t ring = nullptr;   // writer: uacReceiveTask (onSpkData), reader: playTask

// ---- Counters, written by the audio tasks and read once a second by loop()
static std::atomic<uint32_t> rxFrames{0};      // stereo frames received from USB (= mono samples)
static std::atomic<uint32_t> playedSamples{0}; // mono samples handed to M5.Speaker
static std::atomic<uint32_t> droppedSamples{0};// samples thrown away because the ring was full
static std::atomic<uint32_t> overruns{0};      // times the ring became full
static std::atomic<uint32_t> underruns{0};     // times the ring ran dry while the computer was streaming
static std::atomic<uint32_t> fillMin{RING_SAMPLES}, fillMax{0};  // ring fill (samples) since the last report
static std::atomic<uint32_t> lastRxMs{0};      // millis() of the last USB audio data
static std::atomic<int16_t> rxPeak{0};         // largest |sample| since the last screen update
static std::atomic<bool> streaming{false};     // the computer selected the speaker's streaming alternate setting
static std::atomic<int> boostDb{BOOST_DEFAULT_DB};
static std::atomic<bool> bassCut{true};
static std::atomic<float> limitDbMax{0};       // deepest limiter gain reduction (dB, <= 0) since the last report
static std::atomic<int16_t> outPeak{0};        // largest |sample| sent to the speaker since the last screen update
static std::atomic<uint32_t> vizSum{0}, vizCount{0};  // 10 ms RMS levels from the computer (before its volume) since
                                                       // the last visualizer column, summed and counted; 0 while muted

// High-pass biquad (RBJ cookbook, Q = 0.7071) and the limiter's state; touched only by playTask.
struct Biquad {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
  void highPass(float fc, float fs) {
    float w = 2.0f * (float)M_PI * fc / fs, cw = cosf(w), alpha = sinf(w) / (2.0f * 0.70710678f);
    float a0 = 1.0f + alpha;
    b0 = (1.0f + cw) / 2.0f / a0;
    b1 = -(1.0f + cw) / a0;
    b2 = b0;
    a1 = -2.0f * cw / a0;
    a2 = (1.0f - alpha) / a0;
  }
  float run(float x) {  // transposed direct form II
    float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }
};
static Biquad hpf;
static float limEnv = 0;  // peak envelope: jumps up at once, decays with RELEASE_MS
static const float limRelease = expf(-1.0f / (RELEASE_MS * 0.001f * RATE));

// Bass cut, boost, then a peak limiter: env >= |x| at every sample, so x * LIMIT / env never exceeds LIMIT and the
// boost cannot clip. Instant attack, smooth release.
static void loudness(int16_t* s, size_t n) {
  float g = powf(10.0f, boostDb / 20.0f);
  bool cut = bassCut;
  float grMin = 1.0f;
  int16_t peak = 0;
  for (size_t i = 0; i < n; i++) {
    float x = s[i];
    float f = hpf.run(x);
    if (cut) x = f;  // the filter keeps running when off, so switching it back on does not click
    x *= g;
    float a = fabsf(x);
    limEnv = a > limEnv ? a : limEnv * limRelease;
    float gr = limEnv > LIMIT ? LIMIT / limEnv : 1.0f;
    if (gr < grMin) grMin = gr;
    int16_t y = (int16_t)lrintf(x * gr);
    s[i] = y;
    int16_t ay = y < 0 ? (int16_t)-y : y;
    if (ay > peak) peak = ay;
  }
  float grDb = 20.0f * log10f(grMin);
  if (grDb < limitDbMax) limitDbMax = grDb;
  if (peak > outPeak) outPeak = peak;
}

static void resetCounters() {
  droppedSamples = 0;
  overruns = 0;
  underruns = 0;
  fillMin = RING_SAMPLES;
  fillMax = 0;
}

// ---- USB -> ring (runs in USBAudioCard's receive task, about every 2 ms)
// The host volume and mute of every channel are applied first (uac.applyVolume), then left and right are mixed
// to mono. Halving the sum keeps full-scale stereo from clipping.
static int16_t monoBuf[256];
static bool ringWasFull = false;

static void onSpkData(void* data, uint16_t len) {
  // Visualizer level (S0-3): measured before the host volume, so the bars follow the music, not the volume knob.
  static float vizAcc = 0;
  static size_t vizN = 0;
  {
    const int16_t* r = static_cast<const int16_t*>(data);
    size_t n = len / 4;
    for (size_t i = 0; i < n; i++) {
      float m = ((int32_t)r[2 * i] + r[2 * i + 1]) * 0.5f;
      vizAcc += m * m;
    }
    vizN += n;
    if (vizN >= CHUNK) {
      uint32_t rms = uac.mute(UAC_CHAN_MASTER) ? 0 : (uint32_t)sqrtf(vizAcc / vizN);
      vizSum += rms;
      vizCount++;
      vizAcc = 0;
      vizN = 0;
    }
  }
  uac.applyVolume(data, len);
  const int16_t* s = static_cast<const int16_t*>(data);
  size_t frames = len / 4;  // 2 channels x 2 bytes
  int16_t peak = 0;
  while (frames) {
    size_t n = frames < 256 ? frames : 256;
    for (size_t i = 0; i < n; i++) {
      int16_t m = (int16_t)(((int32_t)s[2 * i] + s[2 * i + 1]) >> 1);
      monoBuf[i] = m;
      int16_t a = m < 0 ? (int16_t)(m == INT16_MIN ? INT16_MAX : -m) : m;
      if (a > peak) peak = a;
    }
    s += 2 * n;
    frames -= n;
    rxFrames += n;
    if (xStreamBufferSpacesAvailable(ring) < n * 2) {
      droppedSamples += n;
      if (!ringWasFull) overruns++;
      ringWasFull = true;
      continue;
    }
    ringWasFull = false;
    xStreamBufferSend(ring, monoBuf, n * 2, 0);
  }
  if (peak > rxPeak) rxPeak = peak;
  lastRxMs = millis();
}

// ---- ring -> M5.Speaker (core 1)
// Waits until START_FILL samples are buffered, then keeps M5.Speaker's two queue slots filled with 10 ms chunks.
// Three chunk buffers in rotation: M5.Speaker reads a buffer until it has played it.
static void playTask(void*) {
  static int16_t buf[3][CHUNK];
  int idx = 0;
  bool playing = false;
  for (;;) {
    uint32_t fill = (uint32_t)(xStreamBufferBytesAvailable(ring) / 2);
    if (playing) {
      if (fill < fillMin) fillMin = fill;
      if (fill > fillMax) fillMax = fill;
    }
    if (!playing) {
      if (fill < START_FILL) {
        vTaskDelay(2 / portTICK_PERIOD_MS);
        continue;
      }
      playing = true;
    }
    if (M5.Speaker.isPlaying(SPK_CH) >= 2) {  // both slots taken: wait for one to be played
      vTaskDelay(1);
      continue;
    }
    if (fill < CHUNK) {
      // Only a real underrun when the computer is still sending; the end of a stream just drains the ring.
      if (streaming && millis() - lastRxMs < 50) underruns++;
      playing = false;
      continue;
    }
    xStreamBufferReceive(ring, buf[idx], CHUNK * 2, 0);
    loudness(buf[idx], CHUNK);
    M5.Speaker.playRaw(buf[idx], CHUNK, RATE, false, 1, SPK_CH, false);
    playedSamples += CHUNK;
    idx = (idx + 1) % 3;
  }
}

// ---- USB events: streaming on/off, host volume and mute (logged as they happen)
static void onUsbEvent(void*, esp_event_base_t base, int32_t id, void* event_data) {
  if (base == ARDUINO_USB_EVENTS) {
    if (id == ARDUINO_USB_STARTED_EVENT) Serial.println("[USB] mounted");
    else if (id == ARDUINO_USB_STOPPED_EVENT) Serial.println("[USB] unmounted");
    else if (id == ARDUINO_USB_SUSPEND_EVENT) Serial.println("[USB] suspended");
    else if (id == ARDUINO_USB_RESUME_EVENT) Serial.println("[USB] resumed");
  } else if (base == ARDUINO_USB_AUDIO_CARD_EVENTS) {
    auto* d = static_cast<arduino_usb_audio_card_event_data_t*>(event_data);
    if (id == ARDUINO_USB_AUDIO_CARD_INTERFACE_ENABLE_EVENT && d->interface_enable.interface == UAC_INTERFACE_SPK) {
      streaming = d->interface_enable.enable;
      Serial.printf("[UAC] speaker stream %s\n", d->interface_enable.enable ? "ON" : "OFF");
    } else if (id == ARDUINO_USB_AUDIO_CARD_VOLUME_EVENT) {
      Serial.printf("[UAC] volume ch%d %d dB\n", (int)d->volume.channel, (int)d->volume.db);
    } else if (id == ARDUINO_USB_AUDIO_CARD_MUTE_EVENT) {
      Serial.printf("[UAC] mute ch%d %d\n", (int)d->mute.channel, (int)d->mute.muted);
    }
  }
}

// ---- USB link watch (from RADIO DAYS SDR)
// Cardputer ADV does not expose VBUS sensing to TinyUSB in this Launcher configuration. After a physical unplug,
// TinyUSB can therefore retain stale mounted state and fail to announce itself when the cable is reinserted.
// Watch the real SOF counter and periodically pulse the device pull-up until host traffic returns.
static uint32_t usbLastSofValue = 0, usbLastSofMs = 0, usbLastReconnectMs = 0;
static bool usbSawHost = false, sofMoving = false;
static void usbLinkService() {
  uint32_t now = millis();
  uint32_t sof = (USB0.dsts >> 8) & 0x3FFF;
  if (sof != usbLastSofValue) {
    usbLastSofValue = sof;
    usbLastSofMs = now;
    usbSawHost = true;
    sofMoving = true;
    return;
  }
  if (now - usbLastSofMs > 100) sofMoving = false;
  if (!usbSawHost || now - usbLastSofMs < 1200) return;
  if (now - usbLastReconnectMs < 2000) return;
  usbLastReconnectMs = now;
  tud_disconnect();
  delay(120);
  tud_connect();
}

// ---- Rates since streaming started, measured against the Cardputer's own clock (millis)
// rxPpm: how much faster (+) or slower (-) than 48000 Hz the computer sends.
// playPpm: the same for what the speaker consumes. The difference is the drift S1 has to correct.
struct RateWindow {
  bool active = false;
  uint32_t startMs = 0, rx0 = 0, played0 = 0;
};
static RateWindow rw;

static float ppm(uint32_t samples, uint32_t ms) {
  if (ms < 1000) return 0;
  double rate = samples * 1000.0 / ms;
  return (float)((rate - RATE) / RATE * 1e6);
}

// ---- Screen (240 x 135)
static M5Canvas canvas(&M5Cardputer.Display);
static constexpr uint16_t COL_BG = 0x0000, COL_FG = 0xFFFF, COL_DIM = 0x8410, COL_OK = 0x07E0, COL_WARN = 0xFFE0,
                          COL_BAD = 0xF800, COL_AMBER = 0xFD20;

static void drawScreen(float rxPpm, float playPpm, uint32_t fillNow, uint32_t elapsedS) {
  char l[48];
  canvas.fillSprite(COL_BG);
  canvas.setTextSize(2);
  canvas.setTextColor(COL_AMBER);
  canvas.drawString("KeySpeaker S0", 4, 2);
  canvas.setTextSize(1);
  canvas.setTextColor(COL_DIM);
  canvas.drawString(BUILD_TAG, 206, 2);

  bool mounted = (bool)USB;
  bool st = streaming;
  canvas.setTextColor(st ? COL_OK : mounted ? COL_WARN : COL_BAD);
  snprintf(l, sizeof l, "USB %s  SOF%c", st ? "STREAMING" : mounted ? "MOUNTED (no stream)" : "NOT CONNECTED", sofMoving ? '+' : '-');
  canvas.drawString(l, 4, 24);

  // Buffer bar: 0..500 ms, the start level (100 ms) marked.
  uint32_t fillMs = fillNow * 1000 / RATE;
  int w = (int)(fillNow * 232 / RING_SAMPLES);
  canvas.drawRect(4, 38, 232, 10, COL_DIM);
  canvas.fillRect(4, 38, w, 10, fillMs < 30 ? COL_BAD : fillMs > 400 ? COL_WARN : COL_OK);
  canvas.drawFastVLine(4 + (int)(START_FILL * 232 / RING_SAMPLES), 36, 14, COL_FG);
  canvas.setTextColor(COL_FG);
  snprintf(l, sizeof l, "buffer %3lu ms  (min %lu / max %lu)", (unsigned long)fillMs,
           (unsigned long)(fillMin.load() == RING_SAMPLES ? 0 : fillMin.load() * 1000 / RATE), (unsigned long)(fillMax.load() * 1000 / RATE));
  canvas.drawString(l, 4, 52);

  uint32_t u = underruns, o = overruns;
  canvas.setTextColor(u || o ? COL_BAD : COL_FG);
  snprintf(l, sizeof l, "underrun %lu  overrun %lu", (unsigned long)u, (unsigned long)o);
  canvas.drawString(l, 4, 64);

  canvas.setTextColor(COL_FG);
  snprintf(l, sizeof l, "drift PC %+.0f  SPK %+.0f ppm", rxPpm, playPpm);
  canvas.drawString(l, 4, 76);
  snprintf(l, sizeof l, "since %lu:%02lu:%02lu", (unsigned long)(elapsedS / 3600), (unsigned long)(elapsedS / 60 % 60), (unsigned long)(elapsedS % 60));
  canvas.drawString(l, 4, 88);

  snprintf(l, sizeof l, "PC vol %d dB%s  L%d R%d", (int)uac.volume(UAC_CHAN_MASTER), uac.mute(UAC_CHAN_MASTER) ? " MUTE" : "",
           (int)uac.volume(UAC_CHAN_LEFT), (int)uac.volume(UAC_CHAN_RIGHT));
  canvas.drawString(l, 4, 100);

  // Level meters: what arrives from USB (after the host volume) and what goes to the speaker (after the limiter)
  int16_t pkIn = rxPeak.exchange(0), pkOut = outPeak.exchange(0);
  canvas.drawRect(4, 112, 60, 8, COL_DIM);
  canvas.fillRect(4, 112, pkIn * 60 / 32768, 8, COL_OK);
  canvas.drawRect(68, 112, 60, 8, COL_DIM);
  canvas.fillRect(68, 112, pkOut * 60 / 32768, 8, pkOut >= (int16_t)(LIMIT * 0.98f) ? COL_WARN : COL_OK);
  snprintf(l, sizeof l, "+%ddB %s", boostDb.load(), bassCut ? "cut" : "full");
  canvas.drawString(l, 134, 112);

  canvas.setTextColor(COL_DIM);
  canvas.drawString("-/= boost  b bass  r reset  v view", 4, 125);
  canvas.pushSprite(0, 0);
}

// ---- Flight visualizer screen (S0-3): about 30 frames a second, drawn straight into the canvas buffer.
// It runs in loop(), below the audio tasks' priority, so a slow frame only lowers the frame rate.
static constexpr uint32_t FRAME_MS = 33;
static bool vizView = false;
static flightviz::State viz;
static std::atomic<uint32_t> drawMaxUs{0};     // slowest visualizer frame (draw + push) since the last report

static void drawViz(uint32_t now) {
  uint32_t t0 = micros();
  if (!viz.ready || now - viz.lastColMs > 1000) flightviz::init(viz, now);  // first time, or back from the other screen
  while (now - viz.lastColMs >= flightviz::COL_MS) {
    viz.lastColMs += flightviz::COL_MS;
    uint32_t n = vizCount.exchange(0), sum = vizSum.exchange(0);
    flightviz::push(viz, n ? (float)sum / n : 0);
  }
  flightviz::update(viz, now);
  flightviz::render(viz, static_cast<uint16_t*>(canvas.getBuffer()), now);

  // Status line, kept small and dim so the terrain stays the main thing.
  char l[48];
  bool mounted = (bool)USB, st = streaming;
  uint32_t u = underruns, o = overruns;
  canvas.setTextSize(1);
  canvas.setTextColor(st ? COL_DIM : mounted ? COL_WARN : COL_BAD);
  snprintf(l, sizeof l, "%s  +%ddB %s", st ? "USB" : mounted ? "USB no stream" : "not connected", boostDb.load(), bassCut ? "cut" : "full");
  canvas.drawString(l, 4, 2);
  canvas.setTextColor(u || o ? COL_BAD : COL_DIM);
  snprintf(l, sizeof l, "%sU%lu O%lu", uac.mute(UAC_CHAN_MASTER) ? "MUTE " : "", (unsigned long)u, (unsigned long)o);
  canvas.drawRightString(l, 236, 2);
  canvas.pushSprite(0, 0);

  uint32_t us = micros() - t0;
  if (us > drawMaxUs) drawMaxUs = us;
}

// ---- Local keys
static void keyService() {
  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return;
  auto ks = M5Cardputer.Keyboard.keysState();
  for (char c : ks.word) {
    if (c == '-' || c == '=') {
      int v = boostDb + (c == '=' ? BOOST_STEP_DB : -BOOST_STEP_DB);
      if (v < 0) v = 0;
      if (v > BOOST_MAX_DB) v = BOOST_MAX_DB;
      boostDb = v;
      Serial.printf("[KEY] boost +%d dB\n", v);
    } else if (c == 'b') {
      bassCut = !bassCut;
      Serial.printf("[KEY] bass cut %s\n", bassCut ? "on" : "off");
    } else if (c == 'r') {
      resetCounters();
      rw.active = false;
      Serial.println("[KEY] counters reset");
    } else if (c == 'v') {
      vizView = !vizView;
      Serial.printf("[KEY] view %s\n", vizView ? "flight" : "measure");
    }
  }
}

void setup() {
  USB.onEvent(onUsbEvent);
  Serial.begin(115200);
  Serial.printf("[BOOT] KeySpeaker %s\n", BUILD_TAG);

  auto cfg = M5.config();
  cfg.internal_imu = false;
  cfg.internal_spk = true;
  cfg.internal_mic = false;
  cfg.serial_baudrate = 0;  // Serial is USB CDC, already started
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(128);
  canvas.createSprite(240, 135);
  canvas.setFont(&fonts::Font0);

  ring = xStreamBufferCreate(RING_SAMPLES * 2, 1);
  if (!ring) {
    Serial.println("[BOOT] no memory for the ring buffer");
    M5Cardputer.Display.drawString("no memory", 4, 4);
    for (;;) delay(1000);
  }

  auto spk = M5.Speaker.config();
  spk.sample_rate = RATE;
  spk.task_pinned_core = APP_CPU_NUM;
  M5.Speaker.config(spk);
  M5.Speaker.begin();
  M5.Speaker.setVolume(SPK_MASTER_UNITY);
  hpf.highPass(HPF_HZ, RATE);

  xTaskCreatePinnedToCore(playTask, "play", 4096, nullptr, 3, nullptr, APP_CPU_NUM);
  uac.onEvent(onUsbEvent);
  uac.onData(onSpkData);
  if (!uac.begin()) Serial.println("[BOOT] USB audio failed to start");
  Serial.printf("[BOOT] ready, heap %u\n", (unsigned)ESP.getFreeHeap());
}

void loop() {
  M5Cardputer.update();
  usbLinkService();
  keyService();

  static uint32_t lastScreen = 0, lastFrame = 0, lastLog = 0;
  uint32_t now = millis();

  // The rate window starts with the first data of a stream and ends when the stream stops.
  bool live = streaming && now - lastRxMs < 200;
  if (live && !rw.active) {
    rw.active = true;
    rw.startMs = now;
    rw.rx0 = rxFrames;
    rw.played0 = playedSamples;
  } else if (!live && rw.active && now - lastRxMs > 2000) {
    rw.active = false;
  }
  uint32_t elapsedMs = rw.active ? now - rw.startMs : 0;
  float rxPpm = rw.active ? ppm(rxFrames - rw.rx0, elapsedMs) : 0;
  float playPpm = rw.active ? ppm(playedSamples - rw.played0, elapsedMs) : 0;
  uint32_t fillNow = (uint32_t)(xStreamBufferBytesAvailable(ring) / 2);

  if (vizView) {
    if (now - lastFrame >= FRAME_MS) {
      lastFrame = now;
      drawViz(now);
    }
  } else if (now - lastScreen >= 250) {
    lastScreen = now;
    drawScreen(rxPpm, playPpm, fillNow, elapsedMs / 1000);
  }
  if (now - lastLog >= 1000) {
    lastLog = now;
    uint32_t fmin = fillMin.exchange(RING_SAMPLES), fmax = fillMax.exchange(0);
    float lim = limitDbMax.exchange(0);
    Serial.printf("[S0] t=%lu stream=%d sof=%d fill=%lu ms min=%lu max=%lu under=%lu over=%lu drop=%lu rx_ppm=%+.1f spk_ppm=%+.1f vol=%d mute=%d boost=%d cut=%d lim=%.1f dB heap=%u view=%s draw_max=%.1f ms\n",
                  (unsigned long)(elapsedMs / 1000), (int)streaming.load(), (int)sofMoving, (unsigned long)(fillNow * 1000 / RATE),
                  (unsigned long)(fmin == RING_SAMPLES ? 0 : fmin * 1000 / RATE), (unsigned long)(fmax * 1000 / RATE), (unsigned long)underruns.load(),
                  (unsigned long)overruns.load(), (unsigned long)droppedSamples.load(), rxPpm, playPpm, (int)uac.volume(UAC_CHAN_MASTER),
                  (int)uac.mute(UAC_CHAN_MASTER), boostDb.load(), (int)bassCut.load(), lim, (unsigned)ESP.getFreeHeap(),
                  vizView ? "flight" : "measure", drawMaxUs.exchange(0) / 1000.0f);
  }
  delay(5);
}
