# Pinouts and maps

Reference tables for the Rev B carrier board. The firmware's `SITES[]` table (top of `phantom_meter_v2.ino`) is the single place the site-to-pin assignment lives; if your head harness is wired differently, change it there.

## Electrode sites: DB25 pin and multiplexer channel

The same pin number is used on **J1** (phantom/head harness) and **J2** (Electro-Cap), following the Electro-Cap DB25 assignment. Drive, sense and return banks all use the same channel number for a site.

| Site | DB25 pin | Mux ch | | Site | DB25 pin | Mux ch | | Site | DB25 pin | Mux ch |
|---|---|---|---|---|---|---|---|---|---|---|
| Fp1 | 1 | 0 | | T3 | 7 | 7 | | T6 | 21 | 16 |
| Fp2 | 14 | 1 | | C3 | 3 | 8 | | O1 | 5 | 17 |
| F7 | 6 | 2 | | Cz | 22 | 9 | | O2 | 18 | 18 |
| F3 | 2 | 3 | | C4 | 16 | 10 | | GND | 9 | 19 |
| Fz | 10 | 4 | | T4 | 20 | 11 | | A1 | 11 | 20 |
| F4 | 15 | 5 | | T5 | 8 | 12 | | A2 | 24 | 21 |
| F8 | 19 | 6 | | P3 | 4 | 13 | | | | |
| | | | | Pz | 23 | 14 | | | | |
| | | | | P4 | 17 | 15 | | | | |

DB25 pins **12, 13 and 25** are not connected on either connector. Each bank is two 16-channel chips: on the return bank, channels 0–15 go through U5 and 16–31 (including all the calibration channels) through U6.

## Internal channels

| Ch | Connected to | Use |
|---|---|---|
| 22 | R20 1.00 kΩ 0.1 % | calibration anchor |
| 23 | R21 4.70 kΩ 0.1 % | mid-scale linearity check (expect ≈ 4700) |
| 24 | R22 100 kΩ 0.1 % | calibration anchor |
| 25 | open | leakage / stuck-switch self-test |
| 26 | short | return-path offset |
| 27–31 | unused | |

Each standard is wired MUX-D ch n → resistor → MUX-R ch n, and sensed on MUX-S ch n.

## Mode matrix (U7, U8: SN74HC4066)

| Switch | U12 bit | Joins | Used for |
|---|---|---|---|
| SW1 | GPA0 | MUX-R common → J2 pin 11 (cap A1) | Mode B via A1 |
| SW2 | GPA1 | MUX-R common → J2 pin 24 (cap A2) | Mode B via A2; ear-to-ear reference |
| SW3 | GPA2 | J1 pin 11 (phantom A1) → TIA | Mode B via A1 |
| SW4 | GPA3 | J1 pin 24 (phantom A2) → TIA | Mode B via A2; ear-to-ear reference |
| SW5 | GPA4 | MUX-R common → TIA | Mode A, ears, calibration, bridge checks |
| SW6 | GPA5 | J4 probe → TIA | unused |

Firmware masks: Mode A `SW5`; Mode B via A1 `SW1|SW3`; via A2 `SW2|SW4`.

## MCP23017 port map

| Chip | Addr | Pins | Net | Function |
|---|---|---|---|---|
| U11 | 0x20 | GPA0–3 | SEL_DS0–3 | channel select, drive and sense banks (always the same site) |
| U11 | 0x20 | GPA4, GPA5 | nEN_DSLO, nEN_DSHI | enable low (ch 0–15) or high (16–31) chip; both high = all off |
| U11 | 0x20 | GPB0–3 | SEL_R0–3 | channel select, return bank (can differ: salt-bridge checks) |
| U11 | 0x20 | GPB4, GPB5 | nEN_RLO, nEN_RHI | return bank enables |
| U11 | 0x20 | GPA6–7, GPB6–7 | SPARE1–4 | to J6 |
| U12 | 0x21 | GPA0–5 | SW1–SW6 | mode matrix (high = closed) |
| U12 | 0x21 | GPA6 | BUZ | piezo LS1 (toggled near its 4 kHz resonance) |
| U12 | 0x21 | GPA7 | LED_MODE | LED1, yellow: lit in Mode B |
| U12 | 0x21 | GPB0 | LED_REF | LED2, green: references valid |
| U12 | 0x21 | GPB1 | FOOTSW | J5 pin 1, internal pull-up, pedal to GND |
| U12 | 0x21 | GPB2–3 | SPARE5–6 | to J6 |

The MCP23017 interrupt outputs are not wired to the display, so the footswitch is polled.

## I²C addresses

| Addr | Device | Where |
|---|---|---|
| 0x18 | PCA9557 panel-reset expander | display (expected) |
| 0x20 | MCP23017 U11 | carrier |
| 0x21 | MCP23017 U12 | carrier |
| 0x48 | ADS1115 M2, voltage | carrier (socket ties ADDR to GND) |
| 0x49 | ADS1115 M3, current | carrier (socket ties ADDR to 3.3 V) |
| 0x5D | GT911 touch | display (may only appear after a sketch has started touch) |
| 0x60 / 0x62 | MCP4725 M1 | carrier (generic / Adafruit; firmware searches 0x60–0x63) |

Bus: GPIO19 SDA, GPIO20 SCL, 400 kHz in the main firmware.

## Headers and sockets

| Header | Pin | Signal |
|---|---|---|
| J3 | 1 | +5 V in (to R14 → U13 LDO only) |
| J3 | 2 | GND |
| J3 | 3 | SDA |
| J3 | 4 | SCL |
| J3 | 5 | n/c (was ADC data-ready, removed 17 Oct 2026) |
| J3 | 6 | n/c |
| J4 | 1, 2 | probe lead, GND (probe unused) |
| J5 | 1, 2 | footswitch, GND |
| J6 | 1–6 | SPARE1–6 |

| Module socket | Holes, 1 = square pad |
|---|---|
| M1 (6) | +3.3 V · GND · SCL · SDA · DAC_OUT · unused. **Hand-wired**, see [README](README.md#2-m1-mcp4725-dac-hand-wired-not-plugged-in). |
| M2, M3 (10) | VDD · GND · SCL · SDA · ADDR · ALRT · A0 · A1 · A2 · A3. A0 = signal, A1 = VREF. M3 hole 6 is wrongly tied to TIA_OUT. |

## Display (Elecrow CrowPanel 7.0", DIS08070H)

| Connector | Pins | Used for |
|---|---|---|
| I2C | SCL·IO20, SDA·IO19, 3V3, GND | the carrier bus. **3V3 wire cut and insulated.** |
| GPIO_D | IO38 | not used (the firmware drives GPIO38 as a dummy touch reset) |
| UART0 | IO44, IO43 | not used |
| UART1 | IO17, IO18 | leave empty: GPIO17/18 are the speaker's I²S lines |
| SPK | 2-pin PH2.0 | optional 8 Ω speaker |
| TF | microSD | optional sound files |
