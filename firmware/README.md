# firmware

Cardputer ADV firmware. Each stage is its own sketch folder with its app-only `.ino.bin` and `SHA256SUMS.txt`.

| Folder | Stage | Status |
| --- | --- | --- |
| [`KeySpeaker_S0/`](KeySpeaker_S0/) | S0: USB Audio Class 1 speaker only; measures the buffer and the clock drift. S0-3 adds a flight visualizer screen (`v`). S1 (now `S1-6`) corrects the clock drift and fixes two USB restarts | On a Mac and Windows 11: 8 hours with no dropouts (without the S1 correction, one every 1.4 hours); ten pause/resume cycles without a restart. Now `S2.5-10`: USB keyboard, media keys and a tilt mouse too, a spinning Earth on the keyboard screen, and Media Jog's jog dial if it is plugged in. Test plan: [`../docs/S0_TEST.md`](../docs/S0_TEST.md) |

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
(`KeySpeaker_S0/UsbNames.h`) instead of the core's "TinyUSB UAC1", and `-Wl,--wrap=tinyusb_enable_interface`, which
puts an Interface Association Descriptor in front of the speaker (`KeySpeaker_S0/UsbAudioDesc.h`). Without it Windows
does not start the speaker (Code 10); the keyboard still works. The same file makes the output a speaker rather than
headphones, so Windows lists "Speakers (KeySpeaker)".

The emblem screen `KeySpeaker_S0/Splash.h` is made from the layers of `assets/splash.png` (the author's KariPom emblem
from `assets/splash_original.webp`: the grille fills the screen, the emblem at 80 %): `assets/splash_grille.png`, and
`assets/splash_emblem.png` with its alpha `assets/splash_emblem_alpha.png`, kept apart so the emblem can shake on loud
beats. Run
`python3 tools/make_splash.py assets/splash_grille.png assets/splash_emblem.png assets/splash_emblem_alpha.png KeySpeaker_S0/Splash.h`
(macOS, uses `sips`).

Install the app-only `.ino.bin` through a launcher. Do not use the Arduino IDE's **Upload**: it also writes the
bootloader and the partition table, which can replace your launcher.
