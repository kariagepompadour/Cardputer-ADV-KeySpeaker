# KeySpeaker

### USB Speaker & BLE Keyboard for M5Stack Cardputer ADV

**A pocket speaker and keyboard for your computer, phone or console. Keep it in your bag.**

[日本語版 README](README_JA.md)

> **Status: early development.** The design is in [`docs/DESIGN_JA.md`](docs/DESIGN_JA.md) (Japanese). The USB
> speaker ([`firmware/KeySpeaker_S0/`](firmware/KeySpeaker_S0/), now with the clock-drift correction) plays on a
> Mac at a good level: 3 hours without a dropout, and pausing and resuming playback works. The keyboard is next.
> Windows and the other devices are not tested yet. Test notes: [`docs/S0_TEST_JA.md`](docs/S0_TEST_JA.md)
> (Japanese).

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

`Fn+M` switches the screen between a data view, a visualizer and the keyboard. Keys go to the other device only on
the keyboard screen, where Shift, Ctrl, Alt, Command/Windows, cursor keys and F-keys work through an `Fn` layer. On
the other two screens, a few keys control the volume, mute and play/pause. The keyboard is small: it is meant for
short entries, not long texts.

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
