# firmware

Cardputer ADV firmware. Not written yet: the first stage is S0, a USB Audio Class 1 speaker-only probe
(see [`../docs/DESIGN_JA.md`](../docs/DESIGN_JA.md), section 9).

Planned build settings: Arduino `m5stack:esp32` 3.3.10, board M5Cardputer, USB Mode = USB-OTG (TinyUSB),
Flash 8MB, Partition `default_8MB`. Install the app-only `.ino.bin` through a launcher.
