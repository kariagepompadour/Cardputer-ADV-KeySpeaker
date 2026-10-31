# Design: KeySpeaker (USB Speaker & BLE Keyboard)

[日本語版](DESIGN_JA.md)

Written: 2026-10-29. Status: design draft. The USB speaker and keyboard (S0 to S2) have been checked on a Mac and on
Windows 11, and over USB on an iPhone 15 (chapter 12, "Progress"). Everything else is not yet checked on the device.

## 1. Purpose and product idea

Turn the M5Stack Cardputer ADV into an "external speaker and keyboard" that you can keep in your bag.

The product name is **KeySpeaker**. Wherever it stands out, it comes with the description **"USB Speaker & BLE
Keyboard"** (decided 2026-10-30). The screen, the USB device name, file names and so on use the short "KeySpeaker".
The name in the computer's list of outputs (a placeholder for now) becomes "KeySpeaker" in S2.

- **Plugged in over USB:** an external speaker and keyboard (one cable; no software or driver on the other device)
- **Unplugged:** a Bluetooth LE keyboard
- **Install:** just put the app-only `.ino.bin` on it through the Launcher

Intended uses:

- Plug it into a computer without speakers (a desktop PC, a Mac mini, a monitor and so on)
- Plug it into a device that can send audio over USB-C (a phone, a tablet, a digital audio player, a game console)
- Unplug it and use it as a Bluetooth (BLE) keyboard (Mac, iPhone, iPad and so on; game consoles and TVs are a bonus
  if they work)
- Plug earphones into the Cardputer's earphone jack and use it as a "USB-C earphone adapter" (chapter 6)

Priorities: works as soon as it is plugged in, few dropouts, stable over long runs, easy to hear, volume and mute
control. Basic function and stability come before looks.

### 1.1 Design principles

- **One app, one job.** More modes make people keep track of "which mode am I in". Internet radio is RADIO DAYS's job,
  the jog dial is Media Jog's, and switching between them is the Launcher's.
- **No settings to make.** It works as soon as it starts, with nothing to choose. Switching between USB and BLE is
  automatic too.
- A new, independent project, not an add-on to RADIO DAYS SDR. The existing repositories are not changed.

## 2. Scope

| Group | Targets |
|---|---|
| **Core targets** | USB connection to macOS and Windows (speaker plus keyboard), BLE keyboard |
| **Targets to add as they are checked** | iPhone 15 and later and USB-C iPads, Android, digital audio players with USB audio output, game consoles. Game consoles and TVs as a BLE keyboard. Checked so far: iPhone 15 over USB (sound, keyboard, Japanese input and volume keys, S2-10, 2026-10-31). Android is expected to work the same way over USB, but has not been tried |
| **Out of scope** | ChromeOS, wireless audio, internet radio, analog input through the 3.5 mm jack, a sender app on the computer |

## 3. Feasibility summary

| Wish | Verdict | Method |
|---|---|---|
| USB: speaker (no driver) | ◎ Expected to work | USB Audio Class 1 (UAC1) |
| USB: keyboard at the same time | ◎ | A UAC1 and HID composite device |
| Wireless: keyboard | ◎ (proven in Media Jog) | BLE HID |
| USB: use as an earphone adapter | ○ Likely (to be checked) | The ES8311's earphone output |
| Wireless: speaker | ✕ Out of scope (the other device would need software) | — |

## 4. Findings

### 4.1 The USB speaker can be built with the usual tools

- The m5stack:esp32 3.3.10 core (the same as RADIO DAYS SDR) includes a `USBAudioCard` class (checked in the local
  files). It was added to arduino-esp32 in April 2026
  ([issue #12053](https://github.com/espressif/arduino-esp32/issues/12053),
  [PR #12466](https://github.com/espressif/arduino-esp32/pull/12466)).
- On the ESP32-S3 it is UAC1, 48 kHz, 16 bit. UAC1 works with the built-in drivers of macOS, Windows, iOS and iPadOS,
  and most Android devices.
- The author of the PR reports testing on Windows 11, Ubuntu and a Mac.
- It can be combined with the same core's USB HID (keyboard, media keys).
- Espressif's UAC component for ESP-IDF "can support only one of Mac and Windows", according to its documentation
  ([documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_device/usb_device_uac.html)).
  This is another reason the Arduino side fits the goal.
- Caution: `USBAudioCard` is new, and the PR review raised points about composite devices.
- Bugs found on the device, and their workarounds (2026-10-30, S1-4 to S1-6):
  - The core's task that handles USB events has only a 2 KB stack. Printing a log there overflows it and restarts the
    Cardputer. The events are only noted there and printed in `loop()`.
  - When the computer resumes playback after a pause, TinyUSB (0.20.1) tries to close the audio endpoint that is still
    open (the ISO OUT endpoint), never finishes, and the watchdog restarts the Cardputer. A linker option in
    `firmware/build.sh` (`-Wl,--wrap=dcd_edpt_iso_activate`) puts `UsbIsoFix.h` in front of it: when the settings are
    the same, the endpoint is kept as it is instead of being closed.

### 4.2 The cause of regular dropouts

- The `USBAudioCard` speaker is adaptive and has no feedback endpoint.
- The clock difference between the other device and the Cardputer (about ±100 ppm) builds up in the buffer, which
  then overflows or runs dry at regular intervals. At 48 kHz that is about 17,000 samples (about 0.36 s) per hour.
- atomic14's report that "only the speaker crackles" ([article](https://blog.atomic14.com/2025/09/26/esp32-s3-usb-uac.html))
  matches this kind of problem.
- The ESP32-S3 has no APLL to fine-tune the output clock. So the Cardputer watches how full the buffer is and stretches
  or shrinks the playback speed very slightly (a simple asynchronous sample rate conversion). This is the most
  important fix for stability.
- Measured (2026-10-30, Mac): the computer runs about 10 to 17 ppm slower than the Cardputer. Without the correction
  (S0-6), the sound dropped out every 1.39 hours (3 times in 6 hours). With the S1 correction (up to ±200 ppm,
  proportional control from the buffer level, 4-point Hermite interpolation, `RateAdjust.h`), 3 hours of playback had
  no dropouts, and the correction settled at about −16 ppm.

### 4.3 Why wireless audio is out of scope

- The ESP32-S3 has BLE only: no Classic Bluetooth and no LE Audio. It cannot be a Bluetooth speaker (A2DP)
  ([ESP-ADF documentation](https://docs.espressif.com/projects/esp-adf/en/latest/solution-center/bluetooth-audio.html)).
- The Cardputer ADV (ESP32-S3FN8) has no PSRAM ([specs](https://docs.m5stack.com/en/core/Cardputer-Adv)). Every public
  AirPlay receiver for the ESP32 needs PSRAM ([airplay-esp32](https://github.com/rbouteiller/airplay-esp32),
  [squeezelite-esp32](https://github.com/sle118/squeezelite-esp32)).
- Windows and others have no standard way to send the computer's sound to an ordinary Wi-Fi speaker. Sending sound
  over Wi-Fi needs a sender app on the other device, which breaks the rule "just install the `.ino.bin`".

### 4.4 The 3.5 mm jack is output only

The Cardputer ADV's earphone jack is output only; it cannot take audio in. Plugging something in stops the built-in
speaker's amplifier ([M5Stack](https://docs.m5stack.com/en/core/Cardputer-Adv)). Audio comes in through USB-C.

### 4.5 Support as a BLE keyboard

macOS, Windows, iOS, iPadOS and Android accept a BLE (Bluetooth Low Energy) HID keyboard out of the box. Media Jog
already sends keys as a BLE keyboard. Whether game consoles, TVs and so on accept a Bluetooth keyboard over BLE
differs from device to device, and the documentation does not say. So the goal is "works as a BLE keyboard", checked
first on a Mac (S5). Other devices are tried in S6, starting with the ones at hand (decided 2026-10-30).

### 4.6 Prior work

- "Vibe Cardputer" did not turn up in searches (to be checked if there is a URL).
- No example was found that uses the Cardputer ADV's speaker and keyboard at the same time, or that supports both USB
  and BLE. This combination is new ground.

## 5. System overview

```
┌──────────────── Cardputer ADV (one firmware) ──────────────────┐
│ Keyboard (TCA8418) → key map / Fn layer → HID reports          │
│                                   ├→ USB HID   (USB mode)      │
│                                   └→ BLE HID   (BLE mode)      │
│ USB UAC1 48 kHz stereo                                         │
│     → L+R mix → speed fine-tuning → ring buffer → ES8311       │
│                               → built-in speaker / earphones   │
│ Link watch: USB SOF counter → automatic mode switch            │
│ Screen: link / volume / mute / buffer / battery / warnings     │
└────────────────────────────────────────────────────────────────┘
```

### 5.1 USB mode

- A composite device: the UAC1 speaker, HID (keyboard, Consumer Control), and a debug CDC used only during
  development.
- The number of endpoints should fit within the ESP32-S3's limit.
- Volume: in the data and game modes, `[` / `]` / `\` on the Cardputer send the HID volume keys to the other device
  (8.3). When the other device changes the volume, the value reaches the Cardputer through the UAC Feature Unit and is
  shown on the screen. There is one volume, and the other device's value is the one that counts.

### 5.2 BLE mode

- A BLE HID keyboard. Port Media Jog's NimBLE code (the HID service, saved pairings, releasing held keys when
  switching).
- No audio. No Wi-Fi either (Wi-Fi is used only by the Launcher for installing).
- Switching the connection (channels 1 to 3): like Logitech's Easy-Switch, it remembers pairings with up to three
  devices (a Mac, an iPad, a phone and so on) and switches which one it is connected to. Each channel uses its own BLE
  address, so the other devices see separate keyboards. The channel is chosen on the connection screen (`Fn+B`), and
  its number is shown on the keyboard mode's screen (added 2026-10-30; not yet checked on the device).

### 5.3 Switching modes

- Automatic: USB mode when there are USB SOFs (the signal the host sends every millisecond), BLE mode when there are
  none.
  - The Cardputer ADV cannot detect the cable from the supply voltage, so it uses SOF watching, proven in RADIO DAYS
    SDR's `usbLinkService()`.
  - With only a charger plugged in, it goes into BLE mode as well.
- Manual: `Fn+B` switches (the same key as Media Jog; a fallback).
- BLE is stopped while in USB mode, so the other device does not see two keyboards.
- Sending the keys to a BLE device while USB stays connected (or pairing one then) was considered and dropped
  (2026-11-01, with the user): few keyboards do that, and it would make the behavior harder to follow. To use or pair
  a BLE device, unplug USB first. Keeping it simple.
- The USB configuration is fixed at startup and cannot change while running. So every function is always included.

## 6. Use as an earphone adapter

While it runs as a USB speaker, plugging earphones into the Cardputer's earphone jack should stop the built-in
speaker and play through the earphones. It then works as a "USB-C earphone adapter" for phones and computers without
an earphone jack.

- Little or no extra code should be needed. The ES8311's output is already wired to the earphone jack.
- The ES8311 is mono, so both ears get the same sound.
- Not yet known: whether software can detect that earphones are plugged in (to change the screen or the volume limit).
- Check in S0 that the volume is not too loud and that there is no noise.

## 7. Power

This chapter is about the USB cable connection (speaker plus keyboard). As a BLE keyboard it needs no cable and runs
on the Cardputer's own battery, so the other device's power does not matter (a handheld console needs no dock).

- **Over USB, the best match is a device on mains power.** A desktop PC, a Mac mini, a laptop on its charger, a home
  console or a handheld console in its dock works just by plugging in, and charges the Cardputer too.
- **Phones and tablets** have a single USB-C port, so they cannot charge while the Cardputer is plugged in.
  - A USB-C hub with power pass-through (USB PD) lets them charge while in use (charger → hub → phone, with the
    Cardputer on the hub's data port).
  - For a short time, it is fine to use it as is. The phone's battery powers the Cardputer.
- Not yet known, as it depends on the charging circuit: whether software can stop the Cardputer from charging (to
  spare the phone). To check in S0.
- With the Cardputer's power switch OFF, it does not run and does not charge (a note from M5Stack).

## 8. Keyboard

TCA8418, 4 × 14 = 56 keys. Key presses and releases are read from the event FIFO. It suits "quick typing" (search
boxes, text input on consoles and TVs, short messages) more than long texts.

### 8.1 Keys sent as they are

Ordinary characters, Shift, Ctrl, Tab, Enter, Del (Backspace).

### 8.2 OS profiles for the modifier keys (chosen on the Cardputer and saved)

| Cardputer key | Mac profile | Windows profile |
|---|---|---|
| `opt` | Option | Win |
| `alt` | Command | Alt |

### 8.3 Modes and keys (decided 2026-10-30, built in S2)

**The emblem screen**

- At startup, the emblem screen shows for 2 seconds: a speaker grille with KariPom's metal emblem
  (`firmware/assets/splash.png`).
  - Its colors: the emblem in the original picture is gold, but on this LCD it looks silver. At one point colors were
    added to make it look gold, but silver looks more like a speaker's badge, so that was dropped. A little gold still
    remained, so the emblem's colors were turned to grays to make it plainly silver (2026-10-31, the user's decision;
    S2-8).
  - Its layout: the original picture's rubber bumper and black border are filled with grille, so the grille fills the
    screen. The emblem is at 80 % of its original size. As on a real speaker, the grille is the main part, but
    KariPom's face is still big enough to recognize. The user decided this on 2026-10-30. Versions with the bumper and
    a white base were made too, but dropped: white on the screen never matches the white of the Cardputer's case (a
    lit white against a lighted white, which look different under different light, and the screen has a black rim
    around it).
- A key press moves on at once. USB and audio get ready in the meantime.
- It is not one of the modes (seeing it now and then makes it fun; decided 2026-10-30, built in S1-7).
- In the data and game modes, after 1 minute with no key and no sound (louder than about −60 dB), the screen switches
  to it (the speaker resting). It stays until a key is pressed, and that key only brings the screen back; it does
  nothing else (decided with the user on 2026-10-31, built in S2-6. The first idea was the game screen only, coming
  back when music starts; instead both modes come back with a key. It does not switch while music is playing).
- Only the bottom of the grille is darkened, to show `h HELP  Fn+M KEYBOARD` (the keys in orange). This also shows at
  startup. At startup, keys still held from the Launcher are ignored, so nothing happens by mistake. When it starts in
  keyboard mode, only `Fn+M MODE` is shown.

**The emblem shakes**

- The emblem on this screen shakes with loud sounds, like a speaker cone (2026-10-31, the user's idea; built in S2-7).
- Every frame (about 33 ms), the sound's peak (after the computer's volume) is compared with the level just before
  (smoothed over about 0.3 s). When it is more than 1.4 times that level, the emblem moves 1 pixel (2 pixels for an
  especially big peak) in one of 8 directions, goes back 1 pixel in the next frame, and back home in the frame after.
  About 0.1 s in all.
- Quiet sound (a peak under 2000) does not shake it. It only reads how loud the sound is and does not touch the
  audio processing.
- When music starts after the screen has come up from being left alone, the resting speaker starts shaking with the
  sound.
- The grille and the emblem are kept apart (`firmware/assets/splash_grille.png`, `splash_emblem.png`,
  `splash_emblem_alpha.png`), and the emblem's edge blends smoothly over the grille, so no seams show when it moves.

**The help screen**

The data and game modes have a help screen that lists the keys they use (decided 2026-10-31 on the user's idea, built
in S2-6. There are many keys, so a screen of its own reads better than a hint that disappears after 2 seconds).

- `h` opens it, and `h` again goes back. A key with no job (a letter key and so on) works like `h`: it opens the help,
  and while the help is shown it goes back (2026-10-31, the user's idea after trying it on the device).
- While it is open, the keys with a job (volume, playback, `-` `=` `b` `r` and so on) still work, and the screen stays.
  You can try them while reading.
- `v` and `Fn+M` close the help and switch the screen or the mode.
- Both modes use the same keys, so it is the same help (the current mode is shown next to the title). Keys in orange,
  meanings in white.
- On the emblem screen at startup, the keys with a job work as usual and close the screen. `v` goes to the data view,
  and `h` and keys with no job go to the help.
- Not shown in keyboard mode (every key goes to the other device; that mode's keys are on its cheat sheet, which is
  always on the screen).

S1-8 first showed the emblem screen for 2 seconds on a key with no job. S2-1 added the `Fn+M  KEYBOARD` hint below it
and all the modes and keys in the tables below (except `Fn+B`; the keyboard mode's screen is text only at first, and
the spinning Earth comes in a later version). S2-6 replaced that with the help screen and the emblem screen after
being left alone.

**Modes**

Each `Fn+M` moves to the next mode: **data view → game screen → keyboard**. Keys go to the computer only in keyboard
mode. In the other two modes, the keys control the Cardputer and the sound. The screen tells you which way it is
being used. It remembers the last mode and starts in it next time.

When USB is not connected (used as a BLE keyboard), only keyboard mode is available. No sound comes in, so it does not
switch to the data view or the game screen, and `Fn+M` does not change the mode. Plugging in USB makes all three
modes available.

**In every mode**

| Key | Function |
|---|---|
| `Fn+M` | Switch modes (does nothing when USB is not connected) |
| `Fn+B` | Open or close the connection screen (S5; choose BLE channel 1 to 3) |

**Data view mode / game screen mode** (used as a speaker)

| Key | Function |
|---|---|
| `[` / `]` / `\` | Computer volume down / up / mute (HID media keys) |
| `Space` | Play / pause |
| `,` / `/` | Previous / next track (the same directions as the ← → printed on the keys) |
| `-` / `=` | Less / more boost (from S0; to be tidied up later) |
| `b` | Bass cut on or off (from S0) |
| `r` | Reset the counters (from S0) |
| `v` | Switch between the data view and the game screen (kept from S0; works like `Fn+M`) |
| `h` | Open or close the help screen (S2-6) |
| Any other key | Like `h`, opens or closes the help screen (S2-6) |

**Keyboard mode** (every key goes to the computer as an ordinary key; no volume control)

| Key | Function |
|---|---|
| Characters, Shift, Ctrl, Tab, Enter, Del | Sent as they are (Del is Backspace) |
| `opt` / `alt` | Follow the OS profile (8.2) |
| `Fn+; , . /` | ↑ ← ↓ → (as printed on the keys) |
| `Fn+Shift+arrow` | PgUp / Home / PgDn / End |
| `` Fn+` `` | Esc |
| `Fn+1` … `Fn+=` | F1 to F12 |
| `Fn+Del` | Forward delete |
| `Fn+[` / `Fn+]` | Eisu / Kana (HID LANG2 / LANG1; the same left-right order as the Eisu and Kana keys on a Mac keyboard. Whether they work on Windows is to be checked) |
| `Fn+O` | Switch the OS profile (Mac / Win, saved) |

**The keyboard mode's screen**

- At the top left, a slowly spinning Earth (to tie in with "Earth Defense" on the game screen; changed from KariPom's
  face on 2026-10-30). Built in S2-11 (`firmware/KeySpeaker_S0/KeyEarth.h`); BLE's gray Earth comes with S5.
  - The Earth is drawn with the same rectangular blocks as the graphic EQ and the terrain (6 × 3 px, every 8 px across
    and 4 px down), 12 columns by 24 rows (about 96 px square). It was first 16 by 32 (about 128 px square), and was
    made smaller so the cheat sheet can always be shown (2026-10-31, decided with the user from mockups). Sea in blue,
    land in green, polar regions in white, darker toward the edge.
  - A large `KEY` `BOARD` sits in white over the middle of the Earth, with the Earth behind the letters dimmed to a
    third of its brightness so they read well (2026-10-31: the letters on the Earth were the user's idea; the dimming
    was chosen over a black outline only and over amber letters, by comparing mockups). Only 3 px around each line is
    dimmed, a narrower area for `KEY` over a wider one for `BOARD`, so the corners beside `KEY` stay bright
    (2026-11-01, the user's idea; S2-12).
  - The land shapes come from the same original coarse map as the mockup (10-degree steps), to fit the coarseness of
    the blocks and match the terrain and the graphic EQ (2026-10-30, the user's decision).
  - As on the real Earth, the land flows from left to right, slowly, about one turn a minute.
  - Which latitude and longitude each block falls on is computed once at the start. Spinning only shifts the longitude
    and looks it up in the table (to keep drawing light).
  - The background is the same starry sky as the game screen (a black sky with mostly white stars, plus yellow and pink
    ones, in the same colors and proportions; the white stars twinkle now and then; none drawn behind the Earth).
  - While waiting for BLE pairing, the Earth is gray (with the land still faintly visible). It takes on color once
    connected.
- Under the Earth, lines 9 px apart: the connection (`USB`, or `BLE CH1` to `3`), the OS profile and `A/あ` on one
  line, and below them `Fn+B BLE CH` (over BLE, from S5), `Fn+O Mac/Win` and `Fn+[ ] A/あ`. `Fn+M mode` is at the top
  of the right side (it works in every mode), above the cheat sheet; it is shown only while USB is connected. Decided
  2026-11-01 with the user, by mockups: `Fn+[ ]` moved here from the cheat sheet, next to the `A/あ` it switches;
  `Fn+M` went to the top right, which leaves room under the Earth for `Fn+B` (the user's ideas). Putting all of these over the Earth, and shrinking the Earth to about 80 px so
  five lines fit under it, were tried too: the first dimmed most of the Earth, the second left little Earth around
  "KEY BOARD".
- A cheat sheet of the keys that only keyboard mode has (the Fn combinations, the jobs of `alt` and `opt` under the
  OS profile, and copy and paste; Eisu / Kana is under the Earth) is always on the screen (2026-10-30, the user's idea; keys in orange,
  meanings in white).
  - The input last switched with `Fn+[` / `Fn+]` is shown as "A" or "あ", as in the Mac's menu bar, to the right of the
    connection and OS on the left. The Mac does not report its input state, so it only shows the one switched from the
    Cardputer.
  - "A/あ" is always shown, with the last switched one in orange. The Mac starts in its last input even after a
    restart, so at startup neither is assumed, and both are shown in white. It is not remembered (2026-10-30, decided
    with the user).
  - "あ" is not in Font0, so it is drawn as pixel art made from Hiragino.
  - The keys' marks sit beside their names, 7 × 7 px pixel art: ⌘ beside Command and ⌥ beside Option on a Mac, and
    four panes beside Win on Windows (not the Windows logo itself) (2026-11-01, the user's idea; built in S2-11).
  - S2-2 put it on the right of the text-only screen. The cheat sheet stays when the Earth is drawn.
  - The right side's lines are 9 px apart, so `Fn+M`, the cheat sheet and the air mouse's four lines (8.3.1) all fit
    (checked in a mockup, 2026-11-01). The version tag is not shown on this screen; the other screens show it.
- No effects such as flashing on every key press (that would be too much). The game screen is lively, so keyboard
  mode has a quiet screen.

### 8.3.1 Moving the cursor by tilting (air mouse; decided 2026-10-30, to be built in S2.5)

In keyboard mode, tilting the Cardputer (its IMU) moves the mouse cursor. It is not for everyday use, but a fallback
when there is no mouse. So that the cursor does not wander while typing, it moves only while the button on the side
(G0) is held.

| Action | Function |
|---|---|
| Hold G0 and tilt | The cursor moves in the tilt direction (small tilts are ignored; a bigger tilt is faster) |
| Short press of G0 | Left click |
| `Fn+G0` | Right click |
| Hold G0 and press `;` / `.` | Scroll up / down (the ↑↓ printed on the keys) |

- A mouse (HID) is added to USB alongside the keyboard and media keys. The same over BLE (S5).
- These actions go on the keyboard mode's cheat sheet too (the user's wish). The screen layout was decided with
  mockups (8.3, 2026-10-31).
- The tilt directions and speeds, and the line between a long and a short press of G0, are tuned on the device.

### 8.4 Notes for Japanese setups

- The keys are printed in the US layout. The other device needs this keyboard set to the US (ANSI) layout.
- Eisu / Kana switching is `Fn+[` / `Fn+]` (8.3).

## 9. Audio processing

- It receives 48 kHz stereo from the other device and mixes L+R to mono on the Cardputer (with headroom so it does
  not clip). S0 also compares this with declaring itself as mono.
- Some devices want to keep sending 44.1 kHz (some audio players and so on). Support 44.1 kHz too if needed.
- To protect the small 1 W speaker and make voices easier to hear: a bass cut (150 Hz), a boost and a limiter (off
  with earphones). Built in S0-2 and checked on the device: the Cardputer's output level stays at unity, with a boost
  of +9 dB (fine up to +12 dB) and the bass cut on by default. The bass cut was hard to hear either way, but it is on
  to give the limiter more room when boosted. The `-` `=` `b` keys for comparing by ear will be tidied up in a later
  stage.
- Tuning the sound (after S1): a 2 to 4 kHz lift to make voices clearer, a virtual bass that adds bass harmonics so the
  bass seems present, and a limiter with less pumping and wobble. Each is decided by listening on the device.
- Output starts with `M5.Speaker`, proven in RADIO DAYS SDR. If that is not enough, move to driving I2S directly.
- Speed fine-tuning: set a target buffer level, and change the playback speed smoothly within ± a few hundred ppm
  according to the difference (linear interpolation).

## 10. Tools and existing code

- Arduino, m5stack:esp32 3.3.10, an arduino-cli build script (the same setup as RADIO DAYS SDR and Media Jog).
- Board settings: USB Mode = USB-OTG (TinyUSB), Flash 8MB, Partition `default_8MB`.
- Fallback: even if `USBAudioCard` has bugs, the audio function of the TinyUSB built into the core
  (`CONFIG_TINYUSB_AUDIO_ENABLED`) is already enabled. A UAC class of our own can live in the sketch, without moving to
  ESP-IDF.
- Code to reuse (copied; the original repositories are not changed):
  - RADIO DAYS SDR: the ideas in `RadioDays_Playout.h` (when to rebuffer and when to drop), SOF watching, what was
    learned about drawing the screen and the keyboard
  - Cardputer ADV Media Jog: NimBLE HID, the HID report layout, releasing held keys, NVS (saving settings)

## 11. Launcher compatibility

Expected to be kept.

- The app-only `.ino.bin` is installed through the Launcher's Wi-Fi upload (as in the two existing projects).
- RADIO DAYS SDR's finding applies: "started from the Launcher, Hardware CDC is not recognized, but TinyUSB works".
  TinyUSB is required for UAC and HID anyway, so this is no problem.
- Size: Media Jog (BLE) is 0.90 MB. With no Wi-Fi, adding USB audio to that should still fit with room to spare. To
  check in S0.

## 12. Development and test plan

| Stage | Content | Pass criteria |
|---|---|---|
| **S0** | A UAC1-speaker-only prototype. Log the buffer level to the CDC. Also look into the earphone jack, power and charging, and whether 44.1 kHz is needed | Recognized by macOS and Windows 11. Measure how the buffer overflows and runs dry over 1 hour of continuous playback |
| S1 | Build the speed fine-tuning | 8 hours of continuous playback with no audible dropouts |
| S1.5 | Tune the sound: a lift for the voice band, virtual bass, a better limiter (chapter 9) | Judged easier to hear than S0-2 when compared by ear on the device |
| S2 | A composite device of UAC, HID keyboard and media keys | On macOS and Windows, both work over one cable |
| S2.5 | Move the cursor by tilting (while G0 is held; 8.3.1) | On a Mac, holding G0 and tilting moves the cursor, and clicking and scrolling work. Releasing it stops the cursor |
| S3 | The full key map, OS profiles, the Fn layer | Every key checked on each OS |
| S4 | The status screen (usefulness first) | Every item is shown |
| S5 | Port BLE HID, switch modes automatically by SOF, switch connection channels (5.2) | Plugging and unplugging switches modes, and no key stays held. Typing works on a Mac as a BLE keyboard. Switching channels between two devices works |
| S6 | Widen the targets: iPhone and iPad, Android, audio players, game consoles | Record the sound, keyboard and power results for each device |
| S7 | A retro-device style screen design | — |

**Progress (2026-11-01)**

- S0: Recognized on a Mac and on Windows 11, and plays at a good level (a boost of +9 to +12 dB is good). A 6-hour
  measurement found the clock drift. Unplugging and plugging back in works on Windows. The earphone jack, power and
  44.1 kHz are not yet checked.
- S1: Passed. 8 hours 47 minutes of continuous playback on Windows 11 (S2-10) with no dropouts and no restart. Before
  that, 3 hours on a Mac and 3 hours 15 minutes on Windows (S2-9) with no dropouts, and ten pause and resume cycles
  with no restart on each.
- S2: Passed. On a Mac and on Windows 11 the speaker, the keyboard (including the A/あ key) and the media keys work over
  one cable. Windows needed an IAD in front of the speaker (S2-9), and lists it as "Speakers (KeySpeaker)" (S2-10). The
  keyboard screen's Earth (8.3) is built in S2-11 and checked on the device.
- S6 (early): an iPhone 15 over USB plays the sound and takes the keyboard and the volume keys.
- The device records are in `docs/S0_TEST.md`.

## 13. Main risks

1. How stable `USBAudioCard` really is on the Cardputer ADV (checked in S0). Nothing else works if this does not, so it
   is tested first. → On a Mac, the clock drift correction and the workarounds for the two bugs found (4.1) made it
   stable. Windows 11 needed an IAD in front of the speaker (S2-9) and then was stable for 8 hours (S2-10).
2. Whether power from phones and tablets gets the connection refused or drains the battery too fast (checked in S0
   and S6).
3. Whether a BLE keyboard works on each device (the Mac in S5, other devices in S6).
