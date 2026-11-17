# MP3 input (S16-10): design and measurements

MP3 files on the microSD card as a fourth audio input. Work in progress on branch `worktree-mp3-stage0-1`, tested on a Cardputer ADV on 2026-11-13. The decoder is **minimp3** (CC0) since S16-2; the first S16-1 builds used Helix (RPSL), see `MP3_LICENSE.md`.

## Behaviour

- Fn+I cycles **USB -> MIC -> AUX (only when the unit answers) -> MP3 -> USB**.
- MP3 needs a card with `.mp3` files (in `/music`, else in `/`) and a power-up in which the **BLE stack has not been started yet**, with or without USB (see "BLE and memory"; since S17-5 this holds with USB too: BLE also starts when the computer sleeps for more than 3 s). Otherwise it shows a red word and stays where it is: `NO SD`, `NO MP3`, `NO MEM`, or `RESTART` (BLE is already up: switch the power off and on).
- The three screens stay. FlightViz / the equalizer follow the MP3's own PCM. There is no MP3-specific screen: the measure screen shows `MP3 n/N m:ss`, the flight screen `MP3 n/N`.
- On the measure and flight screens: space = pause / play (the time display stands still while paused), `,` = previous, `/` = next (on the keyboard screen every key goes to the computer, as always). Tracks play in directory order and loop.
- The computer's volume does not reach MP3: a fixed gain of 0.3 (about -10 dB) is applied, and the boost / bass cut / limiter of the speaker path apply as usual.
- USB lost while MP3 is on: MP3 keeps playing and the BLE keyboard does not start (it would take 72 KB). Leaving MP3 with Fn+I lets BLE start as usual. Only if BLE is already up when USB goes is MP3 stopped.
- **Without a computer** (battery, a power bank or a plain USB charger): after switching on, the BLE keyboard starts in 10 s; every Fn+I pressed meanwhile holds it back another 10 s, so there is time to reach MP3 (USB -> MIC -> [AUX] -> MP3). While MP3 plays BLE stays off.
- The last input is remembered: if it was MP3, the next start **without a computer** (checked 4 s after power-on) begins with MP3. With USB the start is always USB.

## BLE and memory

The BLE stack (NimBLE) takes **71.8 KB** of the internal heap in `BLEDevice::init` alone (83.3 KB free before, 11.6 KB after; then 1.0 KB for the server, 1.0 KB for the services, 0.5 KB for advertising: 9.0 KB free). That is fixed by the SDK build and cannot be returned: `BLEDevice::deinit(false)` and `deinit(true)` both end in a PANIC reset (measured, 2026-11-13). So MP3 and a started BLE stack cannot share the heap.

The way out is not to start BLE while MP3 plays: BLE starts only after USB has been gone for 3 s (10 s before USB was ever seen), and not while MP3 is on or for 10 s after an Fn+I press without USB. Once BLE has been started in a power-up, MP3 is refused (`RESTART`), since S17-5 with USB too: it used to try with USB and fail as `NO SD` for lack of memory.

### Free heap with BLE running (S17-6, measured on the device 2026-11-17)

Measured with a diagnostic build of S17-6 (`KS_DIAG 1`, which shows the free heap on the keyboard screen: F now, m lowest since start, B largest free block, and the BLE reports sent and refused). That build has about 2.6 KB more static RAM than the normal one, so the normal build should have a little more room (an estimate). The AUX unit was on the Grove port; no USB cable, a Mac paired.

| Step | F (free now) | m (lowest) | B (largest block) |
|---|---|---|---|
| Before BLE starts | 83.0 KB | 82.7 KB | 43.0 KB |
| BLE paired and connected | 8.0 KB | 7.6 KB | 7.7 KB |
| After 30 min of typing and the air mouse (2,580 reports sent, 0 refused) | 8.0 KB | 7.5 KB | 7.7 KB |
| Then `Fn+I` to the microphone (BLE still on), several minutes | 5.1 KB | 3.2 KB | 3.1 KB |

- No restart, no loss of the BLE link, and the lowest value did not move over the 30 minutes: no sign of a leak. The microphone takes about 3 KB; with BLE on it is the tightest case, and 3.2 KB is a small margin.
- Not measured: the Grove port with nothing on it. There the firmware opens the UART for the AUX search (about 4 KB of buffer) every 3 seconds and gives it back 100 ms later; with the unit found the buffer stays open, which is the case measured here. Judged by reasoning, not by measurement, nothing-on-the-port is not tighter.
- One time that day the Mac showed KeySpeaker as a connected keyboard (the Cardputer's screen showed a green BLE, the Mac listed its keyboard and mouse as HID devices) but no key and no mouse movement arrived. It did not come back after the diagnostic build was flashed again and the link made anew (reports then went out with 0 refused), and the cause is **not known**. Nothing in the BLE code changed since S5. If it comes back, read `k m ok ng rc e` on the diagnostic build's keyboard screen at once.

## Memory (measured on the device, USB input, no BLE)

| Step | Free heap |
|---|---|
| Firmware running, before MP3 | about 83.8 KB (81.4 KB after the first MP3 round trip: a one-time 2.4 KB) |
| `SD.begin` | -30 KB |
| minimp3 decoder state (`mp3dec_t`) | about -6 KB |
| MP3 running (S16-7) | **about 33.7 KB free** (Helix build: 21.7 KB); S16-8 and later: 61.3 KB |
| After 20 track changes in a row | 40.4 KB (about 100 B less than at the start), no restart |

The ring's storage (48 KB, allocated once, the stream buffer made over it with `xStreamBufferCreateStatic`) is split on entry: the first 12 KB are a shorter ring (about 130 ms), the rest holds the decoder's input / PCM buffers, the resampler and the task's stack (`xTaskCreateStaticPinnedToCore`, **20 KB**). On exit the ring goes back to 48 KB. `playTask` is held (`playHold` / `playHeld`) while the ring is swapped.

minimp3 keeps its per-frame scratch on the stack (about 17 KB). S16-2 gave the task 16 KB and the task crashed on the first frame (stack overflow); with 20 KB the least free stack seen is 2.6 KB, so the margin is small and should be watched in longer runs.

## SD card memory (S16-8)

`SD.begin` took 30.1 KB: the SDK's FAT driver uses 4 KB buffers (`FF_MAX_SS` = 4096, `sizeof(FIL)` = 4136, `sizeof(FATFS)` = 4152) and the Arduino SD library reserves five open-file slots by default, 20.7 KB of them. MP3 has one file open at a time, so the card is mounted with **one slot** (`SD.begin(cs, SPI, 25 MHz, "/sd", 1)`, every `File` is closed before the next is opened) and the track names are read into a list sized to the real names (two passes). `SD.begin` now takes 13.7 KB, the list 0.4 KB (it was 6.1 KB fixed), and the heap with MP3 running is **61.3 KB free** (S16-7: 38.6 KB). `under` stayed at 0 over a round of track changes.

## Track changes (S16-5 to S16-7)

- A track change used to walk the whole SD folder twice (count, then name): later tracks took longer than the 130 ms ring holds and the speaker path ran dry (`under` grew by one per change, worst for tracks 8-10 of 10). The names are now read **once on entry** (6 KB on the heap, up to 96 tracks, freed on exit); a change copies the name and opens the file.
- A track change used to step the waveform (an audible click). The last block of the old track is played faded out and the first block of the new one faded in (about 25 ms each, `mp3Process(..., fade)`).
- Measured on the device (10 tracks, one round of `/`): `under` stays at its entry value 2, no restart; the click is gone by ear. The 2 at entry is not explained yet (SD start or the first track's preparation).

## CPU, SD, stack (device, MP3 task on core 0)

- CPU of the minimp3 build is **not measured yet** (the Helix build: 5.3 ms per 26.1 ms frame, 20 % of one core; minimp3 uses floats on the S3's FPU).
- SD: 235 reads of 4 KB in 60 s, slowest 4.3 ms.
- MP3 task stack (minimp3): 2.6 KB of 20 KB never used (least value seen).
- USB audio after leaving MP3: buffer 59-75 ms, 0 underruns / overruns / drops.

## Resampling

RateAdjust.h's Hermite is made for +-200 ppm. For the fixed 44.1 -> 48 kHz step, `Sinc48.h` is used: 16-tap Kaiser sinc, 64 phases, linear interpolation between phases; the table is built once (about 250 ms) on the first track, later tracks only set the rate. The decoder side converts, so the ring carries 48 kHz mono and playTask / RateAdjust are unchanged.

## Not done yet

- 320 kbps and mono files (tested and fine on the device with Helix, 2026-11-13: 44.1 kHz stereo 64 / 128 kbps, 48 kHz stereo 128 kbps, a short VBR file; all 7 tracks in a loop, track changes with no break, the flight screen follows the music).
- Tested with minimp3 on the device (2026-11-13): 7 tracks in a loop, 20 `/` presses in a row, pause / play, return to USB audio (the Mac must be playing again). Also tried: a 256 kbps 48 kHz stereo file (7.5 MB) and a 320 kbps stereo and a mono file played through, no breaks heard. Not yet: CPU measurement.
- Long run (hours), the flight view's worst case with MP3, USB suspend / resume during MP3, switching input about 1000 times.
- Battery life with MP3: on battery alone it played 10 tracks on repeat for 10 h (07:26 to 17:26, 2026-11-14, S16-10, screen on) with underrun 0, overrun 0, buffer 94-126 ms, `boot 1`, and was still playing when stopped. The full run time until the battery is empty is not measured.
- MP3 after the BLE keyboard has started needs a power cycle (see "BLE and memory").
- License: settled by replacing Helix with minimp3 (CC0), see `MP3_LICENSE.md`. The sound and the speed of minimp3 against Helix are checked by ear only.
- Reading the serial port resets nothing on KeySpeaker, but it does on `mp3_spike`.
- S17-5 (from the code review before release; S17-6 tried on the device 2026-11-17 by the author with no problem, also with the card pulled out for 10 s while MP3 played): a track that gives no sound (the card pulled out, a file that cannot be played) is followed by a short rest, data that is not MP3 is passed over with a rest every 16 KB, and a file below 16 kHz ends at once. Before, the MP3 task never slept in these cases and the task watchdog (5 s, core 0's idle task) would restart the Cardputer.
- The card must be FAT32: this core's FAT driver is built without exFAT, so an exFAT card (the usual format from 64 GB up) gives `NO SD` (read from the SDK configuration, 2026-11-17; not tried with such a card).
