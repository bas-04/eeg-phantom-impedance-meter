"""Tiny SVG helper used by diagrams.py. Plain strings, no dependencies.

Colours follow one fixed legend across every diagram in docs/img:
  CURRENT  orange  - the test-current path (drive and return)
  SENSE    blue    - the voltage-sense path (carries no current)
  CONTROL  grey    - I2C and digital control
  POWER    black   - supply wiring
  ALERT    red     - things that destroy hardware if done wrong
"""
from html import escape

CURRENT = "#f47230"
SENSE = "#257de0"
RETURN = "#00a888"
CONTROL = "#666666"
POWER = "#191919"
ALERT = "#dc3c3d"
GOOD = "#0aac23"
WARN = "#f2a600"
INK = "#191919"
INK2 = "#666666"
RULE = "#c2c2c2"
PANEL = "#f6f7f8"
BOX = "#ffffff"
FONT = "Helvetica, Arial, sans-serif"
MONO = "Menlo, Consolas, 'DejaVu Sans Mono', monospace"


class SVG:
    def __init__(self, w, h, title):
        self.w, self.h = w, h
        self.parts = []
        self.title = title

    def add(self, s):
        self.parts.append(s)

    # -- primitives -------------------------------------------------------
    def rect(self, x, y, w, h, fill=BOX, stroke=RULE, sw=1.5, rx=6, dash=None, opacity=None):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        o = f' fill-opacity="{opacity}"' if opacity is not None else ""
        self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}"{o} '
                 f'stroke="{stroke}" stroke-width="{sw}"{d}/>')

    def text(self, x, y, s, size=13, weight="normal", fill=INK, anchor="start", mono=False, italic=False):
        fam = MONO if mono else FONT
        st = ' font-style="italic"' if italic else ""
        self.add(f'<text x="{x}" y="{y}" font-family="{fam}" font-size="{size}" font-weight="{weight}" '
                 f'fill="{fill}" text-anchor="{anchor}"{st}>{escape(s)}</text>')

    def lines(self, x, y, rows, size=12, gap=None, **kw):
        gap = gap or size + 4
        for i, r in enumerate(rows):
            self.text(x, y + i * gap, r, size=size, **kw)

    def line(self, pts, color=INK, sw=2, dash=None, arrow=False, arrow_start=False):
        d = "M" + " L".join(f"{x} {y}" for x, y in pts)
        da = f' stroke-dasharray="{dash}"' if dash else ""
        me = f' marker-end="url(#ah-{color[1:]})"' if arrow else ""
        ms = f' marker-start="url(#ah-{color[1:]})"' if arrow_start else ""
        self.add(f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{sw}" '
                 f'stroke-linejoin="round" stroke-linecap="round"{da}{me}{ms}/>')

    def circle(self, cx, cy, r, fill=BOX, stroke=INK, sw=1.5):
        self.add(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>')

    def dot(self, cx, cy, color=INK, r=3.5):
        self.circle(cx, cy, r, fill=color, stroke=color, sw=0)

    def hexagon(self, cx, cy, r, fill, stroke="none", sw=1):
        import math
        pts = " ".join(f"{cx + r * math.cos(math.radians(a)):.1f},{cy + r * math.sin(math.radians(a)):.1f}"
                       for a in range(0, 360, 60))
        self.add(f'<polygon points="{pts}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>')

    def resistor(self, x, y, length=60, vertical=False, color=INK, label=None, value=None, lpos="above"):
        """Zig-zag resistor centred on (x, y)."""
        n, amp = 6, 6
        pts = []
        half = length / 2
        for i in range(n + 1):
            t = -half + 10 + i * (length - 20) / n
            off = 0 if i in (0, n) else (amp if i % 2 else -amp)
            pts.append((x + t, y + off) if not vertical else (x + off, y + t))
        lead1 = [(x - half, y), (x - half + 10, y)] if not vertical else [(x, y - half), (x, y - half + 10)]
        lead2 = [(x + half - 10, y), (x + half, y)] if not vertical else [(x, y + half - 10), (x, y + half)]
        self.line(lead1, color, 1.8)
        self.line(pts, color, 1.8)
        self.line(lead2, color, 1.8)
        if label:
            if vertical:
                self.text(x + 12, y - 2, label, 12, "bold")
                if value:
                    self.text(x + 12, y + 13, value, 12, fill=INK2)
            else:
                ty = y - 12 if lpos == "above" else y + 22
                txt = f"{label}  {value}" if value else label
                self.text(x, ty, txt, 12, "bold", anchor="middle")

    def opamp(self, x, y, size=70, label=None, sub=None, plus_top=False):
        """Triangle pointing right; (x, y) = left-edge centre. Returns pin coords."""
        h = size
        self.add(f'<polygon points="{x},{y - h / 2} {x},{y + h / 2} {x + h},{y}" fill="{BOX}" '
                 f'stroke="{INK}" stroke-width="1.8"/>')
        top, bot = (y - h / 4, y + h / 4)
        s_top, s_bot = ("+", "−") if plus_top else ("−", "+")
        self.text(x + 8, top + 5, s_top, 15, "bold")
        self.text(x + 8, bot + 5, s_bot, 15, "bold")
        if label:
            self.text(x + h * 0.38, y + 4, label, 11, "bold", anchor="middle")
        if sub:
            self.text(x + h / 2, y + h / 2 + 16, sub, 11, fill=INK2, anchor="middle")
        pins = {"top": (x, top), "bot": (x, bot), "out": (x + h, y)}
        pins["+"] = pins["top"] if plus_top else pins["bot"]
        pins["-"] = pins["bot"] if plus_top else pins["top"]
        return pins

    def ground(self, x, y, color=INK):
        self.line([(x, y), (x, y + 8)], color, 1.8)
        for i, w in enumerate((14, 9, 4)):
            self.line([(x - w, y + 8 + i * 4), (x + w, y + 8 + i * 4)], color, 1.8)

    def pill(self, x, y, s, color, size=11, anchor="middle"):
        w = len(s) * size * 0.6 + 14
        x0 = x - w / 2 if anchor == "middle" else x
        self.rect(x0, y - size - 3, w, size + 9, fill=color, stroke=color, rx=(size + 9) / 2)
        self.text(x0 + w / 2, y + 1, s, size, "bold", fill="#ffffff", anchor="middle")


    def opamp_left(self, x, y, size=64, label=None, plus_top=True):
        """Triangle pointing LEFT; (x, y) = right-edge centre (inputs on the right)."""
        h = size
        self.add(f'<polygon points="{x},{y - h / 2} {x},{y + h / 2} {x - h},{y}" fill="{BOX}" '
                 f'stroke="{INK}" stroke-width="1.8"/>')
        top, bot = (y - h / 4, y + h / 4)
        s_top, s_bot = ("+", "−") if plus_top else ("−", "+")
        self.text(x - 16, top + 5, s_top, 15, "bold")
        self.text(x - 16, bot + 5, s_bot, 15, "bold")
        if label:
            self.text(x - h * 0.40, y + 4, label, 11, "bold", anchor="middle")
        pins = {"top": (x, top), "bot": (x, bot), "out": (x - h, y)}
        pins["+"] = pins["top"] if plus_top else pins["bot"]
        pins["-"] = pins["bot"] if plus_top else pins["top"]
        return pins

    def capacitor(self, x, y, color=INK, label=None, value=None):
        """Vertical capacitor: lead enters from above at (x, y); plates below; returns bottom lead point."""
        self.line([(x, y), (x, y + 16)], color, 1.8)
        self.line([(x - 12, y + 16), (x + 12, y + 16)], color, 2.4)
        self.line([(x - 12, y + 23), (x + 12, y + 23)], color, 2.4)
        self.line([(x, y + 23), (x, y + 36)], color, 1.8)
        if label:
            self.text(x + 16, y + 18, label, 12, "bold")
            if value:
                self.text(x + 16, y + 32, value, 12, fill=INK2)
        return (x, y + 36)

    def switch(self, x0, x1, y, color=INK, closed=False):
        """Horizontal SPST between x0 (left contact) and x1 (right contact)."""
        self.dot(x0, y, color, 3)
        self.dot(x1, y, color, 3)
        if closed:
            self.line([(x0, y), (x1, y)], color, 2)
        else:
            self.line([(x0, y), (x1 - 4, y - 11)], color, 2)

    # -- output -----------------------------------------------------------
    def render(self):
        colors = {CURRENT, SENSE, RETURN, CONTROL, POWER, ALERT, INK, INK2, GOOD}
        markers = "".join(
            f'<marker id="ah-{c[1:]}" viewBox="0 0 10 10" refX="8.5" refY="5" markerWidth="7" '
            f'markerHeight="7" orient="auto-start-reverse"><path d="M0 0 L10 5 L0 10 z" fill="{c}"/></marker>'
            for c in sorted(colors))
        body = "\n".join(self.parts)
        return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.w} {self.h}" '
                f'width="{self.w}" height="{self.h}" role="img" aria-label="{escape(self.title)}">\n'
                f'<title>{escape(self.title)}</title>\n<defs>{markers}</defs>\n'
                f'<rect width="{self.w}" height="{self.h}" rx="10" fill="#ffffff"/>\n{body}\n</svg>\n')

    def save(self, path):
        with open(path, "w") as f:
            f.write(self.render())
