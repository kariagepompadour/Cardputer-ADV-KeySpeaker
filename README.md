# KeySpeaker

### USB Speaker & BLE Keyboard for M5Stack Cardputer ADV

**A pocket speaker and keyboard for your computer, phone or console. Keep it in your bag.**

[日本語版 README](README_JA.md)

> **Status: early development.** The design is in [`docs/DESIGN_JA.md`](docs/DESIGN_JA.md) (Japanese). The first
> probe, a USB speaker only ([`firmware/KeySpeaker_S0/`](firmware/KeySpeaker_S0/)), plays on a Mac at a good
> level. Long playback and the other checks are still to do.

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
- For typing on a console's text-entry screen, such as a PS5, as a Bluetooth LE keyboard.
- As a **USB-C earphone adapter**: plug earphones into the Cardputer's jack (mono: both ears get the same sound).

Shift, Ctrl, Alt, Command/Windows, cursor keys, F-keys, volume, mute and play/pause work through an `Fn` layer. The
keyboard is small: it is meant for short entries, not long texts. The screen shows the connection, volume, mute,
buffer health, battery and warnings.

## Scope

| | Devices |
| --- | --- |
| **Main targets** | macOS and Windows over USB (speaker + keyboard); Bluetooth LE keyboard |
| **To be checked one by one** | iPhone 15 or later and USB-C iPads, Android, digital audio players with USB audio out, PS5, Nintendo Switch; the Bluetooth LE keyboard on a PS5 |
| **Not planned** | ChromeOS, wireless audio, internet radio, audio input through the 3.5 mm jack |

### Why there is no wireless audio

The Cardputer ADV cannot be a wireless speaker without software on the other device:

- The ESP32-S3 has no Classic Bluetooth, so Bluetooth audio (A2DP) is not available.
- The Cardputer ADV has no PSRAM; the known AirPlay receivers for the ESP32 need it.
- Sending audio over Wi-Fi would need a sender app on the other device.

### One app, one job

The Cardputer ADV already has other jobs: internet radio is [RADIO DAYS](https://github.com/kariagepompadour/RADIODAYS),
a jog dial is [Media Jog](https://github.com/kariagepompadour/Cardputer-ADV-Media-Jog). Each is its own app, and
you switch between them with the launcher. KeySpeaker has no modes to pick.

## Power

Works best with a device that is plugged into power (a PC, a Mac mini, a console): the Cardputer is charged at the
same time. A phone or tablet with a single USB-C port powers the Cardputer from its own battery; a USB-C hub with
power pass-through (USB PD) lets it charge while in use. The Cardputer's power switch must be **on**.

## Repository layout

| Folder | Contents |
| --- | --- |
| [`docs/`](docs/) | Design and, later, verification notes |
| [`firmware/`](firmware/) | Cardputer ADV firmware (Arduino, `m5stack:esp32` 3.3.10) |

## Hardware

- M5Stack **Cardputer ADV** (ESP32-S3FN8, ES8311 codec, 1 W speaker, TCA8418 keyboard). The original Cardputer is
  not a target.
- USB-C **data** cable.

## License

[MIT](LICENSE)
