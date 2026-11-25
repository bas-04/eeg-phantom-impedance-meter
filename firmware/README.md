# Firmware

Arduino sketches for the ESP32-S3 on the **Elecrow CrowPanel 7.0"** (DIS08070H). The display board runs everything; the carrier board has no processor.

```
firmware/
├── phantom_meter_v2/        the instrument
│   ├── phantom_meter_v2.ino     measurement, calibration, UI, salt bridges, footswitch
│   ├── board_config.h           CrowPanel pin map, panel timing, touch transform, setting checks
│   ├── pm_audio.h               WAV playback from microSD through the speaker port
│   └── pm_fonts.h               anti-aliased Inter Display glyphs (generated, don't edit)
└── diagnostics/
    ├── pm_i2c_scan/             lists every I2C address, on screen
    ├── pm_analog_probe/         measures one channel four ways; proves the analog chain
    └── pm_touch_calibrate/      five-point touch calibration
```

Firmware version on the bench: **2026-11-22 a**. The calibration screen prints the version, sense gain and phantom scale in its bottom-left corner, so you can always check what is actually running.

## Toolchain

Arduino IDE 2. Pinned versions, because GFX Library and the ESP32 core must match:

| Component | Version |
|---|---|
| **esp32 by Espressif Systems** (Boards Manager; URL `https://espressif.github.io/arduino-esp32/package_esp32_index.json`) | **2.0.17** |
| GFX Library for Arduino (moononournation) | **1.4.9** |
| Adafruit ADS1X15 | 2.6.2 |
| Adafruit MCP4725 | 2.0.2 |
| Adafruit MCP23017 Arduino Library | 2.3.2 |
| TAMC_GT911 | 1.0.2 |
| Adafruit BusIO | 1.17.4 (installed with the above) |

GFX 1.4.9 needs core 2.0.17; GFX 1.6.x needs core 3.3.x. Mixing them fails to compile. 2.0.17 + 1.4.9 is what the instrument was brought up on. SD, SPI, Wire and Preferences come with the core.

## Board settings (Tools menu)

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | **Disabled** (checked at compile time) |
| PSRAM | **OPI PSRAM** (checked at compile time) |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | Huge APP (3MB No OTA/1MB SPIFFS) |
| Flash Mode | QIO 80MHz |
| Upload Speed | 921600 |

These are remembered per *saved sketch location*, so check them whenever you open a sketch from a new folder. If upload fails with "no serial data received": hold BOOT, tap RESET, release BOOT, upload again. Upload from the computer, then run the display from the charger: GPIO19/20 are the native USB pins and a USB host disturbs the I²C bus.

## Settings in `phantom_meter_v2.ino`

All near the top of the file.

| Constant | Default | What it does |
|---|---|---|
| `G_SENSE` | `1.0f` | Sense amplifier gain. **Must match the board:** 1.0 with R8 linked (as built), 4.0 on a stock Rev B. |
| `PHANTOM_SCALE` | `0.5f` | Scalp-site and GND readings are multiplied by this so a careful prep on the skinless phantom reads like a head. Ears stay in true ohms. 1.0 = true ohms (use it for resistor tests). |
| `SALT_BRIDGE` | `1` | Compile salt-bridge checking in (1) or out (0). Turn it off while chasing other faults: it adds about 20 measurements per sweep. |
| `BRIDGE_CHECK_GND` | `false` | Include the three GND pairs in bridge checking. Off because GND and Fz are 4 mm apart on this phantom. |
| `BR_TRIP`, `BR_CLEAR` | 50 k, 80 k | Bridge trips below, clears above. Tune against real gel. |
| `BEEP_VOLUME` | `3` | Piezo: 0 silent, 1 soft, 2 medium, 3 loud (pitch moves toward the 4 kHz resonance). |
| `BEEP_HZ_LOUD` | `4000` | Try 3600–4400 by ear if 3 isn't the loudest. |
| `BEEP_ON_PASS / FAIL / BRIDGE / MODE` | on / off / on / on | Which events beep. |
| `SOUND_ON_OPENING / CLICK / MODE / BRIDGE` | all on | Which speaker sounds play (if the file loaded). |
| `SOUND_VOLUME`, `VOL_*` | 80, 100/60/100/100 | Master and per-sound level, percent. |
| `SOUND_WAIT_FOR_OPENING` | `true` | Calibrate only after `opening.wav` finishes (a tap skips it). |

Calibration coefficients and the per-site zero are stored in flash (ESP32 Preferences, namespace `phantom`) and survive power cycles; calibration is re-run at every power-up anyway.

## Diagnostics

Each is its own sketch (the IDE compiles every `.ino` in a folder together, so never put two in one folder).

- **`pm_i2c_scan`**: first thing to run whenever anything on the bus goes quiet. Results on the display, every three seconds. Expected: 0x18, 0x20, 0x21, 0x48, 0x49, 0x60 or 0x62, and 0x5D once touch has been started.
- **`pm_analog_probe`**: measures the same calibration or site channel by four sampling methods and names the verdict on screen. It is what told "no current" from "current not sampled" on 24 Oct 2026; keep it for any analog fault. Tap to step through channels.
- **`pm_touch_calibrate`**: crosshairs at five positions; tap each and fit `raw = a + b·screen` per axis, then put the inverse into `mapTouch()` in `board_config.h`. Only needed if taps miss, or for a different panel.

Three one-time bring-up sketches (`pm_panel_test`, `pm_touch_probe`, `pm_dac_hold`) and the Sunton CYD 4.3" support were removed when the repository was created. They are at the git tag `dual-display-snapshot`:

```
git checkout dual-display-snapshot -- firmware/
```

## `board_config.h` is copied into every sketch folder

The Arduino IDE builds only the files inside a sketch's own folder, so each sketch carries an identical copy. If you change one, copy it to the others:

```
for d in firmware/diagnostics/*/; do cp firmware/phantom_meter_v2/board_config.h "$d"; done
```

## Things that will bite you

- **Never use the Adafruit ADS1X15 library on 0x49** on a Rev B board with M3 pin 6 still in place: the library enables the ALRT pin, which is wired to the current signal. The shipped sketches use register-level reads with the comparator off.
- **Don't replace the sampling in `measureRaw()` with anything faster** unless `pm_analog_probe` shows that exact method working on your board. Continuous mode and concurrent single-shot both returned stale data on the bench.
- **`footPoll()` must never be called from inside `measureRaw()`**: a long press reconfigures the switch fabric mid-reading.
- `TX` and `TY` are taken by the ESP32 core; GFX 1.6.x has no `BLACK`/`WHITE`.

## Credits for bundled material

`pm_fonts.h` contains glyph bitmaps rendered from **Inter Display** by Rasmus Andersson ([rsms.me/inter](https://rsms.me/inter)), licensed under the SIL Open Font License 1.1. The libraries listed above are installed separately and are not included in this repository.
