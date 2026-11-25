# Changelog and lessons

A condensed, dated record of how the instrument got from a design on paper to a working meter. The raw bench notes this is drawn from are in [`archive/`](archive/).

## Where the project started

The original proposal was a 3D-printed head phantom with conductive inserts at the 10-20 sites, an ESP32 **signal generator** to play microvolt-level EEG-like waveforms into the head, and an impedance-check mode, so students could practise cap placement, gelling and salt-bridge avoidance without a volunteer. The signal generator was never built. What exists, and what this repository holds, is the head and the impedance meter.

An early impedance front end around the AD5933 was also explored and dropped: its range starts at 1 kΩ and its excitation runs at kilohertz, neither of which suits a low-frequency check on a phantom whose bare inserts measure 300–610 Ω.

## Rev A → Rev B: from a factory design to one you can solder

Rev A's measuring scheme (small AC test current, Kelvin drive/sense split at the DB25 pin, current measured rather than assumed, three-electrode decomposition for Mode B) was kept. Its parts were not:

| Rev A problem | Rev B answer |
|---|---|
| ADG731, INA333, ADS1115 and ADP151 only in 0.5 mm-pitch or leadless packages | Every chip a socketed DIP or a plug-in module; nothing finer than 2.54 mm |
| Loop-back dongle specified as a male DB25 (couldn't mate with the cap) | Female |
| "Learn Harness" probe wired to a channel with no return path | Probe closes the loop at the TIA through S6 (feature still unwritten) |
| Reference-pair row selected mux channel 11 (A1's DB25 pin) instead of 20 (A1's channel) | One site → pin → channel table in firmware |
| Second ADC ready line on GPIO18 (needs a solder jumper on the display) | Later removed entirely |
| INA333 instrumentation amp doing nothing useful (both ADC inputs already share VREF) | One op-amp stage |
| Precision resistors in six places | Only the three calibration standards are 0.1 % |
| ESD diode arrays and a cable-ID resistor with no return path | Deleted |

The 74HC multiplexers' ~150 Ω on-resistance was checked and accepted: drive side outside the Kelvin loop, sense side carries no current, return side at a virtual ground.

## Bring-up timeline (2026)

| Date | Event |
|---|---|
| 11 Sep | KiCad DRC clean on Rev B.1 after removing ground pours. |
| 19 Sep | **Erratum:** DB25 footprints mirrored; fit vertical connectors. |
| 20 Sep | **Erratum:** M1 footprint swaps A0/VOUT. |
| 13 Oct | Display (Sunton CYD 4.3") bring-up: panel, touch (inverted *and* compressed: its controller carries the config of a 480×272 sibling board), I²C. LEDs could only flicker and the map flickered: both fixed. |
| 14–16 Oct | Second display, **Elecrow CrowPanel 7.0"**: pin map verified, touch at 0x5D (not the 0x14 published configs claim), five-point touch fit, all seven I²C addresses on the bus. Carrier powered from a second charger port; 3V3 wire cut. |
| 17 Oct | ADC data-ready line dropped (GPIO17 is audio on the CrowPanel), making J3 four wires and the firmware portable. Firmware ported to both boards. Salt-bridge detection and footswitch written (tested in simulation). Screen redesigned. |
| 23 Oct | Multiplexers in. Guide's "expect 4990" corrected to 4700 (R21 had become a 4.70 k part). Test-lead procedure corrected: J1 and J2 must be bridged, a resistor inside one plug can't close the loop. |
| 24 Oct | Calibration reported every standard OPEN. `pm_analog_probe` showed current flowing but the ADC returning a constant. Two sampling rewrites failed the same way; the third used exactly the call the probe had proved. Root cause that night: **M3's ALRT pin tied to TIA_OUT** on the PCB. Fixed in firmware (registers, comparator off). Open-circuit threshold made absolute. Tap-counted-twice bug fixed. |
| 24–27 Oct | Drive current far below design. An MCP4725 adapter board turned out to have an open VDD: the DAC ran off its data lines at 2.2 V. Replaced by jumper wires: **calibration PASS, 2.50 µA, R21 = 4,708 Ω, short −0.8 Ω**. A 4.72 k lead read 5.2 k on Fp1 (U5 vs U6 switch resistance): **per-site zero** added. All 22 channels confirmed through a straight-through cable. |
| 30 Oct | Mode B reference rule: each ear under 5 k on its own (was the pair under 20 k). Ear hexagons reading 0.0 in Mode B fixed. |
| 31 Oct | Erratic cold starts on the CYD: VREF read before it settled. Firmware now waits for VREF. |
| 1 Nov | **First gelled cap.** Salt-bridge checking had been shipped switched off; now on. Every Mode A site read 0.0: electrode DC offset clipping the ADC. Range now chosen from the peak voltage seen; "DC" shown when the amp saturates. |
| 4 Nov | First real bridge (Fz–GND). Alarm made unmistakable; MUTE and RECAL buttons. Every scalp site read "DC" in Mode A (electrode offset above about 0.36 V saturates the sense amp at gain 4). |
| 5 Nov | **R8 linked, sense gain 1.** GND left out of bridge checking (4 mm from Fz). Firmware date shown on the calibration screen. |
| 8 Nov | Calibration PASS at gain 1 (2.48 µA, R21 4,660 Ω). Careful prep read 4.3–13 kΩ true; Mode B agreed within 1–2 k. **PHANTOM_SCALE 0.5** added. Scan progress bar. Meter reading a gelled Electro-Cap on the phantom in both modes. |
| 9 Nov | Piezo driven at its 4 kHz resonance; per-event beep switches; threshold beeps with hysteresis. |
| 22 Nov | Speaker WAV sounds from the CrowPanel's microSD (firmware 2026-11-22 a). |
| 25 Nov | Repository created; firmware trimmed to the CrowPanel only (the dual-display version is at tag `dual-display-snapshot`). |

## Lessons worth keeping

- **A measurement only covers the code it actually ran.** Two sampling fixes were "verified" by a probe that called a different function. The probe now times four methods side by side, and only a proven one goes into the firmware.
- **Stubs verify your assumptions, not the hardware.** The simulation fixture returned a clean signal because it was told to; it now adds noise and has a "stuck" mode.
- **"No data" and "open circuit" must never look the same.** The `adcStuck` guard turned a multi-day chase into one power-up.
- **A chip that answers on I²C is not necessarily powered.** Measure the supply pin.
- **One corner tap cannot calibrate a touch panel.** Inversion and inversion-plus-scaling agree near the corners.
- **Put the diagnostics on the display, not on serial,** when the I²C pins are also the USB pins.
- **Read the stale number before the builder does.** The 4990 → 4700 correction landed at the moment the measurement path was first switched on, exactly when a wrong expectation costs most.
