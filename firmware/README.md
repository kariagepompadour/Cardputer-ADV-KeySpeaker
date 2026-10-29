# firmware

Cardputer ADV firmware. Each stage is its own sketch folder with its app-only `.ino.bin` and `SHA256SUMS.txt`.

| Folder | Stage | Status |
| --- | --- | --- |
| [`KeySpeaker_S0/`](KeySpeaker_S0/) | S0: USB Audio Class 1 speaker only; measures the buffer and the clock drift. S0-3 adds a flight visualizer screen (`v`) | Plays on a Mac at a good level; about an hour without audible dropouts. The visualizer runs on the device; the final look and its effect on the audio are still to check. Test plan: [`../docs/S0_TEST_JA.md`](../docs/S0_TEST_JA.md) |

## Building

```bash
./build.sh KeySpeaker_S0
```

Needs `arduino-cli`, the `m5stack:esp32` core 3.3.10 and the libraries M5Cardputer, M5Unified and M5GFX. Settings
used: board M5Cardputer, USB Mode = USB-OTG (TinyUSB), USB CDC On Boot = Enabled, Flash 8MB, Partition
`default_8MB`.

Install the app-only `.ino.bin` through a launcher. Do not use the Arduino IDE's **Upload**: it also writes the
bootloader and the partition table, which can replace your launcher.
