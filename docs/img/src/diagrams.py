"""Generates the explanatory diagrams in docs/img/*.svg.

Run from anywhere:  python3 docs/img/src/diagrams.py
Every diagram is drawn from the Rev B build guide, ERRATA and firmware; the
KiCad schematic remains the authority for exact pin-level connections.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from svgkit import (SVG, CURRENT, SENSE, RETURN, CONTROL, POWER, ALERT, GOOD, WARN,
                    INK, INK2, RULE, PANEL, BOX)

OUT = os.path.join(os.path.dirname(__file__), "..")


def legend(s, x, y, items):
    for i, (color, label, dash) in enumerate(items):
        yy = y + i * 20
        s.line([(x, yy), (x + 34, yy)], color, 3, dash=dash)
        s.text(x + 44, yy + 4, label, 12, fill=INK2)


# ----------------------------------------------------------------------------
def system_overview():
    s = SVG(1000, 620, "System overview: training head, carrier board, display and power")
    s.text(24, 34, "System overview", 20, "bold")
    s.text(24, 56, "Everything that is wired together on the bench. Nothing here touches a person: the head is plastic.",
           13, fill=INK2)

    # --- training head panel
    s.rect(20, 76, 300, 430, fill=PANEL, stroke=RULE)
    s.text(36, 102, "TRAINING HEAD", 12, "bold", fill=INK2)
    cx, cy, r = 170, 230, 98
    s.circle(cx, cy, r, fill="#ffffff", stroke=INK, sw=2)
    # nose and ears to read as a head seen from above
    s.add(f'<path d="M{cx - 14} {cy - r + 3} L{cx} {cy - r - 18} L{cx + 14} {cy - r + 3}" fill="#ffffff" '
          f'stroke="{INK}" stroke-width="2" stroke-linejoin="round"/>')
    for side in (-1, 1):
        ex = cx + side * (r + 6)
        s.add(f'<ellipse cx="{ex}" cy="{cy + 6}" rx="9" ry="20" fill="#ffffff" stroke="{INK}" stroke-width="2"/>')
    sites = [(-0.31, -0.95), (0.31, -0.95), (-0.81, -0.59), (-0.40, -0.53), (0, -0.50), (0.40, -0.53),
             (0.81, -0.59), (-1.0, 0), (-0.5, 0), (0, 0), (0.5, 0), (1.0, 0), (-0.81, 0.59), (-0.40, 0.53),
             (0, 0.50), (0.40, 0.53), (0.81, 0.59), (-0.31, 0.88), (0.31, 0.88), (0, -0.80)]
    for (x, y) in sites:
        s.hexagon(cx + x * 82, cy + y * 82, 8.5, fill="#3a3a3a")
    s.text(cx, cy + r + 34, "22 conductive-PLA inserts at the", 12, anchor="middle")
    s.text(cx, cy + r + 50, "10-20 sites, in a printed PLA shell", 12, anchor="middle")
    s.text(cx, cy + r + 74, "Electro-Cap (tin electrodes) worn on", 12, anchor="middle", fill=INK2)
    s.text(cx, cy + r + 90, "top; gel injected at each site", 12, anchor="middle", fill=INK2)

    # --- carrier board
    bx, by, bw, bh = 420, 76, 270, 430
    s.rect(bx, by, bw, bh, fill="#eef6f0", stroke="#2f7d46", sw=2)
    s.text(bx + 56, by + 26, "CARRIER BOARD  Rev B", 12, "bold", fill="#2f7d46")
    s.text(bx + 56, by + 44, "160 × 116 mm, all through-hole", 11, fill=INK2)
    inner = [("6× CD74HC4067 analog mux", 0), ("drive · sense · return banks", 12),
             ("2× SN74HC4066 mode switches", 0), ("2× MCP6004 op-amps", 0),
             ("current pump, TIA, sense amp", 12), ("MCP4725 DAC, 2× ADS1115 ADC", 0),
             ("2× MCP23017 I/O expanders", 0), ("R20–R22 0.1 % cal standards", 0),
             ("piezo, 3 LEDs, 3.3 V LDO", 0)]
    for i, (t, ind) in enumerate(inner):
        s.text(bx + 60 + ind, by + 150 + i * 19, t, 12, fill=INK2 if ind else INK)
    # connectors on the left edge
    for (yy, name, sub, col) in [(130, "J1", "PHANTOM", CURRENT), (250, "J2", "CAP", RETURN)]:
        s.rect(bx - 14, yy - 22, 56, 44, fill="#ffffff", stroke=col, sw=2.2, rx=4)
        s.text(bx + 14, yy - 3, name, 13, "bold", anchor="middle")
        s.text(bx + 14, yy + 13, sub, 9, "bold", fill=INK2, anchor="middle")
    s.rect(bx - 14, 330, 56, 44, fill="#ffffff", stroke=RETURN, sw=2.2, rx=4)
    s.text(bx + 14, 349, "A1/A2", 11, "bold", anchor="middle")
    s.text(bx + 14, 364, "jacks", 9, "bold", fill=INK2, anchor="middle")
    # right edge connectors
    s.rect(bx + bw - 40, 150, 54, 60, fill="#ffffff", stroke=CONTROL, sw=2.2, rx=4)
    s.text(bx + bw - 13, 177, "J3", 13, "bold", anchor="middle")
    s.text(bx + bw - 13, 194, "5V·I²C", 9, "bold", fill=INK2, anchor="middle")
    s.rect(bx + bw - 40, 420, 54, 44, fill="#ffffff", stroke=CONTROL, sw=2.2, rx=4)
    s.text(bx + bw - 13, 439, "J5", 13, "bold", anchor="middle")
    s.text(bx + bw - 13, 454, "pedal", 9, "bold", fill=INK2, anchor="middle")

    # head -> J1 / cap -> J2 / ears -> jacks
    s.line([(bx - 16, 130), (360, 130), (360, 160), (276, 160)], CURRENT, 3.2, arrow=True)
    s.text(394, 116, "head harness", 11, "bold", fill=CURRENT, anchor="end")
    s.text(356, 182, "insert wires,", 11, fill=INK2, anchor="end")
    s.text(356, 196, "DB25 male", 11, fill=INK2, anchor="end")
    s.line([(270, 268), (bx - 16, 250)], RETURN, 3.2, arrow=True)
    s.text(342, 244, "cap cable DB25", 11, "bold", fill=RETURN, anchor="middle")
    s.line([(282, 300), (330, 352), (bx - 16, 352)], RETURN, 2.2, dash="7 5", arrow=True)
    s.lines(326, 392, ["ear electrodes", "A1/A2 via 1.5 mm", "touch-proof jacks", "→ J2 pins 11, 24"],
            11, gap=14, fill=RETURN)

    # --- display
    dx, dy, dw, dh = 770, 76, 210, 290
    s.rect(dx, dy, dw, dh, fill=PANEL, stroke=RULE)
    s.text(dx + 16, dy + 26, "DISPLAY", 12, "bold", fill=INK2)
    s.rect(dx + 16, dy + 40, dw - 32, 108, fill="#0f1a24", stroke=INK, rx=4)
    for i, (x, y) in enumerate(sites[:20]):
        col = [GOOD, GOOD, WARN, GOOD, ALERT][i % 5]
        s.hexagon(dx + 70 + x * 40, dy + 94 + y * 40, 5, fill=col)
    s.text(dx + 150, dy + 92, "Fz", 13, "bold", fill="#ffffff", anchor="middle")
    s.text(dx + 150, dy + 110, "3.9k", 11, fill=GOOD, anchor="middle")
    s.lines(dx + 16, dy + 172, ["Elecrow CrowPanel 7.0\"", "ESP32-S3, 800×480 touch",
                                "runs all firmware"], 12, gap=17)
    s.text(dx + 16, dy + 230, "optional: 8 Ω speaker + microSD", 11, fill=INK2)
    s.text(dx + 16, dy + 256, "Its I²C cable's 3V3 wire:", 11, "bold", fill=ALERT)
    s.text(dx + 16, dy + 272, "cut it back and insulate it", 11, "bold", fill=ALERT)

    # I2C from display to J3, GND to J5
    s.line([(dx, 180), (bx + bw + 16, 180)], CONTROL, 2.6, arrow=True, arrow_start=True)
    s.text((dx + bx + bw + 14) / 2, 170, "SDA · SCL", 11, "bold", fill=CONTROL, anchor="middle")
    s.text((dx + bx + bw + 14) / 2, 200, "GPIO19/20", 10, fill=INK2, anchor="middle")
    s.line([(dx + 30, dy + dh), (dx + 30, 442), (bx + bw + 16, 442)], CONTROL, 1.8, dash="3 4")
    s.text(dx + 38, 436, "I²C GND → J5 pin 2", 11, fill=CONTROL)
    s.text(dx + 38, 462, "J5 pin 1: footswitch (opt.)", 11, fill=INK2)

    # --- power
    px, py = 560, 548
    s.rect(px, py, 230, 50, fill="#ffffff", stroke=POWER, sw=1.8)
    s.text(px + 115, py + 22, "Two-port 5 V USB charger", 12, "bold", anchor="middle")
    s.text(px + 115, py + 39, "one supply, shared ground", 11, fill=INK2, anchor="middle")
    s.line([(px + 200, py), (px + 200, 520), (dx + 175, 520), (dx + 175, dy + dh + 2)], POWER, 2.2, arrow=True)
    s.text(dx + 183, 505, "USB-C", 11, fill=INK2)
    s.line([(px + 40, py), (px + 40, 520), (bx + bw + 40, 520), (bx + bw + 40, 214)], POWER, 2.2, arrow=True)
    s.text(px + 48, 538, "USB-A pigtail → J3 pins 1, 2", 11, fill=INK2)

    legend(s, 24, 548, [(CURRENT, "test current out (and Kelvin sense) — phantom side", None),
                        (RETURN, "test current back — cap side", None),
                        (CONTROL, "I²C bus", None)])
    s.save(os.path.join(OUT, "system-overview.svg"))


# ----------------------------------------------------------------------------
def block(s, x, y, w, h, title, sub=None, color=RULE, fill=BOX, size=12.5):
    s.rect(x, y, w, h, fill=fill, stroke=color, sw=2)
    if sub:
        s.text(x + w / 2, y + h / 2 - 3, title, size, "bold", anchor="middle")
        if isinstance(sub, str):
            sub = [sub]
        for i, t in enumerate(sub):
            s.text(x + w / 2, y + h / 2 + 13 + i * 14, t, 11, fill=INK2, anchor="middle")
    else:
        s.text(x + w / 2, y + h / 2 + 4, title, size, "bold", anchor="middle")


def carrier_block_diagram():
    s = SVG(1000, 650, "Carrier board block diagram: drive, sense and return paths")
    s.text(24, 34, "Inside the carrier board", 20, "bold")
    s.text(24, 56, "One site at a time: a known AC current is pushed out through the phantom insert and back through "
           "the cap electrode,", 13, fill=INK2)
    s.text(24, 74, "while the voltage across that loop is read at the connector pin itself.", 13, fill=INK2)

    yD, yS, yR = 140, 265, 420
    # drive row
    block(s, 20, yD - 28, 128, 56, "M1  MCP4725", ["12-bit DAC, 31.25 Hz"], CURRENT)
    block(s, 172, yD - 28, 92, 56, "R7 + C9", ["low-pass"], CURRENT)
    block(s, 288, yD - 28, 92, 56, "U9A", ["buffer"], CURRENT)
    block(s, 404, yD - 28, 132, 56, "U9B Howland", ["current pump ±2.5 µA"], CURRENT)
    block(s, 560, yD - 28, 82, 56, "R17", ["150 k limit"], CURRENT)
    block(s, 666, yD - 28, 128, 56, "MUX-D  U1+U2", ["32 → 1"], CURRENT)
    for x0, x1 in [(148, 172), (264, 288), (380, 404), (536, 560), (642, 666)]:
        s.line([(x0, yD), (x1 - 2, yD)], CURRENT, 3, arrow=True)
    # J1 / J2 connectors
    s.rect(836, yD - 24, 52, 48, fill=BOX, stroke=CURRENT, sw=2.2, rx=4)
    s.text(862, yD + 5, "J1", 13, "bold", anchor="middle")
    s.rect(836, yR - 24, 52, 48, fill=BOX, stroke=RETURN, sw=2.2, rx=4)
    s.text(862, yR + 5, "J2", 13, "bold", anchor="middle")
    s.line([(794, yD), (834, yD)], CURRENT, 3, arrow=True)
    s.dot(815, yD, SENSE, 5)
    s.text(815, yD - 40, "Kelvin", 10, "bold", fill=SENSE, anchor="middle")
    s.text(815, yD - 28, "junction", 10, "bold", fill=SENSE, anchor="middle")

    # off-board stack
    s.rect(906, 92, 84, 372, fill=PANEL, stroke=RULE, rx=8, dash="5 4")
    s.text(948, 112, "OFF-BOARD", 10, "bold", fill=INK2, anchor="middle")
    for (yy, t1, t2) in [(185, "phantom", "insert"), (262, "gel", ""), (338, "cap", "electrode")]:
        s.rect(914, yy - 26, 68, 52, fill=BOX, stroke=INK, sw=1.5)
        s.text(948, yy - 2 if t2 else yy + 4, t1, 12, "bold", anchor="middle")
        if t2:
            s.text(948, yy + 14, t2, 12, "bold", anchor="middle")
    s.line([(888, yD), (948, yD), (948, 157)], CURRENT, 3, arrow=True)
    s.line([(948, 211), (948, 234)], CURRENT, 3, arrow=True)
    s.line([(948, 288), (948, 310)], CURRENT, 3, arrow=True)
    s.line([(948, 364), (948, yR), (890, yR)], RETURN, 3, arrow=True)
    s.text(948, 482, "the part the", 11, fill=INK2, anchor="middle")
    s.text(948, 496, "student controls", 11, fill=INK2, anchor="middle")

    # sense row
    s.line([(815, yD), (815, yS), (796, yS)], SENSE, 2.4, arrow=True)
    block(s, 666, yS - 28, 128, 56, "MUX-S  U3+U4", ["no current flows"], SENSE)
    block(s, 484, yS - 28, 152, 56, "U10A sense amp", ["gain 1 (R8 linked)"], SENSE)
    block(s, 20, yS - 28, 150, 56, "M2  ADS1115 0x48", ["loop voltage"], SENSE)
    s.line([(666, yS), (638, yS)], SENSE, 2.4, arrow=True)
    s.line([(484, yS), (172, yS)], SENSE, 2.4, arrow=True)

    # return row
    block(s, 666, yR - 28, 128, 56, "MUX-R  U5+U6", ["32 → 1"], RETURN)
    block(s, 484, yR - 28, 152, 56, "U7/U8 mode switches", ["S5 = Mode A, S1–S4 = B"], RETURN)
    block(s, 300, yR - 28, 154, 56, "U9C transimpedance", ["R18 200 k → volts"], RETURN)
    block(s, 20, yR - 28, 150, 56, "M3  ADS1115 0x49", ["loop current"], RETURN)
    for x0, x1 in [(836, 796), (666, 638), (484, 456), (300, 172)]:
        s.line([(x0, yR), (x1, yR)], RETURN, 3, arrow=True)
    s.text(862, yR + 44, "ears A1/A2", 10, fill=INK2, anchor="middle")
    s.text(862, yR + 57, "pins 11, 24", 10, fill=INK2, anchor="middle")

    # cal bank
    s.rect(666, 310, 128, 70, fill=BOX, stroke=RULE, sw=1.5, dash="5 4")
    s.text(730, 328, "CAL BANK, ch 22–26", 10.5, "bold", fill=INK2, anchor="middle")
    s.text(730, 344, "D ch n → R → R ch n", 10.5, fill=INK2, anchor="middle")
    s.text(730, 358, "1k · 4.7k · 100k", 10.5, fill=INK2, anchor="middle")
    s.text(730, 372, "open · short", 10.5, fill=INK2, anchor="middle")

    # VREF + control + bus
    block(s, 300, 486, 154, 56, "U9D  VREF 1.65 V", ["reference for every stage"], INK2)
    block(s, 20, 486, 250, 56, "U11 / U12  MCP23017", ["mux selects · S1–S6 · LEDs · piezo · pedal"], CONTROL)
    s.line([(145, 542), (145, 600), (560, 600), (560, yR + 30)], CONTROL, 1.8, dash="4 4", arrow=True)
    s.line([(560, 600), (730, 600), (730, yR + 30)], CONTROL, 1.8, dash="4 4", arrow=True)
    s.text(740, 590, "select / enable lines", 11, fill=CONTROL)
    s.text(740, 604, "(all three mux banks)", 11, fill=CONTROL)
    s.text(24, 636, "I²C to the display (via J3): M1, M2, M3, U11, U12.   Both ADC readings are taken "
           "against VREF, so Z = (V / gain) ÷ (V_TIA / 200 k).", 11.5, fill=INK2)
    s.save(os.path.join(OUT, "carrier-block-diagram.svg"))


# ----------------------------------------------------------------------------
def analog_front_end():
    s = SVG(1040, 720, "Analog front end schematic, redrawn")
    s.text(24, 34, "Analog front end", 20, "bold")
    s.text(24, 56, "Redrawn for reading. Values are the as-built bench board (R8 linked, gain 1). "
           "U9 and U10 are MCP6004; pin numbers in brackets.", 13, fill=INK2)

    # ---- drive: DAC -> RC -> U9A buffer
    s.text(24, 92, "EXCITATION AND CURRENT PUMP", 11, "bold", fill=INK2)
    s.rect(20, 124, 96, 44, fill=BOX, stroke=CURRENT, sw=2)
    s.text(68, 143, "M1 VOUT", 12, "bold", anchor="middle")
    s.text(68, 159, "MCP4725", 11, fill=INK2, anchor="middle")
    s.text(20, 186, "VREF ± 0.25 V,", 11, fill=INK2)
    s.text(20, 200, "flips every 16 ms", 11, fill=INK2)
    s.line([(116, 146), (128, 146)], INK, 1.8)
    s.resistor(158, 146, 60, label="R7", value="10k")
    s.line([(188, 146), (240, 146)], INK, 1.8)
    s.dot(205, 146)
    b = s.capacitor(205, 146)
    s.text(188, 166, "C9", 12, "bold", anchor="end")
    s.text(188, 180, "47n", 12, fill=INK2, anchor="end")
    s.ground(*b)
    a = s.opamp(240, 162, 64, "U9A", plus_top=True)
    s.text(272, 210, "buffer [3,2→1]", 11, fill=INK2, anchor="middle")
    out = a["out"]
    s.line([out, (318, 162)], INK, 1.8)
    s.dot(318, 162)
    s.line([(318, 162), (318, 196), (228, 196), (228, 178), (240, 178)], INK, 1.8)

    # ---- Howland pump U9B (+ on top)
    s.line([(318, 162), (340, 162), (340, 214), (364, 214)], INK, 1.8)
    s.resistor(394, 214, 60, label="R3", value="100k")
    s.line([(424, 214), (470, 214)], INK, 1.8)
    s.dot(450, 214, CURRENT, 4.5)
    pb = s.opamp(470, 232, 72, "U9B", plus_top=True)
    s.text(506, 300 + 36, "Howland pump [5,6→7]", 11, fill=INK2, anchor="middle")
    s.line([(542, 232), (575, 232)], INK, 1.8)
    s.dot(575, 232)
    # R4 over the top: output -> + node
    s.line([(575, 232), (575, 180), (546, 180)], INK, 1.8)
    s.resistor(516, 180, 60, label="R4", value="100k")
    s.line([(486, 180), (450, 180), (450, 214)], INK, 1.8)
    s.dot(450, 180)
    # R1 from - node to VREF, R2 from - node to output (under)
    s.line([(470, 250), (450, 250), (424, 250)], INK, 1.8)
    s.dot(450, 250)
    s.resistor(394, 250, 60, label="R1", value="100k", lpos="below")
    s.line([(364, 250), (356, 250)], INK, 1.8)
    s.text(352, 254, "VREF", 11, "bold", fill=INK2, anchor="end")
    s.line([(450, 250), (450, 290), (486, 290)], INK, 1.8)
    s.resistor(516, 290, 60, label="R2", value="100k", lpos="below")
    s.line([(546, 290), (575, 290), (575, 232)], INK, 1.8)
    s.text(600, 266, "R1 = R2 = R3 = R4  ⇒", 11.5, fill=INK2)
    s.text(600, 282, "I = (V_U9A − VREF) / R3 = ±2.5 µA,", 11.5, "bold", fill=CURRENT)
    s.text(600, 298, "set by the DAC, not by the load", 11.5, fill=INK2)

    # ---- load path: + node -> R17 -> MUX-D -> J1
    s.line([(450, 180), (450, 120), (600, 120)], CURRENT, 2.6)
    s.resistor(630, 120, 60, color=CURRENT, label="R17", value="150k limit")
    s.line([(660, 120), (690, 120)], CURRENT, 2.6, arrow=True)
    s.rect(692, 100, 104, 40, fill=BOX, stroke=CURRENT, sw=2)
    s.text(744, 118, "MUX-D", 12, "bold", anchor="middle")
    s.text(744, 132, "U1+U2", 11, fill=INK2, anchor="middle")
    s.line([(796, 120), (950, 120), (950, 148)], CURRENT, 2.6, arrow=True)
    s.dot(852, 120, SENSE, 5.5)
    s.text(852, 104, "J1 pin n", 12, "bold", anchor="middle")
    s.text(852, 90, "Kelvin junction", 10.5, "bold", fill=SENSE, anchor="middle")
    s.rect(900, 150, 104, 84, fill=PANEL, stroke=INK, sw=1.8, rx=6)
    s.text(952, 176, "Z_site", 14, "bold", anchor="middle")
    s.lines(952, 196, ["insert", "+ gel", "+ electrode"], 11, gap=13, fill=INK2, anchor="middle")

    # ---- sense: J1 junction -> MUX-S -> U10A
    s.text(24, 372, "VOLTAGE SENSE  (no current flows here, so mux resistance drops out)", 11, "bold", fill=SENSE)
    s.line([(852, 120), (852, 402), (822, 402)], SENSE, 2.4, arrow=True)
    s.rect(730, 382, 92, 40, fill=BOX, stroke=SENSE, sw=2)
    s.text(776, 400, "MUX-S", 12, "bold", anchor="middle")
    s.text(776, 414, "U3+U4", 11, fill=INK2, anchor="middle")
    s.line([(730, 402), (692, 402)], SENSE, 2.4)
    u = s.opamp_left(692, 418, 64, "U10A", plus_top=True)
    s.line([u["-"], (708, 434), (708, 474)], INK, 1.8)
    s.dot(708, 474)
    s.line([(708, 474), (690, 474)], INK, 1.8)
    s.resistor(660, 474, 60, label=None)
    s.text(660, 466, "R8 300k", 11.5, "bold", anchor="middle")
    s.line([(630, 474), (614, 474), (614, 418)], INK, 1.8)
    s.dot(614, 418)
    s.line([(708, 474), (708, 498), (614, 498), (614, 474)], ALERT, 2.2)
    s.text(661, 513, "wire link across R8", 11, "bold", fill=ALERT, anchor="middle")
    s.line([(708, 474), (736, 474)], INK, 1.8)
    s.resistor(766, 474, 60)
    s.text(766, 466, "R9 100k", 11.5, "bold", anchor="middle")
    s.line([(796, 474), (806, 474)], INK, 1.8)
    s.text(810, 478, "VREF", 11, "bold", fill=INK2)
    s.text(660, 536, "gain = 1 + R8/R9: 4 stock, 1 linked", 11, fill=INK2, anchor="middle")
    s.line([(628, 418), (614, 418), (566, 418)], SENSE, 2.4, arrow=True)
    s.rect(400, 396, 164, 44, fill=BOX, stroke=SENSE, sw=2)
    s.text(482, 414, "M2 ADS1115 0x48", 12, "bold", anchor="middle")
    s.text(482, 430, "AIN0 sense · AIN1 VREF", 10.5, fill=INK2, anchor="middle")

    # ---- return: load -> J2 -> MUX-R -> S5 -> TIA
    s.text(24, 648, "RETURN CURRENT  (measured, not assumed)", 11, "bold", fill=RETURN)
    s.line([(952, 234), (952, 620), (824, 620)], RETURN, 2.6, arrow=True)
    s.text(960, 606, "J2 pin n", 12, "bold")
    s.rect(730, 600, 92, 40, fill=BOX, stroke=RETURN, sw=2)
    s.text(776, 618, "MUX-R", 12, "bold", anchor="middle")
    s.text(776, 632, "U5+U6", 11, fill=INK2, anchor="middle")
    s.line([(730, 620), (706, 620)], RETURN, 2.6)
    s.switch(706, 680, 620, RETURN, closed=True)
    s.text(693, 606, "S5", 11, "bold", fill=RETURN, anchor="middle")
    s.line([(680, 620), (624, 620)], RETURN, 2.6)
    s.dot(638, 620, RETURN, 4.5)
    t = s.opamp_left(624, 636, 64, "U9C", plus_top=False)
    s.line([(638, 620), (638, 580), (606, 580)], INK, 1.8)
    s.resistor(576, 580, 60)
    s.text(576, 572, "R18 200k", 11.5, "bold", anchor="middle")
    s.line([(546, 580), (546, 636)], INK, 1.8)
    s.dot(546, 636)
    s.line([t["+"], (640, 652), (650, 652)], INK, 1.8)
    s.text(654, 656, "VREF", 11, "bold", fill=INK2)
    s.text(600, 690, "virtual ground held at VREF  [10,9→8]", 11, fill=INK2, anchor="middle")
    s.line([(560, 636), (546, 636), (520, 636)], RETURN, 2.4, arrow=True)
    s.rect(352, 614, 166, 44, fill=BOX, stroke=RETURN, sw=2)
    s.text(435, 632, "M3 ADS1115 0x49", 12, "bold", anchor="middle")
    s.text(435, 648, "AIN0 TIA out · AIN1 VREF", 10.5, fill=INK2, anchor="middle")

    # ---- VREF generator
    s.rect(20, 404, 300, 210, fill=PANEL, stroke=RULE, rx=8, dash="5 4")
    s.text(34, 424, "VREF GENERATOR  U9D [12,13→14]", 11, "bold", fill=INK2)
    s.text(48, 446, "+3.3 V", 11, "bold")
    s.line([(60, 452), (60, 462)], INK, 1.8)
    s.resistor(60, 490, 56, vertical=True, label="R5", value="100k")
    s.dot(60, 518)
    s.resistor(60, 546, 56, vertical=True, label="R6", value="100k")
    s.ground(60, 574)
    s.line([(60, 518), (130, 518)], INK, 1.8)
    s.dot(130, 518)
    c = s.capacitor(130, 518, label="C10", value="10µ")
    s.ground(*c)
    s.line([(130, 518), (196, 518)], INK, 1.8)
    d = s.opamp(196, 534, 60, "U9D", plus_top=True)
    s.line([(256, 534), (270, 534)], INK, 1.8)
    s.dot(270, 534)
    s.line([(270, 534), (270, 570), (184, 570), (184, 549), (196, 549)], INK, 1.8)
    s.line([(270, 534), (296, 534)], INK, 1.8, arrow=True)
    s.text(232, 598, "VREF = 1.65 V", 12, "bold", anchor="middle")

    s.text(24, 708, "Z = (V_sense / gain) ÷ (V_TIA / 200 k) = 200 000 × V_sense / V_TIA at gain 1. "
           "Self-calibration then maps raw Z onto R20 and R22.", 11.5, fill=INK2)
    s.save(os.path.join(OUT, "analog-front-end.svg"))


# ----------------------------------------------------------------------------
GEL, GEL_EDGE, ELEC = "#d3f0e8", "#7cc9b6", "#c8c8c8"


def _site(s, x, name, gel=True, label_left=False):
    s.hexagon(x, 160, 18, fill="#3a3a3a")
    lx, anc = (x - 24, "end") if label_left else (x + 24, "start")
    s.text(lx, 165, f"insert {name}", 11, "bold", anchor=anc)
    if gel:
        s.add(f'<ellipse cx="{x}" cy="{200}" rx="26" ry="12" fill="{GEL}" stroke="{GEL_EDGE}" stroke-width="1.5"/>')
    s.circle(x, 238, 14, fill=ELEC, stroke="#555555", sw=1.5)
    s.text(lx, 243, f"electrode {name}", 11, fill=INK2, anchor=anc)


def _drive(s, x):
    s.line([(x, 104), (x, 140)], CURRENT, 3, arrow=True)
    s.text(x, 100 - 2, "drive (MUX-D)", 10.5, "bold", fill=CURRENT, anchor="middle")


def _board(s, ox, tia_x):
    s.rect(ox + 10, 300, PW - 20, 176, fill=PANEL, stroke=RULE, rx=8, dash="5 4")
    s.text(ox + 22, 318, "ON THE BOARD", 10, "bold", fill=INK2)
    s.rect(tia_x, 420, 96, 40, fill=BOX, stroke=RETURN, sw=2)
    s.text(tia_x + 48, 445, "TIA → I", 12, "bold", anchor="middle")


PW = 330


def measurement_modes():
    s = SVG(1030, 590, "Three ways the same switch fabric routes the test current")
    s.text(24, 34, "Three measurements from one switch fabric", 20, "bold")
    s.text(24, 56, "Each insert is isolated in plastic, so current can only cross from the phantom to the cap "
           "through gel. Where it is allowed back decides what is measured.", 13, fill=INK2)
    for ox, title in [(10, "MODE A"), (350, "MODE B"), (690, "BRIDGE CHECK")]:
        s.rect(ox, 76, PW, 504, fill="#ffffff", stroke=RULE, rx=10)
        s.text(ox + PW - 14, 94, title, 11, "bold", fill=INK2, anchor="end")
    # ---------------- Mode A
    ox = 10
    x1 = ox + 90
    _site(s, x1, "X")
    _drive(s, x1)
    _board(s, ox, ox + 200)
    s.rect(ox + 40, 340, 100, 36, fill=BOX, stroke=RETURN, sw=2)
    s.text(ox + 90, 363, "MUX-R ch X", 11.5, "bold", anchor="middle")
    s.line([(x1, 252), (x1, 338)], RETURN, 3, arrow=True)
    s.line([(ox + 140, 358), (ox + 170, 358)], RETURN, 3)
    s.switch(ox + 170, ox + 200, 358, RETURN, closed=True)
    s.text(ox + 185, 348, "S5", 11, "bold", fill=RETURN, anchor="middle")
    s.line([(ox + 200, 358), (ox + 248, 358), (ox + 248, 418)], RETURN, 3, arrow=True)
    s.lines(ox + 22, 506, ["Z = Z_X  (plus the return path,", "removed by self-calibration and",
                           "the per-site zero)"], 12, gap=16)
    # ---------------- Mode B
    ox = 350
    x1, x2 = ox + 70, ox + 210
    _site(s, x1, "X", label_left=False)
    _site(s, x2, "A1")
    _drive(s, x1)
    _board(s, ox, ox + 150)
    s.rect(ox + 22, 340, 96, 36, fill=BOX, stroke=RETURN, sw=2)
    s.text(ox + 70, 363, "MUX-R ch X", 11.5, "bold", anchor="middle")
    s.line([(x1, 252), (x1, 338)], RETURN, 3, arrow=True)
    s.line([(ox + 118, 358), (ox + 140, 358)], RETURN, 3)
    s.switch(ox + 140, ox + 166, 358, RETURN, closed=True)
    s.text(ox + 153, 348, "S1", 11, "bold", fill=RETURN, anchor="middle")
    s.line([(ox + 166, 358), (x2, 358), (x2, 254)], RETURN, 3, arrow=True)
    s.text(x2 + 8, 292, "J2 pin 11", 10.5, fill=INK2)
    s.line([(x2, 142), (x2, 118), (ox + 312, 118), (ox + 312, 440), (ox + 290, 440)], RETURN, 3)
    s.text(ox + 304, 112, "J1 pin 11", 10.5, fill=INK2, anchor="end")
    s.switch(ox + 290, ox + 264, 440, RETURN, closed=True)
    s.text(ox + 277, 430, "S3", 11, "bold", fill=RETURN, anchor="middle")
    s.line([(ox + 264, 440), (ox + 248, 440)], RETURN, 3, arrow=True)
    s.lines(ox + 22, 506, ["via A1:  Z_X + Z_A1    (S1 + S3)", "via A2:  Z_X + Z_A2    (S2 + S4)",
                           "ears:    Z_A1 + Z_A2", "Z_X = ½ (Z_XA1 + Z_XA2 − Z_A1A2)"], 12, gap=16)
    # ---------------- Salt bridge
    ox = 690
    x1, x2 = ox + 70, ox + 210
    _site(s, x1, "X", gel=False)
    _site(s, x2, "Y", gel=False)
    s.add(f'<path d="M{x1 - 26} 200 C {x1 - 26} 184, {x2 + 26} 184, {x2 + 26} 200 C {x2 + 26} 216, '
          f'{x1 - 26} 216, {x1 - 26} 200 z" fill="{GEL}" stroke="{ALERT}" stroke-width="2" '
          f'stroke-dasharray="6 4"/>')
    s.text((x1 + x2) / 2, 205, "gel bridge", 11, "bold", fill=ALERT, anchor="middle")
    _drive(s, x1)
    _board(s, ox, ox + 30)
    s.rect(ox + 162, 340, 96, 36, fill=BOX, stroke=RETURN, sw=2)
    s.text(ox + 210, 363, "MUX-R ch Y", 11.5, "bold", anchor="middle")
    s.line([(x2, 252), (x2, 338)], RETURN, 3, arrow=True)
    s.line([(x1, 252), (x1, 300)], INK2, 1.6, dash="3 4")
    s.text(x1 + 6, 292, "ch X not selected", 10.5, fill=INK2)
    s.line([(ox + 210, 376), (ox + 210, 400), (ox + 180, 400)], RETURN, 3)
    s.switch(ox + 180, ox + 154, 400, RETURN, closed=True)
    s.text(ox + 167, 390, "S5", 11, "bold", fill=RETURN, anchor="middle")
    s.line([(ox + 154, 400), (ox + 100, 400), (ox + 100, 418)], RETURN, 3, arrow=True)
    s.lines(ox + 22, 506, ["no bridge → no path → OPEN", "bridge → reads the gel path:",
                           "trips below 50 k, clears above 80 k", "(the gap stops it chattering)"], 12, gap=16)
    s.save(os.path.join(OUT, "measurement-modes.svg"))


# ----------------------------------------------------------------------------
def _header(s, x, y, name, pins, color=INK, pitch=34, note=None):
    """Vertical pin header: returns {label: (x, y)} of each pin's left edge."""
    h = pitch * len(pins) + 16
    s.rect(x, y, 150, h, fill=BOX, stroke=color, sw=2)
    s.text(x + 75, y - 10, name, 13, "bold", anchor="middle")
    if note:
        s.text(x + 75, y + h + 18, note, 11, fill=INK2, anchor="middle")
    out = {}
    for i, (num, lab) in enumerate(pins):
        py = y + 16 + i * pitch + 8
        s.rect(x + 10, py - 8, 16, 16, fill="#d4a72c" if i else "#d4a72c", stroke=INK, sw=1, rx=2 if i else 0)
        s.text(x + 36, py + 4, f"{num}", 12, "bold")
        s.text(x + 54, py + 4, lab, 12, fill=INK2)
        out[num] = (x, py)
    return out


def display_wiring():
    s = SVG(980, 520, "Wiring the CrowPanel display and the charger to the carrier board")
    s.text(24, 34, "Display and power wiring", 20, "bold")
    s.text(24, 56, "Four signals and two supply wires. The CrowPanel has no 5 V pin, so the carrier is "
           "fed from the charger's second port.", 13, fill=INK2)

    # CrowPanel I2C connector
    s.rect(24, 96, 230, 196, fill=PANEL, stroke=RULE)
    s.text(40, 120, "CrowPanel 7.0\"  I2C connector", 12, "bold")
    s.text(40, 136, "go by the silkscreen labels", 11, fill=INK2)
    cp = {}
    for i, lab in enumerate(["SCL · IO20", "SDA · IO19", "3V3", "GND"]):
        py = 166 + i * 34
        s.rect(200, py - 9, 18, 18, fill="#ffffff", stroke=INK, sw=1.5, rx=3)
        s.text(190, py + 4, lab, 12, "bold" if lab != "3V3" else "bold", anchor="end",
               fill=ALERT if lab == "3V3" else INK)
        cp[lab] = (218, py)

    # charger pigtail
    s.rect(24, 330, 230, 120, fill=PANEL, stroke=RULE)
    s.text(40, 354, "Charger port 2", 12, "bold")
    s.text(40, 370, "USB-A to bare-wire pigtail", 11, fill=INK2)
    ch = {}
    for i, lab in enumerate(["+5 V", "GND"]):
        py = 400 + i * 30
        s.rect(200, py - 9, 18, 18, fill="#ffffff", stroke=INK, sw=1.5, rx=3)
        s.text(190, py + 4, lab, 12, "bold", anchor="end")
        ch[lab] = (218, py)
    s.text(40, 470, "measure which lead is + with a meter;", 11, fill=INK2)
    s.text(40, 484, "pigtail colours are not reliable", 11, fill=INK2)

    # carrier headers
    j3 = _header(s, 640, 96, "J3  (carrier)", [(1, "5V in"), (2, "GND"), (3, "SDA"), (4, "SCL"),
                                               (5, "n/c (was DRDY)"), (6, "n/c")], CONTROL)
    j5 = _header(s, 640, 356, "J5  (carrier)", [(1, "FOOTSW"), (2, "GND")], CONTROL)

    def wire(a, b, color, label=None, sw=2.6, mid=None):
        mx = mid or (a[0] + b[0]) / 2
        s.line([a, (mx, a[1]), (mx, b[1]), (b[0] - 2, b[1])], color, sw)
        s.dot(*a, color=color, r=3)
        if label:
            s.text(mx + 8, (a[1] + b[1]) / 2, label, 11, fill=color)

    wire(cp["SDA · IO19"], j3[3], CONTROL, mid=440)
    wire(cp["SCL · IO20"], j3[4], CONTROL, mid=470)
    wire(cp["GND"], j5[2], CONTROL, mid=500)
    wire(ch["+5 V"], j3[1], POWER, mid=560)
    wire(ch["GND"], j3[2], POWER, mid=590)
    # 3V3: cut
    x0, y0 = cp["3V3"]
    s.line([(x0, y0), (x0 + 60, y0)], ALERT, 2.6)
    s.line([(x0 + 52, y0 - 9), (x0 + 70, y0 + 9)], ALERT, 3)
    s.line([(x0 + 52, y0 + 9), (x0 + 70, y0 - 9)], ALERT, 3)
    s.text(x0 + 82, y0 - 6, "cut back + insulate", 12, "bold", fill=ALERT)
    s.text(x0 + 82, y0 + 9, "never to J3 pin 1", 11, fill=ALERT)

    s.rect(24, 498 - 0, 0, 0)
    s.text(270, 500, "Why the 3V3 wire matters: J3 pin 1 is a 5 V input with no blocking diode. Tied to the "
           "display's 3.3 V rail it back-feeds", 11, fill=INK2)
    s.text(270, 514, "5 V into the ESP32-S3 (3.6 V max). If a footswitch occupies J5, land the I²C ground on "
           "J4 pin 2 instead.", 11, fill=INK2)
    s.save(os.path.join(OUT, "display-wiring.svg"))


def dac_wiring():
    s = SVG(900, 470, "Hand-wiring the MCP4725 DAC module to the M1 socket")
    s.text(24, 34, "M1: wire the DAC by label, don't plug it in", 20, "bold")
    s.text(24, 56, "The Rev B footprint swaps the module's last two pins (A0 and VOUT). Plugged straight in, "
           "VOUT lands on hole 6, which goes nowhere.", 13, fill=INK2)
    # module
    s.rect(40, 86, 600, 70, fill="#1f3a5f", stroke=INK, sw=1.5, rx=8)
    s.text(340, 112, "MCP4725 module (pin order varies by maker: read the labels)", 13, "bold",
           fill="#ffffff", anchor="middle")
    mod = ["VCC / VDD", "GND", "SCL", "SDA", "OUT / VOUT", "A0 (if fitted)"]
    xs = [90 + i * 100 for i in range(6)]
    for x, lab in zip(xs, mod):
        s.text(x, 142, lab, 11.5, "bold", fill="#ffffff", anchor="middle")
        s.dot(x, 172, "#d4a72c", 6)
        s.line([(x, 156), (x, 166)], INK, 2)
    # socket
    s.rect(40, 330, 600, 80, fill=PANEL, stroke=INK, sw=1.5, rx=8)
    sock = [("1", "+3.3 V"), ("2", "GND"), ("3", "SCL"), ("4", "SDA"), ("5", "DAC_OUT"), ("6", "unused")]
    for x, (n, lab) in zip(xs, sock):
        if n == "1":
            s.rect(x - 7, 330 - 7, 14, 14, fill="#d4a72c", stroke=INK, sw=1, rx=0)
        else:
            s.dot(x, 330, "#d4a72c", 7)
        s.text(x, 362, n, 15, "bold", anchor="middle")
        s.text(x, 384, lab, 11.5, fill=INK2 if n == "6" else SENSE, anchor="middle")
    s.text(340, 432, "M1 socket on the carrier (hole 1 = square pad)", 12, "bold", fill=INK2, anchor="middle")
    for i in range(4):
        s.line([(xs[i], 178), (xs[i], 318)], RETURN, 3, arrow=True)
    s.line([(xs[4], 178), (xs[4], 318)], CURRENT, 3.4, arrow=True)
    s.text(xs[4] + 8, 250, "OUT → hole 5", 11.5, "bold", fill=CURRENT)
    s.line([(xs[5], 178), (xs[5], 250)], INK2, 2.4)
    s.ground(xs[5], 250, INK2)
    s.text(xs[5] + 18, 238, "A0 → GND", 11.5, "bold", fill=INK2)
    s.text(xs[5] + 18, 280, "hole 6", 11.5, fill=INK2)
    s.text(xs[5] + 18, 294, "stays empty", 11.5, fill=INK2)
    s.text(720, 100, "Check with a meter,", 12, "bold")
    s.text(720, 116, "powered:", 12, "bold")
    s.lines(720, 140, ["module VCC must read", "3.3 V. About 2.2 V means", "the power wire is open",
                       "and the chip is running", "off SDA/SCL. It still", "answers on the bus.",
                       "", "Address: 0x60 (generic)", "or 0x62 (Adafruit, A0", "to GND). The firmware",
                       "searches 0x60–0x63."], 11.5, gap=15, fill=INK2)
    s.save(os.path.join(OUT, "dac-wiring.svg"))


if __name__ == "__main__":
    system_overview()
    carrier_block_diagram()
    analog_front_end()
    measurement_modes()
    display_wiring()
    dac_wiring()
