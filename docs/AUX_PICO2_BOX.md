# Pico AUX unit: what is true, and what may be printed on the box

Japanese version: [AUX_PICO2_BOX_JA.md](AUX_PICO2_BOX_JA.md). Wiring and build steps: [AUX_PICO2_BUILD.md](AUX_PICO2_BUILD.md). Design: [AUX_PICO2_DESIGN.md](AUX_PICO2_DESIGN.md)

Marks: ✅ read in this repository's code or measured by the author, 🟡 expected or general knowledge, ⛔ not checked.

## 1. What the Grove cable carries

| Cardputer mark | Wire (author's cable) | Carries | Pico side | Source |
|---|---|---|---|---|
| `G1` | Yellow | UART, Pico → Cardputer (sound and dial frames) | GP0 (TX), via 330 Ω | ✅ `KeySpeaker_S0.ino:1602` (RX = GPIO1), `AuxPico2.ino:23` |
| `G2` | White | UART, Cardputer → Pico (commands `KS?` `KS+` `KS-`) | GP1 (RX), via 330 Ω | ✅ `KeySpeaker_S0.ino:1602` (TX = GPIO2), `AuxPico2.ino:23` |
| `5V` | Red | Power, Cardputer → unit | VSYS (pin 39) and PCM1808 VDD | ✅ design §4 |
| `G` | Black | Ground | GND | ✅ design §4 |

- It is a **UART, not I²C**: 8N1 at **2,000,000 bps**, 3.3 V logic (✅ `AuxFrame.h:17`; the same file is in both firmwares).
- Sound goes one way (Pico → Cardputer): 48 kHz, 16-bit, mono, 240 samples every 5 ms, with CRC-16.
- The same two pins also carry I²C for the optional jog dial. Only one of the two is open at a time (✅ `KeySpeaker_S0.ino:1590`).
- Colours differ between Grove cables. Go by the marks `G`, `5V`, `G2`, `G1` beside the port, never by colour.

## 2. Power

- The unit is powered **from the Grove red wire** (✅ design §5: about 5.1 V under load, measured). Total draw is about 100 mA at 5 V (🟡 design figure).
- The Pico's own USB port is **not needed in use**. It is for writing the program (BOOTSEL) and for a status text line.
- **The Cardputer's own USB and the Grove cable may be used together** (✅ the author measured Grove red at 5.347 V on USB and ran sound this way). The rule below is only about the **Pico's** USB port.
- **Never connect the Grove cable and the Pico's USB at the same time.** (✅ build guide §2.) Both would feed VSYS. A Pico 2 has a diode from USB (VBUS) to VSYS (🟡 Raspberry Pi's datasheet, not in this repo), so the Pico itself is protected, but the author has **never measured the two connected together** (⛔). The risk is USB current flowing back into the Cardputer's 5 V pin.
- The red wire must stay at or below 5.5 V, the Pico 2's VSYS limit. The Cardputer gives 5.0 to 5.35 V (✅ measured), so there is only about 0.15 V of room. Do not feed the unit from a higher rail.
- On the Cardputer ADV, the switch beside `PORT.A` must be on `5VOUT`.

## 3. What a computer sees

✅ `AuxPico2.ino` opens only `Serial` (USB CDC, 115200 bps) and prints a status line each second. It has no USB audio code.

So over USB the Pico shows up as a **serial port (COM port)**, not as a sound card. It does not send audio to the computer over USB. A label such as "Line-in → USB" would be wrong. The sound goes out over the Grove UART only.

## 4. Is anything Cardputer-ADV-specific?

| Part | Cardputer-specific? |
|---|---|
| Pico firmware (`AuxPico2`) | No. It knows nothing about the Cardputer. It only answers the three commands and sends frames. |
| Frame format (`AuxFrame.h`) | No. Plain UART frames with a CRC. |
| Receiving code in KeySpeaker | **Pin numbers are baked in** (`Serial1.begin(..., 1, 2)`, `Wire.begin(2, 1, ...)`), and the whole program is built for the Cardputer ADV (M5Cardputer library, ES8311 codec, its keyboard and screen). |

So **KeySpeaker itself works only on the Cardputer ADV.** The unit, however, is generic.

## 5. What may be written about other boards

Allowed, with conditions:

> "Any 3.3 V UART host that can supply 5 V (up to 5.5 V, about 100 mA) and speak the unit's protocol (2 Mbps UART, see `AuxFrame.h`). Tested only with the Cardputer ADV running KeySpeaker."

Not allowed: "works with any ESP32 board" or "plug and play". Other boards have no ready software. They need a program that sends `KS?` / `KS+` and reads the frames, and an ESP32 that cannot do 2 Mbps reliably on its pins is untested (⛔). A 5 V-logic host (for example an Arduino Uno) is not safe: the Pico's pins are 3.3 V.

## 6. Items for the manual

1. A wiring table (section 1, with marks, not colours).
2. A power warning (section 2): no Grove and USB together; 5.5 V limit; `5VOUT`.
3. Supported boards (section 5), with "tested only with" wording.
4. What the Pico looks like to a computer (section 3).
5. Troubleshooting additions: "No `AUX`: swap check of G1 and G2"; "Pico disappears from the computer when Grove is plugged in" is normal only if USB is also unplugged; "Cardputer restarts or the screen dims while the Pico's own USB and Grove are both connected" means stop and unplug the Pico's USB.
6. Soldered-build checks (section 7).
7. Two ways to build the power and ground lines. The breadboard manual uses the + and − rails, so the PCM1808 board's I²S-side GND also goes to the − rail. The author's compact box has no rails: the Grove red and black wires are each split in two (Pico and PCM1808 board), and the Pico's AGND (pin 33) is joined to the board's CN3 GND (the middle pin, between R_IN and L_IN) with one extra wire. The I²S-side GND pin is left empty, and the unit works that way (author's unit; why it works was not measured). Say in the manual that the box is the rail-less version of the same wiring.

## 7. A first build in a box (checks for the author)

Most connections are plug-in jumper wires. Solder is used only where the Grove cable is split and joined to the two 330 Ω resistors (heat-shrink on the joints). Read from the author's photos of 2026-11-09; ⛔ = cannot be judged from the photos.

- Plug-in (Dupont) connectors hold only by friction and can work loose when the box is moved. Put a dab of hot glue or a turn of tape over each one, especially the 5 V and GND ones. ⛔

- The loose jumper bundle can shake and short against the Pico's pins or the ADC board. Tie it or fix it with hot glue or a cable tie. ⛔
- Any bare joint near another one (5 V, GND, the three I²S lines) needs insulation: heat-shrink, or a small piece of tape. ⛔
- Joints on stranded wire: tug each wire gently. A dull, round joint can be cold. Re-melt it. ⛔
- The Grove cable needs strain relief where it leaves the box (a knot inside, a notch, or a clamp). A pulled cable otherwise tears the joints. ⛔
- The lid groove is deep (author), so wires should not be pinched in the rail. But in the 2026-11-09 photo the wire loops rise above the box rim: the lid will press them down onto the Pico pins and the boards. Shorten or bundle the loops so everything sits below the rim.
- With the power off, test 5 V against GND with a multimeter's beep mode before the first power-on (the build guide step for this stays valid).
- The 330 Ω resistors on the two Grove lines must still be in the soldered version.
- Put a label of 5V and GND at the Grove exit so that the cable cannot be fitted the wrong way.

**Checked from the author's photos (2026-11-09, box opened for the photos).** The Pico's micro-USB connector sits inside its square hole, and the PCM1808 board's jack sits inside the round hole. Both boards have four small tabs in their compartment that line up with their mounting holes (whether the tabs enter the holes was not visible). The 330 Ω resistors read orange-orange-brown. The Grove red and black splits are covered by heat-shrink with no bare wire showing. All connections of the table in `AUX_PICO2_DESIGN.md` section 4 were matched with the author's description, and `Fn+I` offers AUX, which shows that both Grove lines are connected the right way round. Not checked: a cable actually going into the USB hole and the jack, and the lid closing over the loops.

## 8. Text on the box (author's plan; CASE4 model, 2026-11-09)

The Cardputer ADV sits on the lid (two 4 mm pegs fit its back holes), so the lid top is covered. The text goes on the sides, engraved with a 3D printer. The unit's name stays generic and KeySpeaker is named as compatible software.

| Where | Text |
|---|---|
| Long side without holes, large | `PICO AUX` |
| Same side, small | `Line-in → Grove`, `Compatible: KeySpeaker` |
| Beside the round jack hole | `AUX IN` |
| Beside the Grove notch | `TO CARDPUTER` (`DIAL` may be added when the jog dial is routed through it) |
| Above the Pico's USB hole | `PICO 2 USB`, `SET UP ONLY`, `UNPLUG GROVE`, `FIRST` (four lines) |

Not used: "Line-in → USB" (the Pico shows up as a serial port, not as sound over USB), and "for Cardputer ADV" (it reads as ADV-only; the unit itself is generic). Keep the strip where the Cardputer overlaps clear of text.

**Why the USB label.** Connecting the Pico's own USB while Grove is plugged in has not been measured (the Cardputer's own USB together with Grove has: that is fine). The label is an instruction, not a claim that something breaks. The manual should carry the same warning.

**Engraving.** All text is a negative part (Bambu Studio: Operation = Cut) with the same depth of 0.5 mm; the CASE4 model measures 0.49 mm on both long faces, and the wall is 2 mm. Small text was made bold. Not yet checked: a test print of the smallest text (`Compatible: KeySpeaker`) on the A1 mini (0.4 mm nozzle). Some text sits very close to a wall corner on the front face (about 0.2 mm from the edge in the model).
