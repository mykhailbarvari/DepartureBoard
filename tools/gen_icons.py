# -*- coding: utf-8 -*-
"""Genererar 24x24 1-bits ikoner som C-arrayer for drawBitmapMask().

drawBitmapMask() forvantar radvis packning, MSB forst per byte.
bytesPerRow = (24 + 7) // 8 = 3.
"""
import math

W = H = 24
CX = CY = 11.5          # geometriskt centrum for 24x24


def blank():
    return [[0] * W for _ in range(H)]


def put(g, x, y, v=1):
    xi, yi = int(round(x)), int(round(y))
    if 0 <= xi < W and 0 <= yi < H:
        g[yi][xi] = v


def disc(g, cx, cy, r, v=1):
    for y in range(H):
        for x in range(W):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                g[y][x] = v


def ring(g, cx, cy, r_out, r_in, v=1):
    for y in range(H):
        for x in range(W):
            d2 = (x - cx) ** 2 + (y - cy) ** 2
            if r_in * r_in <= d2 <= r_out * r_out:
                g[y][x] = v


def rect(g, x0, y0, w, h, v=1):
    for y in range(y0, y0 + h):
        for x in range(x0, x0 + w):
            if 0 <= x < W and 0 <= y < H:
                g[y][x] = v


def line(g, x0, y0, x1, y1, v=1):
    n = int(max(abs(x1 - x0), abs(y1 - y0)) * 2) + 1
    for i in range(n + 1):
        t = i / n
        put(g, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, v)


def norm_angle(a):
    """atan2 ger (-180, 180]; hanterar intervall som wrappar forbi +/-180."""
    while a <= -180: a += 360
    while a > 180:   a -= 360
    return a


def arc(g, cx, cy, r_out, r_in, a0, a1, v=1):
    lo, hi = norm_angle(a0), norm_angle(a1)
    for y in range(H):
        for x in range(W):
            d = math.hypot(x - cx, y - cy)
            if not (r_in <= d <= r_out):
                continue
            a = math.degrees(math.atan2(y - cy, x - cx))
            inside = (lo <= a <= hi) if lo <= hi else (a >= lo or a <= hi)
            if inside:
                g[y][x] = v


# ---------------------------------------------------------------- LJUSSTYRKA
def icon_sun():
    g = blank()
    disc(g, CX, CY, 5.2)                      # solskiva
    for k in range(8):                        # 8 strålar
        a = k * math.pi / 4
        line(g,
             CX + math.cos(a) * 7.6, CY + math.sin(a) * 7.6,
             CX + math.cos(a) * 10.5, CY + math.sin(a) * 10.5)
    return g


# ------------------------------------------------------------------ FÄRGTEMA
def icon_palette():
    """Tre overlappande cirklar - lasbart aven i 24 px."""
    g = blank()
    r = 5.0
    for (dx, dy) in ((0, -4.2), (-4.6, 3.0), (4.6, 3.0)):
        ring(g, CX + dx, CY + dy, r, r - 1.8)
    return g


# ------------------------------------------------------------------- NATVERK
def icon_wifi():
    """Klassiska WiFi-bagar. Baspunkten pa y=17.5 centrerar symbolen vertikalt;
    med 19.5 blev den bottentung med sju tomma rader over."""
    g = blank()
    by = 17.5
    for r in (13.5, 9.5, 5.5):
        arc(g, CX, by, r, r - 1.9, -142, -38)
    disc(g, CX, by, 1.9)
    return g


# ----------------------------------------------------------------- AVGANGAR
def icon_list():
    """Tre vagrata staplar av olika langd. Listsymbol framfor buss, eftersom
    tavlan aven visar tunnelbana, pendeltag och sparvagn."""
    g = blank()
    for i, (x0, w) in enumerate(((3, 18), (3, 13), (3, 16), (3, 11))):
        rect(g, x0, 4 + i * 5, w, 3)
    return g


# --------------------------------------------------------------------- SYSTEM
def icon_gear():
    """Polart ritat: tandad ytterradie + hal. Ger en ren ring med jamna kuggar,
    till skillnad fran att rita varje kugg som streck (som blev lumpigt)."""
    g = blank()
    teeth   = 8
    R_body  = 7.8
    R_tooth = 10.4
    R_hole  = 3.6
    duty    = 0.42        # andel av varje kuggperiod som ar kugg

    for y in range(H):
        for x in range(W):
            dx, dy = x - CX, y - CY
            r = math.hypot(dx, dy)
            if r < R_hole:
                continue
            a = math.atan2(dy, dx)
            # + duty/2 centrerar en kugg pa 0 grader, annars ser hjulet snett ut
            phase = (a * teeth / (2 * math.pi) + duty / 2.0) % 1.0
            r_out = R_tooth if phase < duty else R_body
            if r <= r_out:
                g[y][x] = 1
    return g


def icon_pin():
    """Kartnal: cirkel med hal + spets nedat."""
    g = blank()
    hx, hy, R = CX, 8.0, 6.2
    ring(g, hx, hy, R, R - 2.2)
    # spets: triangel fran cirkelns underkant ner till y=21
    for y in range(int(hy), 22):
        t = (y - hy) / (21.0 - hy)
        half = max(0.0, R * (1.0 - t) * 0.9)
        for x in range(W):
            if abs(x - hx) <= half and (x - hx) ** 2 + (y - hy) ** 2 > (R - 2.2) ** 2:
                g[y][x] = 1
    return g

ICONS = [
    ("icon_brightness_24", icon_sun()),
    ("icon_palette_24",    icon_palette()),
    ("icon_pin_24",        icon_pin()),
    ("icon_wifi_24",       icon_wifi()),
    ("icon_list_24",       icon_list()),
    ("icon_gear_24",       icon_gear()),
]


def pack(g):
    """Radvis, MSB forst, 3 byte per rad."""
    out = []
    for y in range(H):
        for byte_i in range(3):
            b = 0
            for bit in range(8):
                x = byte_i * 8 + bit
                if x < W and g[y][x]:
                    b |= 0x80 >> bit
            out.append(b)
    return out


def preview(name, g):
    print(f"\n{name}")
    for row in g:
        print("  " + "".join("##" if v else ".." for v in row))


lines = [
    "// GENERERAD FIL - andra generatorn, inte den har filen.",
    "// 24x24 1-bits ikoner for karusellmenyn, packade radvis MSB forst",
    "// sa att drawBitmapMask() kan rita dem direkt.",
    "",
    '#include "ui_icons.h"',
    "",
]

for name, g in ICONS:
    data = pack(g)
    lines.append(f"const uint8_t {name}[{len(data)}] = {{")
    for y in range(H):
        row = data[y * 3:(y + 1) * 3]
        lines.append("  " + ", ".join(f"0x{b:02X}" for b in row) + ",")
    lines.append("};")
    lines.append("")
    preview(name, g)

open("lib/ui/src/ui_icons.cpp", "w", newline="\r\n").write("\n".join(lines))
print("\nskrev lib/ui/src/ui_icons.cpp")
