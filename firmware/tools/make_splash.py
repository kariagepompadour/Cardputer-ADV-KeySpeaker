#!/usr/bin/env python3
"""Turns an image into the startup screen header (KeySpeaker_S0/Splash.h): 240 x 135 RGB565 pixels.

Usage (macOS, uses sips for scaling):
  python3 tools/make_splash.py assets/splash.png KeySpeaker_S0/Splash.h

assets/splash.png is assets/splash_original.webp with the rubber bumper and the black border replaced by more of the
grille's weave (copied from the weave left of the emblem, brightness matched, with soft edges), so the grille fills the
screen, and the emblem redrawn at 80 % of its size over more weave.

The Cardputer ADV's LCD shows dark colors lighter and bluer than a computer screen, and the gold emblem turned silver
on it (photo, 2026-10-30). So the colors are corrected for it: a gamma that sinks the darks, and warmth (more red,
less blue) that grows with brightness on colors that are already warm, so the emblem stays gold, the black grille
only loses its blue cast, and whites and grays stay neutral.
Pass --no-correct to keep the colors as they are.

The pixels are stored high byte first, the layout of M5Canvas's frame buffer, so the sketch copies them straight in.
"""
import os, struct, subprocess, sys, tempfile

W, H = 240, 135
GAMMA = 1.35                     # > 1 sinks the darks and mid-tones; white stays white
WARM_R, WARM_B = 0.10, -0.16     # red and blue change at full brightness (none at black)
DARK_B = -0.06                   # blue change at black, against the LCD's blue cast
correct = "--no-correct" not in sys.argv
sys.argv = [a for a in sys.argv if a != "--no-correct"]


def fix(r, g, b):
    if not correct:
        return r, g, b
    lum = (0.299 * r + 0.587 * g + 0.114 * b) / 255
    warmth = max(0.0, min(1.0, (r - b) / 60))  # how gold the pixel already is
    out = []
    for v, warm in ((r, WARM_R * lum * warmth), (g, 0.0), (b, WARM_B * lum * warmth + DARK_B * (1 - lum))):
        v = 255 * (v / 255) ** GAMMA * (1 + warm)
        out.append(max(0, min(255, round(v))))
    return out

src, out = sys.argv[1], sys.argv[2]

with tempfile.TemporaryDirectory() as tmp:
    bmp = os.path.join(tmp, "s.bmp")
    subprocess.run(["sips", "-s", "format", "bmp", "-z", str(H), str(W), src, "--out", bmp],
                   check=True, stdout=subprocess.DEVNULL)
    d = open(bmp, "rb").read()

off = struct.unpack_from("<I", d, 10)[0]
w, h = struct.unpack_from("<ii", d, 18)
bpp = struct.unpack_from("<H", d, 28)[0]
assert (w, abs(h)) == (W, H) and bpp in (24, 32), (w, h, bpp)
top_down, bp = h < 0, bpp // 8
stride = (W * bp + 3) & ~3

words = []
for y in range(H):
    row = y if top_down else H - 1 - y
    for x in range(W):
        o = off + row * stride + x * bp
        r, g, b = fix(d[o + 2], d[o + 1], d[o])
        c = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        words.append(((c & 0xFF) << 8) | (c >> 8))  # high byte first

with open(out, "w") as f:
    f.write("// Startup screen, 240 x 135 RGB565 (high byte first, as in M5Canvas's buffer).\n")
    f.write("// Made by firmware/tools/make_splash.py from firmware/assets/splash.png, the KariPom emblem on a speaker\n")
    f.write("// grille (the author's trademark). Do not edit by hand.\n")
    f.write("#pragma once\n#include <stdint.h>\n\n")
    f.write(f"static constexpr int SPLASH_W = {W}, SPLASH_H = {H};\n")
    f.write("static const uint16_t SPLASH[SPLASH_W * SPLASH_H] = {\n")
    for i in range(0, len(words), 16):
        f.write("  " + ", ".join(f"0x{v:04x}" for v in words[i:i + 16]) + ",\n")
    f.write("};\n")
print(f"{out}: {len(words)} pixels")
