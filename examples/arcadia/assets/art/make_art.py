#!/usr/bin/env python3
"""Draws Arcadia's invented art: for each game a cover (2:3), a wide hero and three screenshots with the game's HUD,
and the friends' avatars. Everything is flat SVG (paths, rectangles and circles with solid fills, strokes and
opacity), the part of SVG AffrescoUI draws: a sky's gradient is a stack of bands. Each drawing is seeded by its
name, so a run always gives the same files.

    python3 examples/arcadia/assets/art/make_art.py     # rewrites examples/arcadia/assets/art/<game>/ and art/avatars/
"""

import math
import os
import random
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))

COVER = (200, 300)  # drawn at twice these sizes
HERO = (960, 310)
SHOT = (640, 360)
AVATAR = (64, 64)


def num(v):
    """A coordinate, rounded to a tenth without a trailing zero."""
    text = "%.1f" % v
    return text[:-2] if text.endswith(".0") else text


def rgb(c):
    """A colour's three channels."""
    return [int(c[i:i + 2], 16) for i in (1, 3, 5)]


def mix(a, b, t):
    """The colour a fraction t of the way from a to b."""
    return "#%02x%02x%02x" % tuple(max(0, min(255, round(x + (y - x) * t))) for x, y in zip(rgb(a), rgb(b)))


def wave(rng, octaves=3, freq=1.0):
    """A smooth random function on [0, 1] with values in [-1, 1]."""
    terms = [(freq * rng.uniform(0.7, 1.3) * 2**o, rng.uniform(0, 6.283), 0.55**o) for o in range(octaves)]
    total = sum(a for _, _, a in terms)
    return lambda x: sum(a * math.sin(f * x * 6.283 + p) for f, p, a in terms) / total


def circle_d(cx, cy, r):
    """A circle as path data, clockwise like the polygons, to join many in one path."""
    return "M%s %sa%s %s 0 1 1 %s 0a%s %s 0 1 1 %s 0" % (num(cx - r), num(cy), num(r), num(r), num(2 * r), num(r),
                                                        num(r), num(-2 * r))


def poly_d(points, close=True):
    """A polygon (or a polyline) as path data."""
    head, *rest = points
    d = "M%s %s" % (num(head[0]), num(head[1])) + "".join("L%s %s" % (num(x), num(y)) for x, y in rest)
    return d + ("Z" if close else "")


class Svg:
    """An SVG drawing being built, element by element."""

    def __init__(self, w, h):
        self.w, self.h, self.parts = w, h, []

    def add(self, tag, **attrs):
        """Adds an element; attribute names use _ for -."""
        text = " ".join('%s="%s"' % (k.rstrip("_").replace("_", "-"), num(v) if isinstance(v, float) else v)
                        for k, v in attrs.items() if v is not None)
        self.parts.append("<%s %s/>" % (tag, text))

    def rect(self, x, y, w, h, fill, opacity=None, rx=None):
        """A filled rectangle."""
        self.add("rect", x=float(x), y=float(y), width=float(w), height=float(h), rx=None if rx is None else float(rx),
                 fill=fill, opacity=None if opacity is None else "%.2f" % opacity)

    def circle(self, cx, cy, r, fill, opacity=None):
        """A filled circle."""
        self.add("circle", cx=float(cx), cy=float(cy), r=float(r), fill=fill,
                 opacity=None if opacity is None else "%.2f" % opacity)

    def path(self, d, fill="none", opacity=None, stroke=None, width=None, cap=None, dash=None, transform=None,
             rule=None):
        """A path, filled or stroked."""
        if d:
            self.add("path", d=d, fill=fill, fill_rule=rule, stroke=stroke,
                     stroke_width=None if width is None else float(width), stroke_linecap=cap,
                     stroke_linejoin="round" if stroke else None, stroke_dasharray=dash, transform=transform,
                     opacity=None if opacity is None else "%.2f" % opacity)

    def text(self):
        """The finished document."""
        return ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">' %
                (self.w, self.h, self.w, self.h) + "".join(self.parts) + "</svg>\n")


# Scenery, drawn back to front. Positions are in the drawing's pixels; sizes follow its height u = h / 360.


def sky(s, top, bottom, y0=0.0, y1=None, bands=40):
    """A vertical gradient as bands."""
    y1 = s.h if y1 is None else y1
    step = (y1 - y0) / bands
    for i in range(bands):
        s.rect(0, y0 + i * step, s.w, step + 1, mix(top, bottom, i / (bands - 1)))


def glow(s, cx, cy, r, color, rings=6, strength=0.1):
    """A soft halo: wider rings, fainter."""
    for k in range(rings, 0, -1):
        s.circle(cx, cy, r * (1 + 0.45 * k), color, strength)


def stars(s, rng, n, color, y1=None):
    """Scattered stars over the top of the drawing."""
    y1 = s.h * 0.6 if y1 is None else y1
    big = "".join(circle_d(rng.uniform(0, s.w), rng.uniform(0, y1), rng.uniform(0.5, 1.4) * s.h / 360) for _ in range(n))
    dim = "".join(circle_d(rng.uniform(0, s.w), rng.uniform(0, y1), rng.uniform(0.4, 0.9) * s.h / 360) for _ in range(n))
    s.path(big, color, 0.9)
    s.path(dim, color, 0.45)


def ridge_points(s, rng, base, amp, freq=1.0, sharp=True, steps=48):
    """The top edge of a range of hills or mountains."""
    f = wave(rng, 3, freq)
    points = []
    for i in range(steps + 1):
        x = i / steps
        v = f(x)
        v = (1 - abs(v)) ** 1.6 if sharp else 0.5 + 0.5 * v
        points.append((x * s.w, base - amp * v))
    return points


def ridge(s, rng, base, amp, color, freq=1.0, sharp=True, opacity=None, caps=None):
    """A range filled to the bottom; caps is (line, colour) for snow above that height."""
    top = ridge_points(s, rng, base, amp, freq, sharp)
    s.path(poly_d(top + [(s.w, s.h), (0, s.h)]), color, opacity)
    if caps:
        line, cap_color = caps
        jag = wave(rng, 2, 6)
        bottom = [(x, line + s.h * 0.02 * jag(x / s.w)) for x, _ in top]
        upper = [(x, min(y, b)) for (x, y), (_, b) in zip(top, bottom)]
        s.path(poly_d(upper + bottom[::-1]), cap_color, 0.9)
    return top


def height_at(points, x):
    """The height of a ridge at x."""
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        if x0 <= x <= x1:
            return y0 + (y1 - y0) * (x - x0) / max(x1 - x0, 1e-6)
    return points[-1][1]


def hills(s, rng, base, amp, color, n=3, opacity=None):
    """Rounded hills filled to the bottom."""
    d = "M0 %s" % num(s.h)
    x = -s.w * 0.1
    d += "L%s %s" % (num(x), num(base))
    while x < s.w * 1.1:
        width = s.w / n * rng.uniform(0.7, 1.3)
        d += "Q%s %s %s %s" % (num(x + width / 2), num(base - amp * rng.uniform(0.6, 1.4)), num(x + width), num(base))
        x += width
    s.path(d + "L%s %sZ" % (num(s.w * 1.1), num(s.h)), color, opacity)


def sea(s, rng, y, color, light, rows=9):
    """Water from y down, with highlight strokes closer together near the horizon."""
    s.rect(0, y, s.w, s.h - y, color)
    u = s.h / 360
    d = ""
    for r in range(rows):
        t = (r + 1) / rows
        yy = y + (s.h - y) * t**1.7
        for _ in range(int(3 + 5 * t)):
            x = rng.uniform(0, s.w)
            length = (10 + 40 * t) * u * rng.uniform(0.5, 1.2)
            d += "M%s %sh%s" % (num(x), num(yy), num(length))
    s.path(d, stroke=light, width=1.2 * u, opacity=0.35, cap="round")


def clouds(s, rng, n, y0, y1, color, opacity=0.8, size=1.0):
    """Puffy clouds made of circles."""
    u = s.h / 360 * size
    d = ""
    for _ in range(n):
        cx, cy, w = rng.uniform(0, s.w), rng.uniform(y0, y1), rng.uniform(40, 90) * u
        for k in range(5):
            d += circle_d(cx - w / 2 + w * k / 4, cy - math.sin(k / 4 * math.pi) * w * 0.18, w * rng.uniform(0.16, 0.24))
        d += poly_d([(cx - w / 2, cy), (cx + w / 2, cy), (cx + w / 2, cy + w * 0.12), (cx - w / 2, cy + w * 0.12)])
    s.path(d, color, opacity)


def pines(s, rng, line, n, h0, h1, color, x0=None, x1=None):
    """Pine trees standing on a ridge (a list of points) or a height."""
    x0 = -10 if x0 is None else x0
    x1 = s.w + 10 if x1 is None else x1
    d = ""
    for _ in range(n):
        x = rng.uniform(x0, x1)
        base = height_at(line, x) + 4 if isinstance(line, list) else line
        h = rng.uniform(h0, h1)
        for tier in range(3):
            top = base - h * (1 - tier * 0.22)
            width = h * (0.18 + tier * 0.07)
            bottom = base - h * (0.35 - tier * 0.15)
            d += poly_d([(x, top), (x + width, bottom), (x - width, bottom)])
        d += poly_d([(x - h * 0.03, base - h * 0.2), (x + h * 0.03, base - h * 0.2), (x + h * 0.03, base),
                     (x - h * 0.03, base)])
    s.path(d, color)


def round_trees(s, rng, base, n, r0, r1, color, trunk):
    """Round trees with trunks."""
    leaves, trunks = "", ""
    for _ in range(n):
        x, r = rng.uniform(-10, s.w + 10), rng.uniform(r0, r1)
        top = base - r * 2.2
        trunks += poly_d([(x - r * 0.12, top + r), (x + r * 0.12, top + r), (x + r * 0.15, base), (x - r * 0.15, base)])
        leaves += circle_d(x, top, r) + circle_d(x - r * 0.6, top + r * 0.35, r * 0.7) + circle_d(x + r * 0.6, top + r * 0.3,
                                                                                              r * 0.75)
    s.path(trunks, trunk)
    s.path(leaves, color)


def figure_d(x, base, h, hood=True, cape=1.0):
    """A standing hero seen from behind, cloaked."""
    head = base - h * 0.86
    d = circle_d(x, head, h * 0.075)
    if hood:
        d += poly_d([(x - h * 0.08, head + h * 0.02), (x, head - h * 0.12), (x + h * 0.08, head + h * 0.02)])
    d += poly_d([(x - h * 0.1, base - h * 0.78), (x + h * 0.1, base - h * 0.78), (x + h * 0.13 * cape + h * 0.05,
                                                                                  base - h * 0.22),
                 (x + h * 0.02, base - h * 0.27), (x - h * 0.14 * cape - h * 0.04, base - h * 0.2)])
    d += poly_d([(x - h * 0.07, base - h * 0.32), (x - h * 0.02, base - h * 0.32), (x - h * 0.03, base),
                 (x - h * 0.08, base)])
    d += poly_d([(x + h * 0.02, base - h * 0.32), (x + h * 0.07, base - h * 0.32), (x + h * 0.08, base),
                 (x + h * 0.03, base)])
    return d


def figure(s, x, base, h, color, rim, item=None, item_color=None, hood=True):
    """A hero, rim-lit from one side, holding a sword, a staff, a lantern or a rifle."""
    body = figure_d(x, base, h, hood)
    s.path(body, rim, 0.85, transform="translate(%s 0)" % num(-h * 0.012))
    s.path(body, color)
    hand = (x + h * 0.15, base - h * 0.5)
    if item == "sword":
        s.path(poly_d([(hand[0] - h * 0.01, hand[1]), (hand[0] + h * 0.01, hand[1]), (hand[0] + h * 0.06, base - h * 1.12)]),
               item_color)
        s.path(poly_d([(hand[0] - h * 0.04, hand[1] - h * 0.01), (hand[0] + h * 0.05, hand[1] - h * 0.03)], False),
               stroke=item_color, width=h * 0.015)
    elif item == "staff":
        s.path(poly_d([(hand[0], base), (hand[0] + h * 0.03, base - h * 1.05)], False), stroke=color, width=h * 0.02)
        glow(s, hand[0] + h * 0.03, base - h * 1.08, h * 0.035, item_color, 4, 0.14)
        s.circle(hand[0] + h * 0.03, base - h * 1.08, h * 0.035, item_color)
    elif item == "lantern":
        glow(s, hand[0], hand[1] + h * 0.1, h * 0.04, item_color, 5, 0.12)
        s.rect(hand[0] - h * 0.025, hand[1] + h * 0.06, h * 0.05, h * 0.07, item_color, rx=h * 0.01)
    elif item == "rifle":
        s.path(poly_d([(x - h * 0.05, base - h * 0.58), (x + h * 0.32, base - h * 0.66)], False), stroke=color,
               width=h * 0.035, cap="round")
        s.circle(x + h * 0.33, base - h * 0.66, h * 0.012, item_color)


def castle(s, x, base, k, color, light):
    """A keep with towers, crenellations and lit windows; k is its height."""
    d, windows = "", ""
    for dx, w, h in ((-0.55, 0.16, 0.75), (-0.3, 0.5, 0.5), (0.0, 0.22, 1.0), (0.25, 0.5, 0.42), (0.5, 0.15, 0.68)):
        x0, top = x + dx * k, base - h * k
        d += poly_d([(x0 - w * k / 2, base), (x0 - w * k / 2, top), (x0 + w * k / 2, top), (x0 + w * k / 2, base)])
        for c in range(4):
            cx = x0 - w * k / 2 + (c + 0.5) * w * k / 4
            d += poly_d([(cx - w * k / 16, top), (cx - w * k / 16, top - k * 0.04), (cx + w * k / 16, top - k * 0.04),
                         (cx + w * k / 16, top)])
        if w < 0.3:
            d += poly_d([(x0 - w * k * 0.62, top - k * 0.03), (x0, top - k * 0.22), (x0 + w * k * 0.62, top - k * 0.03)])
            windows += poly_d([(x0 - k * 0.012, top + k * 0.12), (x0 + k * 0.012, top + k * 0.12),
                               (x0 + k * 0.012, top + k * 0.18), (x0 - k * 0.012, top + k * 0.18)])
    s.path(d, color)
    s.path(windows, light)


def skyline(s, rng, base, h0, h1, color, light, lit=0.3):
    """City blocks with a grid of windows, some lit."""
    u = s.h / 360
    d, windows = "", ""
    x = -5.0
    while x < s.w:
        w, h = rng.uniform(18, 46) * u, rng.uniform(h0, h1)
        d += poly_d([(x, base), (x, base - h), (x + w, base - h), (x + w, base)])
        for wy in range(int(h / (7 * u)) - 1):
            for wx in range(int(w / (6 * u))):
                if rng.random() < lit:
                    px, py = x + 2 * u + wx * 6 * u, base - h + 4 * u + wy * 7 * u
                    windows += "M%s %sh%sv%sh%sZ" % (num(px), num(py), num(3 * u), num(3.5 * u), num(-3 * u))
        x += w + rng.uniform(0, 6) * u
    s.path(d, color)
    s.path(windows, light, 0.9)


def grid_floor(s, horizon, color, rows=10, cols=18):
    """A perspective grid from the horizon to the bottom, to a vanishing point in the middle."""
    u = s.h / 360
    d = ""
    for c in range(-cols, cols + 1):
        d += "M%s %sL%s %s" % (num(s.w / 2 + c * s.w * 0.004), num(horizon), num(s.w / 2 + c * s.w * 0.11), num(s.h))
    for r in range(1, rows + 1):
        y = horizon + (s.h - horizon) * (r / rows)**2
        d += "M0 %sH%s" % (num(y), num(s.w))
    s.path(d, stroke=color, width=1.1 * u, opacity=0.75)


def gear_d(cx, cy, r, teeth):
    """A gear with a hole (fill it even-odd)."""
    points = []
    for i in range(teeth * 4):
        a = i / (teeth * 4) * 6.283
        rr = r if (i // 2) % 2 == 0 else r * 0.82
        points.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    return poly_d(points) + circle_d(cx, cy, r * 0.35)


def hexagon(cx, cy, r):
    """A hexagon with a flat top."""
    return [(cx + r * math.cos(a * 1.0472), cy + r * math.sin(a * 1.0472)) for a in range(6)]


def blob_d(rng, cx, cy, r, bumps=10):
    """A rounded irregular shape, for islands and bushes."""
    f = wave(rng, 2, 2)
    return poly_d([(cx + r * (1 + 0.25 * f(i / 36)) * math.cos(i / 36 * 6.283),
                    cy + r * 0.8 * (1 + 0.25 * f(i / 36)) * math.sin(i / 36 * 6.283)) for i in range(36)])


def shade_bottom(s, fraction, color, strength=0.8, bands=10):
    """Darkens the bottom of the drawing, for a title to stand on."""
    y0 = s.h * (1 - fraction)
    step = (s.h - y0) / bands
    alpha = 1 - (1 - strength * 0.35) ** (1 / bands)
    for i in range(bands):  # nested, so the steps add up without seams
        s.rect(0, y0 + i * step, s.w, s.h - y0 - i * step, color, round(alpha, 3))


def shade_left(s, fraction, color, strength=0.7, bands=10):
    """Darkens the left of the drawing."""
    step = s.w * fraction / bands
    alpha = 1 - (1 - strength * 0.3) ** (1 / bands)
    for i in range(bands):  # nested, so the steps add up without seams
        s.rect(0, 0, (bands - i) * step, s.h, color, round(alpha, 3))


# One function per game: the world, with the hero at fx (a fraction of the width) and size k (of the height).


def starfall(s, rng, p, fx, k):
    sky(s, "#0b1030", "#4a2f6e")
    stars(s, rng, 70, "#fff3d6")
    px, py = s.w * (0.25 if fx > 0.5 else 0.72), s.h * 0.3
    glow(s, px, py, s.h * 0.16, p["accent"], 5, 0.07)
    s.circle(px, py, s.h * 0.16, p["accent"])
    s.circle(px + s.h * 0.04, py - s.h * 0.03, s.h * 0.12, "#f7d98a", 0.6)
    s.path("M%s %sA%s %s 0 0 0 %s %s" % (num(px - s.h * 0.3), num(py + s.h * 0.02), num(s.h * 0.3), num(s.h * 0.06),
                                          num(px + s.h * 0.3), num(py + s.h * 0.02)), stroke="#fde7b0",
           width=s.h * 0.012, opacity=0.8)
    ridge(s, rng, s.h * 0.72, s.h * 0.28, "#3b2a66", 1.4)
    ridge(s, rng, s.h * 0.82, s.h * 0.2, "#261a4a", 2.0)
    near = ridge(s, rng, s.h * 0.95, s.h * 0.12, "#120c26", 1.0, False)
    figure(s, s.w * fx, height_at(near, s.w * fx) + 2, s.h * k, "#0a0716", p["accent"], "staff", "#9fe8ff")


def ember(s, rng, p, fx, k):
    sky(s, "#1a0604", "#b8401a")
    vx = s.w * (0.3 if fx > 0.5 else 0.7)
    glow(s, vx, s.h * 0.45, s.h * 0.1, "#ff8a3d", 5, 0.08)
    s.path(poly_d([(vx - s.h * 0.7, s.h * 0.85), (vx - s.h * 0.08, s.h * 0.42), (vx + s.h * 0.08, s.h * 0.42),
                   (vx + s.h * 0.7, s.h * 0.85)]), "#3a1410")
    s.path(poly_d([(vx - s.h * 0.02, s.h * 0.42), (vx + s.h * 0.04, s.h * 0.42), (vx + s.h * 0.12, s.h * 0.7),
                   (vx - s.h * 0.05, s.h * 0.85), (vx - s.h * 0.06, s.h * 0.6)]), "#ff6a1f", 0.85)
    d = "".join(circle_d(rng.uniform(0, s.w), rng.uniform(0, s.h * 0.8), rng.uniform(0.6, 2.2) * s.h / 360)
                for _ in range(60))
    s.path(d, "#ffb35c", 0.85)
    ridge(s, rng, s.h * 0.86, s.h * 0.16, "#240a07", 1.6)
    near = ridge(s, rng, s.h * 0.97, s.h * 0.1, "#0e0403", 1.0, False)
    figure(s, s.w * fx, height_at(near, s.w * fx) + 2, s.h * k, "#0b0302", "#ff7a3d", "sword", "#ffd0a0", False)


def tidebreaker(s, rng, p, fx, k):
    sky(s, "#0d2a3f", "#e7a86b", 0, s.h * 0.62)
    sx = s.w * (0.35 if fx > 0.5 else 0.65)
    glow(s, sx, s.h * 0.58, s.h * 0.07, "#ffd59a", 5, 0.1)
    s.circle(sx, s.h * 0.58, s.h * 0.07, "#ffe2b0")
    clouds(s, rng, 5, s.h * 0.1, s.h * 0.4, "#f4c79a", 0.35)
    sea(s, rng, s.h * 0.6, "#0e3b4f", "#9fe3f0")
    lx = s.w * (0.12 if fx > 0.5 else 0.88)
    s.path(blob_d(rng, lx, s.h * 0.7, s.h * 0.16), "#0a1f29")
    s.path(poly_d([(lx - s.h * 0.035, s.h * 0.62), (lx + s.h * 0.035, s.h * 0.62), (lx + s.h * 0.02, s.h * 0.3),
                   (lx - s.h * 0.02, s.h * 0.3)]), "#e9eef0")
    s.path(poly_d([(lx - s.h * 0.03, s.h * 0.5), (lx + s.h * 0.03, s.h * 0.5), (lx + s.h * 0.027, s.h * 0.44),
                   (lx - s.h * 0.027, s.h * 0.44)]), "#c0392b")
    s.path(poly_d([(lx, s.h * 0.27), (lx + s.w * 0.4 * (1 if fx > 0.5 else -1), s.h * 0.2),
                   (lx + s.w * 0.4 * (1 if fx > 0.5 else -1), s.h * 0.36)]), "#fff2c4", 0.18)
    s.circle(lx, s.h * 0.28, s.h * 0.025, "#fff2c4")
    bx, by, b = s.w * fx, s.h * 0.78, s.h * k
    s.path(poly_d([(bx - b * 0.5, by), (bx + b * 0.55, by), (bx + b * 0.35, by + b * 0.16), (bx - b * 0.38, by + b * 0.16)]),
           "#2a1a12")
    s.path(poly_d([(bx, by), (bx, by - b * 0.95)], False), stroke="#2a1a12", width=b * 0.03)
    s.path(poly_d([(bx + b * 0.02, by - b * 0.9), (bx + b * 0.45, by - b * 0.12), (bx + b * 0.02, by - b * 0.08)]),
           "#f1e6cf")
    s.path(poly_d([(bx - b * 0.02, by - b * 0.75), (bx - b * 0.36, by - b * 0.12), (bx - b * 0.02, by - b * 0.1)]),
           "#d9cdb4")
    s.path(poly_d([(bx, by - b * 0.95), (bx + b * 0.16, by - b * 0.9), (bx, by - b * 0.85)]), p["accent"])


def neon(s, rng, p, fx, k):
    horizon = s.h * 0.58
    sky(s, "#12062e", "#7a1f6e", 0, horizon)
    sx, r = s.w * 0.5, s.h * 0.22
    glow(s, sx, horizon - r * 0.35, r, "#ff3ea5", 4, 0.07)
    s.circle(sx, horizon - r * 0.35, r, "#ffb03b")
    s.path(poly_d([(sx - r, horizon - r * 0.35), (sx + r, horizon - r * 0.35), (sx + r, horizon), (sx - r, horizon)]),
           "#ff3ea5", 0.9)
    for i in range(5):
        y = horizon - r * 0.3 + i * r * 0.07
        s.rect(sx - r * 1.1, y, r * 2.2, r * (0.012 + i * 0.012), mix("#4a1360", "#7a1f6e", i / 4))
    skyline(s, rng, horizon, s.h * 0.05, s.h * 0.26, "#1b0b35", "#5ad1e6", 0.22)
    s.rect(0, horizon, s.w, s.h - horizon, "#0d0420")
    grid_floor(s, horizon, "#ff3ea5")
    cx, cy, c = s.w * fx, s.h * 0.92, s.h * k * 0.9
    s.path(poly_d([(cx - c * 0.55, cy), (cx - c * 0.5, cy - c * 0.16), (cx - c * 0.2, cy - c * 0.3), (cx + c * 0.25,
                                                                                                    cy - c * 0.3),
                   (cx + c * 0.52, cy - c * 0.14), (cx + c * 0.55, cy)]), "#0a0218")
    s.path(poly_d([(cx - c * 0.17, cy - c * 0.27), (cx + c * 0.22, cy - c * 0.27), (cx + c * 0.36, cy - c * 0.16),
                   (cx - c * 0.32, cy - c * 0.16)]), "#5ad1e6", 0.35)
    s.rect(cx - c * 0.55, cy - c * 0.12, c * 0.16, c * 0.04, "#ff3ea5")
    s.rect(cx + c * 0.39, cy - c * 0.12, c * 0.16, c * 0.04, "#ff3ea5")
    s.path(circle_d(cx - c * 0.33, cy, c * 0.09) + circle_d(cx + c * 0.33, cy, c * 0.09), "#000000")


def mossheart(s, rng, p, fx, k):
    sky(s, "#9fd8c4", "#e9f5d0")
    glow(s, s.w * 0.8, s.h * 0.2, s.h * 0.06, "#fff7c2", 5, 0.12)
    hills(s, rng, s.h * 0.62, s.h * 0.12, "#7fbf72", 3)
    round_trees(s, rng, s.h * 0.66, 9, s.h * 0.05, s.h * 0.08, "#4f9a55", "#3b5d33")
    hills(s, rng, s.h * 0.78, s.h * 0.1, "#3f7d3f", 4)
    round_trees(s, rng, s.h * 0.84, 6, s.h * 0.08, s.h * 0.12, "#2e6b35", "#2a3f22")
    s.rect(0, s.h * 0.86, s.w, s.h * 0.14, "#1f4a26")
    d = "".join(circle_d(rng.uniform(0, s.w), rng.uniform(s.h * 0.4, s.h * 0.85), s.h * 0.005) for _ in range(30))
    s.path(d, "#fdf6a3", 0.9)
    x, base, c = s.w * fx, s.h * 0.9, s.h * k * 0.55
    s.path(circle_d(x, base - c * 0.45, c * 0.45), "#c98a4b")
    s.path(poly_d([(x - c * 0.38, base - c * 0.7), (x - c * 0.3, base - c * 1.05), (x - c * 0.1, base - c * 0.85)]) +
           poly_d([(x + c * 0.38, base - c * 0.7), (x + c * 0.3, base - c * 1.05), (x + c * 0.1, base - c * 0.85)]),
           "#c98a4b")
    s.path(circle_d(x - c * 0.15, base - c * 0.55, c * 0.08) + circle_d(x + c * 0.15, base - c * 0.55, c * 0.08),
           "#1a1a1a")
    s.path(circle_d(x, base - c * 0.3, c * 0.22), "#f1d6ad")
    s.path(poly_d([(x - c * 0.1, base - c * 1.0), (x + c * 0.1, base - c * 1.0), (x, base - c * 1.25)]), p["accent"])
    for i in range(7):
        mx, my = rng.uniform(0, s.w), s.h * rng.uniform(0.9, 0.98)
        m = s.h * 0.03
        s.rect(mx - m * 0.15, my - m * 0.6, m * 0.3, m * 0.6, "#f3e8d4")
        s.path("M%s %sQ%s %s %s %sZ" % (num(mx - m * 0.6), num(my - m * 0.55), num(mx), num(my - m * 1.5),
                                        num(mx + m * 0.6), num(my - m * 0.55)), "#d9483b")


def ironhollow(s, rng, p, fx, k):
    sky(s, "#16120f", "#6b5843")
    clouds(s, rng, 7, s.h * 0.05, s.h * 0.35, "#2b2420", 0.7, 1.4)
    ridge(s, rng, s.h * 0.7, s.h * 0.22, "#3a2f27", 1.2)
    cx = s.w * (0.3 if fx > 0.5 else 0.7)
    s.path(poly_d([(cx - s.h * 0.5, s.h), (cx - s.h * 0.3, s.h * 0.62), (cx + s.h * 0.3, s.h * 0.6), (cx + s.h * 0.5,
                                                                                                     s.h)]), "#241c17")
    castle(s, cx, s.h * 0.62, s.h * 0.34, "#1a1411", p["accent"])
    d = ""
    for _ in range(9):
        bx, by, b = rng.uniform(0, s.w), rng.uniform(s.h * 0.1, s.h * 0.4), s.h * 0.012
        d += "M%s %sq%s %s %s %sq%s %s %s %s" % (num(bx - b), num(by), num(b * 0.5), num(-b * 0.7), num(b), num(0),
                                                 num(b * 0.5), num(-b * 0.7), num(b), num(0))
    s.path(d, stroke="#0d0a08", width=s.h * 0.005)
    near = ridge(s, rng, s.h * 0.97, s.h * 0.08, "#0f0c0a", 1.0, False)
    figure(s, s.w * fx, height_at(near, s.w * fx) + 2, s.h * k, "#0a0807", "#c9a36b", "sword", "#d9c3a0")
    bx = s.w * fx + s.h * k * 0.3
    s.path(poly_d([(bx, height_at(near, bx)), (bx, height_at(near, bx) - s.h * k * 1.1)], False), stroke="#0a0807",
           width=s.h * 0.008)
    s.path(poly_d([(bx, height_at(near, bx) - s.h * k * 1.1), (bx + s.h * k * 0.3, height_at(near, bx) - s.h * k * 1.0),
                   (bx, height_at(near, bx) - s.h * k * 0.85)]), "#8e2b22")


def skyward(s, rng, p, fx, k):
    sky(s, "#2a4a7a", "#f7c59b")
    clouds(s, rng, 6, s.h * 0.55, s.h * 0.9, "#fbe3c8", 0.6, 1.6)
    for i, (ix, iy, r) in enumerate(((0.2, 0.42, 0.16), (0.55, 0.3, 0.1), (0.82, 0.5, 0.2))):
        x, y, r = s.w * ix, s.h * iy, s.h * r
        s.path(poly_d([(x - r, y), (x + r, y), (x + r * 0.2, y + r * 1.3), (x - r * 0.3, y + r * 0.9)]), "#6b4b3a")
        s.path(poly_d([(x - r * 1.05, y + r * 0.08), (x - r * 1.05, y - r * 0.05), (x + r * 1.05, y - r * 0.05),
                       (x + r * 1.05, y + r * 0.08)]), "#7cc06a")
        round_trees(s, rng, y - r * 0.03, 3, r * 0.12, r * 0.18, "#4f9a55", "#4a3328") if r > s.h * 0.12 else None
        s.rect(x + r * 0.5, y + r * 0.05, r * 0.06, r * 1.4, "#cdeaf5", 0.7)
    clouds(s, rng, 4, s.h * 0.8, s.h * 1.0, "#fff1de", 0.9, 2.0)
    ax, ay, a = s.w * fx, s.h * 0.22, s.h * k * 0.8
    s.path(poly_d([(ax - a * 0.5, ay), (ax + a * 0.5, ay), (ax + a * 0.4, ay + a * 0.12), (ax - a * 0.4, ay + a * 0.12)]),
           "#5b3a2a")
    s.path("M%s %sa%s %s 0 1 1 %s 0Z" % (num(ax - a * 0.45), num(ay - a * 0.15), num(a * 0.45), num(a * 0.2), num(a * 0.9)),
           p["accent"])
    s.path(poly_d([(ax - a * 0.35, ay - a * 0.05), (ax - a * 0.35, ay), (ax + a * 0.35, ay), (ax + a * 0.35, ay - a * 0.05)]),
           "#5b3a2a")


def quietorbit(s, rng, p, fx, k):
    sky(s, "#03050c", "#121a36")
    stars(s, rng, 110, "#dfe6ff", s.h)
    px, r = s.w * 0.5, s.h * 1.2
    glow(s, px, s.h + r * 0.62, r, "#9aa8ff", 3, 0.06)
    s.circle(px, s.h + r * 0.62, r, "#1c2a5c")
    s.circle(px - r * 0.1, s.h + r * 0.68, r, "#25387a", 0.6)
    ox, oy, o = s.w * fx, s.h * 0.4, s.h * k * 0.9
    s.path("M%s %sa%s %s 0 1 0 %s 0a%s %s 0 1 0 %s 0" % (num(ox - o * 1.2), num(oy), num(o * 1.2), num(o * 0.3),
                                                         num(o * 2.4), num(o * 1.2), num(o * 0.3), num(-o * 2.4)),
           stroke="#9aa8ff", width=o * 0.02, opacity=0.6)
    s.rect(ox - o * 0.08, oy - o * 0.5, o * 0.16, o, "#c9d1ec")
    s.rect(ox - o * 0.3, oy - o * 0.06, o * 0.6, o * 0.12, "#8f9ac2")
    for side in (-1, 1):
        x0 = ox + side * o * 0.32
        s.rect(min(x0, x0 + side * o * 0.6), oy - o * 0.16, o * 0.6, o * 0.32, "#2f4fa8")
        d = "".join("M%s %sv%s" % (num(min(x0, x0 + side * o * 0.6) + o * 0.1 * j), num(oy - o * 0.16), num(o * 0.32))
                    for j in range(1, 6))
        s.path(d, stroke="#9aa8ff", width=o * 0.01, opacity=0.6)
    s.circle(ox, oy - o * 0.5, o * 0.06, p["accent"])
    glow(s, ox, oy - o * 0.5, o * 0.04, p["accent"], 3, 0.15)


def lumen(s, rng, p, fx, k):
    sky(s, "#0b4f5c", "#03141f")
    for _ in range(6):
        x = rng.uniform(0, s.w)
        s.path(poly_d([(x, 0), (x + s.h * 0.08, 0), (x + s.h * 0.4, s.h), (x + s.h * 0.15, s.h)]), "#bff7ff", 0.05)
    ridge(s, rng, s.h * 0.9, s.h * 0.25, "#062a33", 1.5)
    d = ""
    for _ in range(14):
        x, base, h = rng.uniform(0, s.w), s.h, s.h * rng.uniform(0.15, 0.4)
        sway = rng.uniform(-1, 1) * s.h * 0.04
        d += "M%s %sQ%s %s %s %s" % (num(x), num(base), num(x + sway), num(base - h * 0.5), num(x - sway * 0.5),
                                     num(base - h))
    s.path(d, stroke="#0f6b5c", width=s.h * 0.012, cap="round")
    for _ in range(5):
        jx, jy, r = rng.uniform(0, s.w), rng.uniform(s.h * 0.15, s.h * 0.6), s.h * rng.uniform(0.025, 0.05)
        color = rng.choice(("#7af0ff", "#ff9bf2", "#b6ff9b"))
        glow(s, jx, jy, r, color, 4, 0.08)
        s.path("M%s %sa%s %s 0 0 1 %s 0Z" % (num(jx - r), num(jy), num(r), num(r), num(2 * r)), color, 0.85)
        t = "".join("M%s %sq%s %s %s %s" % (num(jx - r * 0.6 + j * r * 0.4), num(jy), num(r * 0.2), num(r), num(0),
                                            num(r * 2)) for j in range(4))
        s.path(t, stroke=color, width=r * 0.08, opacity=0.6)
    x, y, c = s.w * fx, s.h * 0.55, s.h * k * 0.8
    glow(s, x + c * 0.3, y - c * 0.05, c * 0.06, p["accent"], 5, 0.1)
    s.path(poly_d([(x - c * 0.5, y + c * 0.05), (x + c * 0.25, y - c * 0.08), (x + c * 0.3, y + c * 0.02), (x - c * 0.45,
                                                                                                          y + c * 0.16)]),
           "#020a0f")
    s.path(circle_d(x + c * 0.3, y - c * 0.05, c * 0.08), "#020a0f")
    s.path(circle_d(x + c * 0.33, y - c * 0.06, c * 0.04), p["accent"])
    s.path(poly_d([(x - c * 0.5, y + c * 0.08), (x - c * 0.75, y), (x - c * 0.72, y + c * 0.12)]), "#020a0f")


def driftwood(s, rng, p, fx, k):
    sky(s, "#f59e7b", "#ffe3a8", 0, s.h * 0.6)
    glow(s, s.w * 0.5, s.h * 0.55, s.h * 0.1, "#fff0c0", 5, 0.12)
    s.circle(s.w * 0.5, s.h * 0.55, s.h * 0.1, "#fff3cf")
    clouds(s, rng, 5, s.h * 0.08, s.h * 0.35, "#ffd1b0", 0.6)
    sea(s, rng, s.h * 0.58, "#3f8fb0", "#e6fbff")
    s.path("M0 %sQ%s %s %s %sL%s %sL0 %sZ" % (num(s.h * 0.8), num(s.w * 0.5), num(s.h * 0.68), num(s.w), num(s.h * 0.78),
                                              num(s.w), num(s.h), num(s.h)), "#f1d7a5")
    s.path("M0 %sQ%s %s %s %s" % (num(s.h * 0.8), num(s.w * 0.5), num(s.h * 0.68), num(s.w), num(s.h * 0.78)),
           stroke="#ffffff", width=s.h * 0.008, opacity=0.7)
    x, base, c = s.w * fx, s.h * 0.82, s.h * k
    d = "".join("M%s %sv%s" % (num(x + c * dx), num(base - c * 0.35), num(c * 0.35)) for dx in (-0.4, -0.1, 0.2, 0.4))
    s.path(d, stroke="#6b4a32", width=c * 0.03)
    s.rect(x - c * 0.45, base - c * 0.75, c * 0.9, c * 0.42, "#a8754e")
    s.path(poly_d([(x - c * 0.55, base - c * 0.72), (x, base - c * 1.05), (x + c * 0.55, base - c * 0.72)]), "#7a3b2e")
    s.rect(x - c * 0.3, base - c * 0.62, c * 0.16, c * 0.14, "#ffe08a")
    s.rect(x + c * 0.08, base - c * 0.62, c * 0.18, c * 0.29, "#5a3a26")
    px = x + c * (0.9 if fx < 0.6 else -0.9)
    s.path("M%s %sQ%s %s %s %s" % (num(px), num(base + c * 0.05), num(px + c * 0.15), num(base - c * 0.5),
                                   num(px + c * 0.05), num(base - c * 1.1)), stroke="#6b4a32", width=c * 0.05, cap="round")
    d = ""
    for a in range(6):
        ang = a / 6 * 6.283
        tx, ty = px + c * 0.05, base - c * 1.1
        d += poly_d([(tx, ty), (tx + math.cos(ang) * c * 0.45, ty + math.sin(ang) * c * 0.2 + c * 0.1),
                     (tx + math.cos(ang + 0.3) * c * 0.3, ty + math.sin(ang + 0.3) * c * 0.15)])
    s.path(d, "#3f8a4a")


def frostline(s, rng, p, fx, k):
    sky(s, "#050f1f", "#2b5876")
    stars(s, rng, 50, "#e8f4ff", s.h * 0.5)
    for i, color in enumerate(("#5af2b5", "#7ad7ff", "#b38cff")):
        f = wave(rng, 2, 1.2)
        top = [(x * s.w / 40, s.h * (0.15 + 0.08 * i) + s.h * 0.08 * f(x / 40)) for x in range(41)]
        bottom = [(x, y + s.h * 0.12) for x, y in top]
        s.path(poly_d(top + bottom[::-1]), color, 0.16)
    ridge(s, rng, s.h * 0.7, s.h * 0.35, "#3b5f80", 1.3, caps=(s.h * 0.48, "#e9f3ff"))
    line = ridge(s, rng, s.h * 0.85, s.h * 0.12, "#e3eef8", 1.0, False)
    pines(s, rng, line, 18, s.h * 0.08, s.h * 0.16, "#0b2233")
    s.rect(0, s.h * 0.9, s.w, s.h * 0.1, "#f0f6fc")
    d = "".join(circle_d(rng.uniform(0, s.w), rng.uniform(0, s.h), s.h * rng.uniform(0.002, 0.005)) for _ in range(60))
    s.path(d, "#ffffff", 0.8)
    figure(s, s.w * fx, s.h * 0.93, s.h * k, "#06121c", "#7ad7ff", "lantern", "#ffd27a")


def hexfall(s, rng, p, fx, k):
    sky(s, "#140c2c", "#2e1a5e")
    r = s.h * 0.06
    d, lit = "", ""
    for row in range(int(s.h / (r * 1.73)) + 2):
        for col in range(int(s.w / (r * 1.5)) + 2):
            cx, cy = col * r * 1.5, row * r * 1.73 + (r * 0.87 if col % 2 else 0)
            d += poly_d(hexagon(cx, cy, r * 0.94))
            if rng.random() < 0.12:
                lit += poly_d(hexagon(cx, cy, r * 0.8))
    s.path(d, stroke="#5a3ea8", width=s.h * 0.003, opacity=0.6)
    s.path(lit, "#7c5cff", 0.25)
    for _ in range(7):
        cx, cy, c = rng.uniform(0, s.w), rng.uniform(0, s.h * 0.7), s.h * rng.uniform(0.02, 0.05)
        s.path(poly_d(hexagon(cx, cy, c)), rng.choice(("#ff5fa2", "#5af2e0", "#ffd166")), 0.8)
    x, y, c = s.w * fx, s.h * 0.55, s.h * k * 0.9
    glow(s, x, y, c * 0.3, "#5af2e0", 5, 0.08)
    s.path(poly_d([(x, y - c * 0.6), (x + c * 0.25, y - c * 0.1), (x, y + c * 0.5), (x - c * 0.25, y - c * 0.1)]),
           "#5af2e0")
    s.path(poly_d([(x, y - c * 0.6), (x + c * 0.25, y - c * 0.1), (x, y + c * 0.5)]), "#2bb8c9")
    s.path(poly_d([(x - c * 0.25, y - c * 0.1), (x + c * 0.25, y - c * 0.1)], False), stroke="#d8fffb", width=c * 0.01)
    for side in (-1, 1):
        sx = x + side * c * 0.5
        s.path(poly_d([(sx, y - c * 0.3), (sx + c * 0.12, y), (sx, y + c * 0.28), (sx - c * 0.12, y)]), "#ff5fa2", 0.9)


def dead_tree(d, x, y, length, angle, depth, rng):
    """A bare tree's branches as path data (recursive)."""
    if depth == 0:
        return d
    x2, y2 = x + length * math.cos(angle), y - length * math.sin(angle)
    d[depth] = d.get(depth, "") + "M%s %sL%s %s" % (num(x), num(y), num(x2), num(y2))
    for turn in (-0.45, 0.4):
        dead_tree(d, x2, y2, length * rng.uniform(0.6, 0.75), angle + turn + rng.uniform(-0.15, 0.15), depth - 1, rng)
    return d


def thornwick(s, rng, p, fx, k):
    sky(s, "#07070d", "#2a2238")
    mx = s.w * (0.25 if fx > 0.5 else 0.75)
    glow(s, mx, s.h * 0.25, s.h * 0.1, "#e6e2d3", 5, 0.05)
    s.circle(mx, s.h * 0.25, s.h * 0.1, "#e6e2d3")
    s.path(circle_d(mx - s.h * 0.03, s.h * 0.23, s.h * 0.02) + circle_d(mx + s.h * 0.035, s.h * 0.29, s.h * 0.014),
           "#c9c4b2")
    ridge(s, rng, s.h * 0.75, s.h * 0.1, "#15121e", 1.0, False)
    hx, base, c = s.w * (0.62 if fx < 0.5 else 0.4), s.h * 0.72, s.h * 0.36
    s.path(poly_d([(hx - c * 0.7, base), (hx - c * 0.7, base - c * 0.55), (hx - c * 0.45, base - c * 0.85),
                   (hx - c * 0.2, base - c * 0.55), (hx + c * 0.2, base - c * 0.55), (hx + c * 0.2, base - c * 1.1),
                   (hx + c * 0.32, base - c * 1.35), (hx + c * 0.44, base - c * 1.1), (hx + c * 0.44, base - c * 0.6),
                   (hx + c * 0.8, base - c * 0.6), (hx + c * 0.8, base)]), "#0b0910")
    s.rect(hx - c * 0.1, base - c * 0.85, c * 0.06, c * 0.25, "#0b0910")
    d = ""
    for wx, wy in ((-0.55, 0.35), (-0.35, 0.35), (0.28, 0.85), (0.55, 0.35), (0.0, 0.3), (0.28, 0.4)):
        if rng.random() < 0.75:
            d += "M%s %sh%sv%sh%sZ" % (num(hx + wx * c), num(base - wy * c - c * 0.1), num(c * 0.07), num(c * 0.1),
                                       num(-c * 0.07))
    s.path(d, "#ffcf6b")
    for _ in range(3):
        branches = dead_tree({}, rng.uniform(0, s.w), s.h, s.h * rng.uniform(0.12, 0.18), 1.57, 5, rng)
        for depth, data in branches.items():
            s.path(data, stroke="#050408", width=s.h * 0.003 * depth, cap="round")
    for i in range(3):
        s.add("ellipse", cx=float(s.w * rng.uniform(0.1, 0.9)), cy=float(s.h * (0.82 + i * 0.05)), rx=float(s.w * 0.4),
              ry=float(s.h * 0.04), fill="#8a7fa8", opacity="0.12")
    s.rect(0, s.h * 0.92, s.w, s.h * 0.08, "#050408")
    figure(s, s.w * fx, s.h * 0.94, s.h * k, "#030205", "#8a7fa8", "lantern", "#ffcf6b")


def pebble(s, rng, p, fx, k):
    sky(s, "#7cc8f5", "#e6f6ff")
    s.circle(s.w * 0.85, s.h * 0.18, s.h * 0.08, "#ffe066")
    clouds(s, rng, 6, s.h * 0.08, s.h * 0.4, "#ffffff", 0.95)
    hills(s, rng, s.h * 0.68, s.h * 0.12, "#a6e07a", 3)
    hills(s, rng, s.h * 0.82, s.h * 0.1, "#6cc04a", 4)
    d = ""
    for _ in range(30):
        fx2, fy = rng.uniform(0, s.w), rng.uniform(s.h * 0.84, s.h * 0.98)
        d += circle_d(fx2, fy, s.h * 0.008)
    s.path(d, "#ff7aa8")
    pines(s, rng, s.h * 0.8, 5, s.h * 0.14, s.h * 0.22, "#2f8f4e")
    x, base, c = s.w * fx, s.h * 0.92, s.h * k * 0.6
    s.add("ellipse", cx=float(x), cy=float(base - c * 0.38), rx=float(c * 0.5), ry=float(c * 0.4), fill="#9aa4b1")
    s.add("ellipse", cx=float(x - c * 0.12), cy=float(base - c * 0.5), rx=float(c * 0.22), ry=float(c * 0.15),
          fill="#b9c2cc")
    s.path(circle_d(x - c * 0.16, base - c * 0.45, c * 0.1) + circle_d(x + c * 0.16, base - c * 0.45, c * 0.1), "#ffffff")
    s.path(circle_d(x - c * 0.14, base - c * 0.44, c * 0.05) + circle_d(x + c * 0.18, base - c * 0.44, c * 0.05), "#1d2433")
    s.path(poly_d([(x - c * 0.3, base - c * 0.02), (x - c * 0.1, base - c * 0.02), (x - c * 0.1, base + c * 0.04),
                   (x - c * 0.3, base + c * 0.04)]) + poly_d([(x + c * 0.1, base - c * 0.02), (x + c * 0.3, base - c * 0.02),
                                                             (x + c * 0.3, base + c * 0.04), (x + c * 0.1, base + c * 0.04)]),
           "#ff8a3d")
    s.path(poly_d([(x + c * 0.05, base - c * 0.78), (x + c * 0.1, base - c * 1.0), (x + c * 0.2, base - c * 0.8)]),
           "#3fae5a")


def aurora(s, rng, p, fx, k):
    sky(s, "#100822", "#4b2a6e")
    stars(s, rng, 60, "#f5e9ff", s.h * 0.6)
    s.circle(s.w * 0.2, s.h * 0.2, s.h * 0.05, "#e6d8ff")
    s.circle(s.w * 0.3, s.h * 0.12, s.h * 0.025, "#ffb3e6")
    for i, color in enumerate(("#ff7ad9", "#b38cff")):
        f = wave(rng, 2, 1.5)
        top = [(x * s.w / 40, s.h * (0.1 + 0.1 * i) + s.h * 0.06 * f(x / 40)) for x in range(41)]
        s.path(poly_d(top + [(x, y + s.h * 0.18) for x, y in top][::-1]), color, 0.15)
    d = ""
    for _ in range(9):
        x, h, w = rng.uniform(0, s.w), s.h * rng.uniform(0.2, 0.5), s.h * rng.uniform(0.03, 0.07)
        d += poly_d([(x - w, s.h * 0.8), (x, s.h * 0.8 - h), (x + w, s.h * 0.8)])
    s.path(d, "#8d7bd6", 0.8)
    s.rect(0, s.h * 0.78, s.w, s.h * 0.22, "#d9d2f5")
    s.path("M0 %sL%s %sL%s %sL0 %sZ" % (num(s.h * 0.86), num(s.w), num(s.h * 0.82), num(s.w), num(s.h), num(s.h)),
           "#bfb4ea")
    x, y, c = s.w * fx, s.h * 0.82, s.h * k * 0.8
    s.path(poly_d([(x - c * 0.6, y), (x - c * 0.4, y - c * 0.2), (x + c * 0.3, y - c * 0.24), (x + c * 0.6, y - c * 0.08),
                   (x + c * 0.5, y + c * 0.05)]), "#1d1235")
    s.path(poly_d([(x + c * 0.5, y - c * 0.04), (x + c * 1.6, y - c * 0.12), (x + c * 1.6, y + c * 0.02)]), "#ff7ad9", 0.35)
    figure(s, x - c * 0.05, y - c * 0.18, c * 0.7, "#1d1235", "#ff7ad9")


def clockwork(s, rng, p, fx, k):
    sky(s, "#2b1d0e", "#c08a48")
    for _ in range(4):
        cx, cy, r = rng.uniform(0, s.w), rng.uniform(0, s.h * 0.4), s.h * rng.uniform(0.08, 0.16)
        s.path(gear_d(cx, cy, r, 10), "#e2b26b", 0.18, rule="evenodd")
    skyline(s, rng, s.h * 0.68, s.h * 0.1, s.h * 0.3, "#3a2615", "#ffd58a", 0.25)
    d = ""
    for _ in range(6):
        x, y = rng.uniform(0, s.w), s.h * rng.uniform(0.3, 0.45)
        for j in range(4):
            d += circle_d(x + j * s.h * 0.02, y - j * s.h * 0.05, s.h * (0.02 + j * 0.01))
    s.path(d, "#d9c3a0", 0.25)
    sea(s, rng, s.h * 0.68, "#1d2b33", "#e2b26b", 6)
    gx, gy, r = s.w * fx, s.h * 0.55, s.h * k * 0.55
    s.path(gear_d(gx, gy, r, 12), "#a8743a", rule="evenodd")
    s.path(gear_d(gx + r * 1.15, gy + r * 0.5, r * 0.5, 8), "#7a5226", rule="evenodd")
    for _ in range(3):
        bx, by = rng.uniform(0, s.w), s.h * rng.uniform(0.75, 0.9)
        b = s.h * 0.05
        s.path(poly_d([(bx - b, by), (bx + b, by), (bx + b * 0.7, by + b * 0.3), (bx - b * 0.7, by + b * 0.3)]), "#2a1a0d")
        s.rect(bx - b * 0.2, by - b * 0.7, b * 0.15, b * 0.7, "#2a1a0d")


def saltmarsh(s, rng, p, fx, k):
    sky(s, "#d7dccf", "#f3e2c0", 0, s.h * 0.6)
    s.circle(s.w * 0.3, s.h * 0.45, s.h * 0.06, "#fbe8c2")
    for i in range(3):
        s.add("ellipse", cx=float(s.w * rng.uniform(0.2, 0.8)), cy=float(s.h * (0.5 + 0.04 * i)), rx=float(s.w * 0.5),
              ry=float(s.h * 0.03), fill="#ffffff", opacity="0.3")
    ridge(s, rng, s.h * 0.6, s.h * 0.04, "#9aa595", 1.0, False)
    sea(s, rng, s.h * 0.6, "#8fa6a3", "#f6f2e6", 7)
    d, heads = "", ""
    for _ in range(80):
        x, base = rng.uniform(0, s.w), s.h * rng.uniform(0.75, 1.0)
        h = s.h * rng.uniform(0.08, 0.2)
        lean = rng.uniform(-0.03, 0.03) * s.h
        d += "M%s %sQ%s %s %s %s" % (num(x), num(base), num(x + lean * 0.5), num(base - h * 0.5), num(x + lean),
                                     num(base - h))
        if rng.random() < 0.3:
            heads += "M%s %sv%s" % (num(x + lean), num(base - h), num(s.h * 0.03))
    s.path(d, stroke="#5e6b45", width=s.h * 0.004)
    s.path(heads, stroke="#6b4a2e", width=s.h * 0.01, cap="round")
    hx, hy, c = s.w * (0.8 if fx < 0.5 else 0.2), s.h * 0.68, s.h * 0.12
    s.add("ellipse", cx=float(hx), cy=float(hy - c * 0.6), rx=float(c * 0.3), ry=float(c * 0.14), fill="#3a3f45")
    s.path("M%s %sQ%s %s %s %s" % (num(hx + c * 0.25), num(hy - c * 0.65), num(hx + c * 0.45), num(hy - c * 1.1),
                                   num(hx + c * 0.3), num(hy - c * 1.2)), stroke="#3a3f45", width=c * 0.06)
    s.path("M%s %sl%s %s" % (num(hx), num(hy - c * 0.5), num(0), num(c * 0.5)) + "M%s %sl%s %s" % (
        num(hx + c * 0.06), num(hy - c * 0.5), num(c * 0.04), num(c * 0.5)), stroke="#3a3f45", width=c * 0.025)
    bx, by, b = s.w * fx, s.h * 0.7, s.h * k
    s.path("M%s %sQ%s %s %s %sZ" % (num(bx - b * 0.5), num(by), num(bx), num(by + b * 0.25), num(bx + b * 0.55), num(by)),
           "#7a4b2e")
    figure(s, bx, by + b * 0.02, b * 0.55, "#2c2a2a", "#f3e2c0", None, None)
    s.path("M%s %sQ%s %s %s %s" % (num(bx + b * 0.08), num(by - b * 0.3), num(bx + b * 0.6), num(by - b * 0.8),
                                   num(bx + b * 0.9), num(by - b * 0.2)), stroke="#2c2a2a", width=b * 0.01)


def glasswing(s, rng, p, fx, k):
    sky(s, "#0f2b4a", "#7fd3ec")
    clouds(s, rng, 6, s.h * 0.45, s.h * 0.8, "#e6f7ff", 0.5, 1.5)
    d = ""
    for _ in range(7):
        x, h, w = rng.uniform(0, s.w), s.h * rng.uniform(0.3, 0.7), s.h * rng.uniform(0.02, 0.05)
        d += poly_d([(x - w, s.h), (x - w * 0.4, s.h - h), (x, s.h - h - w * 3), (x + w * 0.4, s.h - h), (x + w, s.h)])
    s.path(d, "#163a5c", 0.85)
    x, y, c = s.w * fx, s.h * 0.4, s.h * k * 1.1
    for side in (-1, 1):
        s.path(poly_d([(x, y), (x + side * c * 0.6, y - c * 0.45), (x + side * c * 0.75, y - c * 0.1), (x + side * c * 0.15,
                                                                                                    y + c * 0.05)]),
               "#9ff3ff", 0.45)
        s.path(poly_d([(x, y + c * 0.02), (x + side * c * 0.45, y + c * 0.35), (x + side * c * 0.12, y + c * 0.3)]),
               "#c7a4ff", 0.45)
        s.path(poly_d([(x, y), (x + side * c * 0.6, y - c * 0.45), (x + side * c * 0.75, y - c * 0.1)], False),
               stroke="#ffffff", width=c * 0.008, opacity=0.8)
    s.path(figure_d(x, y + c * 0.25, c * 0.4, False), "#0a1a2a")
    for _ in range(8):
        bx, by, b = rng.uniform(0, s.w), rng.uniform(s.h * 0.1, s.h * 0.6), s.h * 0.015
        s.path(poly_d([(bx, by), (bx - b, by - b), (bx - b * 0.5, by + b * 0.3)]) + poly_d([(bx, by), (bx + b, by - b),
                                                                                         (bx + b * 0.5, by + b * 0.3)]),
               "#ffffff", 0.7)


def rivermill(s, rng, p, fx, k):
    sky(s, "#8fd0f2", "#f1f7d6", 0, s.h * 0.55)
    clouds(s, rng, 5, s.h * 0.05, s.h * 0.3, "#ffffff", 0.9)
    hills(s, rng, s.h * 0.55, s.h * 0.08, "#8cc56b", 3)
    for i, color in enumerate(("#c9d86a", "#e8c35c", "#9fd06f", "#d9b04c")):
        y = s.h * (0.6 + 0.08 * i)
        s.rect(0, y, s.w, s.h * 0.09, color)
        d = "".join("M%s %sl%s %s" % (num(x * s.w / 30), num(y), num(s.h * 0.02), num(s.h * 0.09)) for x in range(31))
        s.path(d, stroke="#ffffff", width=s.h * 0.003, opacity=0.25)
    s.path("M%s %sQ%s %s %s %sL%s %sQ%s %s %s %sZ" % (
        num(s.w * 0.45), num(s.h * 0.55), num(s.w * 0.2), num(s.h * 0.8), num(s.w * 0.35), num(s.h), num(s.w * 0.55),
        num(s.h), num(s.w * 0.35), num(s.h * 0.8), num(s.w * 0.5), num(s.h * 0.55)), "#6fc3e8")
    mx, base, c = s.w * fx, s.h * 0.78, s.h * k * 0.9
    s.rect(mx - c * 0.3, base - c * 0.6, c * 0.6, c * 0.6, "#e9dcc4")
    s.path(poly_d([(mx - c * 0.38, base - c * 0.58), (mx, base - c * 0.95), (mx + c * 0.38, base - c * 0.58)]), "#a0442f")
    s.rect(mx - c * 0.08, base - c * 0.28, c * 0.16, c * 0.28, "#6b4a32")
    wx, wy, r = mx + c * 0.42, base - c * 0.25, c * 0.28
    d = "".join("M%s %sL%s %s" % (num(wx), num(wy), num(wx + r * math.cos(a / 8 * 6.283)),
                                  num(wy + r * math.sin(a / 8 * 6.283))) for a in range(8))
    s.path(circle_d(wx, wy, r), stroke="#6b4a32", width=c * 0.04)
    s.path(d, stroke="#6b4a32", width=c * 0.025)
    d = ""
    for _ in range(6):
        sx, sy, r = rng.uniform(0, s.w), s.h * rng.uniform(0.62, 0.95), s.h * 0.018
        d += circle_d(sx, sy, r) + circle_d(sx + r * 0.9, sy - r * 0.3, r * 0.8) + circle_d(sx - r * 0.9, sy - r * 0.2,
                                                                                         r * 0.8)
        s.circle(sx + r * 1.6, sy - r * 0.5, r * 0.5, "#2b2b2b")
    s.path(d, "#ffffff")


def volt(s, rng, p, fx, k):
    sky(s, "#07080c", "#2a3140")
    d = ""
    for _ in range(3):
        x = rng.uniform(0, s.w)
        pts, y = [], 0.0
        while y < s.h * 0.6:
            pts.append((x, y))
            x += rng.uniform(-1, 1) * s.h * 0.06
            y += s.h * rng.uniform(0.05, 0.1)
        d += poly_d(pts, False)
    s.path(d, stroke="#ffe14a", width=s.h * 0.02, opacity=0.25)
    s.path(d, stroke="#fff7c2", width=s.h * 0.006)
    for side in (0.15, 0.85):
        x, base, h = s.w * side, s.h * 0.8, s.h * 0.6
        lattice = poly_d([(x - h * 0.12, base), (x, base - h), (x + h * 0.12, base)], False)
        lattice += "".join("M%s %sL%s %s" % (num(x - h * 0.12 * (1 - t)), num(base - h * t), num(x + h * 0.12 * (1 - t - 0.15)),
                                             num(base - h * (t + 0.15))) for t in (0.0, 0.2, 0.4, 0.6))
        lattice += "M%s %sH%s" % (num(x - h * 0.2), num(base - h * 0.75), num(x + h * 0.2))
        s.path(lattice, stroke="#11141b", width=s.h * 0.008)
    skyline(s, rng, s.h * 0.85, s.h * 0.05, s.h * 0.2, "#11141b", "#ffe14a", 0.08)
    s.rect(0, s.h * 0.85, s.w, s.h * 0.15, "#0b0d12")
    for i, dx in enumerate((-0.35, 0.0, 0.35)):
        x, h = s.w * fx + dx * s.h * k, s.h * k * (0.85 if i != 1 else 1.0)
        figure(s, x, s.h * 0.92, h, "#05060a", "#ffe14a", "rifle", "#ffe14a", False)
        s.rect(x - h * 0.05, s.h * 0.92 - h * 0.88, h * 0.1, h * 0.025, "#ffe14a")


def hollowsong(s, rng, p, fx, k):
    sky(s, "#07060d", "#1d1430")
    top = ridge_points(s, rng, s.h * 0.12, -s.h * 0.15, 2.5)
    s.path(poly_d(top + [(s.w, 0), (0, 0)]), "#0d0a18")
    d = ""
    for _ in range(18):
        x, h = rng.uniform(0, s.w), s.h * rng.uniform(0.05, 0.18)
        y = height_at(top, x) - 2
        d += poly_d([(x - h * 0.15, y), (x, y + h), (x + h * 0.15, y)])
    s.path(d, "#0d0a18")
    ridge(s, rng, s.h * 0.85, s.h * 0.15, "#140f24", 1.8)
    for _ in range(5):
        rx, ry, r = rng.uniform(0, s.w), rng.uniform(s.h * 0.3, s.h * 0.6), s.h * rng.uniform(0.03, 0.05)
        glow(s, rx, ry, r, "#7af0d0", 4, 0.06)
        s.path(circle_d(rx, ry, r), stroke="#7af0d0", width=r * 0.12)
        s.path("M%s %sv%sM%s %sh%s" % (num(rx), num(ry - r * 0.6), num(r * 1.2), num(rx - r * 0.5), num(ry), num(r)),
               stroke="#7af0d0", width=r * 0.12)
    near = ridge(s, rng, s.h * 0.95, s.h * 0.06, "#06040c", 1.0, False)
    figure(s, s.w * fx, height_at(near, s.w * fx) + 2, s.h * k * 0.7, "#030207", "#7af0d0", "sword", "#e6fff8", False)
    s.path(circle_d(s.w * fx - s.h * k * 0.02, height_at(near, s.w * fx) - s.h * k * 0.6, s.h * k * 0.012), "#ffffff")


def tinyfleet(s, rng, p, fx, k):
    s.rect(0, 0, s.w, s.h, "#1b5f86")
    d = "".join("M%s 0V%s" % (num(x * s.h * 0.1), num(s.h)) for x in range(int(s.w / (s.h * 0.1)) + 1))
    d += "".join("M0 %sH%s" % (num(y * s.h * 0.1), num(s.w)) for y in range(11))
    s.path(d, stroke="#ffffff", width=s.h * 0.002, opacity=0.12)
    islands = [(rng.uniform(0, s.w), rng.uniform(0, s.h), s.h * rng.uniform(0.06, 0.14)) for _ in range(6)]
    for ix, iy, r in islands:
        shape = random.Random(int(ix * 7 + iy))
        s.path(blob_d(shape, ix, iy, r * 1.25), "#3f86ad")
        shape = random.Random(int(ix * 7 + iy))
        s.path(blob_d(shape, ix, iy, r * 1.08), "#f1d7a5")
        shape = random.Random(int(ix * 7 + iy))
        s.path(blob_d(shape, ix, iy, r * 0.85), "#5fae5a")
    (ax, ay, _), (bx, by, _) = islands[0], islands[1]
    s.path("M%s %sQ%s %s %s %s" % (num(ax), num(ay), num((ax + bx) / 2 + s.h * 0.2), num((ay + by) / 2), num(bx), num(by)),
           stroke="#ffffff", width=s.h * 0.006, dash="%s %s" % (num(s.h * 0.02), num(s.h * 0.015)), opacity=0.8)
    for i in range(7):
        x, y, b = rng.uniform(0, s.w), rng.uniform(0, s.h), s.h * 0.035 * (1.6 if i == 0 else 1)
        if i == 0:
            x, y = s.w * fx, s.h * 0.55
        s.path("M%s %sl%s %s" % (num(x - b * 0.8), num(y + b * 0.1), num(-b * 1.5), num(b * 0.4)) + "M%s %sl%s %s" % (
            num(x - b * 0.8), num(y - b * 0.1), num(-b * 1.5), num(-b * 0.4)), stroke="#e6fbff", width=b * 0.08, opacity=0.6)
        s.path(poly_d([(x - b, y - b * 0.25), (x + b * 0.6, y - b * 0.25), (x + b, y), (x + b * 0.6, y + b * 0.25),
                       (x - b, y + b * 0.25)]), "#7a4b2e" if i < 4 else "#3b3f4a")
        s.path(poly_d([(x - b * 0.3, y - b * 0.2), (x + b * 0.4, y), (x - b * 0.3, y + b * 0.2)]),
               p["accent"] if i < 4 else "#e05a4a")
        if i == 0:
            s.add("ellipse", cx=float(x), cy=float(y), rx=float(b * 1.4), ry=float(b * 0.7), fill="none",
                  stroke="#5af2b5", stroke_width=float(b * 0.08))
    for i in range(6):
        s.rect(0, i * s.h * 0.02, s.w, s.h * 0.02, "#0b2a3d", 0.5 - i * 0.08)


# Heads-up displays laid over the screenshots, by kind of game.


def hud_rpg(s, rng, p):
    u = s.h / 360
    s.circle(28 * u, 28 * u, 18 * u, "#000000", 0.5)
    s.path(circle_d(28 * u, 28 * u, 18 * u), stroke=p["accent"], width=2 * u)
    for i, (color, full) in enumerate((("#e04848", rng.uniform(0.6, 0.95)), ("#4a8cf0", rng.uniform(0.4, 0.9)))):
        s.rect(52 * u, (18 + i * 12) * u, 130 * u, 8 * u, "#000000", 0.55, 2 * u)
        s.rect(53 * u, (19 + i * 12) * u, 128 * u * full, 6 * u, color, None, 2 * u)
    s.circle(s.w - 44 * u, 44 * u, 32 * u, "#000000", 0.45)
    s.path(circle_d(s.w - 44 * u, 44 * u, 32 * u), stroke="#ffffff", width=1.5 * u, opacity=0.6)
    s.path(poly_d([(s.w - 44 * u, 38 * u), (s.w - 40 * u, 49 * u), (s.w - 48 * u, 49 * u)]), "#ffffff")
    s.path("".join(circle_d(s.w - 44 * u + rng.uniform(-20, 20) * u, 44 * u + rng.uniform(-20, 20) * u, 2 * u)
                   for _ in range(4)), p["accent"])
    for i in range(3):
        s.rect(s.w - (150 - (0 if i == 0 else 10)) * u, (92 + i * 12) * u, (120 - i * 25) * u, 5 * u,
               p["accent"] if i == 0 else "#ffffff", 0.75, 2 * u)
    x0 = s.w / 2 - 3 * 28 * u
    for i in range(6):
        s.rect(x0 + i * 28 * u, s.h - 36 * u, 24 * u, 24 * u, "#000000", 0.55, 3 * u)
        s.path(circle_d(x0 + i * 28 * u + 12 * u, s.h - 24 * u, 6 * u), rng.choice((p["accent"], "#9fe8ff", "#e04848")),
               0.85)


def hud_racing(s, rng, p):
    u = s.h / 360
    s.rect(16 * u, 16 * u, 64 * u, 40 * u, "#000000", 0.55, 4 * u)
    s.rect(24 * u, 24 * u, 22 * u, 24 * u, p["accent"], None, 2 * u)
    s.rect(52 * u, 26 * u, 20 * u, 5 * u, "#ffffff", 0.8)
    s.rect(52 * u, 36 * u, 14 * u, 5 * u, "#ffffff", 0.5)
    cx, cy, r = s.w - 60 * u, s.h - 50 * u, 38 * u
    s.circle(cx, cy, r + 6 * u, "#000000", 0.5)
    s.path("M%s %sA%s %s 0 1 1 %s %s" % (num(cx - r * 0.7), num(cy + r * 0.7), num(r), num(r), num(cx + r * 0.7),
                                          num(cy + r * 0.7)), stroke="#ffffff", width=4 * u, opacity=0.3)
    s.path("M%s %sA%s %s 0 0 1 %s %s" % (num(cx - r * 0.7), num(cy + r * 0.7), num(r), num(r), num(cx + r * 0.6),
                                          num(cy - r * 0.8)), stroke=p["accent"], width=4 * u)
    s.path("M%s %sL%s %s" % (num(cx), num(cy), num(cx + r * 0.5), num(cy - r * 0.6)), stroke="#ffffff", width=2 * u)
    s.path(poly_d([(30 * u, s.h - 30 * u), (40 * u, s.h - 90 * u), (90 * u, s.h - 100 * u), (110 * u, s.h - 60 * u),
                   (70 * u, s.h - 25 * u)]), stroke="#ffffff", width=3 * u, opacity=0.7)
    s.circle(90 * u, s.h - 100 * u, 4 * u, p["accent"])


def hud_strategy(s, rng, p):
    u = s.h / 360
    s.rect(0, 0, s.w, 20 * u, "#000000", 0.6)
    for i, color in enumerate(("#f2c14e", "#8fd16a", "#9aa8ff", "#e07a5f")):
        s.circle((16 + i * 80) * u, 10 * u, 5 * u, color)
        s.rect((26 + i * 80) * u, 7 * u, rng.uniform(25, 45) * u, 6 * u, "#ffffff", 0.7, 2 * u)
    s.rect(s.w - 130 * u, s.h - 90 * u, 120 * u, 80 * u, "#000000", 0.65, 3 * u)
    d = "".join(blob_d(rng, s.w - 130 * u + rng.uniform(20, 100) * u, s.h - 90 * u + rng.uniform(20, 60) * u, 12 * u)
                for _ in range(3))
    s.path(d, "#5fae5a", 0.8)
    s.rect(s.w - 100 * u, s.h - 70 * u, 36 * u, 24 * u, "none")
    s.path("M%s %sh%sv%sh%sZ" % (num(s.w - 100 * u), num(s.h - 70 * u), num(36 * u), num(24 * u), num(-36 * u)),
           stroke="#ffffff", width=1.2 * u)
    s.rect(10 * u, s.h - 64 * u, 170 * u, 54 * u, "#000000", 0.6, 3 * u)
    s.rect(16 * u, s.h - 58 * u, 42 * u, 42 * u, p["accent"], 0.85, 2 * u)
    for i in range(3):
        s.rect(66 * u, s.h - (54 - i * 12) * u, (100 - i * 20) * u, 5 * u, "#ffffff", 0.7, 2 * u)
    for _ in range(3):
        s.add("ellipse", cx=float(rng.uniform(0.3, 0.7) * s.w), cy=float(rng.uniform(0.5, 0.75) * s.h), rx=float(14 * u),
              ry=float(6 * u), fill="none", stroke="#5af2b5", stroke_width=float(1.5 * u))


def hud_shooter(s, rng, p):
    u = s.h / 360
    cx, cy = s.w / 2, s.h / 2
    s.path("M%s %sh%sM%s %sh%sM%s %sv%sM%s %sv%s" % (num(cx - 14 * u), num(cy), num(8 * u), num(cx + 6 * u), num(cy),
                                                       num(8 * u), num(cx), num(cy - 14 * u), num(8 * u), num(cx),
                                                       num(cy + 6 * u), num(8 * u)), stroke="#ffffff", width=1.5 * u)
    s.circle(cx, cy, 1.5 * u, p["accent"])
    s.rect(16 * u, s.h - 34 * u, 140 * u, 18 * u, "#000000", 0.55, 3 * u)
    s.rect(38 * u, s.h - 29 * u, 112 * u * rng.uniform(0.5, 0.9), 8 * u, "#ffffff", 0.9, 2 * u)
    s.path("M%s %sh%sM%s %sv%s" % (num(21 * u), num(s.h - 25 * u), num(10 * u), num(26 * u), num(s.h - 30 * u),
                                   num(10 * u)), stroke=p["accent"], width=3 * u)
    for i in range(10):
        s.rect(s.w - (24 + i * 7) * u, s.h - 34 * u, 4 * u, 18 * u, p["accent"] if i < 7 else "#ffffff",
               0.9 if i < 7 else 0.3, 1 * u)
    s.rect(s.w / 2 - 120 * u, 10 * u, 240 * u, 14 * u, "#000000", 0.45, 2 * u)
    s.path("".join("M%s %sv%s" % (num(s.w / 2 - 110 * u + i * 20 * u), num(13 * u), num(8 * u if i % 3 == 0 else 4 * u))
                   for i in range(12)), stroke="#ffffff", width=1.2 * u, opacity=0.8)


def hud_puzzle(s, rng, p):
    u = s.h / 360
    s.rect(16 * u, 16 * u, 110 * u, 46 * u, "#000000", 0.55, 4 * u)
    s.rect(26 * u, 26 * u, 50 * u, 8 * u, "#ffffff", 0.85, 2 * u)
    s.rect(26 * u, 42 * u, 80 * u, 10 * u, p["accent"], None, 2 * u)
    s.rect(s.w - 96 * u, 16 * u, 80 * u, 80 * u, "#000000", 0.55, 4 * u)
    s.path(poly_d(hexagon(s.w - 56 * u, 56 * u, 20 * u)), "#ff5fa2")
    s.rect(s.w * 0.25, s.h - 22 * u, s.w * 0.5, 8 * u, "#000000", 0.55, 4 * u)
    s.rect(s.w * 0.25, s.h - 22 * u, s.w * 0.5 * rng.uniform(0.3, 0.8), 8 * u, "#5af2e0", None, 4 * u)


def heart_d(x, y, r):
    """A heart."""
    return "M%s %sC%s %s %s %s %s %sC%s %s %s %s %s %sZ" % (
        num(x), num(y + r * 0.9), num(x - r * 1.4), num(y), num(x - r * 0.6), num(y - r * 0.9), num(x), num(y - r * 0.3),
        num(x + r * 0.6), num(y - r * 0.9), num(x + r * 1.4), num(y), num(x), num(y + r * 0.9))


def hud_platform(s, rng, p):
    u = s.h / 360
    full = rng.randint(1, 3)
    for i in range(3):
        if i < full:
            s.path(heart_d((26 + i * 26) * u, 26 * u, 10 * u), "#ff4d6d")
        else:
            s.path(heart_d((26 + i * 26) * u, 26 * u, 10 * u), stroke="#ffffff", width=2 * u)
    s.rect(s.w - 100 * u, 14 * u, 84 * u, 26 * u, "#000000", 0.4, 13 * u)
    s.circle(s.w - 86 * u, 27 * u, 8 * u, "#ffd23f")
    s.rect(s.w - 72 * u, 23 * u, 44 * u, 8 * u, "#ffffff", 0.9, 2 * u)


def hud_horror(s, rng, p):
    u = s.h / 360
    for i in range(6):
        t = 0.5 - i * 0.08
        s.rect(0, 0, s.w, (6 + i * 6) * u, "#000000", t)
        s.rect(0, s.h - (6 + i * 6) * u, s.w, (6 + i * 6) * u, "#000000", t)
    s.rect(s.w - 66 * u, s.h - 40 * u, 44 * u, 20 * u, "none")
    s.path("M%s %sh%sv%sh%sZ" % (num(s.w - 66 * u), num(s.h - 40 * u), num(40 * u), num(20 * u), num(-40 * u)),
           stroke="#ffffff", width=1.5 * u, opacity=0.8)
    s.rect(s.w - 26 * u, s.h - 34 * u, 4 * u, 8 * u, "#ffffff", 0.8)
    s.rect(s.w - 63 * u, s.h - 37 * u, 34 * u * rng.uniform(0.2, 0.6), 14 * u, "#ffcf6b")
    s.rect(s.w / 2 - 60 * u, s.h - 44 * u, 120 * u, 22 * u, "#000000", 0.6, 11 * u)
    s.circle(s.w / 2 - 46 * u, s.h - 33 * u, 7 * u, "#ffffff", 0.9)
    s.rect(s.w / 2 - 32 * u, s.h - 36 * u, 80 * u, 6 * u, "#ffffff", 0.7, 2 * u)


def hud_sim(s, rng, p):
    u = s.h / 360
    s.rect(16 * u, 14 * u, 96 * u, 24 * u, "#000000", 0.45, 12 * u)
    s.circle(30 * u, 26 * u, 7 * u, "#ffd23f")
    s.rect(42 * u, 22 * u, 56 * u, 8 * u, "#ffffff", 0.9, 2 * u)
    cx, cy = s.w - 40 * u, 40 * u
    s.circle(cx, cy, 24 * u, "#fff7e6", 0.9)
    s.path(circle_d(cx, cy, 24 * u), stroke="#6b4a32", width=2 * u)
    s.path("M%s %sl%s %sM%s %sl%s %s" % (num(cx), num(cy), num(0), num(-16 * u), num(cx), num(cy), num(10 * u),
                                         num(6 * u)), stroke="#6b4a32", width=2 * u, cap="round")
    x0 = s.w / 2 - 4 * 30 * u
    for i in range(8):
        s.rect(x0 + i * 30 * u, s.h - 38 * u, 26 * u, 26 * u, "#fff7e6", 0.85, 4 * u)
        s.path("M%s %sh%sv%sh%sZ" % (num(x0 + i * 30 * u), num(s.h - 38 * u), num(26 * u), num(26 * u), num(-26 * u)),
               stroke=p["accent"] if i == 2 else "#6b4a32", width=(2.5 if i == 2 else 1) * u)
        if i < 5:
            s.path(circle_d(x0 + i * 30 * u + 13 * u, s.h - 25 * u, 7 * u), rng.choice(("#5fae5a", "#e8c35c", "#d9483b",
                                                                                     "#6fc3e8")))


# The games: their worlds, accents and HUDs. fx and k place the hero in a cover, a hero banner and the shots.

GAMES = {
    "starfall": (starfall, "#f2c14e", hud_rpg),
    "ember": (ember, "#ff7a3d", hud_rpg),
    "tidebreaker": (tidebreaker, "#5ad1e6", hud_rpg),
    "neon": (neon, "#ff3ea5", hud_racing),
    "mossheart": (mossheart, "#8fd16a", hud_platform),
    "ironhollow": (ironhollow, "#c9a36b", hud_strategy),
    "skyward": (skyward, "#f4a261", hud_rpg),
    "quietorbit": (quietorbit, "#9aa8ff", hud_strategy),
    "lumen": (lumen, "#7af0ff", hud_puzzle),
    "driftwood": (driftwood, "#e07a5f", hud_sim),
    "frostline": (frostline, "#7ad7ff", hud_rpg),
    "hexfall": (hexfall, "#5af2e0", hud_puzzle),
    "thornwick": (thornwick, "#ffcf6b", hud_horror),
    "pebble": (pebble, "#ff8a3d", hud_platform),
    "aurora": (aurora, "#ff7ad9", hud_racing),
    "clockwork": (clockwork, "#e2b26b", hud_strategy),
    "saltmarsh": (saltmarsh, "#5e6b45", hud_sim),
    "glasswing": (glasswing, "#9ff3ff", hud_shooter),
    "rivermill": (rivermill, "#e8c35c", hud_sim),
    "volt": (volt, "#ffe14a", hud_shooter),
    "hollowsong": (hollowsong, "#7af0d0", hud_platform),
    "tinyfleet": (tinyfleet, "#f2c14e", hud_strategy),
}

VIEWS = {  # (size, fx, k, HUD?)
    "cover": (COVER, 0.5, 0.42, False),
    "hero": (HERO, 0.74, 0.62, False),
    "shot1": (SHOT, 0.4, 0.3, True),
    "shot2": (SHOT, 0.62, 0.24, True),
    "shot3": (SHOT, 0.5, 0.4, True),
}


def draw_game(game, view):
    """One picture of a game."""
    scene, accent, hud = GAMES[game]
    (w, h), fx, k, with_hud = VIEWS[view]
    rng = random.Random(zlib.crc32(("%s/%s" % (game, view if view != "hero" else "cover")).encode()))
    s = Svg(w, h)
    scene(s, rng, {"accent": accent}, fx, k)
    if view == "cover":
        shade_bottom(s, 0.4, "#000000")
    elif view == "hero":
        shade_left(s, 0.45, "#000000")
        shade_bottom(s, 0.3, "#000000", 0.5)
    elif view == "shot3":
        s.rect(0, 0, w, h, accent, 0.08)
    if with_hud:
        hud(s, random.Random(zlib.crc32(("%s/%s/hud" % (game, view)).encode())), {"accent": accent})
    return s.text()


# Avatars: a creature's head on a coloured ground.

AVATARS = [
    ("#3b82f6", "cat"), ("#f59e0b", "fox"), ("#10b981", "frog"), ("#8b5cf6", "owl"), ("#ef4444", "robot"),
    ("#0ea5e9", "astronaut"), ("#64748b", "knight"), ("#ec4899", "ghost"), ("#a16207", "bear"), ("#22c55e", "alien"),
    ("#f97316", "bird"), ("#14b8a6", "slime"),
]


def draw_avatar(ground, kind):
    """A friend's picture."""
    w, h = AVATAR
    s = Svg(w, h)
    s.rect(0, 0, w, h, ground)
    s.circle(w * 0.75, h * 0.2, w * 0.4, "#ffffff", 0.12)
    face = mix(ground, "#ffffff", 0.75)
    dark = "#1f2330"
    cx, cy = w / 2, h * 0.6
    if kind in ("cat", "fox"):
        ear = "#f6d1a8" if kind == "fox" else face
        s.path(poly_d([(cx - 18, cy - 8), (cx - 14, cy - 30), (cx - 2, cy - 16)]) + poly_d([(cx + 18, cy - 8), (cx + 14,
                                                                                                          cy - 30),
                                                                                         (cx + 2, cy - 16)]), ear)
        s.circle(cx, cy, 18, "#f6a04d" if kind == "fox" else face)
        if kind == "fox":
            s.path(poly_d([(cx - 18, cy + 2), (cx, cy + 18), (cx + 18, cy + 2), (cx, cy + 8)]), "#fff4e6")
    elif kind == "robot":
        s.rect(cx - 17, cy - 18, 34, 32, face, rx=6)
        s.path("M%s %sv-10" % (num(cx), num(cy - 18)), stroke=face, width=2.5)
        s.circle(cx, cy - 30, 3, "#fde047")
        s.rect(cx - 10, cy + 6, 20, 3, dark, rx=1.5)
    elif kind == "astronaut":
        s.circle(cx, cy, 20, face)
        s.rect(cx - 13, cy - 9, 26, 15, "#1e3a5f", rx=7)
        s.rect(cx - 9, cy - 7, 8, 4, "#ffffff", 0.6, rx=2)
    elif kind == "knight":
        s.path("M%s %sQ%s %s %s %sV%sH%sZ" % (num(cx - 18), num(cy + 16), num(cx), num(cy - 44), num(cx + 18),
                                               num(cy + 16), num(cy + 16), num(cx - 18)), face)
        s.rect(cx - 12, cy - 4, 24, 4, dark, rx=2)
        s.path("M%s %sv%s" % (num(cx), num(cy - 26), num(-8)), stroke="#ef4444", width=4, cap="round")
    elif kind == "ghost":
        s.path("M%s %sV%sa18 18 0 0 1 36 0V%sl-6 -5l-6 5l-6 -5l-6 5l-6 -5Z" % (
            num(cx - 18), num(cy + 18), num(cy - 4), num(cy + 18)), face)
    elif kind == "bear":
        s.path(circle_d(cx - 14, cy - 15, 7) + circle_d(cx + 14, cy - 15, 7), face)
        s.circle(cx, cy, 18, face)
        s.add("ellipse", cx=float(cx), cy=float(cy + 7), rx=8.0, ry=6.0, fill="#ffffff", opacity="0.7")
        s.circle(cx, cy + 4, 2.5, dark)
    elif kind == "alien":
        s.path("M%s %sl-6 -14M%s %sl6 -14" % (num(cx - 7), num(cy - 14), num(cx + 7), num(cy - 14)), stroke=face, width=2.5)
        s.path(circle_d(cx - 13, cy - 28, 3) + circle_d(cx + 13, cy - 28, 3), face)
        s.add("ellipse", cx=float(cx), cy=float(cy), rx=17.0, ry=19.0, fill=face)
    elif kind == "owl":
        s.path(poly_d([(cx - 18, cy - 10), (cx - 16, cy - 26), (cx - 6, cy - 16)]) + poly_d([(cx + 18, cy - 10), (cx + 16,
                                                                                                           cy - 26),
                                                                                          (cx + 6, cy - 16)]), face)
        s.circle(cx, cy, 18, face)
        s.path(circle_d(cx - 8, cy - 3, 8) + circle_d(cx + 8, cy - 3, 8), "#ffffff")
        s.path(poly_d([(cx - 3, cy + 6), (cx + 3, cy + 6), (cx, cy + 11)]), "#f59e0b")
    elif kind == "frog":
        s.path(circle_d(cx - 10, cy - 14, 7) + circle_d(cx + 10, cy - 14, 7), face)
        s.add("ellipse", cx=float(cx), cy=float(cy), rx=20.0, ry=15.0, fill=face)
        s.path("M%s %sq8 6 16 0" % (num(cx - 8), num(cy + 5)), stroke=dark, width=2, cap="round")
    elif kind == "bird":
        s.circle(cx, cy, 18, face)
        s.path(poly_d([(cx + 14, cy - 2), (cx + 26, cy + 2), (cx + 14, cy + 6)]), "#fbbf24")
        s.path(poly_d([(cx - 4, cy - 18), (cx + 2, cy - 30), (cx + 6, cy - 17)]), face)
    elif kind == "slime":
        s.path("M%s %sQ%s %s %s %sZ" % (num(cx - 22), num(cy + 18), num(cx), num(cy - 40), num(cx + 22), num(cy + 18)),
               face)
        s.circle(cx - 6, cy - 6, 4, "#ffffff", 0.7)
    if kind not in ("astronaut", "knight"):
        eyes = (-8, 8) if kind not in ("owl", "frog") else ((-8, 8) if kind == "owl" else (-10, 10))
        ey = cy - (3 if kind == "owl" else 14 if kind == "frog" else 2)
        s.path("".join(circle_d(cx + e, ey, 3) for e in eyes), dark)
    return s.text()


def write(path, text):
    """Writes a file, its folder made if needed."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as out:
        out.write(text)


def main():
    """Draws every picture."""
    for game in GAMES:
        for view in VIEWS:
            write(os.path.join(HERE, game, view + ".svg"), draw_game(game, view))
    for i, (ground, kind) in enumerate(AVATARS):
        write(os.path.join(HERE, "avatars", "%s.svg" % kind), draw_avatar(ground, kind))


if __name__ == "__main__":
    main()
