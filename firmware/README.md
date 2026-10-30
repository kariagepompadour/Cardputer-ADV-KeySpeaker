# firmware

Cardputer ADV firmware. Each stage is its own sketch folder with its app-only `.ino.bin` and `SHA256SUMS.txt`.

| Folder | Stage | Status |
| --- | --- | --- |
| [`KeySpeaker_S0/`](KeySpeaker_S0/) | S0: USB Audio Class 1 speaker only; measures the buffer and the clock drift. S0-3 adds a flight visualizer screen (`v`). S1 (now `S1-6`) corrects the clock drift and fixes two USB restarts | On a Mac: 3 hours with no dropouts (without the S1 correction, one every 1.4 hours); ten pause/resume cycles without a restart. Test plan: [`../docs/S0_TEST_JA.md`](../docs/S0_TEST_JA.md) |

## Building

```bash
./build.sh KeySpeaker_S0
```

Needs `arduino-cli`, the `m5stack:esp32` core 3.3.10 and the libraries M5Cardputer, M5Unified and M5GFX. Settings
used: board M5Cardputer, USB Mode = USB-OTG (TinyUSB), USB CDC On Boot = Enabled, Flash 8MB, Partition
`default_8MB`. `build.sh` also passes the linker option `-Wl,--wrap=dcd_edpt_iso_activate`, which puts a guard
(`KeySpeaker_S0/UsbIsoFix.h`) in front of a TinyUSB function that can hang when the computer resumes playback. A build
without it (for example from the Arduino IDE) still works, but restarts the Cardputer on such a resume. It also passes
`-Wl,--wrap=tinyusb_add_string_descriptor`, so the computer lists the speaker as "KeySpeaker"
(`KeySpeaker_S0/UsbNames.h`) instead of the core's "TinyUSB UAC1".

The startup screen `KeySpeaker_S0/Splash.h` is made from `assets/splash.png` (the author's KariPom emblem from
`assets/splash_original.webp`: the grille fills the screen, the emblem at 80 %) with
`python3 tools/make_splash.py assets/splash.png KeySpeaker_S0/Splash.h` (macOS, uses `sips`).

Install the app-only `.ino.bin` through a launcher. Do not use the Arduino IDE's **Upload**: it also writes the
bootloader and the partition table, which can replace your launcher.
