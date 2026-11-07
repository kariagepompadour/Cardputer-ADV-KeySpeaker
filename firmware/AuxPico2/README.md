# AuxPico2

The program for the Pico 2 H in the AUX unit (docs/AUX_PICO2_DESIGN.md).

Hold BOOTSEL, plug the Pico into USB, and drag `AuxPico2.uf2` onto the `RP2350` drive.

To build it: `arduino-cli compile --fqbn rp2040:rp2040:rpipico2 AuxPico2` (core rp2040:rp2040 6.3.0). The sketch needs no
other libraries.
