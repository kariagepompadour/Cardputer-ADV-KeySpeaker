# KeySpeaker

### USB Speaker & BLE Keyboard for M5Stack Cardputer ADV

**A pocket speaker and keyboard for your computer, phone or console. Keep it in your bag.**

[日本語版 README](README_JA.md)

> **Status: early development.** On a Mac and on Windows 11, one USB-C cable now gives you both: the speaker plays at
> a good level (8 hours without a dropout, pause and resume work) and the keyboard types, with volume and playback keys.
> An iPhone 15 works over USB too. Bluetooth and the tilt mouse are still to come. Firmware: [`firmware/KeySpeaker_S0/`](firmware/KeySpeaker_S0/).
> Design: [`docs/DESIGN.md`](docs/DESIGN.md). Test plan and results: [`docs/S0_TEST.md`](docs/S0_TEST.md).

## The idea

While building [RADIO DAYS SDR](https://github.com/kariagepompadour/RADIODAYS-SDR), I noticed that the Cardputer
ADV's built-in speaker sounds better than expected: good enough for background music, news, internet radio,
podcasts and the sound of videos. The Cardputer ADV also has a physical keyboard. So why not use both at the same
time?

## What it will do

- **Plug in a USB-C cable:** the Cardputer ADV becomes a speaker and a keyboard, over one cable.
- **Unplug it:** it becomes a Bluetooth LE keyboard.
- **Nothing to install on the other device.** Only the app-only `.ino.bin` goes onto the Cardputer, through a
  launcher. It switches between USB and Bluetooth LE by itself: there is nothing to set up.

Ways to use it:

- On a computer without speakers (a desktop PC, a Mac mini, a monitor).
- On a device that can send audio out of its USB-C port: a phone, a tablet, a digital audio player, a game console.
- As a Bluetooth LE keyboard for a Mac, an iPhone or an iPad, and with luck a console or a TV.
- As a **USB-C earphone adapter**: plug earphones into the Cardputer's jack (mono: both ears get the same sound).

The keyboard is small: it is meant for short entries (a search box, a password field, a console's text screen), not
long texts.

## Modes and keys

It starts with the KariPom emblem on a speaker grille, with `h HELP  Fn+M KEYBOARD` at the bottom. `Fn+M` then cycles
through three modes; the last one is remembered. In the Data and Game modes, after a minute with no key and no sound,
the emblem comes back and stays until you press a key (that key only wakes it). When music starts again, the emblem
shakes on the loud beats, like a speaker's cone.

| Mode | Screen | Keys |
| --- | --- | --- |
| **Data** | Buffer, clock drift, volume and level meters | Speaker and playback keys (below) |
| **Game** | A flight over the music: the volume as terrain, a little fighter, a Showa-era graphic equalizer | Speaker and playback keys (below) |
| **Keyboard** | A key cheat sheet (a slowly spinning Earth is coming) | Every key goes to the computer |

Speaker and playback keys (Data and Game modes):

| Key | Does |
| --- | --- |
| `[` / `]` / `\` | Volume down / up / mute on the computer |
| `Space` | Play / pause |
| `,` / `/` | Previous / next track |
| `-` / `=` | Boost down / up |
| `b` | Bass cut on / off |
| `r` | Reset the counters |
| `v` | Data ↔ Game |
| `h` | Opens / closes a help screen of these keys (they keep working while it is open) |
| any other key | Opens / closes the help screen, like `h` |

Keyboard mode (letters, Shift, Ctrl, Tab, Enter and Del go straight through):

| Key | Does |
| --- | --- |
| `Fn+;` `Fn+,` `Fn+.` `Fn+/` | ↑ ← ↓ → |
| `Fn+Shift+` the same | Page Up / Home / Page Down / End |
| `` Fn+` `` | Esc |
| `Fn+1` … `Fn+=` | F1 … F12 |
| `Fn+Del` | Forward delete |
| `Fn+[` / `Fn+]` | Japanese input: A / あ (the 英数 / かな keys, LANG2 / LANG1). The screen shows the last one, like the Mac's menu bar |
| `alt` / `opt` | Mac: Command / Option. Windows: Alt / Win |
| `Fn+O` | Mac ↔ Windows profile (remembered) |

Coming next: a **tilt mouse** for when there is no mouse at hand: hold the side button (G0) and tilt to move the
pointer, tap G0 to click, `Fn+G0` to right-click, G0 with `;` / `.` to scroll.

## Scope

| | Devices |
| --- | --- |
| **Main targets** | macOS and Windows over USB (speaker + keyboard); Bluetooth LE keyboard |
| **To be checked one by one** | iPhone 15 or later and USB-C iPads, Android, digital audio players with USB audio out, game consoles; the Bluetooth LE keyboard on other devices |
| **Not planned** | ChromeOS, wireless audio, internet radio, audio input through the 3.5 mm jack |

### Why there is no wireless audio

The Cardputer ADV cannot be a wireless speaker without software on the other device:

- The ESP32-S3 has no Classic Bluetooth, so Bluetooth audio (A2DP) is not available.
- The Cardputer ADV has no PSRAM; the known AirPlay receivers for the ESP32 need it.
- Sending audio over Wi-Fi would need a sender app on the other device.

### One app, one job

The Cardputer ADV already has other jobs: internet radio is [RADIO DAYS](https://github.com/kariagepompadour/RADIODAYS),
a jog dial is [Media Jog](https://github.com/kariagepompadour/Cardputer-ADV-Media-Jog). Each is its own app, and
you switch between them with the launcher. KeySpeaker needs no setup: it picks USB or Bluetooth LE by itself.

## Power

Works best with a device that is plugged into power (a PC, a Mac mini, a console): the Cardputer is charged at the
same time. A phone or tablet with a single USB-C port powers the Cardputer from its own battery; a USB-C hub with
power pass-through (USB PD) lets it charge while in use. The Cardputer's power switch must be **on**.

## Repository layout

| Folder | Contents |
| --- | --- |
| [`docs/`](docs/) | Design and test notes |
| [`firmware/`](firmware/) | Cardputer ADV firmware (Arduino, `m5stack:esp32` 3.3.10) |

## Hardware

- M5Stack **Cardputer ADV** (ESP32-S3FN8, ES8311 codec, 1 W speaker, TCA8418 keyboard). The original Cardputer is
  not a target.
- USB-C **data** cable.

## License

[MIT](LICENSE)
