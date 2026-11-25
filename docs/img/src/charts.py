"""Generates the data charts in docs/img/*.svg with matplotlib.

    python3 docs/img/src/charts.py

Every number comes from the firmware (firmware/phantom_meter_v2/phantom_meter_v2.ino)
or from the dated bench notes in docs/changelog.md; nothing is simulated except
where a chart says "illustrative".
"""
import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, Rectangle

OUT = os.path.join(os.path.dirname(__file__), "..")

INK, INK2, GRID, AXIS = "#191919", "#666666", "#e7e7e7", "#c2c2c2"
BLUE, ORANGE = "#257de0", "#f47230"
GOOD, WARN, CRIT, OPEN = "#0aac23", "#fbb315", "#dc3c3d", "#9c9c9c"
TIER = {1: "#0f4a96", 2: "#338ced", 3: "#84b9f3"}       # one-hue ordinal ramp, dark = steepest

plt.rcParams.update({
    "font.family": ["Inter", "DejaVu Sans"], "font.size": 11, "text.color": INK,
    "axes.edgecolor": AXIS, "axes.labelcolor": INK2, "xtick.color": INK2, "ytick.color": INK2,
    "axes.spines.top": False, "axes.spines.right": False, "svg.fonttype": "path",
    "figure.facecolor": "white", "axes.facecolor": "white",
})

# --- straight from phantom_meter_v2.ino -------------------------------------
SITES = [("Fp1", -0.31, -0.95), ("Fp2", 0.31, -0.95), ("F7", -0.81, -0.59), ("F3", -0.40, -0.53),
         ("Fz", 0.00, -0.50), ("F4", 0.40, -0.53), ("F8", 0.81, -0.59), ("T3", -1.00, 0.00),
         ("C3", -0.50, 0.00), ("Cz", 0.00, 0.00), ("C4", 0.50, 0.00), ("T4", 1.00, 0.00),
         ("T5", -0.81, 0.59), ("P3", -0.40, 0.53), ("Pz", 0.00, 0.50), ("P4", 0.40, 0.53),
         ("T6", 0.81, 0.59), ("O1", -0.31, 0.88), ("O2", 0.31, 0.88), ("GND", 0.00, -0.80),
         ("A1", -1.28, 0.30), ("A2", 1.28, 0.30)]
BRIDGES = [(10, 11, 1), (8, 7, 1), (7, 20, 1), (11, 21, 1), (4, 0, 1), (4, 1, 1), (4, 19, 1), (5, 6, 1),
           (15, 16, 1), (3, 2, 1), (13, 12, 1), (5, 1, 1), (3, 0, 1), (14, 17, 1), (14, 18, 1),
           (15, 18, 2), (13, 17, 2), (12, 20, 2), (16, 21, 2), (8, 12, 2), (10, 16, 2), (19, 1, 2), (19, 0, 2),
           (9, 3, 3), (9, 5, 3), (9, 15, 3), (9, 13, 3), (9, 4, 3), (9, 10, 3), (9, 14, 3), (9, 8, 3)]
SITE_GND = 19
HALF_PERIOD_MS, SETTLE_MS = 16, 12


def salt_bridge_map():
    fig, ax = plt.subplots(figsize=(8.2, 7.4))
    ax.set_aspect("equal")
    ax.axis("off")
    head = plt.Circle((0, 0), 1.08, fill=False, lw=1.5, color=AXIS)
    ax.add_patch(head)
    ax.plot([-0.12, 0, 0.12], [1.075, 1.22, 1.075], color=AXIS, lw=1.5)   # nose (front = up)
    for up, down, tier in sorted(BRIDGES, key=lambda b: -b[2]):
        (n1, x1, y1), (n2, x2, y2) = SITES[up], SITES[down]
        gnd = SITE_GND in (up, down)
        arr = FancyArrowPatch((x1, -y1), (x2, -y2), arrowstyle="-|>", mutation_scale=13,
                              shrinkA=17, shrinkB=17, lw=2.2 if not gnd else 1.6,
                              color=OPEN if gnd else TIER[tier], linestyle="--" if gnd else "-",
                              zorder=2)
        ax.add_patch(arr)
    for name, x, y in SITES:
        ax.scatter([x], [-y], s=820, marker="H", color="#3a3a3a", edgecolors="white", linewidths=2, zorder=3)
        ax.text(x, -y, name, ha="center", va="center", color="white", fontsize=9.5, fontweight="bold",
                zorder=4)
    ax.set_xlim(-1.5, 1.5)
    ax.set_ylim(-1.32, 1.3)
    handles = [plt.Line2D([], [], color=TIER[1], lw=2.4, label="tier 1: steepest, checked every sweep"),
               plt.Line2D([], [], color=TIER[2], lw=2.4, label="tier 2: every 2nd sweep"),
               plt.Line2D([], [], color=TIER[3], lw=2.4, label="tier 3: flat top, every 3rd sweep"),
               plt.Line2D([], [], color=OPEN, lw=1.6, ls="--", label="GND pairs: off by default")]
    ax.legend(handles=handles, loc="lower center", bbox_to_anchor=(0.5, -0.11), ncol=2, frameon=False,
              fontsize=10, handlelength=2.4, labelcolor=INK2)
    fig.suptitle("Salt-bridge pairs: arrows point downhill, where gel pools", x=0.06, ha="left",
                 fontsize=15, fontweight="bold", y=0.97)
    fig.text(0.06, 0.915, "31 pairs from the firmware table, ranked by slope on a hemisphere model. "
             "Seen from above, nose up.\nFp1–Fp2 and O1–O2 are left out on purpose: level pairs, "
             "already covered by the midline routes.", fontsize=10, color=INK2, va="top")
    fig.subplots_adjust(left=0.02, right=0.98, top=0.86, bottom=0.08)
    fig.savefig(os.path.join(OUT, "salt-bridge-map.svg"))
    plt.close(fig)


def sync_detection():
    """Illustrative: ideal square wave with the firmware's own timing constants."""
    import numpy as np
    t = np.linspace(0, 96, 4801)
    sign = np.where((t // HALF_PERIOD_MS) % 2 == 0, 1.0, -1.0)
    i_ua = 2.5 * sign
    z_site, v_dc = 5000.0, 440.0                       # ohms; electrode offset in mV (bench, 8 Nov 2026)
    v_mv = v_dc + i_ua * 1e-6 * z_site * 1e3
    ts = np.arange(SETTLE_MS, 96, HALF_PERIOD_MS)
    ss = np.where((ts // HALF_PERIOD_MS) % 2 == 0, 1.0, -1.0)

    fig, (a1, a2) = plt.subplots(2, 1, figsize=(8.6, 5.6), sharex=True,
                                 gridspec_kw={"height_ratios": [1, 1.15], "hspace": 0.35})
    for ax in (a1, a2):
        for k in range(6):
            ax.axvspan(k * HALF_PERIOD_MS, k * HALF_PERIOD_MS + SETTLE_MS, color="#f2f2f2", lw=0, zorder=0)
        ax.grid(axis="y", color=GRID, lw=0.8)
        ax.set_axisbelow(True)
    a1.plot(t, i_ua, color=ORANGE, lw=2)
    a1.set_ylim(-3.6, 3.6)
    a1.set_yticks([-2.5, 0, 2.5])
    a1.set_ylabel("µA")
    a1.set_title("Test current through the site: ±2.5 µA square wave, 31.25 Hz", loc="left", fontsize=11.5,
                 fontweight="bold", color=INK)
    a1.text(1, 3.05, "settling, not sampled (12 ms)", fontsize=9.5, color=INK2)
    a2.plot(t, v_mv, color=BLUE, lw=2)
    a2.scatter(ts, v_dc + ss * 12.5, s=60, color=BLUE, edgecolors="white", linewidths=2, zorder=3)
    for x, sg in zip(ts, ss):
        a2.text(x + 0.8, v_dc + sg * 12.5 + (4 if sg > 0 else -7.5), "+" if sg > 0 else "−",
                fontsize=11, fontweight="bold", color=INK)
    a2.axhline(v_dc, color=INK2, lw=1, ls=(0, (4, 3)))
    a2.text(95.5, v_dc + 1.5, "DC offset", ha="right", fontsize=9.5, color=INK2)
    a2.set_ylim(v_dc - 26, v_dc + 26)
    a2.set_ylabel("mV")
    a2.set_xlabel("time (ms)")
    a2.set_title("Voltage at a 5 kΩ site: ±12.5 mV riding on ~440 mV of DC", loc="left", fontsize=11.5,
                 fontweight="bold", color=INK)
    a2.set_xticks(range(0, 97, 16))
    fig.text(0.07, 0.015, "One read pair per half-period, 12 ms after each edge. Samples are added on + halves "
             "and subtracted on − halves,\nso the DC offset cancels and only the part that follows the current "
             "survives. Illustrative: firmware timing, ideal edges.", fontsize=9.5, color=INK2)
    fig.subplots_adjust(left=0.09, right=0.98, top=0.93, bottom=0.2)
    fig.savefig(os.path.join(OUT, "sync-detection.svg"))
    plt.close(fig)


def impedance_scale():
    fig, ax = plt.subplots(figsize=(9.4, 4.4))
    ax.set_xscale("log")
    ax.set_xlim(100, 1e6)
    rows = {"Hexagon colour": 3, "Salt-bridge pair": 2, "On-board standards": 1, "Bench, careful prep": 0}
    h = 0.56

    def band(row, x0, x1, color, label, ink="white", hatch=None):
        y = rows[row] - h / 2
        ax.add_patch(Rectangle((x0, y), x1 - x0, h, facecolor=color, edgecolor="white", lw=2,
                               hatch=hatch, zorder=2))
        if label:
            xm = (x0 * x1) ** 0.5
            ax.text(xm, rows[row], label, ha="center", va="center", fontsize=10, color=ink,
                    fontweight="bold", zorder=3)

    band("Hexagon colour", 100, 5000, GOOD, "PASS  ≤ 5 k")
    band("Hexagon colour", 5000, 10000, WARN, "5–10 k", ink="#0b0b0b")
    band("Hexagon colour", 10000, 150000, CRIT, "> 10 k")
    band("Hexagon colour", 150000, 1e6, OPEN, "OPEN")
    band("Salt-bridge pair", 100, 50000, CRIT, "bridge: trips below 50 k")
    band("Salt-bridge pair", 50000, 80000, "#e7e7e7", None, hatch="////")
    band("Salt-bridge pair", 80000, 1e6, "#f2f2f2", "clear above 80 k", ink=INK2)
    ax.annotate("50–80 k dead band: holds its last state", xy=(63000, rows["Salt-bridge pair"] - h / 2),
                xytext=(40000, 1.52), ha="right", va="center", fontsize=9, color=INK2,
                arrowprops=dict(arrowstyle="-", color=INK2, lw=0.8))
    for x, lab, side in [(1000, "R20 1 k\nanchor", 1), (4700, "R21 4.7 k\ncheck", 1),
                         (100000, "R22 100 k\nanchor", -1)]:
        ax.scatter([x], [rows["On-board standards"]], marker="D", s=70, color=INK, zorder=3)
        ax.text(x * (1.12 if side > 0 else 1 / 1.12), rows["On-board standards"], lab, va="center",
                ha="left" if side > 0 else "right", fontsize=9.5, color=INK2)
    ax.scatter([150000], [rows["On-board standards"]], marker="D", s=70, facecolor="white", edgecolor=INK,
               lw=1.5, zorder=3)
    ax.text(150000 * 1.12, rows["On-board standards"], "OPEN limit\n150 k", va="center", fontsize=9.5,
            color=INK2)
    y0 = rows["Bench, careful prep"]
    ax.add_patch(Rectangle((4300, y0 + 0.02), 13000 - 4300, h / 2 - 0.04, facecolor=BLUE, edgecolor="white",
                           lw=1.5, zorder=2))
    ax.add_patch(Rectangle((2150, y0 - h / 2 + 0.02), 6500 - 2150, h / 2 - 0.04, facecolor="#84b9f3",
                           edgecolor="white", lw=1.5, zorder=2))
    ax.text(14500, y0 + h / 4, "true ohms, Mode A: 4.3–13 k", va="center", fontsize=9.5, color=INK2)
    ax.text(7200, y0 - h / 4, "shown on screen (× PHANTOM_SCALE 0.5): 2.2–6.5 k", va="center", fontsize=9.5,
            color=INK2)
    ax.set_yticks(list(rows.values()))
    ax.set_yticklabels(list(rows.keys()), fontsize=10.5, color=INK)
    ax.tick_params(axis="y", length=0)
    ax.spines["left"].set_visible(False)
    ax.set_ylim(-0.55, 3.55)
    ax.set_xticks([100, 1000, 5000, 10000, 50000, 100000, 1e6])
    ax.set_xticklabels(["100 Ω", "1 k", "5 k", "10 k", "50 k", "100 k", "1 M"])
    ax.grid(axis="x", color=GRID, lw=0.8)
    ax.set_axisbelow(True)
    ax.set_xlabel("impedance (log scale)")
    fig.suptitle("Where everything sits on one impedance axis", x=0.02, ha="left", fontsize=14,
                 fontweight="bold", y=0.98)
    fig.subplots_adjust(left=0.18, right=0.98, top=0.88, bottom=0.15)
    fig.savefig(os.path.join(OUT, "impedance-scale.svg"))
    plt.close(fig)


if __name__ == "__main__":
    salt_bridge_map()
    sync_detection()
    impedance_scale()
