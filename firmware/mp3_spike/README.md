# mp3_spike (MP3 feasibility, stage 1)

A separate sketch, not part of KeySpeaker. It plays nothing; it measures heap behaviour, decode CPU and SD latency for an
MP3 input on a Cardputer ADV (see the header of `mp3_spike.ino`).

Build (same board settings as `firmware/build.sh`; the output stays outside the repository):

```bash
arduino-cli compile --fqbn m5stack:esp32:m5stack_cardputer:PartitionScheme=default_8MB,FlashSize=8M,USBMode=default,CDCOnBoot=cdc \
  --output-dir /tmp/mp3_spike_out firmware/mp3_spike
```

Run: put a few `.mp3` files in `/` or `/music` on the microSD card (128 and 320 kbps, 44.1 and 48 kHz, one with a large
ID3 tag and one VBR are the useful ones), flash, open the serial monitor at 115200 and read the `[SPIKE]` lines. It runs
once at start; `g` runs it again, `h` runs only the heap test.

`Sinc48.h` is the 16-tap windowed-sinc resampler to 48 kHz the spike uses, written to be tested on a computer.
