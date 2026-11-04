# S0 test plan: the USB-speaker-only prototype

[日本語版](S0_TEST_JA.md)

Target: `firmware/KeySpeaker_S0/KeySpeaker_S0.ino.bin` (`S5-13`). This is S0-6 plus the clock drift correction (S1-1),
a record of why it restarted by itself (S1-2, S1-3) and the fix for that (S1-4 to S1-6), the startup screen (S1-7 to
S1-11), the USB keyboard and the three modes (S2-1 to S2-5), the help screen and the emblem screen after being left
alone (S2-6), the silver emblem that shakes with loud sounds (S2-7, S2-8), and the fix that makes it work as a
speaker on Windows (S2-9), with the name "Speakers" (S2-10), the keyboard screen's spinning Earth (S2-11), a surer
`Fn+M` (S2-12), the air mouse (S2.5-1 to S2.5-8), the jog dial (S2.5-10), the BLE keyboard and mouse (S5-1 to S5-12), and the start screen's round-hole grille and name (S5-13). `S5-13` is shown at the top right of the data screen. How to check S1 is
in 5.8, S2 in 5.10, and the air mouse in 5.11. The results up to S0-6 are kept in this document as they were.

Status: volume checked on the device (Mac, 2026-10-29). It was louder and easier to hear than S0-1; a boost of +9 to
+12 dB was good, and the bass cut was hard to hear either way. The defaults stay at +9 dB with the bass cut on. The
clock drift, with S0-3's buffer-trend method, was `drift +28 ppm` (`played` 6:38, underrun 0, overrun 0, Mac,
2026-10-30). The measurement was short, so the error is about ±15 ppm. In 6 hours of continuous playback (S0-6, Mac,
2026-10-30), 6.02 hours of sound had 3 dropouts and 0 skips. The dropouts came regularly, every 1.39 hours, each when
the buffer slowly ran dry, because the computer runs about 10 to 17 ppm slower. The longest frame took 17.2 ms to
draw, and free memory stayed level. The other items are not yet checked.

## 1. What this prototype tells us

- Whether a Mac and Windows recognize the Cardputer ADV as a USB speaker
- Whether the sound plays properly, and whether the computer's volume and mute work
- Over long playback, how the buffer grows or shrinks with the clock difference between the computer and the
  Cardputer (design doc 4.2)
- How the earphone jack, plugging and unplugging, and the power switch behave

S0 deliberately **did not correct** the clock drift. The point was to measure it. From S1-1 there is a correction, and
long playback should have no dropouts (underrun) or skips (overrun).

The keyboard does not send characters to the computer yet. Bluetooth and Wi-Fi are not used either.

## 2. Installing

1. Check `KeySpeaker_S0.ino.bin` against `SHA256SUMS.txt`.
2. Install it as usual through the Launcher's Wi-Fi upload, and start it from the Launcher. (Or burn it from
   M5Burner, which replaces the Launcher; checked with S5-13.)
3. First the startup screen (a speaker grille with KariPom's emblem) shows for 2 seconds. A key press moves on at
   once. USB and audio get ready in the meantime.
4. The screen shows "KeySpeaker S0".

## 3. Reading the screen

| Line | Meaning |
|---|---|
| `USB STREAMING` / `MOUNTED (no stream)` / `NOT CONNECTED` | Sound is arriving / connected, but no sound is coming / not connected |
| `SOF+` / `SOF-` | Whether the computer's signal (every millisecond) is arriving |
| Green bar, `buffer ... ms` | How much sound is buffered (0 to 500 ms). The vertical line marks where playback starts (100 ms) |
| `underrun` / `overrun` | How many dropouts (the buffer ran dry) / skips (the buffer overflowed) |
| `adj ... ppm  trend ...  (m:ss)` | From S1-1. `adj` is the current correction (+ means the computer is faster, so it plays slightly faster). After playing a while, it settles at the clock difference between the computer and the Cardputer. `trend` is the buffer change left over that the correction did not take up; it is near 0 when the correction works. In the brackets is the total time sound has played; `trend` shows after 1 minute (up to S0-6 this was `drift ... ppm (m:ss played)`, the uncorrected drift itself) |
| `since h:mm:ss` | Time since this playback started |
| `PC vol ... dB` | The computer's volume (0 dB is the maximum, −50 dB the minimum) and mute |
| The two bars at the bottom | Left: how loud the sound from the computer is. Right: how loud the sound sent to the speaker is (yellow when the limiter acts) |
| `+9dB cut` | The boost and the bass cut (`cut` = on, `full` = off) |

Keys on the Cardputer:

| Key | Action |
|---|---|
| `-` / `=` | Less / more boost (0 to +18 dB in 3 dB steps; +9 dB at first) |
| `b` | Bass cut on or off (for comparing by ear) |
| `r` | Reset the counters |
| `v` | Switch between the data screen and the flight visualizer (the data screen at first). On the emblem screen, goes back to the data screen |
| Any other key (letters, `Enter`, `Space` and so on) | Shows the emblem screen (the same as the startup screen) for 2 seconds. Pressing again restarts the 2 seconds. `-` `=` `b` `r` do their job and close the screen (S1-8). From S2-6, opens or closes the help screen (5.10) |

### S0-2 changed how the volume works

In S0-1, the signal was already near its maximum at the Cardputer's output level of 192, so turning it up only made
it distort. S0-2 adds the processing that small speaker products commonly use:

1. **Bass cut (below 150 Hz):** removes bass that a 1 W speaker cannot play, and puts that headroom into volume.
2. **Boost:** raises everything by +9 dB (about 2.8 times).
3. **Limiter:** instantly holds down only the peaks of loud sounds, so they do not distort.

In a test on the computer, the output never went over its limit, quiet sounds got 9 dB louder, and 50 Hz bass went
down by about 19 dB.

## 4. Logging (helpful if you can)

Every second, a line like this goes out on the USB serial:

```
[S0] t=125 stream=1 sof=1 fill=103 ms min=98 max=109 under=0 over=0 drop=0 rx_ppm=+41.2 spk_ppm=-2.0 vol=-12 mute=0 boost=9 cut=1 lim=-3.2 dB heap=...
```

**Mac:** in Terminal, first find the port name.

```bash
ls /dev/cu.usbmodem*
```

Then start logging with the name you found (replace `XXXX`). Stop with Ctrl+C.

```bash
cat /dev/cu.usbmodemXXXX | tee ~/Desktop/keyspeaker_s0_mac.log
```

**Windows:** the Arduino IDE's Serial Monitor (115200 bps) shows it. Copy the parts you need.

If you cannot log, a photo of the screen is enough.

## 5. Test items

### 5.1 Recognition and playback (on a Mac and on Windows)

1. Plug it in with a USB-C data cable.
2. Choose the Cardputer as the output device. It should show with a placeholder name such as "M5STACK_CARDPUTER" or
   "TinyUSB UAC1".
   - Mac: System Settings → Sound → Output
   - Windows: Settings → System → Sound → Output
3. Play YouTube, a podcast, music or the like, and check that sound comes out with no noise or distortion.
4. Check that the screen shows `USB STREAMING`.

### 5.2 Volume and mute

1. Turn the computer's volume up and down, and check that `PC vol` on the screen changes and the actual volume does
   too.
2. Mute on the computer, and check that the sound stops and `MUTE` shows.
3. With the computer's volume at the maximum, change the boost with `-` / `=` on the Cardputer and find **the value
   with the best balance between clarity and natural sound**. Too much makes the sound seem squashed, or makes it
   swell and dip as if breathing.
4. Switch the bass cut with `b` and compare which is easier to hear.

### 5.3 One hour of continuous playback (the most important item)

1. Play something that runs without breaks, such as radio or a playlist.
2. Once playback starts, reset the counters with `r`.
3. Let it play for an hour. Log as in chapter 4 if you can.
4. At the end, note `underrun`, `overrun`, `drift` and `buffer` on the screen. `drift` is reliable enough once
   `played` is over 5 minutes. It helps to know roughly when the first dropout or skip happened, too.

Short breaks, such as between songs, are fine. `drift` is computed only over the stretches with sound.

Expected: if drift is +, the buffer slowly grows until an `overrun`. If −, it shrinks until an `underrun`. At about
±100 ppm, it can happen within a quarter of an hour or so.

### 5.4 Plugging and unplugging

1. Unplug the cable during playback, and plug it back in a few seconds later.
2. Check that the computer recognizes the Cardputer again, and that choosing it as the output brings the sound back
   (please also note whether the OS switches back by itself).

### 5.5 The earphone jack (checking a new idea)

> **To protect your ears, first turn the computer's volume down, and the Cardputer's output down with `-`,** before
> putting the earphones in.

1. During playback, plug earphones into the Cardputer's earphone jack.
2. Check that the built-in speaker stops and the earphones play.
3. Note the volume, any noise, and whether both sides play the same sound.

### 5.6 Power switch and charging

1. Use it with the power switch ON, and check that the screen keeps running.
2. If you have time, also note what happens when the power switch is turned OFF while connected over USB (it stops,
   keeps running, and so on).

### 5.7 The flight visualizer (added in S0-3; enemies in S0-4, graphic EQ in S0-6)

Switch to it with `v`. The volume history becomes terrain made of block bars, like an LED level meter (a gradient from
blue at the bottom through green, yellow and orange to red), scrolling to the left, with a small fighter flying above
it.

- Now and then the fighter fires a laser beam with a band of color running forward. Bombs fall in an arc and explode
  when they hit the terrain, digging out three blocks. The fighter, the enemies, the bombs and the explosions are drawn
  in large dots (3 × 3 px), like the terrain.
- On the song's beats (moments that suddenly get louder), small targets appear. The pink eyeball walker appears toward
  the right of the screen, as dots gather from around it; the others fly in from the right edge (even in songs with few
  clear beats, one comes every few seconds; from S5-15, in silence too, every 6 seconds).
- The targets are six kinds of original pixel art: the pink eyeball walker, a fast saucer, a red mutant that zigzags,
  a blue bomber that leaves blinking mines, a spiky pod that splits into three small ones when shot, and those small
  ones.
- The fighter moves forward, matches the height and fires. A hit throws colorful sparks to the edge of the screen. The
  targets sometimes fire orange shots back at the fighter. The fighter usually dodges up or down; when hit, its shield
  (a ring of light-blue dots) only flashes for a moment, and it is never destroyed. There is no score.
- Quiet songs bring no targets (up to S5-14).
- The small text at the top shows the connection, the boost, and the dropout (`U`) and skip (`O`) counts.

1. Play music, press `v`, and check that the terrain moves with the loudness and that the fighter flies smoothly.
   - The bars follow the sound before the computer's volume. Their height is the loudness compared with "this song's
     average over the last few seconds", so loud parts become peaks and quiet parts become valleys. Changing the
     computer's volume does not change the motion. Muting on the computer leaves only the ground. The sky is pitch
     black, with mostly white stars, plus yellow and pink ones, drifting slowly.
   - Every minute, or when music comes back after a silence between songs (1.5 seconds or more), the terrain moves to
     the next of these three patterns, in turn: (1) the scrolling volume terrain alone, (2) a **light-blue graphic
     EQ** (the color of an old stereo's fluorescent display) dancing in front of the terrain, (3) a **KariPom-colored
     graphic EQ** (as in KariPom's EQ Classic, one color per bar from bass to treble: blue, light blue, green,
     yellow-green, yellow, orange, red, magenta) dancing in front of the terrain.
   - The graphic EQ rises from the ground and sinks back into it at the next switch. The terrain keeps scrolling behind
     it meanwhile. The graphic EQ is 30 bars, bass on the left and treble on the right, up to about half the terrain's
     height, with white peak dots that fall slowly.
2. Check that **the sound does not break up or click**. During the graphic EQ in particular, it computes frequencies
   (FFT), so watch `draw_max` in the log and the dropout (`U`) and skip (`O`) counts. Heavy drawing could show up here.
3. Check that the Cardputer's keys (`-` `=` `b`) work on this screen too.
4. If you have time, play for an hour on this screen and compare with the result on the data screen (`draw_max` in
   the log is the longest time one frame took to draw).

### 5.8 Clock drift correction (added in S1-1)

S1-1 plays very slightly slower when the buffer gets low and very slightly faster when it gets high, to keep the
buffer at its level just after playback starts (about 80 ms). The change is at most ±200 ppm (0.35 cents in pitch),
which the ear cannot tell. With a correction of 0, the sound is exactly the same as S0-6.

In a simulation on the Mac, with the same −17 ppm drift as the 6-hour test, 6 hours of playback had 0 dropouts and 0
skips, and the buffer stayed level at 65 to 69 ms. Drifts up to ±150 ppm, and a drift that changes partway, gave the
same result (a drift over ±200 ppm cannot be fully corrected).

**Device result (S1-1, Mac, 2026-10-30):** 3.01 hours of sound with 0 dropouts and 0 skips (without the correction,
S0-6 dropped out every 1.39 hours). The buffer was at least 59 ms and averaged 70 to 73 ms, without falling over time.
The correction `adj` averaged −16.3, −18.3 and −15.8 ppm over each hour. The leftover change `trend` stayed within
±1 ppm. On the way, the Cardputer restarted by itself twice when stopped sound started playing again, so the longest
unbroken stretch of sound was 1 hour 39 minutes (the restarts are in 5.8.1).

1. Play music as usual, and reset the counters with `r` on the data screen.
2. After a few minutes, `adj` should settle at about −10 to −20 ppm (with the same computer as the 6-hour test).
3. Let it play for 3 hours or more (S0-6 dropped out every 1.4 hours, so 3 hours shows that it no longer happens twice
   or more). With the logging in chapter 4, `adj_ppm` in the log also shows how the correction changes.
4. Note whether `underrun 0` and `overrun 0` stay, whether `buffer` settles around 60 to 80 ms, and whether the sound
   has any wobble or noise.

### 5.8.1 Restarting by itself (S1-2 to S1-6)

In S1-1's 3-hour test (Mac, 2026-10-30), the Cardputer restarted by itself twice when stopped sound started playing
again (14:47 and 15:05; S0-6's log had the same pattern several times). The very first start right after installing
S1-2 also restarted with `PANIC` in under a second (2026-10-30 16:15). S1-2 and S1-3 show, after a restart, why it
restarted (`PANIC` for a program fault, `TASK_WDT` or `INT_WDT` for processing that got stuck, and so on) and how far
each task had got just before, in the log and on the screen. S1-3 also reads the record that the Cardputer saves by
itself when it crashes (a core dump), and puts the name of the crashed task and the place in the program on the
`[CRASH]` line of the log (not on the screen; please log as in chapter 4).

**Cause (from S1-3's record):** the crash was in the core's task that handles USB events (sound starting and stopping,
volume and so on). That task has only 2 KB of memory (stack), but the app printed log text there, which overflowed it
when an event came. This problem was there from S0. S1-4 fixed it: that task only notes the events, and the log is
printed elsewhere.

**S1-4 result (Mac, 2026-10-30 20:22 to 20:30):** no more crashes right after starting. But pausing on the Mac and
resuming 10 seconds later restarted it with `TASK_WDT` (processing stuck) all 10 times. According to the record,
TinyUSB, the USB core part, got the signal to resume and then got stuck waiting for the audio endpoint to finish
closing. The same part's interrupt handler can clear the "finished closing" notice first, and then the wait never
ends.

**S1-5:** a step that closes the endpoint with a time limit (`UsbIsoFix.h`) is put in front of that function. It checks
that the closing has finished with a separate marker that the interrupt handler does not clear. It only works in a
build with `firmware/build.sh`.

**S1-5 result (Mac, 2026-10-30 20:41 to 20:46):** it restarted all 8 times. The new step was called, but the endpoint
did not close within 3 ms, and the original function still got stuck after that. The endpoint seems to be in a state
where it cannot be closed at all.

**S1-6:** on resume, if the endpoint is still open with the same settings (type, size) as before, it is kept as it is
instead of being closed and reopened. All the original function did was close it and write the same settings back.
**S1-6 result (Mac, 2026-10-30 20:49 to 20:54):** ten pause and resume cycles, with 0 restarts (`boot 1` stayed). In
the 5 times the computer stopped sending sound and then resumed (on a short pause, the Mac sometimes keeps sending),
the endpoint was always kept (`kept5`), sound kept arriving after resuming, and there were 0 dropouts and 0 skips. At
the end of the log line, `iso=kept/passed` shows how many times the endpoint was kept (kept) and how many times it was
left to the original function (passed).

1. The `since` line on the data screen shows how many times it has started and the reason for the last start, such as
   `boot 1 POWERON`.
2. Play music, pause playback on the computer, wait about 10 seconds, and resume. Repeat a few times.
3. If the `boot` number goes up, it restarted. Take a photo of the `boot` line at that time (for example
   `boot 2 PANIC`). If you are logging, `rst=` and `prev=` give a detailed record.

### 5.9 If you have time

- Plug it into an iPhone or iPad (USB-C) or an Android device, and check whether sound comes out.
- In the Mac's "Audio MIDI Setup", check whether this device's format is "48,000 Hz".

### 5.10 The USB keyboard and the three modes (added in S2-1)

**Device result (S2-1, Mac, 2026-10-30):** recognized as a USB speaker and a keyboard at the same time. On the data
screen, `[` `]` `\` turned the Mac's volume up and down and muted it. In keyboard mode, typing into the Mac worked. The
arrows `Fn+;` `Fn+,` `Fn+.` `Fn+/`, Eisu and Kana with `Fn+[` `Fn+]`, copy and paste with `alt+C` / `alt+V`, and Esc
with `` Fn+` `` (it closed Spotlight opened with `alt+Space`) worked too. Remembering the mode, and pausing and
resuming, are not yet checked.

**S2-2 to S2-4 (Mac, 2026-10-30):** checked that the keyboard screen shows the cheat sheet of keys (orange) and
meanings (white), and `A/あ` to the right of `USB` `Mac` (the one last switched with `Fn+[` / `Fn+]` in orange). Fixed
a hint showing at startup because of a key pressed in the Launcher. With S2-5, the name of the Mac's sound output
changed from "TinyUSB UAC1" to "KeySpeaker" (the output stays chosen, without choosing it again).

**S2-6 (Mac, 2026-10-31):** used the help screen, opened with `h` and keys with no job, for a short while at first, and
found it clearer. Going back with a key with no job while the help is shown was added on an idea from that first use.
Still to check: using it on the go, and the emblem screen coming up after 1 minute left alone.

**S2-7, S2-8 (Mac, 2026-10-31):** checked that after the emblem screen comes up from being left alone, playing music
makes the emblem shake with loud sounds (the user's impression: "subtle, and just about right"). With the added
yellow removed, a little gold still remained, so S2-8 turned the emblem's colors to grays, and it was checked to look
silver.

**Windows 11 (work PC, 2026-10-31):** up to S2-8, it worked as a keyboard and typing worked, but the speaker did not
appear in the list of outputs. Device Manager showed "KeySpeaker" with "This device cannot start. (Code 10)". The
serial had an IAD (a descriptor that groups two interfaces into one function), but the speaker had none, so Windows
was taken to have handled the speaker's two interfaces separately. S2-9 put an IAD in front of the speaker. After
installing it, it started playing at once without replugging, "Headphones (KeySpeaker)" appeared among the outputs,
and Code 10 was gone (Device Manager still shows a second, faded "KeySpeaker", left from the registration up to S2-8;
it does no harm).

On the same PC, S2-9: 3 hours 15 minutes of playback (`since 3:14:51`) with underrun 0 and overrun 0, the buffer between
67 and 81 ms, `adj -10 ppm`, and no restart (`boot 1 SW`). Ten pauses and resumes with no restart and no dropout;
Windows takes 13 seconds or more after a pause before `USB STREAMING` goes off. After unplugging (with the power
switched off) and plugging in again, Windows went back to KeySpeaker by itself and it played. In keyboard mode with
`Fn+O` set to Windows, typing worked and the A/あ key switched Japanese input on and off. In the data mode, `[` and `]`
changed the Windows volume.

S2-10 changes the output terminal type from headphones (0x0302) to speaker (0x0301), to get the name "Speakers". The
first try showed "KeySpeaker" in Device Manager with no error, but no output in the list of outputs, not even with
disabled and disconnected devices shown in `mmsys.cpl`, and the sound came from the PC. After uninstalling KeySpeaker
in Device Manager (under "Sound, video and game controllers") and plugging it in again, "Speakers (KeySpeaker)"
appeared at volume 100 %, `USB STREAMING` showed and it played; it did so again after another replug. So Windows had
kept the registration from S2-9, and a PC that has seen an earlier version needs this uninstall once. A PC that has
never seen KeySpeaker should not; that is not yet checked. Lower the Windows volume after this: it starts at 100 %.
The long playback and the pauses above were on S2-9; S2-10 differs only in those descriptor bytes.

**S2-10 (Mac, 2026-10-31):** macOS lists it as "KeySpeaker" (type USB), as before. It played; typing worked from it
(with `Fn+O` set back to Mac), and the A/あ key switched input; `[`, `]` and `\` in the data mode changed and muted the
Mac's volume. Ten pauses and resumes in one boot with no restart (`on=13 off=13`). The screen showed `boot 1 PANIC`:
the start before this one was a crash, but its crash record matches neither this build nor S1-2 to S1-6, and
KeySpeaker's own notes from before the start were empty, so the crash was in another program, most likely the
launcher (several installs had been cut short by early resets). The Cardputer was then erased and the launcher and
S2-10 installed again; that start showed `boot 1 SW`, with no PANIC.

**S2-10, 8 hours (Windows 11, home PC, 2026-10-31 19:23 to 2026-11-01):** the PC listed "Speakers (KeySpeaker)" as the
default device. After `since 8:47:25`: underrun 0, overrun 0, the buffer between 66 and 80 ms, `adj -17 ppm`, trend 0,
and no restart (`boot 1 SW`), at boost +9 dB and PC volume -11 dB. This meets S1's pass criterion (8 hours of
continuous playback with no dropouts).

**iPhone 15 (USB-C, 2026-10-31):** with S2-10 on a USB cable, the iPhone took KeySpeaker as an external keyboard and a
speaker: typing worked with `Fn+O` set to either Mac or Windows, Japanese input worked, the sound played from
KeySpeaker, and `[` and `]` in the data mode changed the iPhone's volume. Without the cable it is not seen at all, as
expected: Bluetooth (S5) is not built yet. The iOS version was not noted.

**S2-11 (checked on the device, 2026-11-01):** the keyboard screen of design 8.3, judged good on the device. Checked: the block Earth at the top
left turns slowly, the land moving from left to right (one turn a minute); "KEY BOARD" reads well over the dimmed
middle; a few stars around the Earth, some white ones blinking now and then; `USB Mac A/あ` under the Earth (the last
input switched in orange), and below it `Fn+O Mac/Win` and `Fn+[ ] A/あ`; `Fn+M mode` at the top right, and the cheat
sheet under it, lines 9 px apart and still easy to read, with `ctrl+C/V` clear of `copy/paste` under `Fn+O` set to
Windows. ⌘ and ⌥ beside Command and Option on a Mac, four panes beside Win on Windows. This screen no longer shows
the version tag. Noticed: right after start, `Fn+M` now and then does not seem to respond the first time. Typing,
sound and the other modes are unchanged from S2-10.

**S2-12 (checked on the device, 2026-11-01):** two likely causes were found in the code. (1) A mode change needed `M` to
go down while `Fn` was already down; when `M` was read a moment before `Fn`, it counted as a plain `M` (on the data
screen, a key with no job, which opens the help), and `Fn+M` was lost. Now a key already held when `Fn` goes down
counts with `Fn`. In keyboard mode that `M` has already gone to the computer as a letter. (2) Until the computer has
taken the USB device (about a second after start), `Fn+M` does nothing, by design; this is now logged as
`[KEY] Fn+M before USB is up`, and case (1) as `[KEY] Fn+M: Fn came after M`. To check: `Fn+M` right after start, a
few times, with the serial log. S2-12 also dims the Earth only around each line of "KEY BOARD" (the corners beside
"KEY" stay bright). On the device, `Fn+M` responded well this time, and the Earth shows more around the letters,
which looks better (the user's impressions). The serial log was not taken, so which cause it was is not known.

From S2-1, the Cardputer is recognized as a USB keyboard and media keys (volume, playback) as well as a USB speaker.
Each `Fn+M` moves to the next mode: **data screen → game screen (visualizer) → keyboard**. Keys go to the computer as
characters only in keyboard mode. It remembers the last mode and starts in it next time.

**Keys on the data screen and the game screen**

| Key | Action |
|---|---|
| `[` / `]` / `\` | Computer volume down / up / mute |
| `Space` | Play / pause |
| `,` / `/` | Previous / next track |
| `-` / `=`, `b`, `r` | Boost, bass cut, reset the counters (as before) |
| `v` | Switch between the data screen and the game screen |
| `h` | Open or close the help screen (S2-6) |
| Any other key | Like `h`, opens or closes the help screen (S2-6; up to S2-5, the emblem screen for 2 seconds) |
| `Fn+M` | Switch modes |

**Keys in keyboard mode** (all sent to the computer; no volume control)

| Key | Action |
|---|---|
| Characters, Shift, Ctrl, Tab, Enter, Del | As they are (Del is Backspace) |
| `opt` / `alt` | Option / Command on a Mac, Win / Alt on Windows |
| `Fn+;` `Fn+,` `Fn+.` `Fn+/` | ↑ ← ↓ → |
| `Fn+Shift+` the four above | PgUp / Home / PgDn / End |
| `` Fn+` `` | Esc |
| `Fn+1` … `Fn+0` `Fn+-` `Fn+=` | F1 … F12 |
| `Fn+Del` | Forward delete (Delete) |
| `Fn+[` / `Fn+]` | Eisu / Kana |
| `Fn+O` | Switch between Mac and Windows (remembered) |
| `Fn+M` | Switch modes |

The keyboard mode's screen is text only for now (the spinning Earth comes in a later version). Typed characters are
not written to the log.

1. Install it, start it and plug it into a Mac. The first time it is recognized as a keyboard, the Mac may show the
   "Keyboard Setup Assistant". If so, press keys as it asks, or close it. If it asks for the type, choose "ANSI (US)".
2. Play music and check that the sound plays as before (adding the keyboard must not make the sound break up).
3. On the data screen, press `[` `]` `\` and check that the Mac's volume changes and mutes. `Space` plays and pauses,
   and `,` `/` go to the previous and next track.
4. Press `h`, `E` or the like, and check that the help screen opens, and that pressing again goes back. Left alone
   for 1 minute, the emblem screen comes up with `h HELP  Fn+M KEYBOARD` below.
5. Press `Fn+M` and check that it switches to the game screen and then the keyboard.
6. In keyboard mode, type into a text editor or the like. Try capitals (Shift), symbols, Enter, Del, the arrows
   (`Fn+;` and so on), Eisu and Kana with `Fn+[` `Fn+]`, and `alt+C` / `alt+V` (copy and paste on a Mac).
7. Turn it off and on again, and check that it starts in the last mode.
8. If you have time, also try pausing and resuming (5.8.1) a few times, and check that it does not restart.

### 5.11 The air mouse (added in S2.5-1)

**S2.5-1 (Mac, 2026-11-01):** the cursor moved, but the axes were crossed: the top down moved it left, the bottom
down right, the right side down moved it down and the left side down up. The IMU sits a quarter turn from the
screen. S2.5-2 swaps the two tilts (top down: up; right side down: right). S2.5-3 makes the wheel one step per press
of `;` (up) or `.` (down) with G0 held, whatever the tilt. S2.5-3 on the device: picking from a menu by tilting is
hard. S2.5-4 keeps `Fn+;` `Fn+,` `Fn+.` `Fn+/` as the arrow keys while G0 is held, to walk a menu one item at a time.
S2.5-5 dims whole blocks behind "KEY BOARD" and leaves out the block at each outer corner (the user's idea); to check
that it looks softer.

Noticed (2026-11-01): a click from the speaker when KeySpeaker starts, and only then (not on pausing or playing).
M5Unified powers up the ES8311's analog circuits, its DAC and its output and sets 0 dB in one go; a gentler order
(volume at the minimum while powering up, a short wait, then the volume raised smoothly) is to be tried after the
air mouse is checked. How much it helps is not known until it is heard.

**S2.5-9 (tried, 2026-11-01; dropped):** M5Unified wrote the ES8311 set-up, output on and 0 dB, before the I2S clock
(also the codec's clock) started. S2.5-9 sets the speaker up itself (`cfg.internal_spk = false`, the same pins as
M5Unified): the same set-up with the output off and the volume at the minimum, then, once the clock runs, the output
on and the volume raised from about -64 dB to 0 dB over 0.2 s. The log says `[BOOT] speaker on, quietly`. To check:
the click at start, and that sound, volume and the earphone jack are as before. Result on the device: the click was
the same, neither gone nor quieter; sound and volume were as before. So the click does not come from how the codec
is set up but from earlier, most likely the speaker being powered as the Cardputer starts, which software cannot
stop. Taken as unavoidable (decided with the user), and S2.5-9 was dropped: the firmware stays at S2.5-8, with
M5Unified setting up the speaker.

**S2.5-5 (Mac, 2026-11-01):** the corners behind "KEY BOARD" look right ("ばっちり"). The startup click is unchanged,
as expected (not yet worked on). With a right-click menu open, holding G0 and pressing up or down did not move it (they
were the wheel), and pressing keys on G0's own side while holding it was hard. S2.5-6: up and down are the arrow keys,
and `E` `Z` `A` `D` around `S` (left hand) work as well as `;` `.` `,` `/`; `S` is Enter; no wheel for now.

**S2.5-6 (Mac, 2026-11-01):** easy to use (the user's impression: "これは使いやすい"); the left hand's `E` `Z` `A` `D` were the right
choice.

**S2.5-7, S2.5-8 (Mac, 2026-11-01):** while G0 is held, the Earth says `MOUSE` and the right side shows the mouse's
keys; the keyboard's cheat sheet has one `G0+tilt Move` line instead of four. S2.5-8 lays the mouse's keys out as a
cross, as `E` `Z` `A` `D` sit around `S` (both the user's ideas). Checked on the device: easy to use (the user's
impression). On Windows (S2.5-10, 2026-11-01), the speaker, the keyboard and the mouse all worked with the mouse added
to the USB device.

Design 8.3.1. In keyboard mode only. The log line `[BOOT] IMU on` says the tilt sensor was found; without it the
cheat sheet has no mouse lines and G0 does nothing.

1. Hold G0 and tilt the Cardputer left, right, toward you and away. The cursor should move that way, faster the more
   it is tilted, and not at all for a small tilt. Letting go of G0 stops it. If a direction is the wrong way round,
   note which: the signs are one setting each. The log shows `[AIR] side … fore …` twice a second while G0 is held.
2. Hold G0 at a slant (lying down, say): that angle is the rest position, and the cursor stays still.
3. With G0 held, `,` or `A` clicks and `/` or `D` right-clicks. Hold G0 and `,` (or `A`) and tilt: it drags.
4. Right-click to open a menu; with G0 held, `;` / `E` and `.` / `Z` move through it one item at a time, and `S` (or
   Enter) picks it. On a long page, the same keys scroll it.
5. With G0 held, `;` `,` `.` `/` and `E` `A` `S` `Z` `D` type no letters; after letting go of G0 they type again. Typing without G0 is unchanged.
6. On Windows: the mouse is a new part of the USB device. If the keyboard or the speaker stops working there, note it
   (as with S2-10, an old registration may need uninstalling).

### 5.12 The jog dial (added in S2.5-10)

**S2.5-10 (Mac, 2026-11-01):** works well on the device (the user's impression: "ばっちり").

**S2.5-10 on Windows (2026-11-01):** the speaker, the mouse (with G0) and the jog dial all worked with no problem.

Design 8.3.2. Needs Media Jog's Unit Encoder (U135) on the Grove port; connect it while the Cardputer is off. The log
says `[DIAL] Unit Encoder found`.

1. On the data or game screen, turn it right and left: the computer's volume goes up and down one step a click, and
   `PC vol` follows. Press it: the music pauses, and plays again on the next press.
2. In keyboard mode, in a text field: turning it moves the text cursor right and left one character a click; with
   Shift held it selects. Pressing it is Enter.
3. Without the unit, nothing changes and the log stays quiet.

### 5.13 BLE keyboard and mouse (added in S5-1)

**S5-1 (Mac, 2026-11-01):** after unplugging, the screen still said `USB` and BLE never started; the Mac listed
"MediaJog BLE" (its earlier pairing) as not connected. The USB stack still reported "mounted" without the cable (the
Cardputer ADV cannot see the cable). S5-2 counts USB as there only while the computer's SOFs come in (design 5.3).

**S5-2 to S5-8 (Mac, 2026-11-01):**
- USB sometimes did not come up at all after the launcher handed over, with or without BLE built in (a build
  without BLE, S5-5 noBLE, did the same). The Mac saw nothing, not even the launcher's port, until the cable was
  plugged into the Mac again and the Cardputer switched off and on. S5-4 waits 10 s for USB at start before BLE;
  S5-7 also pulses USB every 3 s when no SOF has come since start (the old pulse ran only after SOFs had stopped).
- Over BLE (after removing the old "MediaJog BLE" pairing on the Mac: the Cardputer's pairings were erased with its
  internal flash), the Mac connected and subscribed to the keyboard and the mouse, and the Cardputer sent 574
  reports, all accepted (screen counters of S5-6), yet nothing was typed and the cursor did not move: macOS created
  no HID device for it. The report map is byte for byte Media Jog's.
- After BLE had started, plugging USB back in did not bring USB back.
- With BLE running only 32 KB of memory were free (107 KB before it starts). Taken as the likely cause of the last
  two: S5-8 shrinks the audio ring buffer from 500 ms to 200 ms (it is kept near 80 ms), freeing about 29 KB. Agreed
  with the user as an audio-path change. On the device, USB playback was fine (buffer 66 to 80 ms, no dropouts in the
  first minute), but BLE typing still did not work: memory was not the cause.
- The cause, found in the Mac's own log (`log stream`, process `BTLEServer`, as Codex suggested): macOS read the
  HID service and checked encryption before pairing had finished ("Not creating HID device as the link is not
  encrypted", then "peerDidCompletePairing" 1.3 s later), and did not try again. S5-9: the Cardputer asks for
  encryption as soon as a computer connects, and the HID service can be read only over an encrypted link.

**S5-9 (Mac, 2026-11-01):** after pairing again, macOS created the "KeySpeaker" HID device (it shows in
`ioreg`), typing and the air mouse work over BLE, and the Mac reconnected by itself after the link dropped. Still to
check: a Mac paired in Media Jog.

After a restart of the Cardputer, the Mac reconnected over BLE by itself, and typing and the mouse worked. USB did not
come back after an unplug and replug, not even when replugged within 2 s, before BLE started (the Mac saw nothing from
the unplug on): so not BLE's doing. With the cable in, the power switch does not cut the power (USB powers it); the
switch off plus the reset button does, and USB then came up. S5-10 added `Fn+R` to restart; after it, USB did not come
up (the Mac saw nothing, not even the Launcher, until the power was really cut). S5-11 made `Fn+R` go through 0.2 s of
deep sleep; it restarted, but USB still did not come back. S5-12 takes `Fn+R` out again, as well as the 200 ms
buffer of S5-8 (memory was not the cause) and the start pulse of S5-7 (no sign that it helped). The keyboard screen
says `Off+Reset to USB` without USB.

**S5-12 (Mac, 2026-11-01):** Over BLE, typing and the air mouse worked, and the keyboard screen showed
`Off+Reset to USB` at the lower left. After the switch off plus the reset button with the cable in, USB came up (green
`USB`) and the Mac listed "KeySpeaker" in its sound output, but did not pick it: the sound stayed on the Mac until
KeySpeaker was chosen by hand, then it came from the Cardputer. Which output to use is the Mac's choice; the firmware
cannot set it.

**Media Jog's pairing (Mac, 2026-11-01):** with the Mac paired as "MediaJog BLE" in Media Jog, KeySpeaker stayed at
`BLE wait` (`k0 m0`). The Mac's log showed it seeing KeySpeaker's advertising (HID service) again and again, but it
never tried to connect, and Bluetooth settings showed no Connect button for "MediaJog BLE". So item 6 below does not
hold on a Mac: after switching apps, remove the device on the Mac and pair again (the user's decision).

**S5-12 (Windows 11, 2026-11-01):** the Mac, paired with KeySpeaker, reconnected each time it was disconnected, so
KeySpeaker (one link at a time) did not show on Windows; with the Mac's Bluetooth off it did. Paired on Windows as
"KeySpeaker": typing, the air mouse (moving, `A` / `D` clicks) and reconnecting by itself after the Cardputer was
switched off and on all worked. S5 passed.

**S5-13 (2026-11-01):** checked on the device by the user: the start screen's round-hole grille and "KeySpeaker",
`S5-13` on the measure screen, and the tidied screens (no diagnostics at the bottom right, the flight screen's top
line only when something is wrong) are fine.

**S5-13 from M5Burner (2026-11-01):** the full image (`KeySpeaker_S5-13_MERGED.bin`: the same app as the
published `.ino.bin`, with the bootloader and the partition table) was published in M5Burner (CARDPUTER,
"kariagepompadour Cardputer ADV KeySpeaker"). The user burned it from M5Burner onto the device: no problems.

**iPhone 15 again, with S5-13 (USB-C, 2026-11-02):** a quick check by the user: the sound played from KeySpeaker
and typing worked, as with S2-10. The air mouse was not tried: an iPhone shows a pointer only with AssistiveTouch
turned on.

**S5-14 (2026-11-02):** checked on the device by the user: with no sound and no key, the game screen stays (it used
to give way to the resting emblem screen), and the data view goes to the emblem screen after a minute as before.
While music played (S5-13), the game screen once gave way all the same; the computer's volume was low, so the
sound was probably below the silence level (about −60 dB) for a minute. That was not checked further.
In silence the fighter keeps flying, firing and bombing, but no targets come: silence brought none (S5-15 changes
this).

**PS5 (USB, S5-14, 2026-11-02):** checked by the user. KeySpeaker works as a keyboard. In the Netflix app's search
only English input is offered, and the letters came in. In the PS5's own search, what KeySpeaker typed came in as
Japanese ("あいうえお"), and stayed Japanese with the on-screen keyboard set to ABC and after `Fn+[` / `Fn+]` (those
only change the `A/あ` on the Cardputer's screen). No sound from the speaker. The air mouse was not tried.

**S5-15 (2026-11-02):** checked on the device by the user: `S5-15` on the measure screen. In silence with no key,
the game screen stays, targets come now and then and the fighter shoots them down; the user found this many about
right. When music starts, the screen gets busy all at once, which the user finds fine as it is.

**S5-15 from M5Burner (2026-11-02):** the full image (`KeySpeaker_S5-15_MERGED.bin`: the committed S5-15 `.ino.bin`
at 0x10000, with the same bootloader, partition table and boot_app0 as the S5-13 image) replaced S5-13 in M5Burner,
now named "Cardputer ADV KeySpeaker". The user burned it from M5Burner: `S5-15` on the device, no problems.

**S9-1 (2026-11-04):** a space monster on the game screen (`FlightViz.h`), designed and tried in the web demo first
(the user's design: the beast of the Space Invaders cabinet art, black and red, lit from the left, glowing yellow eyes).
It walks in only while music plays, reaches for the fighter, misses and walks off; see [SCREENS.md](SCREENS.md). Built
with `build.sh`; the logic and the drawing were also run on a computer (no visit in 40 s of silence; the first one 6 s
after music starts and then one every 30 s or so, each going from walking to reaching, grabbing, missing and leaving)
and viewed frame by frame. S9-1 is the S5-15 code plus `FlightViz.h` only: the USB audio, HID keyboard and BLE code are
the same as in S5-15 (the S8-1 AUX input is on its own branch and is not in it).

**S9-1 on the device (2026-11-04), by the user:** first an earlier build of S9-1 that still carried the S8-1 AUX input
was imported into the Cardputer ADV through M5Launcher: it started normally and the monster appeared on the device's
screen. Then the build without the AUX input (the one in this repository: the S5-15 code plus `FlightViz.h`) was
imported through M5Launcher and checked: **it starts normally, the monster appears, USB audio works and the HID keyboard
works.** The web demo with the same monster was published and checked on its public page.

**S9-1 MERGED image on the device (2026-11-04), by the user:** `KeySpeaker_S9-1_MERGED.bin` (the S9-1 `.ino.bin` at
0x10000 with the same bootloader, partition table and boot_app0 as the S5-15 image, compared byte for byte) was written
to the Cardputer ADV: **it starts normally, the monster appears, and typing from the HID keyboard works.** USB audio was
not reported for this image (it was checked through M5Launcher, above).

**Not checked yet, for S9-1:** USB audio on the device after writing the MERGED image; BLE keyboard and mouse; the game
screen's drawing time (the monster adds a few thousand pixel writes a frame; the measure screen's `draw_max` should
stay well under 33 ms); the memory left (`heap` in the log); and whether anything flickers where the arm crosses the
bars and the fighter. M5Burner's listing still carries S5-15 until the S9-1 image is uploaded there. The S8-1 AUX input
is not part of S9-1 and stays unverified on its own branch.

Design 5.2 (plan A: Media Jog's BLE identity and GATT table) and 5.3. Over USB nothing changes. Without USB (on the
battery, or on a charger only) the Cardputer advertises as "KeySpeaker" after 3 seconds; the keyboard screen shows
`BLE wait` in yellow, then `BLE` in green once a computer is connected and ready.

1. Unplug USB. After 3 seconds, `BLE wait` shows. On the Mac, open System Settings > Bluetooth and connect to
   "KeySpeaker" (no code to type). `BLE` turns green.
2. Typing, the A/あ key, `Fn` with the arrows and so on work as over USB.
3. The air mouse: hold G0 and tilt, `A` / `D` clicks, `E` / `Z` and `S` in a right-click menu.
4. With the jog dial plugged in: ← → and Enter.
5. Plug USB back in: BLE stops at once, the speaker and USB keyboard work as before. Unplug again: BLE comes back by
   itself and the Mac reconnects without pairing again. No key stays held across the switch.
6. A Mac already paired in Media Jog: it connects to KeySpeaker without pairing again, and after going back to Media
   Jog, it still connects there. (The Mac may show either name.)
7. At most 2 paired computers (as Media Jog).

## 6. Please report

- For each OS: whether it was recognized, the name shown, whether sound came out, whether volume and mute worked
- The screen's values after 1 hour (a photo is fine), or the log file
- The best boost, and whether the bass cut on or off was easier to hear
- The results for plugging and unplugging, earphones and the power switch
- Any sounds that bothered you (clicks, noise, delay and so on)
