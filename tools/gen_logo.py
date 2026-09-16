# -*- coding: utf-8 -*-
"""Rastrerar MB_Labs.svg till en 4-bitars alfa-bitmap for LED-panelen.

Ingen SVG-rastrerare finns installerad pa maskinen (varken Inkscape, rsvg,
ImageMagick, cairosvg eller Pillow), men filen ar potrace-utdata och anvander
bara kommandona M m c l z. Det gor den fullt hanterbar att rastrera har, i
samma anda som gen_icons.py: ren Python som skriver en packad C-array, inga
nya byggberoenden.

Varfor alfa och inte 1 bit: logotypen ar knappt 100 px bred pa panelen och
bestar till stor del av diagonaler. Utan graderade kanter trappar de sonder.

Kor fran projektroten:  python tools/gen_logo.py
"""
import re

SVG_IN   = "MB_Labs.svg"
CPP_OUT  = "lib/ui/src/ui_logo.cpp"
WEB_OUT  = "lib/portal/src/portal_logo.h"

# Portalens forgrundsfarg. SVG:en ar svart, vilket ar osynligt mot den
# morka sidan, och en <img> arver inte currentColor.
WEB_FILL = "#e8eaed"

PANEL_W, PANEL_H = 128, 64
LOGO_SCALE = 0.8            # andel av panelen logotypen far uppta
SS = 4                      # overprovtagning per axel -> 16 sampel per pixel
FLATTEN = 12                # linjesegment per kubisk bezier


# --------------------------------------------------------------- SVG-parsning

TOKEN = re.compile(r'([MmCcLlZz])|(-?\d*\.?\d+)')


def parse_path(d):
    """Returnerar en lista av delbanor, var och en en lista av (x, y)."""
    toks = [(a or b) for a, b in TOKEN.findall(d)]
    subs, cur = [], []
    pos = start = (0.0, 0.0)
    i, cmd = 0, None

    def num():
        nonlocal i
        v = float(toks[i]); i += 1
        return v

    while i < len(toks):
        if re.match(r'[A-Za-z]', toks[i]):
            cmd = toks[i]; i += 1
            if i >= len(toks) and cmd not in ('Z', 'z'):
                break

        if cmd in ('M', 'm'):
            x, y = num(), num()
            if cmd == 'm':
                x, y = pos[0] + x, pos[1] + y
            if cur:
                subs.append(cur)
            pos = start = (x, y)
            cur = [pos]
            # Efterfoljande par utan kommando ar implicit lineto.
            cmd = 'l' if cmd == 'm' else 'L'

        elif cmd in ('L', 'l'):
            x, y = num(), num()
            if cmd == 'l':
                x, y = pos[0] + x, pos[1] + y
            pos = (x, y)
            cur.append(pos)

        elif cmd in ('C', 'c'):
            c = [num() for _ in range(6)]
            if cmd == 'c':
                p1 = (pos[0] + c[0], pos[1] + c[1])
                p2 = (pos[0] + c[2], pos[1] + c[3])
                p3 = (pos[0] + c[4], pos[1] + c[5])
            else:
                p1, p2, p3 = (c[0], c[1]), (c[2], c[3]), (c[4], c[5])
            p0 = pos
            for k in range(1, FLATTEN + 1):
                u = k / float(FLATTEN); v = 1.0 - u
                cur.append((
                    v*v*v*p0[0] + 3*v*v*u*p1[0] + 3*v*u*u*p2[0] + u*u*u*p3[0],
                    v*v*v*p0[1] + 3*v*v*u*p1[1] + 3*v*u*u*p2[1] + u*u*u*p3[1]))
            pos = p3

        else:  # Z / z
            if cur:
                cur.append(start)
                subs.append(cur)
                cur = []
            pos = start
            cmd = None
            if i < len(toks) and not re.match(r'[A-Za-z]', toks[i]):
                i += 1

    if cur:
        subs.append(cur)
    return subs


def load_polygons(path):
    svg = open(path, encoding="utf-8").read()

    m = re.search(r'transform="translate\(([-\d.]+),([-\d.]+)\)\s*'
                  r'scale\(([-\d.]+),([-\d.]+)\)"', svg)
    tx, ty, sx, sy = map(float, m.groups()) if m else (0.0, 0.0, 1.0, 1.0)

    polys = []
    for d in re.findall(r'\sd="([^"]+)"', svg):
        for sub in parse_path(d):
            if len(sub) > 2:
                polys.append([(tx + sx * x, ty + sy * y) for x, y in sub])
    return polys


# ------------------------------------------------------------------ rastrering

def rasterise(polys):
    xs = [p[0] for s in polys for p in s]
    ys = [p[1] for s in polys for p in s]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)

    # Passa in i panelen med bevarat forhallande, nedskalat med LOGO_SCALE.
    box_w = PANEL_W * LOGO_SCALE
    box_h = PANEL_H * LOGO_SCALE
    scale = min(box_w / (x1 - x0), box_h / (y1 - y0))
    w = max(1, int(round((x1 - x0) * scale)))
    h = max(1, int(round((y1 - y0) * scale)))

    sw, sh = w * SS, h * SS
    s = min(sw / (x1 - x0), sh / (y1 - y0))
    ox, oy = -x0 * s, -y0 * s

    edges = []
    for sub in polys:
        pts = [(p[0] * s + ox, p[1] * s + oy) for p in sub]
        for a, b in zip(pts, pts[1:]):
            if a[1] != b[1]:
                edges.append((a, b))

    # Scanline med nonzero winding: potrace lindar hal motsatt ytterkonturen,
    # sa even-odd skulle fylla igen dem.
    cov = [[0] * sw for _ in range(sh)]
    for yi in range(sh):
        yc = yi + 0.5
        hits = []
        for (ax, ay), (bx, by) in edges:
            if (ay <= yc < by) or (by <= yc < ay):
                t = (yc - ay) / (by - ay)
                hits.append((ax + t * (bx - ax), 1 if by > ay else -1))
        if not hits:
            continue
        hits.sort()
        wind, row = 0, cov[yi]
        for k in range(len(hits) - 1):
            wind += hits[k][1]
            if wind != 0:
                for xi in range(max(0, int(hits[k][0] + 0.5)),
                                min(sw, int(hits[k + 1][0] + 0.5))):
                    row[xi] = 1

    # 16 sampel -> 4 bitar. min(15, s): att sla ihop 15 och 16 ar osynligt.
    alpha = [[min(15, sum(cov[y * SS + dy][x * SS + dx]
                          for dy in range(SS) for dx in range(SS)))
              for x in range(w)] for y in range(h)]
    return alpha, w, h


# --------------------------------------------------------------------- utdata

def preview(alpha):
    ramp = " .:-=+*#%@"
    for row in alpha:
        print("  " + "".join(ramp[min(9, v * 10 // 16)] for v in row))


def pack(alpha, w, h):
    """Tva pixlar per byte, hog nibble = jamnt x."""
    out = []
    for y in range(h):
        for x in range(0, w, 2):
            hi = alpha[y][x]
            lo = alpha[y][x + 1] if x + 1 < w else 0
            out.append((hi << 4) | lo)
    return out


def minify_svg(path):
    """Bantar SVG:en for webben: XML-deklaration, DOCTYPE och metadata bort,
    radbrytningar i bandata ihopslagna, fyllfargen utbytt."""
    import re as _re
    s = open(path, encoding="utf-8").read()

    s = _re.sub(r"<\?xml[^>]*\?>", "", s)
    s = _re.sub(r"<!DOCTYPE[^>]*>", "", s, flags=_re.S)
    s = _re.sub(r"<metadata>.*?</metadata>", "", s, flags=_re.S)
    s = _re.sub(r"<!--.*?-->", "", s, flags=_re.S)

    # Bredd/hojd i pt bort: CSS ska bestamma storleken, viewBox racker.
    s = _re.sub(r'\s(?:width|height)="[^"]*pt"', "", s)

    s = s.replace('fill="#000000"', 'fill="%s"' % WEB_FILL)

    s = _re.sub(r"\s+", " ", s)
    s = _re.sub(r">\s+<", "><", s).strip()
    return s


def main():
    polys = load_polygons(SVG_IN)
    alpha, w, h = rasterise(polys)
    data = pack(alpha, w, h)

    print("delbanor: %d | punkter: %d" % (len(polys), sum(len(s) for s in polys)))
    print("storlek: %dx%d px | packad: %d byte\n" % (w, h, len(data)))
    preview(alpha)

    lines = [
        "// GENERERAD FIL - andra tools/gen_logo.py, inte den har filen.",
        "// MB Labs-logotypen rastrerad ur MB_Labs.svg till 4 bitars alfa,",
        "// tva pixlar per byte (hog nibble = jamnt x).",
        "",
        '#include "ui_logo.h"',
        "",
        "const uint8_t ui_logo[UI_LOGO_BYTES] = {",
    ]
    per_row = (w + 1) // 2
    for y in range(h):
        row = data[y * per_row:(y + 1) * per_row]
        lines.append("  " + ", ".join("0x%02X" % b for b in row) + ",")
    lines += ["};", ""]

    open(CPP_OUT, "w", newline="\r\n").write("\n".join(lines))
    print("\nskrev %s  (%d byte)" % (CPP_OUT, len(data)))
    print("satt UI_LOGO_W=%d, UI_LOGO_H=%d i ui_logo.h" % (w, h))

    # --- samma logotyp, men for webbportalen ---
    svg = minify_svg(SVG_IN)
    raw = len(open(SVG_IN, encoding="utf-8").read())
    assert ')SVG"' not in svg, "SVG:en innehaller raw-strangens avslutare"

    web = [
        "#pragma once",
        "#include <Arduino.h>   // PROGMEM",
        "",
        "// GENERERAD FIL - andra tools/gen_logo.py, inte den har filen.",
        "// MB Labs-logotypen minifierad for webbportalen, serverad pa /logo.svg.",
        "// Fyllfargen ar utbytt mot sidans forgrundsfarg: filen ar svart, och en",
        "// <img> kan inte fargas om med CSS.",
        "",
        'static const char PORTAL_LOGO_SVG[] PROGMEM = R"SVG(%s)SVG";' % svg,
        "",
    ]
    open(WEB_OUT, "w", newline="\r\n").write("\n".join(web))
    print("skrev %s  (%d -> %d byte, -%d%%)"
          % (WEB_OUT, raw, len(svg), 100 - 100 * len(svg) // raw))


if __name__ == "__main__":
    main()
