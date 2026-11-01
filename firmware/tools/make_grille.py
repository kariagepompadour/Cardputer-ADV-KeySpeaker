#!/usr/bin/env python3
"""Draws the emblem screen's speaker grille, assets/splash_grille.png: perforated metal, 240 x 135, the screen's size.

Usage:
  python3 tools/make_grille.py assets/splash_grille.png

Dark metal with round holes in staggered rows, lit from the top left, each hole with a lit rim. It replaced the
fabric-like weave of the original picture in S5-13 (2026-11-01): on the Cardputer's small screen the weave did not
read as a speaker, and round holes do (the user's call, from mockups). It is drawn at the screen's own size so the
holes stay sharp; make_splash.py then applies the LCD correction to it as to the rest.
"""
import math, struct, sys, zlib

W, H = 240, 135
PITCH = 7          # px between hole centers along a row
HOLE_R = 2.4       # hole radius, px
METAL = (78, 78, 82)
HOLE = (6, 6, 8)


def grille():
    rows, row_pitch = [], PITCH * math.sqrt(3) / 2
    for y in range(H):
        row = []
        for x in range(W):
            j = round(y / row_pitch)
            off = PITCH / 2 if j % 2 else 0
            cx, cy = round((x - off) / PITCH) * PITCH + off, j * row_pitch
            d = math.hypot(x - cx, y - cy)
            lit = 1.15 - 0.45 * ((x / W) * 0.4 + (y / H) * 0.6)  # light from the top left
            if d < HOLE_R:
                c = HOLE
            elif d < HOLE_R + 0.9:
                c = tuple(min(255, v * lit * 1.35) for v in METAL)  # the lit rim of each hole
            else:
                c = tuple(v * lit for v in METAL)
            row.append(tuple(max(0, min(255, round(v))) for v in c))
        rows.append(row)
    return rows


def write_png(path, rows):
    raw = b"".join(b"\x00" + bytes(v for px in row for v in px) for row in rows)

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


write_png(sys.argv[1], grille())
print(f"{sys.argv[1]}: {W} x {H}, holes every {PITCH} px")
