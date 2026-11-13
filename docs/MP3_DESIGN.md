# MP3 input (S16-4): design and measurements

MP3 files on the microSD card as a fourth audio input. Work in progress on branch `worktree-mp3-stage0-1`, tested on a Cardputer ADV on 2026-11-13. The decoder is **minimp3** (CC0) since S16-2; the first S16-1 builds used Helix (RPSL), see `MP3_LICENSE.md`.

## Behaviour

- Fn+I cycles **USB -> MIC -> AUX (only when the unit answers) -> MP3 -> USB**.
- MP3 needs **USB** and a card with `.mp3` files (in `/music`, else in `/`). Without them it shows a red word and stays on USB: `NO USB`, `NO SD`, `NO MP3`, `NO MEM`.
- The three screens stay. FlightViz / the equalizer follow the MP3's own PCM. There is no MP3-specific screen: the measure screen shows `MP3 n/N m:ss`, the flight screen `MP3 n/N`.
- On the measure and flight screens: space = pause / play (the time display stands still while paused), `,` = previous, `/` = next (on the keyboard screen every key goes to the computer, as always). Tracks play in directory order and loop.
- The computer's volume does not reach MP3: a fixed gain of 0.3 (about -10 dB) is applied, and the boost / bass cut / limiter of the speaker path apply as usual.
- USB lost while MP3 is on: MP3 stops at once and gives its memory back, so the BLE keyboard can start 3 s later as before.

## Why only with USB (BLE and memory)

With the BLE stack up, the internal heap has about 9 KB free (measured: free 8.5-9.2 KB, largest block 4.6-7.7 KB); without it about 84 KB. `BLEDevice::deinit(false)` and `deinit(true)` both end in a PANIC reset on this SDK build (measured, 2026-11-13), so BLE cannot be released to make room. MP3 therefore lives on the USB side of the firmware's two modes.

## Memory (measured on the device, USB input, no BLE)

| Step | Free heap |
|---|---|
| Firmware running, before MP3 | about 83.8 KB (81.4 KB after the first MP3 round trip: a one-time 2.4 KB) |
| `SD.begin` | -30 KB |
| minimp3 decoder state (`mp3dec_t`) | about -6 KB |
| MP3 running (S16-4) | **about 40.4 KB free** (Helix build: 21.7 KB) |
| After 20 track changes in a row | 40.4 KB (about 100 B less than at the start), no restart |

The ring's storage (48 KB, allocated once, the stream buffer made over it with `xStreamBufferCreateStatic`) is split on entry: the first 12 KB are a shorter ring (about 130 ms), the rest holds the decoder's input / PCM buffers, the resampler and the task's stack (`xTaskCreateStaticPinnedToCore`, **20 KB**). On exit the ring goes back to 48 KB. `playTask` is held (`playHold` / `playHeld`) while the ring is swapped.

minimp3 keeps its per-frame scratch on the stack (about 17 KB). S16-2 gave the task 16 KB and the task crashed on the first frame (stack overflow); with 20 KB the least free stack seen is 2.6 KB, so the margin is small and should be watched in longer runs.

## CPU, SD, stack (device, MP3 task on core 0)

- CPU of the minimp3 build is **not measured yet** (the Helix build: 5.3 ms per 26.1 ms frame, 20 % of one core; minimp3 uses floats on the S3's FPU).
- SD: 235 reads of 4 KB in 60 s, slowest 4.3 ms.
- MP3 task stack (minimp3): 2.6 KB of 20 KB never used (least value seen).
- USB audio after leaving MP3: buffer 59-75 ms, 0 underruns / overruns / drops.

## Resampling

RateAdjust.h's Hermite is made for +-200 ppm. For the fixed 44.1 -> 48 kHz step, `Sinc48.h` is used: 16-tap Kaiser sinc, 64 phases, linear interpolation between phases; the table is built once (about 250 ms) on the first track, later tracks only set the rate. The decoder side converts, so the ring carries 48 kHz mono and playTask / RateAdjust are unchanged.

## Not done yet

- 320 kbps and mono files (tested and fine on the device with Helix, 2026-11-13: 44.1 kHz stereo 64 / 128 kbps, 48 kHz stereo 128 kbps, a short VBR file; all 7 tracks in a loop, track changes with no break, the flight screen follows the music).
- Tested with minimp3 on the device (2026-11-13): 7 tracks in a loop, 20 `/` presses in a row, pause / play, return to USB audio (the Mac must be playing again). Not yet: CPU measurement, 320 kbps / mono files.
- Long run (hours), the flight view's worst case with MP3, USB suspend / resume during MP3, switching input about 1000 times.
- Battery operation (BLE only) has no MP3: that needs about 50 KB more free heap than exists (candidates: 8-bit canvas, a shorter ring, smaller task stacks).
- License: settled by replacing Helix with minimp3 (CC0), see `MP3_LICENSE.md`. The sound and the speed of minimp3 against Helix are checked by ear only.
- Reading the serial port resets nothing on KeySpeaker, but it does on `mp3_spike`.
