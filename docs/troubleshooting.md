# Troubleshooting

Symptoms seen during bring-up and what caused them. Start at the top of the section that matches; the first suspect is listed first.

## Screen and flashing

| Symptom | Cause / fix |
|---|---|
| Won't compile: "PSRAM is disabled" or "USB CDC On Boot" error | Deliberate compile-time checks in `board_config.h`. Set Tools → PSRAM **OPI PSRAM** and USB CDC On Boot **Disabled**. |
| Screen stays black, no error anywhere | PSRAM off (the 768 kB framebuffer can't be allocated). The build log says `qio_qspi` instead of `qio_opi`. Board settings are remembered per *saved sketch location*, so a sketch opened from a new folder comes up with defaults. |
| Upload fails, "no serial data received" | Hold BOOT, tap RESET, release BOOT, upload again. |
| A wall of "redefinition of …" errors | Two `.ino` files in one folder. Every sketch needs its own folder. |
| "conflicting declaration" naming a file you never opened | A variable called `TX` or `TY`: the ESP32 core already defines them. |
| `BLACK` / `WHITE` undefined | GFX Library 1.6.x dropped them. Use raw RGB565 (0x0000, 0xFFFF). |
| Taps land on the wrong hexagon | Re-run `pm_touch_calibrate` and refit `mapTouch()` in `board_config.h`. Five points, not one: a corner tap can't tell an inverted mapping from an inverted-and-scaled one. |
| One tap toggles the mode twice (looks ignored) | Fixed on 24 Oct 2026 (`tapEdge()`); you are running older firmware. Check the firmware date on the calibration screen. |
| New features missing from the screen | An older upload is still running. The calibration screen prints the firmware date, sense gain and phantom scale in its bottom-left corner. |

## I2C and power

| Symptom | Cause / fix |
|---|---|
| Firmware stops on "I2C device missing - check J3 wiring" | It doesn't say which one. Flash `pm_i2c_scan`. A silent serial monitor at this point is expected: GPIO19/20 are the native USB pins. |
| Scan shows only the display's addresses (0x18, maybe 0x5D) | The bus isn't reaching the carrier. Check 3.3 V at U13 (carrier powered?), then SDA→J3.3 and SCL→J3.4. Swapped SDA/SCL looks exactly like no connection. Power and bus are separate cables: running one does not connect the other. |
| Nothing at all ("NONE") | Bus held low or no common ground: check the I²C GND wire to J5 pin 2, and that the display is on the charger, not a computer. |
| Carrier devices appear, touch (0x5D) disappears | A USB host is loading GPIO19/20, or 3V3 and 5V have met. **Power down immediately** and check the cut 3V3 wire. |
| 0x60/0x62 missing | MCP4725 wiring. Measure 3.3 V on the module's VCC pin. |
| DAC answers but drive current is tiny, R21 reads double | Module VCC at about 2.2 V: its power wire is open and it runs off SDA/SCL. This is what the failed adapter board did. |
| 0x20 or 0x21 missing | That MCP23017 is backwards or not fully seated. |
| 0x48 or 0x49 missing | That ADS1115 module is backwards in its socket. |
| Addresses come and go between scans | Shorten the I²C wires (under 15 cm), then fit R15/R16 (4.7 k pull-ups). |

## Calibration

| Report says | Look at |
|---|---|
| "ADC returned constant data - SAMPLING fault, not an open circuit" | The current ADC isn't converting. On Rev B this is the **M3 ALRT pin**: some other sketch used the Adafruit library on 0x49. Reflash `phantom_meter_v2`, which releases ALRT at start-up; clip M3 pin 6 to make it permanent. |
| "No current on any standard" | The pump: U9B, R1–R4, R17, or U8 (switch S5). |
| "1k standard open" | R20, or a drive/sense mux pin. |
| "4.7k standard open" / "100k standard open" | R21 / R22. |
| "open channel is not open - a mux is stuck on" | A bent pin on U1–U6 or a stuck enable line. Look along the rows at eye level first. |
| "100k reads below 1k" | R20 and R22 swapped. |
| PASS, but R21 well off 4700 | The chain isn't linear between the anchors. Suspect R21, then its mux channel. (Old notes saying 4990 are stale: R21 is a 4.70 k part.) |
| Amber "low drive" warning | Under half the design current. Readings stay right (it's ratiometric) but noisier. Check R1–R4 at the U9 socket with U9 out: pin 6–10 R1, 6–7 R2, 5–1 R3, 7–5 R4 (all 100 k), and 9–8 R18 (200 k). Try swapping U9 and U10. |
| Erratic sites after a cold start, fixed by RESET | VREF hadn't settled when the DAC was centred. Fixed 31 Oct 2026 (the firmware waits for three readings within 2 mV); update. |

## Readings

| Symptom | Cause / fix |
|---|---|
| Every site OPEN, calibration passes | Nothing is closing the loop: J1 and J2 aren't joined on the board. Use a lead or cable between them, or the phantom with gelled cap. |
| A resistor test lead reads about half its value | `PHANTOM_SCALE` is 0.5. Expected; set it to 1.0 for true ohms. |
| One site reads a few hundred ohms high on a test lead | Run ZERO SITES with a straight-through cable. |
| One site OPEN through the straight cable during ZERO SITES | That cable pin isn't wired, or that mux channel is bad. Swap in a spare multiplexer. |
| Every gelled site shows `DC` | Electrode offset saturating the sense amp: R8 must be linked and `G_SENSE = 1.0f`. |
| Every site 0.0 in Mode A with gel | Old firmware (pre 1 Nov 2026) chose the ADC range without allowing for the DC offset. Update. |
| Readings shift when you switch Mode A/B on a fixed resistor | The LEDs now stay lit during measurement through the same expander as the mode switches. Never observed, but it's a one-line revert in `measureRaw()`. |
| Mode B: `FIX A1` / `FIX A2` | That ear electrode is over 5 kΩ, or its jack isn't wired to J2 pin 11 / 24. |
| No salt-bridge alarms ever | `SALT_BRIDGE` set to 0, or the pair involves GND (excluded by default). Bench-test with a resistor from the uphill J1 pin to the downhill J2 pin. |
| Constant bridge alarm between GND and Fz | `BRIDGE_CHECK_GND` set to true. Their inserts are 4 mm apart on this phantom. |
| Bridge alarm chatters on and off | Tune `BR_TRIP` / `BR_CLEAR` (50 k / 80 k) against real gel; widen the gap. |
| Footswitch does nothing | J5 pin 1 to pin 2 through a normally-open pedal. With no pedal the pull-up reads "released", by design. |
| Speaker silent | Check the file list on the calibration screen; files must be uncompressed PCM WAV in the card's top folder. Leave the CrowPanel's UART1 socket empty (GPIO17/18 are the I²S lines). |
