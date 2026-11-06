# aux_pico2_check.uf2

A check program for the Pico 2 H in the AUX unit (build guide step 6). It is
`pico2-pcm1808/karipom_pico2/karipom_pico2.ino` from the
[KariPom](https://github.com/kariagepompadour/KariPom) repository, unchanged,
built with arduino-cli for `rp2040:rp2040:rpipico2` (core 6.3.0, arduinoFFT 2.0.4).

Hold BOOTSEL, plug the Pico into USB, and drag this file onto the `RP2350` drive.
Tried on a Pico 2 H with a PCM1808 board on 2026-11-07, written from Windows: `rmsDb=-12.0 gate=OPEN fft/s=25 n=96000 mis=0 ovf=0`.
