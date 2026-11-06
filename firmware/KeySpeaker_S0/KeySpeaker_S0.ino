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
    - (S2-6) A help screen of the keys of the measurement and visualizer modes: h or a key with no job opens and
      closes it. After a minute with no key and no sound the emblem screen comes back and stays until a
      key, which only closes it. The emblem screen shows "h HELP  Fn+M KEYBOARD", at start too.
    - (S2-7) The emblem on that screen shakes a pixel or two on loud beats, like a speaker's cone. Splash.h now has
      the grille and the emblem apart. It only reads the level; the audio path is unchanged.
    - (S2-8) The emblem is plain silver: no extra warmth, and its colors turned to grays.
    - (S2-9) An IAD in front of the speaker's two interfaces (UsbAudioDesc.h), for Windows: without it, Windows 11 did
      not start the speaker (Code 10), though the keyboard worked.
    - (S2-10) The speaker's output terminal is a speaker, not headphones, so Windows lists it as "Speakers (KeySpeaker)".
    - (S2-11) The keyboard screen of design 8.3: the spinning block Earth with "KEY BOARD" over it in a star sky
      (KeyEarth.h), the connection, OS and A/あ under it, and the cheat sheet on the right.
    - (S2-12) Fn+M right after start sometimes did nothing. A key already down when Fn goes down now counts with Fn,
      so Fn+M works when M is read a moment before Fn; and Fn+M before the computer has taken the USB device is logged.
      The Earth is dimmed only around each line of "KEY BOARD", so the corners beside "KEY" stay bright.
    - (S2.5-1) The air mouse of design 8.3.1, in keyboard mode: hold G0 and tilt to move the cursor, measured from how
      the Cardputer was held when G0 went down; with G0 held, , and / are the left and right buttons and ; held with a
      tilt scrolls. A USB mouse is added after the keyboard and the media keys, and the IMU is switched on.
    - (S2.5-2) The IMU sits turned a quarter turn from the screen: its roll is the screen's forward and back tilt and
      its pitch the left and right one. S2.5-1 had them the other way, so the cursor went sideways for a forward tilt.
    - (S2.5-3) The wheel is one step per press of ; (up) or . (down) with G0 held, whatever the tilt: it is mostly
      for picking from a menu opened with a click. The tilt only moves the cursor.
    - (S2.5-4) With G0 held, Fn+; , . / are still the arrow keys (not the mouse), so a menu opened with a click can be
      walked one item at a time; picking one by tilting alone was hard.
    - (S5-1) A BLE keyboard and mouse (BleHid.h, ported from Media Jog with its BLE identity and GATT table, design
      5.2 plan A): 3 s after USB goes away (unplugged, or only a charger), the Cardputer advertises as "KeySpeaker";
      when USB comes back, BLE stops. Keys, the air mouse and the jog dial's keys go to whichever link is up. The
      keyboard screen shows "BLE" (green when ready, "BLE wait" in yellow before).
    - (S5-2) USB counts as there only while the computer's SOFs come in (design 5.3): the Cardputer ADV cannot see the
      cable, and after unplugging, the USB stack still reported "mounted", so S5-1 never started BLE. Without USB only
      the keyboard (and mouse) screen is shown, as before.
    - (S5-3) A log line for each USB / BLE switch, with what USB looked like then (mounted, SOF seen, how long ago),
      to find why S5-2 went to BLE about 2 s after start with USB connected.
    - (S5-4) With S5-2 and S5-3 the Mac never took the USB device: BLE started 3 s after start while the Mac was still
      setting USB up, and USB did not come up. Until USB has been seen once, BLE now waits 10 s; after USB was there
      and went away (unplugged), still 3 s.
    - (S5-5) USB does not come up at all since BLE was built in (S5-4 showed NO USB from the start). While USB is not
      up, the keyboard screen's bottom right shows whether the USB audio started and the free memory, and KS_BLE 0
      builds the same firmware without BLE, to compare.
    - (S5-6) Over BLE the keys did not arrive though the link was ready. The keyboard screen's bottom right now also
      shows, while USB is not up, the keyboard and mouse subscriptions (k, m), the reports accepted and refused, and the
      last refusal's code.
    - (S5-7) USB sometimes never came up after the launcher handed over: the computer sent no SOF at all, and the
      reconnect pulse ran only after SOFs had been seen and stopped. Now it also runs when no SOF has come 3 s after
      start, every 3 s until one does.
    - (S5-8) The ring buffer is 200 ms instead of 500 ms, to leave the BLE stack room: with BLE running only 32 KB were
      free. The buffer is kept near 80 ms (at most 81 ms seen in 8 hours), so 200 ms still leaves 120 ms above it.
    - (S5-9) The BLE keyboard: macOS did not create the HID device because the link was not yet encrypted when it read
      the HID service (seen in the Mac's log). The Cardputer now asks for encryption as soon as a computer connects, and
      the HID service can be read only over an encrypted link (BleHid.h).
    - (S5-10) Fn+R restarts KeySpeaker. After an unplug, USB does not come back when it is plugged in again, BLE or
      not (a quick unplug and replug before BLE started showed it), and the Cardputer ADV cannot sense USB power to
      restart by itself; so without USB the keyboard screen shows "Fn+R to USB".
    - (S5-11) Fn+R goes through 0.2 s of deep sleep instead of a plain restart: once USB had stopped, a plain restart
      (or the Launcher's) did not bring it back, only cutting the power did.
    - (S5-12) Tidied after the tests: the S5-7 pulse at start, the 200 ms buffer of S5-8 and Fn+R (S5-10, S5-11) are
      taken out again, none of them having helped; the buffer is back at the 500 ms of the 8-hour test. USB comes
      back after an unplug only with a real power cut, so without USB the keyboard screen says "Off+Reset to USB".
    - (S5-13) The emblem screen's grille is perforated metal with round holes instead of the weave, which did not
      read as a speaker on this screen (the user's call, from mockups; Splash.h, tools/make_grille.py). The weave
      inside the emblem is turned to black. The line "h HELP  Fn+M KEYBOARD" is shown only on the resting emblem
      screen: at start USB is still coming up, so those keys did not do what it said (the user noticed).
      At start the screen shows "KeySpeaker" under the emblem instead, large, white with a shadow (the user's idea).
      The game screen's top line shows only when something is wrong (not connected, muted, dropouts counted): the
      boost, bass cut and zero counters stayed there as if left over (the user's call).
    - (S5-14) The game screen no longer gives way to the resting emblem screen when left alone: it is there to be
      watched (the user's call). The data view still does.
    - (S5-15) In silence a target still comes every 6 seconds on the game screen (none came before), so it plays like
      a game's demo when left up (the user's call).
      Also left over and gone: the S5-5 / S5-6 diagnostics at the keyboard and mouse screens' bottom right, and "S0"
      in the data screen's title, now just "KeySpeaker" (the user noticed both).
    - (S9-1) A space monster on the game screen (FlightViz.h), the beast from the Space Invaders cabinet art, 69 x 90 px,
      feet on the bottom edge, only while music plays (after 3 s of sound; a gap of up to 2 s is not silence). It walks in
      from the right with the terrain, reaches for the fighter with a clawed hand, misses as the fighter slips away, draws
      its arm back and walks off; the next visit comes 12 to 28 s later. The body is behind the terrain, both arms are in
      front of everything. Black-red, lit from the left, glowing yellow eyes (the user's design, made and tried in the
      web demo first). No new state beyond a few dozen bytes. Nothing else changes: this build is the S5-15 code plus
      FlightViz.h (the S8-1 AUX input is not in it).
    - (S2.5-10) A jog dial, M5Stack's Unit Encoder (U135) on the Grove port as in Media Jog (design 8.3.2), when it is
      there: in the data and game modes turning it changes the computer's volume and pressing it plays or pauses; in
      keyboard mode it sends ← → and Enter. Looked for at start and once a second while missing.
    - (S2.5-8) The mouse's keys are laid out as a cross on the right, as E Z A D sit around S: Up above, Down below,
      Click left, R-click right, Enter in the middle.
    - (S2.5-7) While G0 is held, the Earth says "MOUSE" and the right side lists the mouse's keys, at once; the
      keyboard's cheat sheet keeps one line, "G0+tilt Move", instead of four.
    - (S2.5-6) With G0 held, the left hand's E Z A D (around S) work as well as the arrow keys ; . , /, since pressing
      keys on G0's side while holding it was hard. Up and down are the arrow keys again (a menu opened with a click
      did not move with the wheel), the left and right ones the mouse buttons, and S is Enter. No wheel for now.
    - (S2.5-5) The dark part behind "KEY BOARD" follows the Earth's blocks, whole blocks dimmed, with the block at each
      outer corner (above "KEY", below "BOARD") left out, so its corners are rounded off (KeyEarth.h).
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
#include "USBHIDMouse.h"
#include <Wire.h>
#include <Preferences.h>
#include "esp32-hal-tinyusb.h"
#include "soc/usb_struct.h"
#include <freertos/stream_buffer.h>
#include "FlightViz.h"
#include "RateAdjust.h"
#include "esp_core_dump.h"
#include "UsbIsoFix.h"
#include "UsbNames.h"
#include "UsbAudioDesc.h"
#include "Splash.h"
#include "KeyEarth.h"
#define KS_BLE 1  // (S5-5) 0: build without BLE, to compare
#include "BleHid.h"

#define BUILD_TAG "S10-7"

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
static constexpr size_t RING_SAMPLES = 24000;    // mono ring buffer: 500 ms (200 ms in S5-8 to S5-11)
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
USBHIDMouse mouse;  // (S2.5-1) the air mouse, after the keyboard

static StreamBufferHandle_t ring = nullptr;   // writer: uacReceiveTask (onSpkData), reader: playTask
static std::atomic<bool> micOn{false};         // (S10-1) the built-in microphone is the input: it moves the screen only, no sound out

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
static std::atomic<uint32_t> lastSoundMs{0};   // millis() of the last USB audio that was not silent (after its volume)
static constexpr int16_t SOUND_MIN = 32;       // a peak below this (about -60 dB) counts as silence
static std::atomic<int16_t> beatPeak{0};       // largest |sample| (after the volume) since the emblem screen's last frame
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
  if (micOn) {  // (S10-1) the microphone is the input: USB audio is dropped, and the speaker is off
    lastRxMs = millis();
    diagMark(diagRx, 0);
    return;
  }
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
  if (peak > SOUND_MIN) lastSoundMs = lastRxMs.load();
  if (peak > beatPeak) beatPeak = peak;
  diagMark(diagRx, 0);
}

// ---- Built-in microphone -> screen only (S10-1). The Cardputer ADV's ES8311 codec serves the speaker and the microphone
// on the same BCK / WS pins, and M5Unified's own mic example stops the speaker before starting the microphone, so
// this mode is "speaker off, microphone on" (setMic below). Nothing goes to the ring: the screen's inputs are the same
// as for USB audio (vizPcm for the equalizer, vizSum / vizCount for the terrain, beatPeak / lastSoundMs / rxPeak).
static constexpr float MIC_GAIN[] = {0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f};  // software gain on top of M5.Mic's own
static constexpr int MIC_GAINS = sizeof MIC_GAIN / sizeof MIC_GAIN[0];
static std::atomic<int> micLevel{2};  // the default (x2) was enough on the device (S10-6)
static constexpr uint32_t MIC_RATE = 16000;  // M5Unified's default capture rate, which worked as it is
static std::atomic<bool> micRun{false};
static TaskHandle_t micTaskH = nullptr;

static void micProcess(const int16_t* in, size_t n) {
  float g = MIC_GAIN[micLevel];
  static float vizAcc = 0;
  static size_t vizN = 0;
  int16_t peak = 0;
  uint32_t pos = vizPcmPos.load(std::memory_order_relaxed);
  for (size_t i = 0; i < n; i++) {
    float v = in[i] * g;
    int16_t m = v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : (int16_t)v;
    int16_t a = m < 0 ? (int16_t)(m == INT16_MIN ? INT16_MAX : -m) : m;
    if (a > peak) peak = a;
    vizAcc += (float)m * m;
    vizPcm[pos++ & (VIZ_PCM_N - 1)] = m;
  }
  vizPcmPos.store(pos, std::memory_order_release);
  vizN += n;
  if (vizN >= CHUNK) {
    vizSum += (uint32_t)sqrtf(vizAcc / vizN);
    vizCount++;
    vizAcc = 0;
    vizN = 0;
  }
  uint32_t now = millis();
  if (peak > rxPeak) rxPeak = peak;
  if (peak > SOUND_MIN) lastSoundMs = now;
  if (peak > beatPeak) beatPeak = peak;
}

// M5.Mic.record() only queues a buffer (two slots) and a background task fills it. Three buffers in rotation: after
// queueing buffer i, buffer i - 2 is certainly full, because queueing i needed a free slot.
static void micTask(void*) {
  static int16_t buf[3][CHUNK];
  memset(buf, 0, sizeof buf);
  int idx = 0;
  while (micRun) {
    if (!M5.Mic.record(buf[idx], CHUNK, MIC_RATE)) {
      vTaskDelay(1);
      continue;
    }
    micProcess(buf[(idx + 1) % 3], CHUNK);
    idx = (idx + 1) % 3;
  }
  micTaskH = nullptr;
  vTaskDelete(nullptr);
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
  canvas.drawString("KeySpeaker", 4, 2);
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

  if (micOn) {  // (S10-1)
    snprintf(l, sizeof l, "MIC (no sound out)  sens x%.1f  [- =]", MIC_GAIN[micLevel]);
  } else {
    snprintf(l, sizeof l, "PC vol %d dB%s  L%d R%d", (int)uac.volume(UAC_CHAN_MASTER), uac.mute(UAC_CHAN_MASTER) ? " MUTE" : "",
             (int)uac.volume(UAC_CHAN_LEFT), (int)uac.volume(UAC_CHAN_RIGHT));
  }
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
  canvas.drawString("[ ]\\ vol  spc play  v view  h help", 4, 125);
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

  // Status line, only when something is wrong (S5-13, the user's call; until then it was always there, dim): not
  // connected, muted, or dropouts counted. The rest (boost, bass cut) is on the data screen, so the terrain stays alone.
  char l[48];
  uint32_t u = underruns, o = overruns;
  bool mute = !micOn && uac.mute(UAC_CHAN_MASTER);
  canvas.setTextSize(1);
  if (micOn) {  // (S10-1)
    canvas.setTextColor(COL_DIM);
    snprintf(l, sizeof l, "MIC (no sound out)  x%.1f", MIC_GAIN[micLevel]);
    canvas.drawString(l, 4, 2);
  } else if (!(bool)USB) {
    canvas.setTextColor(COL_BAD);
    canvas.drawString("not connected", 4, 2);
  }
  if (!micOn && (mute || u || o)) {
    canvas.setTextColor(u || o ? COL_BAD : COL_WARN);
    snprintf(l, sizeof l, "%sU%lu O%lu", mute ? "MUTE " : "", (unsigned long)u, (unsigned long)o);
    canvas.drawRightString(l, 236, 2);
  }
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

// (S5-2) USB is there: mounted and the computer's SOFs (one a millisecond) seen within the last second. The Cardputer
// ADV cannot sense the cable, and after unplugging, the USB stack can still report it mounted. A computer asleep
// (USB suspended) sends no SOFs either, so it counts as gone too; BLE then starts, which is harmless.
static bool uacStarted = false;  // (S5-5) uac.begin() succeeded

static bool usbUp() { return (bool)USB && usbSawHost && millis() - usbLastSofMs < 1000; }

// (S10-1, S10-7) Without USB the keyboard screen is shown (Bluetooth), until Fn+M is pressed: then the screens follow Fn+M
// as they do over USB, so the microphone (or nothing) can drive the other two without a cable. With the microphone as the
// input they are there from the start. Once USB is up the choice is forgotten, so unplugging returns to the keyboard screen.
static bool navFree = false;
static uint8_t viewMode() {
  if (usbUp()) navFree = false;
  return usbUp() || micOn || navFree ? mode : (uint8_t)MODE_KEYS;
}

// ---- Emblem screen (S1-7, S2-6, design 8.3): shown at start for SPLASH_MS or until a key, and, in the measurement
// mode (not the game screen since S5-14), after IDLE_MS with no key and no sound, until a key (splashIdle). That key only closes it.
// At start a key does its job as usual; v closes the screen to the measurement screen.
static constexpr uint32_t SPLASH_MS = 2000, IDLE_MS = 60000;
static bool splashOn = false, splashDrawn = false, splashIdle = false;
static uint32_t splashStartMs = 0, lastKeyMs = 0;
static bool helpOn = false;  // the help screen of the measurement and visualizer modes (S2-6)

// The emblem shakes on loud beats (S2-7): a frame's peak well above the recent level kicks it 1 or 2 pixels one way,
// then 1 pixel back, then home, about 0.1 second in all.
static constexpr int16_t BEAT_MIN = 2000;  // quieter peaks never shake it
static int8_t shakeDx = 0, shakeDy = 0, shakeStep = 0, shakeX = 0, shakeY = 0;
static float beatLevel = 0;  // the recent peak level

static void showSplash(bool idle) {
  splashOn = true;
  splashDrawn = false;
  splashIdle = idle;
  splashStartMs = millis();
  beatPeak = 0;
  beatLevel = 0;
  shakeStep = shakeX = shakeY = 0;
}

// Once a frame while the emblem screen is up; true when the emblem has moved.
static bool shakeService() {
  float p = beatPeak.exchange(0);
  int8_t x = 0, y = 0;
  if (shakeStep == 0 && p >= BEAT_MIN && p > beatLevel * 1.4f) {
    static const int8_t DIRS[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
    const int8_t* d = DIRS[esp_random() & 7];
    int amp = p > 8000 && p > beatLevel * 2.2f ? 2 : 1;
    shakeDx = d[0];
    shakeDy = d[1];
    x = (int8_t)(shakeDx * amp);
    y = (int8_t)(shakeDy * amp);
    shakeStep = 1;
  } else if (shakeStep == 1) {
    x = (int8_t)-shakeDx;
    y = (int8_t)-shakeDy;
    shakeStep = 2;
  } else {
    shakeStep = 0;
  }
  beatLevel += (p - beatLevel) * 0.1f;  // about a third of a second
  bool moved = x != shakeX || y != shakeY;
  shakeX = x;
  shakeY = y;
  return moved;
}

// One line of key/meaning pairs, keys in amber and meanings in light gray, centered.
static constexpr uint16_t COL_HINT = 0xD69A;
static void drawHintLine(const char* const* pairs, int count, int y) {
  const int gap = canvas.textWidth("  ");
  int w = -gap;
  for (int i = 0; i < count; i += 2) w += canvas.textWidth(pairs[i]) + canvas.textWidth(" ") + canvas.textWidth(pairs[i + 1]) + gap;
  int x = 120 - w / 2;
  for (int i = 0; i < count; i += 2) {
    canvas.setTextColor(COL_AMBER);
    canvas.drawString(pairs[i], x, y);
    x += canvas.textWidth(pairs[i]) + canvas.textWidth(" ");
    canvas.setTextColor(COL_HINT);
    canvas.drawString(pairs[i + 1], x, y);
    x += canvas.textWidth(pairs[i + 1]) + gap;
  }
}

static void drawSplash() {
  uint16_t* buf = static_cast<uint16_t*>(canvas.getBuffer());
  memcpy(buf, SPLASH_GRILLE, sizeof SPLASH_GRILLE);
  // The emblem over the grille, where shakeService() has put it.
  for (int y = 0; y < EMBLEM_H; y++) {
    uint16_t* row = buf + (EMBLEM_Y + shakeY + y) * SPLASH_W + EMBLEM_X + shakeX;
    for (int x = 0; x < EMBLEM_W; x++) {
      uint32_t a = EMBLEM_A[y * EMBLEM_W + x];
      if (!a) continue;
      uint16_t e = EMBLEM_RGB[y * EMBLEM_W + x], c = e;
      if (a < 255) {
        uint16_t g = (uint16_t)((row[x] << 8) | (row[x] >> 8));
        uint32_t r = ((g >> 11) * (255 - a) + (e >> 11) * a) / 255, gr = (((g >> 5) & 63) * (255 - a) + ((e >> 5) & 63) * a) / 255,
                 b = ((g & 31) * (255 - a) + (e & 31) * a) / 255;
        c = (uint16_t)((r << 11) | (gr << 5) | b);
      }
      row[x] = (uint16_t)((c << 8) | (c >> 8));
    }
  }
  // At rest, darken the bottom of the grille to about 40 % and write how to reach the help and the keyboard on it.
  // Not at start (S5-13): USB is still coming up then, so h and Fn+M would not do what the line says.
  if (splashIdle) {
    for (int i = SPLASH_W * 121; i < SPLASH_W * SPLASH_H; i++) {
      uint16_t c = (uint16_t)((buf[i] << 8) | (buf[i] >> 8));
      uint16_t r = (c >> 11) * 2 / 5, g = ((c >> 5) & 63) * 2 / 5, b = (c & 31) * 2 / 5;
      c = (uint16_t)((r << 11) | (g << 5) | b);
      buf[i] = (uint16_t)((c << 8) | (c >> 8));
    }
    static const char* const SPEAKER[] = {"h", "HELP", "Fn+M", "KEYBOARD"};
    drawHintLine(SPEAKER, 4, 125);
  } else {
    // At start, the name under the emblem in large white letters, straight on the grille with a dark shadow (S5-13,
    // the user's idea; chosen over a dark plate behind it).
    canvas.setTextSize(2);
    canvas.setTextColor(COL_BG);
    canvas.drawString("KeySpeaker", 61, 108);
    canvas.setTextColor(COL_FG);
    canvas.drawString("KeySpeaker", 60, 107);
    canvas.setTextSize(1);
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
  if (!usbUp()) {  // (S5-1) over BLE when its link is ready; else nobody to send to
    if (!BleHid::ready()) {
      sentReport = KeyReport{};
      return;
    }
    if (memcmp(&r, &sentReport, sizeof r) == 0) return;
    static_assert(sizeof(KeyReport) == 8, "the BLE keyboard report is 8 bytes");
    if (BleHid::sendKeyboard((const uint8_t*)&r)) sentReport = r;
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
// While G0 holds the air mouse on (design 8.3.1), the arrow keys ; . , / and the left hand's E Z A D (around S) are
// the mouse's keys (S2.5-6): up and down send the arrow keys, left and right are the mouse buttons (airService), and
// S sends Enter. With Fn the arrow keys are the Fn layer's, as always.
static bool airOn = false;
static bool keysRedraw = false;  // the keyboard screen changes at once when the air mouse goes on or off (S2.5-7)
static uint8_t airHid(uint8_t code) {  // what a key sends while G0 is held; 0 for a mouse button
  switch (code) {
    case 0x33: case 0x08: return HID_UP;    // ; E
    case 0x37: case 0x1D: return HID_DOWN;  // . Z
    case 0x16: return HID_ENTER;            // S
    case 0x36: case 0x04: case 0x38: case 0x07: return 0;  // , A / D: the buttons
    default: return code;
  }
}

// extra: one more key pressed with the held ones (the jog dial's taps, S2.5-10).
static void sendKeys(const Keyboard_Class::KeysState& ks, const uint8_t* labels, int n, uint8_t extra = 0) {
  KeyReport r = {};
  if (ks.ctrl) r.modifiers |= MOD_CTRL;
  if (ks.shift) r.modifiers |= MOD_SHIFT;
  if (ks.alt) r.modifiers |= osWin ? MOD_ALT : MOD_GUI;  // Mac: Command, Windows: Alt
  if (ks.opt) r.modifiers |= osWin ? MOD_GUI : MOD_ALT;  // Mac: Option, Windows: Win
  int k = 0;
  if (!ks.fn) {
    for (uint8_t code : ks.hid_keys) {
      if (airOn) code = airHid(code);
      bool dup = false;
      for (int j = 0; j < k; j++) dup = dup || r.keys[j] == code;
      if (k < 6 && code && !dup) r.keys[k++] = code;
    }
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
  if (extra) {
    bool dup = false;
    for (int j = 0; j < 6; j++) dup = dup || r.keys[j] == extra;
    if (!dup) r.keys[k < 6 ? k : 5] = extra;
  }
  sendReport(r);
}

// The keys held now, sent again; with extra, that key pressed too.
static void resendKeys(uint8_t extra = 0) {
  uint8_t labels[16];
  int n = 0;
  for (const auto& pos : M5Cardputer.Keyboard.keyList())
    if (n < 16) labels[n++] = M5Cardputer.Keyboard.getKeyValue(pos).value_first;
  sendKeys(M5Cardputer.Keyboard.keysState(), labels, n, extra);
}

static void setMode(uint8_t m) {
  if (mode == MODE_KEYS && m != MODE_KEYS) releaseAllKeys();
  mode = m;
  prefs.putUChar("mode", mode);
  splashOn = false;
  helpOn = false;
  Serial.printf("[KEY] mode %s\n", MODE_NAME[mode]);
}

// (S10-1) Input: USB audio (plays on the speaker), or the built-in microphone (moves the screen, no sound out). The
// speaker and the microphone cannot run together on the ADV (shared BCK / WS), so one is stopped for the other.
static bool setMic(bool on) {
  if (on == micOn) return true;
  if (on) {
    micOn = true;  // onSpkData stops feeding the ring
    delay(30);
    xStreamBufferReset(ring);
    M5.Speaker.end();
    delay(50);  // as the CoreS3 project (KariPom/cores3-main) does: wait after Speaker.end() before Mic.begin()
    if (!M5.Mic.begin()) {
      Serial.println("[MIC] Mic.begin failed, back to USB");
      M5.Speaker.begin();
      M5.Speaker.setVolume(SPK_MASTER_UNITY);
      micOn = false;
      return false;
    }
    micRun = true;
    xTaskCreatePinnedToCore(micTask, "mic", 4096, nullptr, 3, &micTaskH, PRO_CPU_NUM);
  } else {
    micRun = false;
    while (micTaskH) delay(5);
    M5.Mic.end();
    M5.Speaker.begin();
    M5.Speaker.setVolume(SPK_MASTER_UNITY);
    micOn = false;
  }
  prefs.putBool("mic", on);
  Serial.printf("[MIC] input: %s\n", on ? "microphone (speaker off)" : "USB audio");
  return true;
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
  // Fn just went down: a key already held counts with it too, for when that key was read a moment before Fn (S2-12).
  bool fnLate = ks.fn && isNew(KEY_FN);
  lastKeyMs = millis();
  bool keysMode = viewMode() == MODE_KEYS;
  bool acted = false, noJob = false;
  if (splashOn && splashIdle && !keysMode) {  // the key that wakes the emblem screen only closes it
    for (int i = 0; i < n; i++)
      if (isNew(labels[i])) splashOn = false;
    memcpy(heldLabels, labels, n);
    heldCount = n;
    return;
  }

  for (int i = 0; i < n; i++) {
    uint8_t c = labels[i];
    bool fresh = isNew(c);
    if (!fresh && !fnLate) continue;
    if (ks.fn) {  // the Cardputer's own Fn keys, in every mode
      if (c == 'm') {
        uint8_t shown = viewMode();  // the screen the user sees now: the next one is counted from it
        navFree = true;
        setMode((uint8_t)((shown + 1) % MODE_COUNT));
        if (!fresh) Serial.println("[KEY] Fn+M: Fn came after M");
        acted = true;
      } else if (keysMode && (c == '[' || c == ']')) {
        inputKana = c == ']';
      } else if (c == 'i') {  // (S10-1) input: USB audio / microphone
        setMic(!micOn);
        keysRedraw = true;
        acted = true;
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
        if (micOn) {  // (S10-1) with the microphone as the input, - and = set its sensitivity
          int l = micLevel + (c == '=' ? 1 : -1);
          if (l < 0) l = 0;
          if (l >= MIC_GAINS) l = MIC_GAINS - 1;
          micLevel = l;
          Serial.printf("[KEY] mic sensitivity x%.1f\n", MIC_GAIN[l]);
          acted = true;
          break;
        }
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
      case 'h':
        helpOn = splashOn || !helpOn;
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
  if (noJob && !keysMode) helpOn = splashOn || !helpOn;  // like h: opens the help, or closes it
  if (acted || noJob) splashOn = false;
  memcpy(heldLabels, labels, n);
  heldCount = n;
}

// ---- Air mouse (S2.5-1, design 8.3.1): in keyboard mode, while G0 is held, the tilt from how the Cardputer was held
// when G0 went down moves the cursor; , or A is the left button and / or D the right one (S2.5-6; the up and down
// keys are sent as arrow keys by sendKeys). Small tilts are ignored, bigger ones go faster. Tuned on the device.
static constexpr uint32_t AIR_MS = 10;                  // a mouse report every 10 ms
static constexpr float AIR_DEAD_DEG = 3.0f;             // tilts smaller than this do nothing
static constexpr float AIR_GAIN = 0.115f;               // px per report = gain * (tilt beyond the dead zone)^1.5
// The IMU is turned a quarter turn from the screen (found on the device, S2.5-2): its pitch is the left and right
// tilt, its roll the forward and back one. Tilting the right side down moves the cursor right; the top down, up.
static constexpr float AIR_X_SIGN = 1, AIR_Y_SIGN = 1;
static constexpr float AIR_SMOOTH = 0.3f;               // share of a new IMU reading in the smoothed one

static bool airHeld(uint8_t label) {
  for (int i = 0; i < heldCount; i++)
    if (heldLabels[i] == label) return true;
  return false;
}

static float airSpeed(float deg, float gain) {  // signed, 0 inside the dead zone
  float e = fabsf(deg) - AIR_DEAD_DEG;
  if (e <= 0) return 0;
  return copysignf(gain * e * sqrtf(e), deg);
}

// One mouse report: to USB, or over BLE without it (S5-1). was: the buttons down before.
static void airMouse(uint8_t buttons, int8_t x, int8_t y, uint8_t was) {
  if (!usbUp()) {
    BleHid::sendMouse(buttons, x, y);
    return;
  }
  if (buttons & ~was) mouse.press(buttons & ~was);
  if (was & ~buttons) mouse.release(was & ~buttons);
  if (x || y) mouse.move(x, y);
}

static void airService(uint32_t now) {
  static uint32_t last = 0, lastLog = 0;
  static float ax = 0, ay = 0, az = 1, roll0 = 0, pitch0 = 0, fx = 0, fy = 0;
  static uint8_t buttons = 0;
  if (now - last < AIR_MS) return;
  last = now;
  bool on = viewMode() == MODE_KEYS && M5.BtnA.isPressed() && M5.Imu.isEnabled();
  if (on != airOn) {
    airOn = on;
    keysRedraw = true;
    if (on) {  // G0 just went down: this way of holding it is the rest position
      M5.Imu.update();
      M5.Imu.getAccel(&ax, &ay, &az);
      roll0 = atan2f(ay, az) * 180.0f / (float)M_PI;
      pitch0 = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / (float)M_PI;
      fx = fy = 0;
      Serial.printf("[AIR] on: accel %.2f %.2f %.2f\n", ax, ay, az);
    } else {
      airMouse(0, 0, 0, buttons);
      buttons = 0;
    }
    if (viewMode() == MODE_KEYS) resendKeys();  // the mouse's keys type again, or stop typing
  }
  if (!airOn) return;

  uint8_t b = (airHeld(',') || airHeld('a') ? MOUSE_LEFT : 0) | (airHeld('/') || airHeld('d') ? MOUSE_RIGHT : 0);
  if (M5Cardputer.Keyboard.keysState().fn) b &= buttons;  // Fn+, and Fn+/ are arrows: no new button press
  if (b != buttons) {
    airMouse(b, 0, 0, buttons);
    buttons = b;
  }

  float nx, ny, nz;
  M5.Imu.update();
  M5.Imu.getAccel(&nx, &ny, &nz);
  ax += (nx - ax) * AIR_SMOOTH;
  ay += (ny - ay) * AIR_SMOOTH;
  az += (nz - az) * AIR_SMOOTH;
  float roll = atan2f(ay, az) * 180.0f / (float)M_PI - roll0;
  float pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / (float)M_PI - pitch0;
  if (roll > 180) roll -= 360;
  if (roll < -180) roll += 360;
  float side = pitch, fore = roll;  // the screen's left-right and forward-back tilts

  fx += airSpeed(side, AIR_GAIN) * AIR_X_SIGN;
  fy += airSpeed(fore, AIR_GAIN) * AIR_Y_SIGN;
  float cx = fx > 127 ? 127 : fx < -127 ? -127 : fx, cy = fy > 127 ? 127 : fy < -127 ? -127 : fy;
  int8_t mx = (int8_t)cx, my = (int8_t)cy;
  fx -= mx;
  fy -= my;
  if (mx || my) airMouse(buttons, mx, my, buttons);
  if (now - lastLog >= 500) {
    lastLog = now;
    Serial.printf("[AIR] side %+.0f fore %+.0f%s%s\n", side, fore, buttons & MOUSE_LEFT ? " L" : "",
                  buttons & MOUSE_RIGHT ? " R" : "");
  }
}

// ---- Jog dial (S2.5-10, design 8.3.2): M5Stack's Unit Encoder (U135) on the Grove port (I2C address 0x40, SDA G2,
// SCL G1), read as Media Jog reads it. Two counts a click, clockwise up; the button reads 0 while pressed. In the data
// and game modes a click is one step of the computer's volume and a press plays or pauses; in keyboard mode a click
// is → or ← (with the keys held, so Shift selects) and a press is Enter. Looked for at start and, while missing, once
// a second; a unit that stops answering is dropped.
static constexpr uint8_t DIAL_ADDR = 0x40, DIAL_REG_COUNT = 0x10, DIAL_REG_BUTTON = 0x20;
static constexpr uint32_t DIAL_MS = 20;

static bool dialRead(uint8_t reg, uint8_t* buf, uint8_t len) {  // false instead of filler bytes
  Wire.beginTransmission(DIAL_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(DIAL_ADDR, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) {
    int b = Wire.read();
    if (b < 0) return false;
    buf[i] = (uint8_t)b;
  }
  return true;
}

static void dialService(uint32_t now) {
  static bool found = false;
  static uint32_t last = 0, lastProbe = 0;
  static int16_t lastCount = 0;
  static int accum = 0;
  static bool down = false;
  if (now - last < DIAL_MS) return;
  last = now;
  uint8_t d[2];
  if (!found) {
    if (lastProbe && now - lastProbe < 1000) return;
    lastProbe = now;
    if (!dialRead(DIAL_REG_COUNT, d, 2)) return;
    lastCount = (int16_t)(d[0] | (d[1] << 8));
    uint8_t b;
    down = dialRead(DIAL_REG_BUTTON, &b, 1) && b == 0;
    accum = 0;
    found = true;
    Serial.println("[DIAL] Unit Encoder found");
    return;
  }
  if (!dialRead(DIAL_REG_COUNT, d, 2)) {
    found = false;
    Serial.println("[DIAL] Unit Encoder lost");
    return;
  }
  int16_t count = (int16_t)(d[0] | (d[1] << 8));
  accum += (int16_t)(count - lastCount);
  lastCount = count;
  uint8_t b = 1;
  bool press = dialRead(DIAL_REG_BUTTON, &b, 1) && b == 0;
  bool keys = viewMode() == MODE_KEYS;
  while (accum >= 2 || accum <= -2) {
    bool up = accum > 0;
    accum += up ? -2 : 2;
    lastKeyMs = now;
    if (keys) {
      resendKeys(up ? HID_RIGHT : HID_LEFT);
      resendKeys();
    } else {
      media.press(up ? CONSUMER_CONTROL_VOLUME_INCREMENT : CONSUMER_CONTROL_VOLUME_DECREMENT);
      media.release();
    }
  }
  if (press && !down) {
    lastKeyMs = now;
    if (keys) {
      resendKeys(HID_ENTER);
      resendKeys();
    } else {
      media.press(CONSUMER_CONTROL_PLAY_PAUSE);
      media.release();
    }
  }
  down = press;
}

// ---- USB or BLE (S5-1, design 5.3): BLE starts once USB has been gone for BLE_AFTER_MS (unplugged, only a charger,
// or not yet taken at start) and stops as soon as USB is there, so the other device never sees two keyboards.
static constexpr uint32_t BLE_AFTER_MS = 3000;
static constexpr uint32_t BLE_AFTER_START_MS = 10000;  // (S5-4) before USB was ever seen: let the computer set it up

static void linkService(uint32_t now) {
  static uint32_t usbGoneSince = 0;
  static bool wasUp = false;
  bool up = usbUp();
  if (up != wasUp) {  // (S5-3) why it changed
    wasUp = up;
    Serial.printf("[LINK] t=%lu usb %s: mounted %d, sof seen %d, last sof %lu ms ago\n", (unsigned long)now,
                  up ? "up" : "gone", (int)(bool)USB, (int)usbSawHost, (unsigned long)(now - usbLastSofMs));
  }
  if (up) {
    usbGoneSince = 0;
    if (BleHid::on) {
      BleHid::stop();
      keysRedraw = true;
    }
  } else {
    if (!usbGoneSince) usbGoneSince = now ? now : 1;
    if (!BleHid::on && now - usbGoneSince >= (usbSawHost ? BLE_AFTER_MS : BLE_AFTER_START_MS)) {
      Serial.printf("[LINK] t=%lu BLE on: mounted %d, sof seen %d, last sof %lu ms ago\n", (unsigned long)now,
                    (int)(bool)USB, (int)usbSawHost, (unsigned long)(now - usbLastSofMs));
      BleHid::start();
      keysRedraw = true;
    }
  }
  if (BleHid::service(now)) {  // ready changed: the computer starts from all keys up
    sentReport = KeyReport{};
    keysRedraw = true;
  }
}

// ---- Keyboard mode screen (design 8.3). S2-2: a cheat sheet of the keys only this mode has, always on the right, keys
// in amber and their meaning in white; the last three lines follow the Mac / Windows profile. S2-11: the spinning Earth
// at the top left (KeyEarth.h), and the connection, OS and A/あ under it.
// あ (Kana), 9 x 8 pixels, the bit for the left pixel first; Font0 has no kana. Drawn from Hiragino Sans W4 at 11 px
// and cleaned up by hand.
static const uint16_t KANA_A[8] = {0x020, 0x0FC, 0x024, 0x07E, 0x0A9, 0x131, 0x121, 0x0C6};

static void drawKanaA(int x, int y, int scale, uint16_t color) {
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 9; c++)
      if (KANA_A[r] & (0x100 >> c)) canvas.fillRect(x + c * scale, y + r * scale, scale, scale, color);
}

// The keys' marks beside their names on the cheat sheet, 7 x 7 pixels, the bit for the left pixel first (S2-11):
// Command (⌘), Option (⌥), and four panes for the Windows key (not the logo itself).
static const uint8_t ICON_CMD[7] = {0x22, 0x55, 0x3E, 0x14, 0x3E, 0x55, 0x22};
static const uint8_t ICON_OPT[7] = {0x67, 0x10, 0x10, 0x08, 0x08, 0x07, 0x00};
static const uint8_t ICON_WIN[7] = {0x77, 0x77, 0x77, 0x00, 0x77, 0x77, 0x77};

static void drawIcon(const uint8_t* icon, int x, int y, uint16_t color) {
  for (int r = 0; r < 7; r++)
    for (int c = 0; c < 7; c++)
      if (icon[r] & (0x40 >> c)) canvas.drawPixel(x + c, y + r, color);
}

struct KeyHelp {
  const char* key;
  const char* meaning;
  const uint8_t* icon = nullptr;  // drawn after the meaning
};

static void drawHelp(const char* key, const char* meaning, int x, int y, int meaningX) {
  canvas.setTextColor(COL_AMBER);
  canvas.drawString(key, x, y);
  canvas.setTextColor(COL_FG);
  canvas.drawString(meaning, meaningX, y);
}

static void drawKeys() {
  bool mounted = usbUp();
  uint32_t now = millis();
  canvas.fillSprite(COL_BG);
  KeyEarth::draw(canvas, now, COL_FG, airOn);
  // Under the Earth, lines 9 px apart: the connection, the OS profile and the input switch, always "A/あ" with the
  // last one chosen in amber (both white before); then the keys of this side, Fn+[ ] next to the A/あ it switches
  // (2026-11-01). The connection: USB, or BLE (S5-1), green when ready, "BLE wait" in yellow while advertising or
  // pairing, "NO USB" before BLE starts.
  int x = 4;
  const char* link = mounted ? "USB" : BleHid::ready() ? "BLE" : BleHid::on ? "BLE wait" : "NO USB";
  canvas.setTextColor(mounted || BleHid::ready() ? COL_OK : COL_WARN);
  canvas.drawString(link, x, 99);
  x += canvas.textWidth(link) + 6;
  canvas.setTextColor(COL_FG);
  canvas.drawString(osWin ? "Win" : "Mac", x, 99);
  x += 24;
  canvas.setTextColor(inputKana == 0 ? COL_AMBER : COL_FG);
  canvas.drawString("A", x, 99);
  canvas.setTextColor(COL_FG);
  canvas.drawString("/", x + 6, 99);
  drawKanaA(x + 12, 99, 1, inputKana == 1 ? COL_AMBER : COL_FG);
  drawHelp("Fn+O", "Mac/Win", 4, 108, 46);
  drawHelp("Fn+[ ]", "A/", 4, 117, 46);
  drawKanaA(46 + 12, 117, 1, COL_FG);
  // (S5-12) Without USB: USB comes back only after a real power cut (switch off, then the reset button)
  if (!mounted) drawHelp("Off+Reset", "to USB", 4, 126, 64);

  // The right side, lines 9 px apart: Fn+M at the top (it works in every mode), then the cheat sheet, and
  // "G0+tilt Move" when the IMU is there. No version tag here: the other screens show it. While G0 is held, the mouse's
  // keys instead (S2.5-7), as a cross like E Z A D around S (S2.5-8).
  static const KeyHelp SHEET[] = {{"Fn+;,./", "Arrows"}, {"Fn+Sh+", "PgUp/End"}, {"Fn+`", "Esc"},
                                  {"Fn+1..=", "F1-F12"}, {"Fn+Del", "Delete"}};
  static const KeyHelp MAC[] = {{"alt", "Command", ICON_CMD}, {"opt", "Option", ICON_OPT}, {"alt+C/V", "copy/paste"}};
  static const KeyHelp WIN[] = {{"alt", "Alt"}, {"opt", "Win", ICON_WIN}, {"ctrl+C/V", "copy/paste"}};
  const int sx = 108, mx = 164;  // clear of "ctrl+C/V", and "copy/paste" still ends inside the visible area
  int y = 2;
  if (airOn) {
    // A key and its meaning, centred on cx: the key in amber, the meaning in white under it.
    auto cross = [](const char* key, const char* meaning, int cx, int y) {
      canvas.setTextColor(COL_AMBER);
      canvas.drawString(key, cx - canvas.textWidth(key) / 2, y);
      canvas.setTextColor(COL_FG);
      canvas.drawString(meaning, cx - canvas.textWidth(meaning) / 2, y + 10);
    };
    const int cl = 128, cm = 172, cr = 216;  // the cross's columns
    drawHelp("tilt", "Move", 110, 3, 140);
    cross("E ;", "Up", cm, 20);
    cross("A ,", "Click", cl, 52);
    cross("S", "Enter", cm, 52);
    cross("D /", "R-click", cr, 52);
    cross("Z .", "Down", cm, 84);
    drawHelp("A+tilt", "Drag", 110, 112, 152);
    canvas.setTextColor(COL_DIM);
    canvas.drawString("let go of G0: keys", 110, 125);
  } else {
    drawHelp("Fn+M", "mode", sx, y, mx);  // (S10-7) works without USB too
    if (micOn) {  // (S10-7) the microphone moves the other screens: a reminder here, since this screen does not react
      canvas.setTextColor(COL_OK);
      canvas.drawString("MIC ON", 198, y);
    }
    y += 12;
    for (const KeyHelp& h : SHEET) {
      drawHelp(h.key, h.meaning, sx, y, mx);
      y += 9;
    }
    y += 3;
    for (const KeyHelp& h : osWin ? WIN : MAC) {
      drawHelp(h.key, h.meaning, sx, y, mx);
      if (h.icon) drawIcon(h.icon, mx + canvas.textWidth(h.meaning) + 4, y, COL_FG);
      y += 9;
    }
    if (M5.Imu.isEnabled()) drawHelp("G0+tilt", "Move", sx, y + 3, mx);  // the way into the air mouse (design 8.3.1)
  }
  // (S5-13) The S5-5 / S5-6 diagnostics at the bottom right (uac, free memory, BLE subscriptions and reports) are gone
  // from the screen now that BLE works; they looked left over (the user noticed). The serial log keeps [LINK] lines.
  canvas.pushSprite(0, 0);
}

// ---- Help screen of the measurement and visualizer modes (S2-6): h or a key with no job opens it and closes it.
// The keys with a job still work while it is open, so they can be tried from it; v and Fn+M close it.
static void drawSpeakerHelp() {
  canvas.fillSprite(COL_BG);
  canvas.setTextSize(2);
  canvas.setTextColor(COL_AMBER);
  canvas.drawString("HELP", 6, 4);
  canvas.setTextSize(1);
  canvas.setTextColor(COL_DIM);
  canvas.drawString(mode == MODE_GAME ? "flight mode" : "measure mode", 60, 10);
  canvas.drawRightString(BUILD_TAG, 236, 4);
  const KeyHelp LEFT[] = {{"[ ]", "volume"}, {"\\", "mute"}, {"space", "play/pause"},
                          {", /", "prev/next"}, {"- =", micOn ? "mic sens" : "boost"}, {"b", "bass cut"}};  // (S10-7) - = sets the mic's sensitivity
  const KeyHelp RIGHT[] = {{"v", "measure/flight"}, {"r", "reset counts"}, {"h", "close help"},
                           {"Fn+M", "next mode"}, {"Fn+O", "Mac/Win"}, {"Fn+I", micOn ? "input: MIC" : "input: USB"}};
  int y = 30;
  for (const KeyHelp& h : LEFT) {
    drawHelp(h.key, h.meaning, 6, y, 44);
    y += 15;
  }
  y = 30;
  for (const KeyHelp& h : RIGHT) {
    drawHelp(h.key, h.meaning, 124, y, 156);
    y += 15;
  }
  char l[32];
  snprintf(l, sizeof l, "+%ddB %s  %s", boostDb.load(), bassCut ? "cut" : "full", uac.mute(UAC_CHAN_MASTER) ? "MUTE" : "");
  canvas.setTextColor(COL_DIM);
  canvas.drawString(l, 124, 30 + 6 * 15);
  canvas.pushSprite(0, 0);
}

void setup() {
  USB.onEvent(onUsbEvent);
  Serial.begin(115200);
  diagBegin();
  crashBegin();
  Serial.printf("[BOOT] KeySpeaker %s\n", BUILD_TAG);

  auto cfg = M5.config();
  cfg.internal_imu = true;  // (S2.5-1) for the air mouse
  cfg.internal_spk = true;
  cfg.internal_mic = true;  // (S10-1) set up, not started: M5.Mic runs only in the microphone input
  cfg.serial_baudrate = 0;  // Serial is USB CDC, already started
  // (S10-6) Init the display BEFORE M5Cardputer.begin(): otherwise M5Unified drives GPIO46
  // (the internal mic's data-in) as an output-high "power hold" and the mic reads a flat line.
  M5.Display.init();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(128);
  canvas.createSprite(240, 135);
  canvas.setFont(&fonts::Font0);
  KeyEarth::begin();
  showSplash(false);
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
  bool micWanted = prefs.getBool("mic", false);

  xTaskCreatePinnedToCore(playTask, "play", 4096, nullptr, 3, nullptr, APP_CPU_NUM);
  uac.onEvent(onUsbEvent);
  uac.onData(onSpkData);
  uacStarted = uac.begin();
  if (!uacStarted) Serial.println("[BOOT] USB audio failed to start");
  kbd.begin();
  media.begin();
  mouse.begin();
  Wire.begin(2, 1, 200000);  // (S2.5-10) the Grove port, for a jog dial: SDA G2, SCL G1, as in Media Jog
  if (micWanted) setMic(true);
  Serial.printf("[BOOT] IMU %s\n", M5.Imu.isEnabled() ? "on" : "not found");
  Serial.printf("[BOOT] ready, heap %u\n", (unsigned)ESP.getFreeHeap());
}

void loop() {
  diagMark(diagLoop, 1);
  M5Cardputer.update();
  diagMark(diagLoop, 2);
  usbLinkService();
  usbEventLog();
  keyService();
  airService(millis());
  dialService(millis());
  linkService(millis());

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
  if (splashOn && !splashIdle && now - splashStartMs >= SPLASH_MS) splashOn = false;
  if (view == MODE_KEYS) helpOn = false;
  uint32_t lastSound = lastSoundMs;
  if (!splashOn && view == MODE_DATA && now - (lastKeyMs > lastSound ? lastKeyMs : lastSound) >= IDLE_MS) {
    helpOn = false;
    showSplash(true);
  }
  if (splashOn) {
    if (now - lastFrame >= FRAME_MS) {
      lastFrame = now;
      if (shakeService()) splashDrawn = false;
    }
    if (!splashDrawn) {
      drawSplash();
      splashDrawn = true;
    }
  } else if (view == MODE_KEYS) {
    if (now - lastScreen >= 250 || keysRedraw) {
      keysRedraw = false;
      lastScreen = now;
      drawKeys();
    }
  } else if (helpOn) {
    if (now - lastScreen >= 250) {
      lastScreen = now;
      drawSpeakerHelp();
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
