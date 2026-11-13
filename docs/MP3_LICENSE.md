# MP3 decoder license (Helix, RPSL / RCSL): what it asked, and the decision

**Decision (2026-11-13): option B.** S16-2 and later use **minimp3** (https://github.com/lieff/minimp3, one header `firmware/KeySpeaker_S0/minimp3.h`, CC0 1.0 / public domain; license text in `docs/minimp3_LICENSE.txt`). The firmware no longer links the SDK's Helix library, so the RPSL notices below are **not needed** for builds from S16-2 on. The text below is kept as the reasoning. The S16-1 test images (Helix) stay with the developer and are not published.

Written on 2026-11-13 from the license texts in `ESP8266Audio/src/libhelix-mp3` (`RPSL.txt`, `RCSL.txt`, `LICENSE.txt`) and the headers of the copy that ships with the m5stack:esp32 3.3.10 SDK (the same notice). This is a reading of the texts for a decision, **not legal advice**.

## What is used

The MP3 input (`firmware/KeySpeaker_S0/Mp3Source.h`) calls the Helix MP3 decoder (RealNetworks, 1995-2002; packaged as `chmorgan/esp-libhelix-mp3`). The source is **not** in this repository: the prebuilt `libchmorgan__esp-libhelix-mp3.a` comes with the SDK, and a KeySpeaker binary (M5Burner image, `firmware/*.bin`) that uses it **contains Helix object code**, which counts as distributing Covered Code.

The files carry "RCSL 1.0 / RPSL 1.0": the RPSL applies unless one holds an RCSL from RealNetworks (nobody here does).

## What the RPSL asks of a distributor of a binary (my reading)

1. **Notice in the code and in the documentation** (2.1(e)): a prominent notice that the Source Code of the Covered Code is available under the RPSL, with how and where to get it, and the **Object Code Notice** in the "about" place or packaging: *"Helix DNA Client technology included. Copyright (c) RealNetworks, Inc., 1995-2002. All rights reserved."*
2. **Keep the notices** of the original code (2.1(a)); a copy of the license goes with any Source Code of Covered Code that is handed on (2.1(b)). Not needed while only the binary is distributed, but the repository would point to the license text.
3. **Modifications** of Helix would have to be published under the RPSL (2.1(d)). KeySpeaker does not modify it.
4. **The combined work** (4.1, 4.2): software combined with Covered Code into one application is exempt from carrying the RPSL itself only if it is "wholly subject to" one of the **Compatible Source Licenses** (Exhibit B) and its source is readily available. **The MIT license is on that list**, and this repository (MIT, public) makes its source available. So the KeySpeaker code itself is fine.
5. **Trademarks** (Exhibit C): "RealNetworks", "Helix" and the like may be used only as the notice above and the trademark guidelines allow.

## The open question

Section 4.2 asks that **everything linked into the application** is wholly under a Compatible Source License. A KeySpeaker binary also contains the Arduino-ESP32 core, which is **LGPL-2.1** (my knowledge of that project; to be verified against the SDK's own files), and Exhibit B marks the LGPL with a note (1): its own reciprocal terms may **prohibit** combining with Covered Code, because Covered Code may only be licensed under the RPSL. Whether static linking of the core with Helix in one binary is a problem is a legal question I cannot settle here. M5Unified, M5GFX and M5Cardputer are MIT; ESP-IDF is Apache-2.0 (on the list).

The same question already exists for anyone who builds an Arduino sketch that uses the SDK's Helix, but the repository's M5Burner images would be the distribution.

## Options

| Option | Effect |
|---|---|
| **A. Keep Helix, comply** | Add the notices above (README, the Help / about screen if there is one, the M5Burner description), and accept the LGPL question above, or ask someone qualified. No code change. |
| **B. Replace the decoder with `minimp3`** (CC0 1.0, one header, by lieff) | Removes the question: CC0 is as permissive as it gets and combines with MIT and LGPL alike. Costs: a day of work and a new measurement round (RAM and CPU are not known yet: its state and scratch are said to be smaller than Helix's 24 KB, but this has not been measured here). It would also decouple the MP3 input from the SDK's prebuilt library. |
| **C. Ship MP3 as a build option that is off in the released image** | Keeps the code in the repository (MIT) and the released `.bin` free of Helix; people who build it themselves get MP3. Weaker as a product feature. |

## Recommendation

**B** if the MP3 input is meant to be in the released images: it removes the license question for good, and the memory gain would help the battery-only case too (not measured). **A** only if someone qualified has looked at the LGPL note first. Until then, the images built for testing (`~/Desktop/ks_mp3_stage01/`) stay with the developer and are not published.
