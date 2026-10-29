# Cardputer ADV KeySpeaker

**Turn your Cardputer ADV into a small external keyboard and speaker for your computer.**

[日本語版 README](README_JA.md)

> **Status: design stage.** No firmware has been written yet. The design is in
> [`docs/DESIGN_JA.md`](docs/DESIGN_JA.md) (Japanese). Nothing in this repository has been tested on hardware.

## The idea

While building [RADIO DAYS SDR](https://github.com/kariagepompadour/RADIODAYS-SDR), I noticed that the Cardputer ADV's built-in
speaker sounds better than expected: good enough for background music, news, internet radio, podcasts and the
sound of videos. The Cardputer ADV also has a physical keyboard. So why not use both at the same time?

## Goals

| Connection | Speaker | Keyboard | Computer side |
| --- | --- | --- | --- |
| **USB cable** | USB Audio Class 1 speaker (48 kHz stereo in, played mono) | USB HID keyboard + media keys | Nothing to install (macOS, Windows, ChromeOS) |
| **Wireless** | Wi-Fi audio stream | Bluetooth LE HID keyboard + media keys | Keyboard: nothing to install. Audio: a small sender app is needed (see below) |

- Shift, Ctrl, Alt, Command/Windows, cursor keys, F-keys, volume, mute, play/pause through an `Fn` layer.
- The screen shows the connection, volume, mute, buffer health, battery and warnings.
- Installed as an app-only `.ino.bin` through an app launcher, like the author's other Cardputer ADV projects.

### Known limit: wireless audio

Wireless audio without any software on the computer is **not possible on this hardware**:

- The ESP32-S3 has no Classic Bluetooth, so Bluetooth audio (A2DP) is not available.
- The Cardputer ADV has no PSRAM; the known AirPlay receivers for the ESP32 need it.
- Windows and ChromeOS have no built-in way to send system audio to a generic Wi-Fi speaker.

Wireless audio will therefore use a small sender app on the computer (Windows first, then macOS).
USB audio and both keyboard connections need nothing installed.

## Repository layout

| Folder | Contents |
| --- | --- |
| [`docs/`](docs/) | Design and, later, verification notes |
| [`firmware/`](firmware/) | Cardputer ADV firmware (Arduino, `m5stack:esp32` 3.3.10) |
| [`sender/`](sender/) | Computer-side sender for wireless audio |

## Hardware

- M5Stack **Cardputer ADV** (ESP32-S3FN8, ES8311 codec, 1 W speaker, TCA8418 keyboard). The original Cardputer is
  not a target.
- USB-C **data** cable.

## License

[MIT](LICENSE)
