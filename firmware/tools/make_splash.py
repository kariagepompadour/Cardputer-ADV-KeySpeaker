#!/usr/bin/env python3
"""Turns the emblem screen's images into KeySpeaker_S0/Splash.h: the grille (240 x 135 RGB565) and the emblem on its own
(RGB565 and an alpha), so the sketch can shake the emblem on loud beats (S2-7).

Usage (macOS, uses sips for scaling):
  python3 tools/make_splash.py assets/splash_grille.png assets/splash_emblem.png assets/splash_emblem_alpha.png \\
      KeySpeaker_S0/Splash.h

The three images are the layers of assets/splash.png, all at the size of assets/splash_original.webp:
- splash_grille.png: the grille alone. The original's rubber bumper, black border and emblem are replaced by more of
  its weave (copied from the weave left of the emblem, brightness matched, with soft edges), so it fills the screen.
- splash_emblem.png: the emblem at 80 % of its size, with its ring of weave and its soft edge, on black (its colors
  already multiplied by its alpha).
- splash_emblem_alpha.png: the emblem's alpha, white where it covers the grille.
The grille with the emblem over it is assets/splash.png.

The Cardputer ADV's LCD shows dark colors lighter and bluer than a computer screen. So the colors are corrected for
it: a gamma that sinks the darks, and less blue in the darks, so the black grille loses its blue cast. The gold
emblem turns silver on this LCD (photo, 2026-10-30). Extra warmth once kept it gold, but silver looks more like a
speaker's badge (2026-10-31). Left alone it still kept a little gold, so the emblem's colors are now turned to grays
(EMBLEM_SAT), which makes it plain silver.
Pass --no-correct to keep the colors as they are.

The grille is stored high byte first, the layout of M5Canvas's frame buffer, so the sketch copies it straight in.
The emblem is stored as plain RGB565 values, since the sketch blends it over the grille pixel by pixel.
"""
import os, struct, subprocess, sys, tempfile

W, H = 240, 135
GAMMA = 1.35                     # > 1 sinks the darks and mid-tones; white stays white
DARK_B = -0.06                   # blue change at black, against the LCD's blue cast
EMBLEM_SAT = 0.0                 # the emblem's color saturation: 0 gray (silver), 1 as drawn (gold)
correct = "--no-correct" not in sys.argv
sys.argv = [a for a in sys.argv if a != "--no-correct"]


def fix(r, g, b):
    if not correct:
        return r, g, b
    lum = (0.299 * r + 0.587 * g + 0.114 * b) / 255
    out = []
    for v, k in ((r, 0.0), (g, 0.0), (b, DARK_B * (1 - lum))):
        v = 255 * (v / 255) ** GAMMA * (1 + k)
        out.append(max(0, min(255, round(v))))
    return out


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def load(src):
    """The image scaled to W x H, as rows of (r, g, b)."""
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
    rows = []
    for y in range(H):
        o = off + (y if top_down else H - 1 - y) * stride
        rows.append([(d[o + x * bp + 2], d[o + x * bp + 1], d[o + x * bp]) for x in range(W)])
    return rows


grille_src, emblem_src, alpha_src, out = sys.argv[1:5]
grille, emblem, alpha = load(grille_src), load(emblem_src), load(alpha_src)

words = []
for row in grille:
    for r, g, b in row:
        c = rgb565(*fix(r, g, b))
        words.append(((c & 0xFF) << 8) | (c >> 8))  # high byte first

# The emblem's box: every pixel it covers at all.
cover = [(x, y) for y in range(H) for x in range(W) if alpha[y][x][0] > 0]
ex0, ex1 = min(x for x, _ in cover), max(x for x, _ in cover) + 1
ey0, ey1 = min(y for _, y in cover), max(y for _, y in cover) + 1
em_rgb, em_a = [], []
for y in range(ey0, ey1):
    for x in range(ex0, ex1):
        a = alpha[y][x][0]
        # back from "multiplied by alpha" to the emblem's own color, then the same LCD correction
        r, g, b = (min(255, round(v * 255 / a)) if a else 0 for v in emblem[y][x])
        gray = 0.299 * r + 0.587 * g + 0.114 * b
        r, g, b = (min(255, round(gray + (v - gray) * EMBLEM_SAT)) for v in (r, g, b))
        em_rgb.append(rgb565(*fix(r, g, b)) if a else 0)
        em_a.append(a)


def table(f, ctype, name, size, values, fmt, per_line):
    f.write(f"static const {ctype} {name}[{size}] = {{\n")
    for i in range(0, len(values), per_line):
        f.write("  " + ", ".join(fmt.format(v) for v in values[i:i + per_line]) + ",\n")
    f.write("};\n")


with open(out, "w") as f:
    f.write("// Emblem screen: the speaker grille, 240 x 135 RGB565 (high byte first, as in M5Canvas's buffer), and the\n")
    f.write("// KariPom emblem (the author's trademark) on its own, RGB565 with an alpha, to draw over it.\n")
    f.write("// Made by firmware/tools/make_splash.py from firmware/assets/splash_*.png. Do not edit by hand.\n")
    f.write("#pragma once\n#include <stdint.h>\n\n")
    f.write(f"static constexpr int SPLASH_W = {W}, SPLASH_H = {H};\n")
    f.write(f"static constexpr int EMBLEM_X = {ex0}, EMBLEM_Y = {ey0}, EMBLEM_W = {ex1 - ex0}, EMBLEM_H = {ey1 - ey0};\n")
    table(f, "uint16_t", "SPLASH_GRILLE", "SPLASH_W * SPLASH_H", words, "0x{:04x}", 16)
    table(f, "uint16_t", "EMBLEM_RGB", "EMBLEM_W * EMBLEM_H", em_rgb, "0x{:04x}", 16)
    table(f, "uint8_t", "EMBLEM_A", "EMBLEM_W * EMBLEM_H", em_a, "{}", 24)
print(f"{out}: grille {len(words)} pixels, emblem {ex1 - ex0} x {ey1 - ey0} at ({ex0}, {ey0})")
