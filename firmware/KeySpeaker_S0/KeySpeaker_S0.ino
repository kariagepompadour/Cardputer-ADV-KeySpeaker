/*
  KeySpeaker — USB Speaker & BLE Keyboard for M5Stack Cardputer ADV
  KeySpeaker_S0: Cardputer ADV as a USB Audio Class 1 speaker (stage S0 probe, with the S1 drift correction)

  What this probe does (docs/DESIGN_JA.md, section 12):
    - Shows up on the computer as a USB speaker (UAC1, 48 kHz, 16 bit, stereo) next to the USB serial port (CDC).
    - Mixes left and right to mono and plays it on the built-in speaker (or the earphone jack) through M5.Speaker.
    - (S0-2) Makes it louder the way small speakers do: a bass cut (the 1 W speaker cannot play deep bass, which
      only wastes headroom), a boost, and a peak limiter so the boost never clips. M5.Speaker itself is set to a
      gain of 1: at its old level 192 the signal was already at full scale, so raising that level only distorted.
    - Measures the clock difference between the computer and the Cardputer: once a second it reports the buffer fill,
      underruns, overruns and the rates in ppm, on the screen and on the USB serial port.
    - (S1-1) Corrects that difference (RateAdjust.h): playTask plays a little faster or slower, at most +-200 ppm,
      to keep the buffer at its starting level, so long listening no longer runs the buffer dry or full.
    - (S1-2) Restart diagnosis: the Cardputer restarted by itself when music started again after a pause. It now
      keeps a few notes in memory that survive a restart (not a power cut) and reports, after a restart, why it
      restarted and what each task was doing just before.
    - (S1-3) The first start after installing S1-2 crashed (PANIC) within a second. The core saves a crash record
      (core dump) to flash; this version reads its summary (task, program address, backtrace) at start and logs it.
    - (S1-4) The record showed the crash in the core's USB event task (2 KB stack), where onUsbEvent printed to the
      serial port: a stack overflow when events came in, at enumeration or when the computer started the stream.
      The handler now only queues the events; loop() prints them.
    - (S1-5) With that fixed, every resume after a pause still restarted it (task watchdog): TinyUSB hangs while it
      switches the audio endpoint back on. UsbIsoFix.h guards that switch; firmware/build.sh links it in.
    - (S1-6) The guard's time-limited switch-off did not work either, so it now keeps the endpoint on when its
      settings are unchanged (UsbIsoFix.h).
    - (S1-7) A startup screen: the KariPom emblem on a speaker grille (Splash.h) for 2 seconds, or until a key.
      USB and the audio start behind it as before.
    - (S1-8) A key with no job here (letters, Enter, ...) shows that screen again for 2 seconds (design 8.3).
    - (S1-9) The emblem at 80 % of its size on the startup screen.
    - (S1-10) The startup screen with the speaker's rubber bumper, white around it and a white base. (S1-11) Back to
      the grille filling the screen: the LCD's white never matches the Cardputer's white body.
    - (S2-1) A USB keyboard and media keys next to the speaker, and three modes on Fn+M (design 8.3): the measurement
      screen and the flight visualizer use the keys for the volume and playback on the computer and for local
      settings; the keyboard mode sends every key to the computer, with an Fn layer and a Mac / Windows profile
      (Fn+O). The mode and the profile are remembered. Without USB only the keyboard mode is shown.
    - (S2-2) The keyboard screen shows a cheat sheet of its keys, keys and meanings in two colors. (S2-3) It also
      shows the last input switch, A or あ (Fn+[ / Fn+]), like the Mac's menu bar.
    - (S2-4) Keys still held from the launcher are ignored at start, and the startup screen never shows the hint.
    - (S2-5) The computer lists the speaker as "KeySpeaker", not "TinyUSB UAC1" (UsbNames.h).
    - (S0-3) A second screen, the flight visualizer (FlightViz.h): the level history as scrolling terrain, with a
      small fighter flying over it. It measures the level of what the computer sends, before its volume, and does
      not touch the audio path. The measurement screen stays the default.
    - (S0-4..6) Targets that shoot back, a laser, and now and then a graphic equalizer in front of the terrain.
      For the equalizer the receive callback also copies the mono samples into a 1024-sample ring; the FFT runs
      at drawing time.
    - No keyboard to the computer, no Bluetooth, no Wi-Fi.

  Keys on the Cardputer (local only, never sent to the computer):
    -  /  =   boost down / up (0 .. +18 dB in 3 dB steps)
    b         bass cut on / off (to compare)
    r         reset the counters
    v         measurement screen / flight visualizer (on the emblem screen: back to the measurement screen)
    others    the emblem screen for 2 seconds

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
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"
#include <Preferences.h>
#include "esp32-hal-tinyusb.h"
#include "soc/usb_struct.h"
#include <freertos/stream_buffer.h>
#include "FlightViz.h"
#include "RateAdjust.h"
#include "esp_core_dump.h"
#include "UsbIsoFix.h"
#include "UsbNames.h"
#include "Splash.h"

#define BUILD_TAG "S2-5"

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
static constexpr float TARGET_FILL = START_FILL - 2 * CHUNK;  // the level right after starting, when both of
                                                              // M5.Speaker's slots are filled; S1 holds it there
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
// (S2-1) The keyboard must be the first HID object: the first one sets the HID interface's protocol, and a boot
// keyboard gets endpoint 1. Both register with TinyUSB in their constructors, before USB starts.
USBHIDKeyboard kbd;
USBHIDConsumerControl media;

static StreamBufferHandle_t ring = nullptr;   // writer: uacReceiveTask (onSpkData), reader: playTask

// ---- Counters, written by the audio tasks and read once a second by loop()
static std::atomic<uint32_t> rxFrames{0};      // stereo frames received from USB (= mono samples)
static std::atomic<uint32_t> playedSamples{0}; // mono samples handed to M5.Speaker
static std::atomic<uint32_t> droppedSamples{0};// samples thrown away because the ring was full
static std::atomic<uint32_t> overruns{0};      // times the ring became full
static std::atomic<uint32_t> underruns{0};     // times the ring ran dry while the computer was streaming
static std::atomic<uint32_t> playStarts{0};    // times playTask (re)started playing after buffering
static std::atomic<uint32_t> fillMin{RING_SAMPLES}, fillMax{0};  // ring fill (samples) since the last report
static std::atomic<float> adjPpm{0};           // S1 correction playTask applies now (+: the computer is faster)
static std::atomic<uint32_t> lastRxMs{0};      // millis() of the last USB audio data
static std::atomic<int16_t> rxPeak{0};         // largest |sample| since the last screen update
static std::atomic<bool> streaming{false};     // the computer selected the speaker's streaming alternate setting
static std::atomic<int> boostDb{BOOST_DEFAULT_DB};
static std::atomic<bool> bassCut{true};
static std::atomic<float> limitDbMax{0};       // deepest limiter gain reduction (dB, <= 0) since the last report
static std::atomic<int16_t> outPeak{0};        // largest |sample| sent to the speaker since the last screen update
static std::atomic<uint32_t> vizSum{0}, vizCount{0};  // 10 ms RMS levels from the computer (before its volume) since
                                                       // the last visualizer column, summed and counted; 0 while muted
static constexpr uint32_t VIZ_PCM_N = 1024;     // newest mono samples (before the host volume; 0 while muted) for the
static int16_t vizPcm[VIZ_PCM_N];               // visualizer's spectrum, written only by onSpkData; read without a lock,
static std::atomic<uint32_t> vizPcmPos{0};      // so a frame may mix two packets, which no one can see

// ---- Restart diagnosis (S1-2)
// RTC memory keeps its contents through a software or crash restart, so after one the log shows the reset reason
// and, for each task, the last step it reached ("stages", see diagMark calls) and the uptime then. The counters add
// up across restarts until the next power-on.
static constexpr uint32_t DIAG_MAGIC = 0x4B535032;
RTC_NOINIT_ATTR static uint32_t diagMagic, diagBoots, diagPlay, diagRx, diagLoop, diagUptimeS, diagOns, diagOffs,
    diagSuspends, diagPulses;
static uint32_t diagPrevPlay, diagPrevRx, diagPrevLoop, diagPrevUptimeS;  // the notes from before this start
static const char* diagReset = "?";
static inline void diagMark(volatile uint32_t& stage, uint32_t v) { stage = v; }
static const char* resetName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "SLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_USB: return "USB";
    default: return "OTHER";
  }
}
static void diagBegin() {
  esp_reset_reason_t r = esp_reset_reason();
  diagReset = resetName(r);
  if (diagMagic != DIAG_MAGIC || r == ESP_RST_POWERON) {
    diagMagic = DIAG_MAGIC;
    diagBoots = diagPlay = diagRx = diagLoop = diagUptimeS = diagOns = diagOffs = diagSuspends = diagPulses = 0;
  }
  diagBoots++;
  diagPrevPlay = diagPlay;
  diagPrevRx = diagRx;
  diagPrevLoop = diagLoop;
  diagPrevUptimeS = diagUptimeS;
  diagPlay = diagRx = diagLoop = diagUptimeS = 0;
}

// The crash record (core dump) the core writes to flash on a PANIC, read once at start. It stays in flash until the
// next crash overwrites it, so compare with rst=PANIC to tell a new one from an old one. The addresses can be turned
// into source lines with the build's .elf (xtensa-esp32s3-elf-addr2line).
static char crashInfo[240] = "";
static void crashBegin() {
  esp_err_t e = esp_core_dump_image_check();
  if (e != ESP_OK) {
    snprintf(crashInfo, sizeof crashInfo, "none (%s)", esp_err_to_name(e));
    return;
  }
  static esp_core_dump_summary_t sum;
  e = esp_core_dump_get_summary(&sum);
  if (e != ESP_OK) {
    snprintf(crashInfo, sizeof crashInfo, "unreadable (%s)", esp_err_to_name(e));
    return;
  }
  int n = snprintf(crashInfo, sizeof crashInfo, "task=%s pc=0x%08lx cause=%lu vaddr=0x%08lx bt%s=", sum.exc_task,
                   (unsigned long)sum.exc_pc, (unsigned long)sum.ex_info.exc_cause, (unsigned long)sum.ex_info.exc_vaddr,
                   sum.exc_bt_info.corrupted ? "(corrupted)" : "");
  for (uint32_t i = 0; i < sum.exc_bt_info.depth && i < 16 && n > 0 && n < (int)sizeof crashInfo - 12; i++)
    n += snprintf(crashInfo + n, sizeof crashInfo - n, " 0x%08lx", (unsigned long)sum.exc_bt_info.bt[i]);
}

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
  diagMark(diagRx, 1);
  // Visualizer level (S0-3): measured before the host volume, so the bars follow the music, not the volume knob.
  static float vizAcc = 0;
  static size_t vizN = 0;
  {
    const int16_t* r = static_cast<const int16_t*>(data);
    size_t n = len / 4;
    bool muted = uac.mute(UAC_CHAN_MASTER);
    uint32_t pos = vizPcmPos.load(std::memory_order_relaxed);
    for (size_t i = 0; i < n; i++) {
      float m = ((int32_t)r[2 * i] + r[2 * i + 1]) * 0.5f;
      vizAcc += m * m;
      vizPcm[pos++ & (VIZ_PCM_N - 1)] = muted ? 0 : (int16_t)m;
    }
    vizPcmPos.store(pos, std::memory_order_release);
    vizN += n;
    if (vizN >= CHUNK) {
      uint32_t rms = uac.mute(UAC_CHAN_MASTER) ? 0 : (uint32_t)sqrtf(vizAcc / vizN);
      vizSum += rms;
      vizCount++;
      vizAcc = 0;
      vizN = 0;
    }
  }
  diagMark(diagRx, 2);
  uac.applyVolume(data, len);
  diagMark(diagRx, 3);
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
  diagMark(diagRx, 0);
}

// ---- ring -> M5.Speaker (core 1)
// Waits until START_FILL samples are buffered, then keeps M5.Speaker's two queue slots filled with 10 ms chunks.
// Three chunk buffers in rotation: M5.Speaker reads a buffer until it has played it.
// (S1) Each chunk is resampled from slightly more or fewer samples than CHUNK, as the controller asks (RateAdjust.h).
static void playTask(void*) {
  static int16_t buf[3][CHUNK];
  static rateadj::Resampler<CHUNK> rs;
  static rateadj::Controller ctl;
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
        diagMark(diagPlay, 1);
        vTaskDelay(2 / portTICK_PERIOD_MS);
        continue;
      }
      playing = true;
      playStarts++;
      rs.reset();
      ctl.reset();
    }
    diagMark(diagPlay, 2);
    if (M5.Speaker.isPlaying(SPK_CH) >= 2) {  // both slots taken: wait for one to be played
      diagMark(diagPlay, 3);
      vTaskDelay(1);
      continue;
    }
    float ppm = ctl.update((float)fill, TARGET_FILL);
    size_t need = rs.need(ppm);
    if (fill < need) {
      // Only a real underrun when the computer is still sending; the end of a stream just drains the ring.
      if (streaming && millis() - lastRxMs < 50) underruns++;
      playing = false;
      adjPpm = 0;
      continue;
    }
    diagMark(diagPlay, 4);
    xStreamBufferReceive(ring, rs.writePtr(), need * 2, 0);
    rs.wrote(need);
    diagMark(diagPlay, 5);
    rs.run(buf[idx], ppm);
    adjPpm = ppm;
    diagMark(diagPlay, 6);
    loudness(buf[idx], CHUNK);
    diagMark(diagPlay, 7);
    M5.Speaker.playRaw(buf[idx], CHUNK, RATE, false, 1, SPK_CH, false);
    playedSamples += CHUNK;
    idx = (idx + 1) % 3;
  }
}

// ---- USB events: streaming on/off, host volume and mute
// The handler runs in the core's USB event task, whose stack is only 2 KB: printing there overflowed it (S1-3's crash
// record). So it only notes each event in a small queue, and loop() prints them (usbEventLog).
enum UsbEv : uint8_t { EV_MOUNTED, EV_UNMOUNTED, EV_SUSPENDED, EV_RESUMED, EV_STREAM, EV_VOLUME, EV_MUTE };
struct UsbEvRec {
  UsbEv kind;
  int8_t ch;
  int16_t value;
};
static constexpr uint32_t USB_EV_N = 16;  // events beyond this between two loop() passes are counted, not kept
static UsbEvRec usbEvQ[USB_EV_N];
static std::atomic<uint32_t> usbEvHead{0}, usbEvLost{0};  // written by the event task
static uint32_t usbEvTail = 0;                             // read by loop()

static void usbEvPush(uint8_t kind, int ch, int value) {  // kind: a UsbEv (a plain type, for the IDE's prototypes)
  uint32_t h = usbEvHead.load(std::memory_order_relaxed);
  if (h - usbEvTail >= USB_EV_N) {  // loop() reads usbEvTail; a stale value only makes this more careful
    usbEvLost++;
    return;
  }
  usbEvQ[h % USB_EV_N] = {(UsbEv)kind, (int8_t)ch, (int16_t)value};
  usbEvHead.store(h + 1, std::memory_order_release);
}

static void onUsbEvent(void*, esp_event_base_t base, int32_t id, void* event_data) {
  if (base == ARDUINO_USB_EVENTS) {
    if (id == ARDUINO_USB_STARTED_EVENT) usbEvPush(EV_MOUNTED, 0, 0);
    else if (id == ARDUINO_USB_STOPPED_EVENT) usbEvPush(EV_UNMOUNTED, 0, 0);
    else if (id == ARDUINO_USB_SUSPEND_EVENT) {
      diagSuspends++;
      usbEvPush(EV_SUSPENDED, 0, 0);
    } else if (id == ARDUINO_USB_RESUME_EVENT) usbEvPush(EV_RESUMED, 0, 0);
  } else if (base == ARDUINO_USB_AUDIO_CARD_EVENTS) {
    auto* d = static_cast<arduino_usb_audio_card_event_data_t*>(event_data);
    if (id == ARDUINO_USB_AUDIO_CARD_INTERFACE_ENABLE_EVENT && d->interface_enable.interface == UAC_INTERFACE_SPK) {
      streaming = d->interface_enable.enable;
      if (d->interface_enable.enable) diagOns++;
      else diagOffs++;
      usbEvPush(EV_STREAM, 0, d->interface_enable.enable);
    } else if (id == ARDUINO_USB_AUDIO_CARD_VOLUME_EVENT) {
      usbEvPush(EV_VOLUME, d->volume.channel, d->volume.db);
    } else if (id == ARDUINO_USB_AUDIO_CARD_MUTE_EVENT) {
      usbEvPush(EV_MUTE, d->mute.channel, d->mute.muted);
    }
  }
}

// Prints the queued USB events, in loop().
static void usbEventLog() {
  uint32_t h = usbEvHead.load(std::memory_order_acquire);
  for (; usbEvTail != h; usbEvTail++) {
    const UsbEvRec& e = usbEvQ[usbEvTail % USB_EV_N];
    switch (e.kind) {
      case EV_MOUNTED: Serial.println("[USB] mounted"); break;
      case EV_UNMOUNTED: Serial.println("[USB] unmounted"); break;
      case EV_SUSPENDED: Serial.println("[USB] suspended"); break;
      case EV_RESUMED: Serial.println("[USB] resumed"); break;
      case EV_STREAM: Serial.printf("[UAC] speaker stream %s\n", e.value ? "ON" : "OFF"); break;
      case EV_VOLUME: Serial.printf("[UAC] volume ch%d %d dB\n", (int)e.ch, (int)e.value); break;
      case EV_MUTE: Serial.printf("[UAC] mute ch%d %d\n", (int)e.ch, (int)e.value); break;
    }
  }
  uint32_t lost = usbEvLost.exchange(0);
  if (lost) Serial.printf("[USB] %lu events not logged (queue full)\n", (unsigned long)lost);
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
  diagPulses++;
  tud_disconnect();
  delay(120);
  tud_connect();
}

// ---- Rates since streaming started, measured against the Cardputer's own clock (millis)
// rxPpm: how much faster (+) or slower (-) than 48000 Hz the computer sends.
// playPpm: the same for what the speaker consumes. Both include the start-up buffering, so for the first minutes they
// are far off; they are only logged. The screen shows the S1 correction and the buffer trend below.
struct RateWindow {
  bool active = false;
  uint32_t startMs = 0, rx0 = 0, played0 = 0;
};
static RateWindow rw;

// ---- Drift from the buffer trend (S0-3)
// The rates above include the start-up buffering, which dominates for minutes. The buffer level itself shows the
// drift: it rises when the computer sends faster than the speaker plays and falls when slower. Once a second the
// change of the buffer's mid level is added up, but only between two seconds of uninterrupted play (no stream gap, no
// restart), so pauses between songs simply leave a gap in the sum. Positive: the computer is faster.
struct BufTrend {
  bool have = false;
  uint32_t lastStarts = 0, lastMs = 0;
  float lastMid = 0;
  double sumDelta = 0, sumMs = 0;  // samples, ms of uninterrupted play
};
static BufTrend bt;
static constexpr double TREND_MIN_MS = 60000;  // show a value after one minute of play

static void bufTrendSample(uint32_t now, bool live, uint32_t fmin, uint32_t fmax) {
  bool valid = live && fmin != RING_SAMPLES && fmax > 0;  // played during the whole last second
  uint32_t starts = playStarts;
  float mid = (fmin + fmax) * 0.5f;
  if (valid && bt.have && starts == bt.lastStarts) {
    bt.sumDelta += mid - bt.lastMid;
    bt.sumMs += now - bt.lastMs;
  }
  bt.have = valid;
  bt.lastStarts = starts;
  bt.lastMs = now;
  bt.lastMid = mid;
}

static bool bufTrendPpm(float* out) {
  if (bt.sumMs < TREND_MIN_MS) return false;
  *out = (float)(bt.sumDelta / (bt.sumMs / 1000.0) / RATE * 1e6);
  return true;
}

static float ppm(uint32_t samples, uint32_t ms) {
  if (ms < 1000) return 0;
  double rate = samples * 1000.0 / ms;
  return (float)((rate - RATE) / RATE * 1e6);
}

// ---- Screen (240 x 135)
static M5Canvas canvas(&M5Cardputer.Display);
static constexpr uint16_t COL_BG = 0x0000, COL_FG = 0xFFFF, COL_DIM = 0x8410, COL_OK = 0x07E0, COL_WARN = 0xFFE0,
                          COL_BAD = 0xF800, COL_AMBER = 0xFD20;

static void drawScreen(uint32_t fillNow, uint32_t elapsedS) {
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
  float drift;
  uint32_t playS = (uint32_t)(bt.sumMs / 1000);
  // S1: "adj" is the correction applied now; "trend" is what is left over in the buffer (near 0 once it works).
  if (bufTrendPpm(&drift))
    snprintf(l, sizeof l, "adj %+.0f ppm  trend %+.0f  (%lu:%02lu)", adjPpm.load(), drift, (unsigned long)(playS / 60),
             (unsigned long)(playS % 60));
  else
    snprintf(l, sizeof l, "adj %+.0f ppm  trend --  (%lu:%02lu)", adjPpm.load(), (unsigned long)(playS / 60), (unsigned long)(playS % 60));
  canvas.drawString(l, 4, 76);
  snprintf(l, sizeof l, "since %lu:%02lu:%02lu  boot %lu %s", (unsigned long)(elapsedS / 3600), (unsigned long)(elapsedS / 60 % 60),
           (unsigned long)(elapsedS % 60), (unsigned long)diagBoots, diagReset);
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
  canvas.drawString("[ ]\\ vol  spc play  v view  Fn+M mode", 4, 125);
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
  if (viz.eq) flightviz::analyze(viz, vizPcm, VIZ_PCM_N - 1, vizPcmPos.load(std::memory_order_acquire));
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

// ---- Modes (S2-1, design 8.3): Fn+M cycles measurement -> visualizer -> keyboard. Only the keyboard mode sends keys
// to the computer; the other two use them for the computer's volume and playback and for local settings. Without USB
// there is nothing to measure or show, so only the keyboard mode is shown (Bluetooth comes in S5).
enum Mode : uint8_t { MODE_DATA, MODE_GAME, MODE_KEYS, MODE_COUNT };
static const char* const MODE_NAME[MODE_COUNT] = {"measure", "flight", "keyboard"};
static uint8_t mode = MODE_DATA;  // chosen with Fn+M, remembered
static bool osWin = false;        // modifier profile (design 8.2), Fn+O, remembered
static int8_t inputKana = -1;     // the last input switch sent with Fn+[ / Fn+]: 0 A, 1 あ, -1 not yet. The computer
                                  // does not report its input mode (a Mac starts in whichever it used last), so this
                                  // is only what was asked for, and nothing is saved.
static Preferences prefs;

static uint8_t viewMode() { return (bool)USB ? mode : (uint8_t)MODE_KEYS; }

// ---- Emblem screen (S1-7, S1-8, design 8.3): shown at start and, in the measurement and visualizer modes, on a key
// with no job, for SPLASH_MS. A key with a job does it and closes the screen; v closes it to the measurement screen;
// Fn+M closes it and changes the mode; another key with no job starts the 2 seconds again. On a key it also shows
// how to get to the keyboard mode (splashHint).
static constexpr uint32_t SPLASH_MS = 2000;
static bool splashOn = false, splashDrawn = false, splashHint = false, splashAtStart = false;
static uint32_t splashStartMs = 0;

static void showSplash(bool hint) {
  if (!(splashOn && splashAtStart)) splashAtStart = false;
  hint = hint && !splashAtStart;  // the startup screen never shows the hint; a key only starts its 2 seconds again
  splashDrawn = splashOn && splashHint == hint;  // loop() draws it when it changes
  splashOn = true;
  splashHint = hint;
  splashStartMs = millis();
}

static void drawSplash() {
  uint16_t* buf = static_cast<uint16_t*>(canvas.getBuffer());
  memcpy(buf, SPLASH, sizeof SPLASH);
  if (splashHint) {
    // Darken the bottom of the grille to about 40 % and write the hint on it.
    for (int i = SPLASH_W * 121; i < SPLASH_W * SPLASH_H; i++) {
      uint16_t c = (uint16_t)((buf[i] << 8) | (buf[i] >> 8));
      uint16_t r = (c >> 11) * 2 / 5, g = ((c >> 5) & 63) * 2 / 5, b = (c & 31) * 2 / 5;
      c = (uint16_t)((r << 11) | (g << 5) | b);
      buf[i] = (uint16_t)((c << 8) | (c >> 8));
    }
    canvas.setTextColor(0xD69A);  // light gray
    canvas.drawCenterString("Fn+M  KEYBOARD", 120, 125);
  }
  canvas.pushSprite(0, 0);
}

// ---- Keyboard to the computer (keyboard mode)
// HID usage IDs of the Fn layer and the modifier bits of a boot keyboard report.
static constexpr uint8_t HID_ENTER = 0x28, HID_ESC = 0x29, HID_BACKSPACE = 0x2A, HID_TAB = 0x2B, HID_F1 = 0x3A,
                         HID_HOME = 0x4A, HID_PGUP = 0x4B, HID_DELETE = 0x4C, HID_END = 0x4D, HID_PGDN = 0x4E,
                         HID_RIGHT = 0x4F, HID_LEFT = 0x50, HID_DOWN = 0x51, HID_UP = 0x52, HID_LANG1 = 0x90,
                         HID_LANG2 = 0x91;
static constexpr uint8_t MOD_CTRL = 0x01, MOD_SHIFT = 0x02, MOD_ALT = 0x04, MOD_GUI = 0x08;
static KeyReport sentReport = {};

static void sendReport(const KeyReport& r) {
  if (!(bool)USB) {  // nobody to send to (and the send would wait for a timeout)
    sentReport = KeyReport{};
    return;
  }
  if (memcmp(&r, &sentReport, sizeof r) == 0) return;
  sentReport = r;
  KeyReport copy = r;
  kbd.sendReport(&copy);
}

static void releaseAllKeys() { sendReport(KeyReport{}); }

// The Fn layer (design 8.3) for one key, given by its unshifted label; 0 when Fn gives it no meaning here.
// *noShift tells the caller to drop Shift (Fn+Shift+arrows are PgUp, Home, PgDn, End).
static uint8_t fnKey(uint8_t label, bool shift, bool* noShift) {
  *noShift = false;
  switch (label) {
    case ';': *noShift = shift; return shift ? HID_PGUP : HID_UP;
    case ',': *noShift = shift; return shift ? HID_HOME : HID_LEFT;
    case '.': *noShift = shift; return shift ? HID_PGDN : HID_DOWN;
    case '/': *noShift = shift; return shift ? HID_END : HID_RIGHT;
    case '`': return HID_ESC;
    case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
      return (uint8_t)(HID_F1 + (label - '1'));
    case '0': return HID_F1 + 9;
    case '-': return HID_F1 + 10;
    case '=': return HID_F1 + 11;
    case KEY_BACKSPACE: return HID_DELETE;
    case '[': return HID_LANG2;  // A (英数 on a Japanese Mac keyboard)
    case ']': return HID_LANG1;  // あ (かな)
    default: return 0;           // Fn+M, Fn+O and the rest stay on the Cardputer
  }
}

// Sends the whole current key state, so a missed change can never leave a key stuck.
static void sendKeys(const Keyboard_Class::KeysState& ks, const uint8_t* labels, int n) {
  KeyReport r = {};
  if (ks.ctrl) r.modifiers |= MOD_CTRL;
  if (ks.shift) r.modifiers |= MOD_SHIFT;
  if (ks.alt) r.modifiers |= osWin ? MOD_ALT : MOD_GUI;  // Mac: Command, Windows: Alt
  if (ks.opt) r.modifiers |= osWin ? MOD_GUI : MOD_ALT;  // Mac: Option, Windows: Win
  int k = 0;
  if (!ks.fn) {
    for (uint8_t code : ks.hid_keys)
      if (k < 6) r.keys[k++] = code;
  } else {
    bool dropShift = false;
    for (int i = 0; i < n && k < 6; i++) {
      bool ns;
      uint8_t code = fnKey(labels[i], ks.shift, &ns);
      if (code) r.keys[k++] = code;
      dropShift = dropShift || ns;
    }
    if (dropShift) r.modifiers &= (uint8_t)~MOD_SHIFT;
  }
  sendReport(r);
}

static void setMode(uint8_t m) {
  if (mode == MODE_KEYS && m != MODE_KEYS) releaseAllKeys();
  mode = m;
  prefs.putUChar("mode", mode);
  splashOn = false;
  Serial.printf("[KEY] mode %s\n", MODE_NAME[mode]);
}

// ---- Keys
static uint8_t heldLabels[16];  // unshifted labels of the keys held at the last change
static int heldCount = 0;
static bool keysLockedAtStart = true;  // keys still held from the launcher (its Enter) are ignored until all are up

static void keyService() {
  if (!M5Cardputer.Keyboard.isChange()) return;
  auto ks = M5Cardputer.Keyboard.keysState();
  uint8_t labels[16];
  int n = 0;
  for (const auto& pos : M5Cardputer.Keyboard.keyList())
    if (n < 16) labels[n++] = M5Cardputer.Keyboard.getKeyValue(pos).value_first;
  if (keysLockedAtStart) {
    if (n > 0) return;
    keysLockedAtStart = false;
  }
  auto isNew = [&](uint8_t c) {
    for (int i = 0; i < heldCount; i++)
      if (heldLabels[i] == c) return false;
    return true;
  };
  bool keysMode = viewMode() == MODE_KEYS;
  bool acted = false, noJob = false;

  for (int i = 0; i < n; i++) {
    uint8_t c = labels[i];
    if (!isNew(c)) continue;
    if (ks.fn) {  // the Cardputer's own Fn keys, in every mode
      if (c == 'm') {
        if ((bool)USB) setMode((uint8_t)((mode + 1) % MODE_COUNT));
        acted = true;
      } else if (keysMode && (c == '[' || c == ']')) {
        inputKana = c == ']';
      } else if (c == 'o') {
        osWin = !osWin;
        prefs.putBool("osWin", osWin);
        Serial.printf("[KEY] profile %s\n", osWin ? "Windows" : "Mac");
        acted = true;
      }
      continue;
    }
    if (keysMode) {  // the computer gets them below; nothing is logged, so what is typed stays private
      acted = true;
      continue;
    }
    switch (c) {
      case '[': media.press(CONSUMER_CONTROL_VOLUME_DECREMENT); media.release(); acted = true; break;
      case ']': media.press(CONSUMER_CONTROL_VOLUME_INCREMENT); media.release(); acted = true; break;
      case '\\': media.press(CONSUMER_CONTROL_MUTE); media.release(); acted = true; break;
      case ' ': media.press(CONSUMER_CONTROL_PLAY_PAUSE); media.release(); acted = true; break;
      case ',': media.press(CONSUMER_CONTROL_SCAN_PREVIOUS); media.release(); acted = true; break;
      case '/': media.press(CONSUMER_CONTROL_SCAN_NEXT); media.release(); acted = true; break;
      case '-':
      case '=': {
        int v = boostDb + (c == '=' ? BOOST_STEP_DB : -BOOST_STEP_DB);
        if (v < 0) v = 0;
        if (v > BOOST_MAX_DB) v = BOOST_MAX_DB;
        boostDb = v;
        Serial.printf("[KEY] boost +%d dB\n", v);
        acted = true;
        break;
      }
      case 'b':
        bassCut = !bassCut;
        Serial.printf("[KEY] bass cut %s\n", bassCut ? "on" : "off");
        acted = true;
        break;
      case 'r':
        resetCounters();
        rw.active = false;
        bt = BufTrend();
        Serial.println("[KEY] counters reset");
        acted = true;
        break;
      case 'v':
        setMode(splashOn || mode == MODE_GAME ? MODE_DATA : MODE_GAME);
        acted = true;
        break;
      case KEY_LEFT_CTRL:
      case KEY_LEFT_SHIFT:
      case KEY_LEFT_ALT:
      case KEY_OPT:
      case KEY_FN:
        break;  // modifiers alone do nothing here
      default:
        noJob = true;  // letters, Enter, Del, Tab, ...
        break;
    }
  }
  if (keysMode) sendKeys(ks, labels, n);
  if (acted) splashOn = false;
  else if (noJob) showSplash(true);
  memcpy(heldLabels, labels, n);
  heldCount = n;
}

// ---- Keyboard mode screen (S2-1: text only; the spinning Earth of design 8.3 comes next). S2-2: a cheat sheet of the
// keys only this mode has, always on the right, keys in amber and their meaning in white; the last three lines follow
// the Mac / Windows profile.
// あ (Kana), 9 x 8 pixels, the bit for the left pixel first; Font0 has no kana. Drawn from Hiragino Sans W4 at 11 px
// and cleaned up by hand.
static const uint16_t KANA_A[8] = {0x020, 0x0FC, 0x024, 0x07E, 0x0A9, 0x131, 0x121, 0x0C6};

static void drawKanaA(int x, int y, int scale, uint16_t color) {
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 9; c++)
      if (KANA_A[r] & (0x100 >> c)) canvas.fillRect(x + c * scale, y + r * scale, scale, scale, color);
}

struct KeyHelp {
  const char* key;
  const char* meaning;
};

static void drawHelp(const char* key, const char* meaning, int x, int y, int meaningX) {
  canvas.setTextColor(COL_AMBER);
  canvas.drawString(key, x, y);
  canvas.setTextColor(COL_FG);
  canvas.drawString(meaning, meaningX, y);
}

static void drawKeys() {
  bool mounted = (bool)USB;
  canvas.fillSprite(COL_BG);
  canvas.setTextSize(2);
  canvas.setTextColor(COL_AMBER);
  canvas.drawString("KEY", 6, 10);
  canvas.drawString("BOARD", 6, 28);
  canvas.setTextSize(1);
  canvas.setTextColor(mounted ? COL_OK : COL_WARN);
  canvas.drawString(mounted ? "USB" : "NO USB", 6, 56);
  canvas.setTextColor(COL_FG);
  canvas.drawString(osWin ? "Win" : "Mac", 6, 70);
  // The input switch, beside the USB and Mac lines: always "A/あ", the last one chosen in amber (both white before).
  const int ix = 36, iy = 58;
  canvas.setTextSize(2);
  canvas.setTextColor(inputKana == 0 ? COL_AMBER : COL_FG);
  canvas.drawString("A", ix, iy);
  canvas.setTextColor(COL_FG);
  canvas.drawString("/", ix + 12, iy);
  canvas.setTextSize(1);
  drawKanaA(ix + 24, iy, 2, inputKana == 1 ? COL_AMBER : COL_FG);
  if (mounted) drawHelp("Fn+M", "mode", 6, 104, 36);
  drawHelp("Fn+O", "Mac/Win", 6, 118, 36);

  static const KeyHelp SHEET[] = {{"Fn+;,./", "Arrows"}, {"Fn+Sh+", "PgUp/End"}, {"Fn+`", "Esc"},
                                  {"Fn+1..=", "F1-F12"}, {"Fn+Del", "Delete"},  {"Fn+[ ]", "A /"}};
  static const KeyHelp MAC[] = {{"alt", "Command"}, {"opt", "Option"}, {"alt+C/V", "copy/paste"}};
  static const KeyHelp WIN[] = {{"alt", "Alt"}, {"opt", "Win"}, {"ctrl+C/V", "copy/paste"}};
  const int x = 116, mx = 166;  // "copy/paste" must end inside the visible area
  int y = 4;
  for (const KeyHelp& h : SHEET) {
    drawHelp(h.key, h.meaning, x, y, mx);
    if (h.meaning[0] == 'A' && h.meaning[1] == ' ') drawKanaA(mx + 24, y, 1, COL_FG);  // "A / あ"
    y += 13;
  }
  y += 3;
  for (const KeyHelp& h : osWin ? WIN : MAC) {
    drawHelp(h.key, h.meaning, x, y, mx);
    y += 13;
  }
  canvas.setTextColor(COL_DIM);
  canvas.drawRightString(BUILD_TAG, 236, 125);
  canvas.pushSprite(0, 0);
}

void setup() {
  USB.onEvent(onUsbEvent);
  Serial.begin(115200);
  diagBegin();
  crashBegin();
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
  showSplash(false);
  splashAtStart = true;
  prefs.begin("keyspeaker", false);
  mode = prefs.getUChar("mode", MODE_DATA);
  if (mode >= MODE_COUNT) mode = MODE_DATA;
  osWin = prefs.getBool("osWin", false);

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
  kbd.begin();
  media.begin();
  Serial.printf("[BOOT] ready, heap %u\n", (unsigned)ESP.getFreeHeap());
}

void loop() {
  diagMark(diagLoop, 1);
  M5Cardputer.update();
  diagMark(diagLoop, 2);
  usbLinkService();
  usbEventLog();
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

  diagUptimeS = now / 1000;
  diagMark(diagLoop, 3);
  static uint8_t lastView = MODE_COUNT;
  uint8_t view = viewMode();
  if (view != lastView) {
    if (lastView == MODE_KEYS) releaseAllKeys();
    lastView = view;
    lastScreen = 0;  // draw the new screen at once
  }
  vizView = view == MODE_GAME;
  if (splashOn && now - splashStartMs >= SPLASH_MS) splashOn = false;
  if (splashOn) {
    if (!splashDrawn) {
      drawSplash();
      splashDrawn = true;
    }
  } else if (view == MODE_KEYS) {
    if (now - lastScreen >= 250) {
      lastScreen = now;
      drawKeys();
    }
  } else if (vizView) {
    if (now - lastFrame >= FRAME_MS) {
      lastFrame = now;
      diagMark(diagLoop, 4);
      drawViz(now);
    }
  } else if (now - lastScreen >= 250) {
    lastScreen = now;
    diagMark(diagLoop, 5);
    drawScreen(fillNow, elapsedMs / 1000);
  }
  if (now - lastLog >= 1000) {
    lastLog = now;
    uint32_t fmin = fillMin.exchange(RING_SAMPLES), fmax = fillMax.exchange(0);
    float lim = limitDbMax.exchange(0);
    bufTrendSample(now, live, fmin, fmax);
    float drift = 0;
    bool driftOk = bufTrendPpm(&drift);
    Serial.printf("[S0] t=%lu stream=%d sof=%d fill=%lu ms min=%lu max=%lu under=%lu over=%lu drop=%lu rx_ppm=%+.1f spk_ppm=%+.1f vol=%d mute=%d boost=%d cut=%d lim=%.1f dB heap=%u view=%s draw_max=%.1f ms buf_ppm=%s%.1f buf_s=%lu adj_ppm=%+.1f boot=%lu rst=%s prev=p%lu/r%lu/l%lu@%lus on=%lu off=%lu susp=%lu pulse=%lu iso=kept%lu/passed%lu\n",
                  (unsigned long)(elapsedMs / 1000), (int)streaming.load(), (int)sofMoving, (unsigned long)(fillNow * 1000 / RATE),
                  (unsigned long)(fmin == RING_SAMPLES ? 0 : fmin * 1000 / RATE), (unsigned long)(fmax * 1000 / RATE), (unsigned long)underruns.load(),
                  (unsigned long)overruns.load(), (unsigned long)droppedSamples.load(), rxPpm, playPpm, (int)uac.volume(UAC_CHAN_MASTER),
                  (int)uac.mute(UAC_CHAN_MASTER), boostDb.load(), (int)bassCut.load(), lim, (unsigned)ESP.getFreeHeap(),
                  MODE_NAME[view], drawMaxUs.exchange(0) / 1000.0f, driftOk ? "" : "~", drift,
                  (unsigned long)(bt.sumMs / 1000), adjPpm.load(), (unsigned long)diagBoots, diagReset, (unsigned long)diagPrevPlay,
                  (unsigned long)diagPrevRx, (unsigned long)diagPrevLoop, (unsigned long)diagPrevUptimeS, (unsigned long)diagOns,
                  (unsigned long)diagOffs, (unsigned long)diagSuspends, (unsigned long)diagPulses,
                  (unsigned long)isoKeptCount.load(), (unsigned long)isoPassedCount.load());
    static uint32_t crashLogs = 0;
    if (crashLogs++ % 10 == 0) Serial.printf("[CRASH] saved dump: %s\n", crashInfo);
  }
  diagMark(diagLoop, 0);
  delay(5);
}
