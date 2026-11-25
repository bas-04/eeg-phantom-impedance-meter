EEG HEAD PHANTOM IMPEDANCE METER - FIRMWARE AND BRING-UP TOOLS
Rev B carrier board + Elecrow CrowPanel 7.0" or Sunton CYD 4.3" display
Complete set as of 22 November 2026 (phantom_meter_v2 firmware 2026-11-22 a)


START HERE

  phantom_meter_v2/      THE INSTRUMENT. Needs all four files in its folder:
                         phantom_meter_v2.ino, board_config.h, pm_fonts.h,
                         pm_audio.h.
  build_guide.html       The build guide (open in any browser). Changes after
                         30 Oct 2026 are summarised in its firmware section
                         and recorded in full in ERRATA.txt.
  ERRATA.txt             Every fault found and every change made, dated.

  Every sketch folder carries its own board_config.h, all set to
  PM_BOARD 2 (Elecrow CrowPanel 7.0"). For the CYD 4.3", change that one line
  to PM_BOARD 1 in EACH folder you flash, and switch the Tools menu to
  Flash 16MB / partition "16M Flash (3MB APP/9.9MB FATFS)".

  Arduino IDE: ESP32 core "esp32 by Espressif Systems" 2.0.17, GFX Library
  for Arduino 1.4.9, Adafruit ADS1X15 2.6.2, Adafruit MCP4725 2.0.2, Adafruit
  MCP23017 2.3.2, TAMC_GT911 1.0.2 (Adafruit BusIO 1.17.4 comes with them).
  Board ESP32S3 Dev Module, USB CDC On Boot Disabled, PSRAM OPI PSRAM.

  This set assumes the BENCH BOARD'S HARDWARE CHANGES:
    - R8 bridged with a wire link (sense amplifier at gain 1), and
      G_SENSE = 1.0f in phantom_meter_v2.ino. A stock board needs 4.0f.
    - MCP4725 DAC wired by jumpers, by label (not the adapter board).
    - A1/A2 ear receptacles wired to J2 pins 11 and 24.
    - Elecrow only: a ground wire from the display to the carrier
      (J4 pin 2 or J3 pin 2), since the carrier has its own 5 V supply.

  Settings near the top of phantom_meter_v2.ino you may want to change:
    PHANTOM_SCALE      0.5   scalp-site readings x this (1.0 = true ohms)
    BEEP_VOLUME        3     0 silent, 1 soft, 2 medium, 3 loud
    BEEP_ON_PASS/FAIL/BRIDGE/MODE   which events beep
    BRIDGE_CHECK_GND   false GND left out of salt-bridge checking
    SALT_BRIDGE        1     bridge checking compiled in
    SOUND_ON_OPENING/CLICK/MODE/BRIDGE   which speaker sounds play (Elecrow)
    SOUND_VOLUME       80    speaker master volume, percent (100 = as recorded)
    VOL_OPENING/CLICK/MODE/BRIDGE        each sound's level, percent of master

  SPEAKER (Elecrow only): an 8 ohm, 1-3 W speaker on the CrowPanel's SPK
  socket, and these four files in the top folder of the TF card:
      opening.wav  click.wav  modechange.wav  saltbridge.wav
  Uncompressed WAV (Audacity: Export as WAV, Signed 16-bit PCM). Missing
  files fall back to the piezo. The calibration screen shows, just above the
  firmware line, which files loaded and how long each is. Details: pm_audio.h.

  The calibration screen shows the firmware date and these settings in its
  bottom-left corner, so the version on the display can always be checked.


================================================================
ORIGINAL BRING-UP NOTES (13 October 2026 onward)
================================================================

EEG HEAD PHANTOM IMPEDANCE METER - BRING-UP SKETCHES
Rev B carrier board + Sunton ESP32-8048S043C ("CYD") 4.3" display
Bundled 13 October 2026


WHAT IS IN HERE

Four sketches, each in its own folder. The Arduino IDE requires the folder
name and the .ino filename to match, and it compiles EVERY .ino file in a
folder together as one program - so never put two of these in the same
folder, or you get a wall of "redefinition of ..." errors.

  pm_panel_test/         RGB panel smoke test. Colour bars, edge border,
                         corner labels, centre crosshair, then solid white and
                         grey fields. Touches NOTHING but the panel - no I2C,
                         no touch, no carrier board - so a failure localises to
                         the pin map or the timing. RUN THIS FIRST on any new
                         display board.

  pm_i2c_scan/           Lists every I2C address that answers, on screen
                         and on serial. First thing to reach for whenever
                         anything on the bus goes quiet.

  pm_touch_probe/        Confirms the GT911 touch controller is alive,
                         shows whether its I2C address shifts when reset,
                         and reports raw touch coordinates live.

  pm_touch_calibrate/    Five-point crosshair calibration. Gives the exact
                         raw-to-pixel transform for the touch panel.

  phantom_meter_v2/      The main firmware, with the touch coordinate fix
                         applied (see below). Otherwise identical to
                         phantom_meter.ino.


BOARD SETTINGS - ALL FOUR SKETCHES

  Board:            ESP32S3 Dev Module
  Flash Size:       16MB (128Mb)
  PSRAM:            OPI PSRAM
  Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)
  USB CDC On Boot:  Disabled

USB CDC On Boot must be Disabled. It is a per-sketch setting, so a newly
created sketch can come up with it enabled. On this board GPIO19 and GPIO20
are the ESP32-S3's native USB D-/D+ pins, and Sunton reused them for the
touch I2C bus - so leaving CDC on hands those pins to the USB peripheral and
the whole I2C bus dies.

The same pin sharing means a USB cable running to a computer loads GPIO19/20
and can break I2C even with CDC disabled. When testing I2C, power the CYD
from a plain USB charger rather than a computer port. That is also why every
sketch here prints its results to the DISPLAY and not only to serial.


WIRING REMINDER - CYD TO CARRIER BOARD

Two separate cables are needed, from two different connectors:

  P1 (JST 1.25, GND/RXD/TXD/5V)      5V  -> J3 pin 1
                                     GND -> J3 pin 2
  P3 (4-pin, IO17/IO18/IO19/IO20)    IO19 (SDA) -> J3 pin 3
                                     IO20 (SCL) -> J3 pin 4

Running only the P1 cable gives a properly powered carrier board with no
I2C reaching it: the scanner then shows 0x5D alone. Running the P1 cable's
four wires straight into J3 pins 1-4 puts RXD and TXD on the I2C pins,
which looks the same.


EXPECTED I2C ADDRESSES

  0x20   MCP23017 port expander U11
  0x21   MCP23017 port expander U12
  0x48   ADS1115 ADC in M2 (voltage sense)
  0x49   ADS1115 ADC in M3 (current / TIA)
  0x62   MCP4725 DAC in M1
  0x5D   GT911 touch controller (on the CYD, not the carrier board)


TOUCH COORDINATE FIX IN phantom_meter_v2

The GT911 raw coordinates are inverted on BOTH axes and also compressed.
Over the full 800 px screen width the raw x value sweeps only 467 counts,
and over 480 px of height only 263 - spans very close to 480 x 272, the
resolution of the sibling ESP32-4827S043 board that shares this PCB (and
which the silkscreen is labelled for). The touch controller carries that
panel's configuration, so it reports in a 480 x 272 space while the display
draws in 800 x 480.

The five-point fit from pm_touch_calibrate (13 Oct 2026) gives

    raw_x = 785.1 - 0.5833 * screen_x
    raw_y = 471.7 - 0.5487 * screen_y

inverted in phantom_meter_v2 as

    int tx = (int)((785.1f - touch.points[0].x) * 1.7143f);
    int ty = (int)((471.7f - touch.points[0].y) * 1.8223f);
    tx = constrain(tx, 0, 799);
    ty = constrain(ty, 0, 479);

Residuals are under 13 px at every calibration point, against a 24 px
hit radius on the hexagons - comfortable, but if taps start landing on the
wrong site near the screen edges, re-run pm_touch_calibrate and refit.

Worth knowing: a single corner tap CANNOT distinguish a plain 180-degree
inversion from this inverted-and-scaled mapping, because near the corner
the two agree. The disagreement only shows up mid-screen. That is why the
five-point calibration exists.

Plain arithmetic is used instead of the library's setRotation() because the
rotation constant names have moved around between versions of TAMC_GT911,
and raw multiply-and-subtract compiles against all of them.


REQUIRED LIBRARIES

  Wire                              (bundled with the ESP32 core)
  Preferences                       (bundled with the ESP32 core)
  SPI                               (bundled with the ESP32 core)
  GFX Library for Arduino           moononournation - tested on 1.6.9
  TAMC_GT911                        tested on 1.0.2
  Adafruit ADS1X15                  tested on 2.6.2
  Adafruit MCP4725                  tested on 2.0.2
  Adafruit MCP23017 Arduino Library tested on 2.3.2
  Adafruit BusIO                    tested on 1.17.4

Only the first four are needed by the three tool sketches; the full list is
for phantom_meter_v2.

On GFX Library for Arduino 1.6.9 the bare color names BLACK and WHITE are
not defined. All four sketches use raw RGB565 values (0x0000, 0xFFFF)
instead, which work on every version.


WHERE TO PUT THESE

Anywhere you like, but not in a Windows temp folder - an unsaved Arduino
sketch lives under AppData\Local\Temp and can be cleared without warning.
Alongside the existing firmware is the obvious home:

  ...\phantom_meter_RevB\firmware\

INDICATOR LEDS (confirmed by observation, 13 Oct 2026)

  Yellow LED1   mode indicator    - lit in Mode B, dark in Mode A
                                    (U12 / 0x21, GPIOA bit 7)
  Green  LED2   refs-valid        - lit when the A1||A2 linked-ears check
                                    reads under 20 kOhm
                                    (U12 / 0x21, GPIOB bit 0)

Seeing either LED light is also the only way to confirm the MCP23017 output
drivers work. An I2C scan proves a chip answers; it says nothing about
whether its pins can drive a load. The on-screen mode text is drawn by the
ESP32 and proves nothing about U12 either - watch the LEDs.

With U1-U6 absent the A1||A2 check always fails, so after the first Mode B
sweep the green LED stays dark and the mode line reads "FIX A1/A2" in red.
That is the firmware correctly reporting that the ear references are not
connected, not a fault.

Two firmware bugs found here and fixed in this copy of phantom_meter_v2:

  - measureRaw() ended with modeSwitches(swMask, false, false, false),
    blanking both LEDs on every reading - many times per second - so they
    could only flicker, never indicate. It now passes the live modeB and
    refsValid state. (If LED current later proves to inject noise into the
    readings at step 5, revisit this.)

  - The mode line's colour was chosen by refsValid regardless of mode, so
    Mode A inherited a red warning from a check Mode A does not use. Mode A
    now draws in neutral grey.


DISPLAY FLICKER - FIXED

The first version of drawHeadMap() opened with

    gfx->fillRect(0, 60, 520, 420, C_BG);

and then repainted the head circle, all 22 hexagons and all 44 labels, five
times a second. drawFocus() blanked the right-hand panel the same way. The
RGB panel scans its framebuffer continuously at around 60 Hz, so it kept
catching the display mid-rebuild - field blanked, only some hexagons painted
back - and the map visibly flickered.

Note the distinction: flicker confined to the elements being repainted is a
drawing-code problem. Horizontal tearing bands sweeping across EVERYTHING,
static title bar included, would instead point at panel bandwidth (PCLK too
fast for PSRAM) and would be fixed by lowering the pixel clock or adding a
bounce buffer. Those are different faults with different fixes.

phantom_meter_v2 now blanks nothing after the first paint:

  - Text is drawn opaque, setTextColor(foreground, background), over
    fixed-width fields. A glyph paints out whatever it replaces, so there is
    never an erase step and never a gap for the scanout to catch. Fixed width
    is what makes it safe - a shorter string cannot leave a tail behind.

  - Only elements whose content actually changed are touched. The firmware
    measures one channel per pass, so in steady state exactly one hexagon is
    repainted per 200 ms.

  - A filled hexagon covers both its old colour and its old label, so a
    changed site needs no erase of its own.

  - The focus ring (r=27) sits outside the hexagon (r=20), so repainting a
    hexagon never damages it. Moving the ring does need an erase, which can
    clip the head-circle outline and nick a close neighbour - GND and Fz are
    only 45 px apart - so a ring erase is followed by redrawing the circle and
    overpainting all 22 sites. That overpaint is invisible: identical pixels,
    with nothing blanked first. It only happens on a tap.

One small layout change came with it: the big reading in the focus panel is
now text size 6 rather than 7, so that a fixed 5-character field ("150.0",
"  2.3", " OPEN") does not overlap the "k" unit label.

The head-map geometry (HX, HY, HR = 270, 260, 150) is now a file-scope
constant used by both the drawing code and the touch hit-test, which
previously repeated the numbers as literals in two places.


DIE TEMPERATURE READOUT

The header bar shows "nnn.n F" at the right. That is the ESP32-S3's
on-chip temperature sensor (temperatureRead(), declared in the core's
esp32-hal.h, returns Celsius; the firmware converts). It is sampled once a
second and lightly smoothed, because the sensor is noisy at the degree level.

Read it as a DELTA, not as an absolute. It measures the silicon die, which
runs warm of the air around it by an offset nobody has characterised for this
board. The useful procedure is:

  1. Run the board open on the bench for 30-60 minutes. Note the figure.
  2. Run it sealed in the phantom base for 30-60 minutes. Note it again.

The difference is what the enclosure actually costs you. The absolute number
tells you very little.

Thermal context for the enclosure: the carrier board draws roughly 30-40 mA at
3.3 V (about 0.12 W) plus around 0.07 W burned in the 3.3 V regulator, which is
why it feels cold. The CYD is the heat source at perhaps 1.5-2.5 W, mostly
backlight. Two to three watts in a base of about 150 x 150 x 80 mm should settle
somewhere around 8-12 C above ambient by estimate - no fan needed.

Enclosure cautions:
  - Vents belong LOW on the sides or in the bottom, never on top. Students work
    with conductive electrode gel directly above this box.
  - Keep printed plastic 10 mm clear of the ESP32-S3 module, the PSRAM package
    and the 3.3 V regulator.
  - PLA creeps under sustained load well below its 60 C glass transition, and
    this base carries the head's weight while students lean on it. PETG is the
    better choice for the base.

Thermal drift does not threaten accuracy: the 0.1% standards are 25 ppm/C, so
10 C is 250 ppm (0.025%), and AC synchronous detection at 31.25 Hz inherently
rejects the near-DC errors - thermal EMFs, op-amp offset drift - that a warming
enclosure produces.


=======================================================================
SECOND DISPLAY OPTION: ELECROW CROWPANEL ESP32 HMI 7.0 INCH
=======================================================================

All four tool sketches support two display boards. Each sketch folder contains
its own copy of board_config.h; edit the copy in the folder you are compiling
and set one number:

    #define PM_BOARD 1     Sunton ESP32-8048S043C, 4.3 inch
    #define PM_BOARD 2     Elecrow CrowPanel ESP32 HMI, 7.0 inch

One line, one number. There is no way to select none or both - an earlier
two-toggle version made "neither" as easy to produce as "one", and the symptom
was a confusing "Error while detecting libraries included by ..." in the build
log, because a #error aborts preprocessing and the library-detection pass
reports that as a failure.

board_config.h also CHECKS the board settings at compile time, because these
are the ones that actually cost hours:

  PSRAM disabled      -> hard error. The core defines BOARD_HAS_PSRAM only when
                         PSRAM is enabled. An 800x480 16-bit framebuffer is
                         768 kB against about 512 kB of internal SRAM, so
                         without PSRAM the panel silently fails to allocate and
                         the screen just stays dark. Look for qio_qspi instead
                         of qio_opi in the build log as the giveaway.
  USB CDC On Boot on  -> hard error. It hands GPIO19/20 to the USB peripheral,
                         and those are the I2C pins on both boards.
  Partition "default" -> warning only. Fine for these small sketches; the main
                         firmware may outgrow a 1.2 MB app partition.

phantom_meter_v2 is still CYD-only. Porting it needs the DRDY decision below
settled first.

WHY THE PORT IS SMALL

Both boards are ESP32-S3 with an 800x480 RGB panel, a GT911 touch controller,
and - the decisive point - I2C on GPIO19/20. So the carrier board's SDA/SCL
wiring is identical on either one, and because the resolution matches exactly,
every coordinate in the drawing code is unchanged. A 7 inch panel at the same
pixel count simply makes the hexagons bigger, which for a student prepping
electrodes at arm's length is a real improvement.

WHAT DIFFERS

  RGB pin map and timing.  Completely different; see makeRGBPanel(). The
     Elecrow values are corroborated by two independent sources - a working
     Arduino_GFX config from that library's discussion #367, and the LovyanGFX
     config in the WZ8048C070 project. If the image is unstable, lower the
     prefer_speed argument from 16000000 to 12000000.

  Touch address.  0x5D on BOTH boards, as measured. Published configs for the
     Elecrow panel claim 0x14; that is wrong for this unit. The GT911 latches
     its address during reset from the INT pin level, and neither board breaks
     INT out, so it is fixed by board wiring. pm_touch_probe and
     pm_touch_calibrate both scan and use whichever address actually answers
     rather than trusting the header.

  Flash size.  4MB, not 16MB, so the partition scheme becomes Huge APP
     (3MB No OTA/1MB SPIFFS). The firmware is comfortably under 3MB and NVS
     still works for calibration storage.

  Touch transform.  Both boards invert both axes, but at different scales. The
     Elecrow's controller really is configured for 800x480, so its slope is
     within 5 percent of unity; the CYD's carries the 480x272 config of the
     sibling panel sharing its PCB, so its slope is 0.58. Both fits are
     measured and written into mapTouch(). Neither is a guess.

  Backlight is GPIO2 on both boards, by coincidence.

  USB CDC On Boot must be Disabled on both. GPIO19/20 are native USB pins on
     both, so the same trap applies.

POWER: THE ONE REAL OBSTACLE

The Elecrow's three connectors (I2C on 20/19, GPIO_D on 38, UART0 on 44/43)
all supply 3V3. None supplies 5V. On the carrier board, 5V reaches only R14
(10 ohm filter), C16, C18 and U13 - the MCP1700 3.3V LDO. Every active part
runs from the regulated 3.3V rail. So the carrier cannot be powered from these
connectors as wired. In order of preference:

The board's only other power connector is BAT, a 2-pin SH1.0 input for a
single-cell LiPo feeding the onboard charger. DO NOT tap it as a source. It is
wired to the charge-management node, so back-feeding it means fighting a
charger IC; and it sits at 3.0-4.2 V depending on state of charge, which the
MCP1700 cannot regulate from once the cell drops below about 3.4 V. That
failure mode is the nasty kind - fine on the bench, drifting during a class.

  1. RECOMMENDED: a two-port 5 V charger. One port runs USB-C to the display
     as normal; the other takes a USB-A-to-bare-wire pigtail into J3 pins 1
     and 2. Ports on one charger share ground internally, so it is effectively
     a single supply, nothing needs modifying, and the carrier's ~40 mA is
     trivial. Grounds also tie through the I2C connector.
  2. Any separate 5 V USB charger for the carrier, grounds tied. Two supplies
     sharing a ground is well behaved here: the only interconnections are I2C
     and GND.
  3. Last resort: short across U13 and feed the carrier from the connector's
     3V3. Electrically fine - the measurement is ratiometric, so rail accuracy
     barely matters - but it puts the analog rail on the display's digital
     supply and discards the regulator that exists to keep them apart.

FOR LATER: the BAT connector makes untethered operation possible - a LiPo
there powers the display for a demo or a classroom with no bench outlet. The
carrier would then need a small 5 V boost module, or option 3 above.

DRDY: TWO OPTIONS

J3 pin 5 carries the ADS1115's conversion-complete interrupt, GPIO17 on the
CYD. GPIO17 is I2S audio data on the Elecrow, so it needs a different pin:

  GPIO44 (UART0 RX) is the cleanest hardware choice - it is an input by
     default. GPIO43 (TX0) is driven as an output at boot, so avoid it.
  GPIO38 (GPIO_D) also works, but Elecrow's wiki says an onboard LED is on
     that pin, which may load it as a logic input. Verify before relying on
     it. Note board_config.h already uses GPIO38 as a harmless dummy touch
     reset, so do not use it for both.

  Better: drop the DRDY line and poll the ADS1115 with conversionComplete().
     The bus is at 400 kHz so each poll costs about 100 us, and the
     synchronous detection already uses fixed blanking rather than relying on
     interrupt latency. That takes J3 from five wires to four and makes the
     firmware portable to any ESP32-S3 board with GPIO19/20 free. Measure the
     timing impact against the interrupt version once the muxes are in.

WATCH FOR ON FIRST POWER-UP

Elecrow's wiki says a PCA9557 I/O expander handles the display reset on this
board. It will appear on the I2C scan around 0x18-0x1B, which clashes with
nothing of ours. If the panel comes up blank, that expander is the first
suspect - the reset may need releasing through it, and this varies by hardware
revision.

TEST ORDER (needs no multiplexers, so it can be done now)

  1. pm_panel_test      - is the RGB pin map and timing right? Colour bars
                          catch a transposed R/B group, the edge border
                          catches bad porch timing, the solid fields catch a
                          pixel clock that is too fast. Deliberately has no
                          I2C in it, so a blank screen means one thing only.
  2. pm_touch_probe     - touch answers, at which address, and are the raw
                          coordinates sanely oriented.
  3. pm_touch_calibrate - five-point transform for mapTouch().
  4. pm_i2c_scan        - with the carrier attached and powered: all five
                          carrier devices plus the touch controller.

Steps 1 to 3 require no carrier board at all, so they run before the power
question is solved. Only step 4 needs the carrier wired and powered.


=======================================================================
ELECROW CROWPANEL 7.0 - HARDWARE FINDINGS, 14 OCTOBER 2026
=======================================================================

The bundle now ships with PM_BOARD set to 2 (Elecrow), since that is the board
under test. Set it back to 1 for the Sunton CYD 4.3 inch.

VERIFIED BY pm_panel_test

  The RGB pin map and timing in makeRGBPanel() are correct. Colour bars in the
  right order (so the R/G/B pin groups are not transposed), a border on all
  four edges (so porch and sync widths are right and the full raster is
  addressable), corner labels in their matching corners (no rotation), centred
  crosshair, and uniform white and grey fields - so 16 MHz is comfortably
  within PSRAM bandwidth and there is no need to fall back to 12 MHz.

  The panel comes up without poking the PCA9557, so the TRY_PCA9557 fallback in
  pm_panel_test is not needed on this unit.

VERIFIED BY pm_touch_probe

  I2C scan shows 0x18 and 0x5D, unchanged before and after touch.begin().

  0x18 is the PCA9557 expander. Present as expected, no address conflict.

  0x5D is the GT911 - NOT 0x14. The third-party LovyanGFX config for the
  WZ8048C070 claims 0x14 and is wrong for this unit, whether through a
  hardware revision difference or an error in that project. board_config.h has
  been corrected. This is why pm_touch_probe scans rather than trusting the
  header, and why pm_touch_calibrate now does the same - a header that
  disagrees with the hardware would make the calibrator see no taps at all,
  which is a silent failure rather than an error.

  The address does not shift across touch.begin(), confirming that GPIO38 is
  inert as a dummy touch reset on this board.

  Touch reports correctly. A tap at the top-left corner reads (795, 474), so
  both axes are inverted with intercepts near 795 and 474.

VERIFIED BY pm_touch_calibrate, 15 OCTOBER 2026

  Five-point fit:

      target      raw        mapped      error
      ---------------------------------------------
       40, 40    778, 437     40, 46      +0,  +6
      760, 40     16, 454    766, 29      +6, -11
       40,440    784,  27     34,444      -6,  +4
      760,440     36,  43    747,429     -13, -11
      400,240    388, 227    411,250     +11, +10

  Least-squares fit, raw on screen:

      raw_x = 819.8 - 1.0486 * x
      raw_y = 483.9 - 1.0263 * y

  inverted and written into mapTouch() as

      x = 781.56 - 0.95296 * raw_x
      y = 470.96 - 0.97204 * raw_y

  Both axes inverted at essentially full scale, confirming the controller is
  configured for 800x480 - unlike the CYD. The worst residual is 13 px against
  a 24 px hexagon hit radius, so every one of the five taps lands on its
  intended target with room to spare. The closest site pair on the layout is
  GND to Fz at about 45 px, which is still comfortable.

  A full affine fit including cross terms (panel skew) was tried and rejected.
  It halved the rms error, from 8.6 to 6.8 px in x and 8.8 to 5.3 px in y, but
  barely moved the worst case: 12.7 to 12.1 px in x, 10.8 to 10.6 px in y. The
  cross terms it found amount to 14 px of x shift across the screen's height
  and 17 px of y shift across its width - the same size as the scatter between
  repeated taps at a single target, which ran 6 to 20 counts in this data. With
  only five points a genuine panel skew cannot be told apart from an unsteady
  finger, and fitting six parameters to five noisy points would be fitting the
  noise. Two parameters per axis it is.

  A second calibration pass is not worth running. The scatter is in the taps,
  not the panel, so another five points would shift the coefficients by a few
  units and the worst-case error would stay around 10 to 13 px - already well
  inside the hit radius. Run it again only if a site turns out to be hard to
  hit in practice, and if so run pm_touch_probe first: it now shows the mapped
  screen coordinates alongside the raw ones, so a mis-hit can be attributed to
  the transform or to the hexagon layout before anything is re-fitted.

VERIFIED BY pm_i2c_scan, 16 OCTOBER 2026 - STEP 4 COMPLETE

  All seven expected addresses answered on the first attempt:

      0x18   PCA9557 panel-reset expander    display
      0x20   MCP23017 U11                    carrier
      0x21   MCP23017 U12                    carrier
      0x48   ADS1115 M2                      carrier
      0x49   ADS1115 M3                      carrier
      0x5D   GT911 touch controller          display
      0x62   MCP4725 M1                      carrier

  What this settles, beyond "the bus works":

    The two-port 5V charger arrangement is sound. The carrier runs from its own
    5V feed through R14 and U13 while the display runs from the other port, and
    the two share ground through the charger plus the I2C cable's ground wire to
    J5 pin 2. No level shifting, no supply interaction, no noise problem at
    100 kHz.

    R15 and R16 stay unpopulated. The three breakout modules' own pull-ups are
    carrying the whole bus - now with the display's two devices on it as well as
    the carrier's five - with no flicker between refreshes. Fit them only if
    that changes at 400 kHz.

    Every socketed part is in the right way round and fully seated. Five
    carrier devices answering means U11, U12, M1, M2 and M3 all have power,
    ground, and both bus lines. That is the whole 3.3V rail proven, since U13
    feeds all of them.

    GPIO19/20 work as I2C on this board with the display's own two devices
    sharing the bus. No address collision: 0x18 and 0x5D sit clear of the
    carrier's five.

  Steps 1 through 4 are now complete on the Elecrow CrowPanel 7.0. Everything
  remaining is blocked on U1-U6 except the firmware port and the DRDY choice.

DRDY: SETTLED 17 OCTOBER 2026 - THE LINE IS GONE

  J3 pin 5 is no longer connected on either board. J3 is a four-wire interface
  now: 5V, GND, SDA, SCL.

  Why not GPIO44 or GPIO38: both would have worked electrically, but both tie
  the firmware to one board's pinout, and GPIO38 is already the dummy touch
  reset in board_config.h. Dropping the line makes the firmware portable to any
  ESP32-S3 board with GPIO19/20 free, which is what let the Elecrow port happen
  at all.

  THE TRAP, because the obvious implementation does not work: you cannot
  replace the interrupt by calling conversionComplete() in a loop. These ADCs
  run in CONTINUOUS mode. The config register's OS bit reads 0 while a
  conversion is in progress, and in continuous mode one always is, so
  conversionComplete() never returns true and the loop hangs. That bit only
  means anything in single-shot mode - which is why measureVref() can still use
  readADC_SingleEnded() safely, since that call switches to single-shot itself.

  What measureRaw() does instead is read at a FIXED interval slightly shorter
  than the conversion period. At 860 SPS a conversion lands every 1163 us;
  SAMPLE_US is 1000, so every conversion is picked up and a few are read twice.

  Duplicates are harmless here. Within one half-period the measured quantity is
  constant - the square wave has already settled, which is what BLANK_US is for
  - so re-reading the same conversion register changes the number of terms in
  the average but not its expected value. A duplicate cannot add information
  either, so the noise floor is still set by the genuine conversions.

  Simulated against a clock that charges 250 us per paired I2C read: 16 reads
  per half-period, 13 of them after the blanking window, against 11.2 genuine
  conversions available in those 13 ms. Verified identical across a 32-bit
  micros() rollover, since the comparison is a signed difference rather than an
  absolute one.

PORTED 17 OCTOBER 2026

  phantom_meter_v2 now uses board_config.h like the four bring-up tools. One
  PM_BOARD line selects the panel, its touch address and its touch transform.
  C_BG, C_TXT and C_DIM come from the header; only the status colours are local
  to the sketch, which is what the redefinition error would otherwise have been.

  Touch address is detected on the bus at startup rather than trusted from the
  header, the same "believe the bus" logic the bring-up tools use - the Elecrow
  is the reason, since published configs claim 0x14 and the hardware answers at
  0x5D.

  This runs with no multiplexers fitted. The head map, touch, LEDs, buzzer and
  temperature readout all work; every site simply reads OPEN until U1-U6 are in.
  That makes it the real test of the touch transform across all 22 hexagons.

SALT-BRIDGE DETECTION - WRITTEN 17 OCTOBER 2026, NOT YET ON HARDWARE

  In phantom_meter_v2.ino, behind "#define SALT_BRIDGE 1". Set it to 0 during
  bring-up steps 5 and 6: 31 extra measurements per cycle are pure latency when
  you are chasing something else.

  HOW IT MEASURES A BRIDGE. The drive and sense banks reach the PHANTOM contacts
  through J1; the return bank reaches the ELECTRO-CAP electrodes through J2. The
  two have independent selects and independent enables, so the firmware can
  drive site X and return through site Y. Current must leave the phantom contact
  at X, cross into the gel, travel to electrode Y and return. Bare PLA between
  the sites means no other path exists, so with no bridge the pair reads open,
  and with gel spanning X to Y the loop closes. No extra hardware, no extra
  wiring, nothing changed in the normal measurement path.

  This also makes the bench test in salt_bridge_pairs.txt exact: a 470 ohm to
  1 kohm resistor from the listed J1 pin to the listed J2 pin is electrically
  what the firmware looks for, and direction matters for a two-terminal resistor
  because the table is ordered uphill-to-downhill.

  SCHEDULE. Tier 1 every cycle, tier 2 every second, tier 3 every third:
  21.7 measurements per cycle instead of 31, with the steepest pairs - the ones
  gel actually runs down - at full rate. Two pairs per pass through loop(), one
  32 ms period each, so a cycle lands near 3.5 s.

  SCREEN THEN CONFIRM. One period is noisy as an absolute reading but ample for
  "is this under 50k". Only a pair that trips is re-measured at four periods, so
  a noise spike cannot raise a false alarm and the extra cost is paid only when
  something is actually there.

  HYSTERESIS IS MANDATORY. Trip below 50k, clear above 80k. A bridge sitting
  near one threshold would cross it on measurement noise alone, and the hexagons
  would chatter while the alarm retriggered every few seconds. Tune both numbers
  against a real gel bridge on the finished phantom; they depend on contact
  geometry and are a starting point, not a measurement.

  DISPLAY. Affected hexagons blink red, through the SAME change-detection path
  as everything else - siteColour() returns a different answer on alternate half
  seconds, so only those hexagons repaint. An arrow runs uphill to downhill,
  stopping 24 px short at each end so a blinking hexagon never chews into it,
  and it points at the site to clean first, because that is where gel pools. The
  panel names the worst pair: steepest tier first, then lowest impedance, with
  "+N" when several are up. The alarm is three short bursts, deliberately unlike
  the single chime that means a site has passed - a student should be able to
  tell good news from bad without looking up from the electrode.

  ARROWS AND REPAINTS. When the SET of bridges changes, the map region is
  blanked and rebuilt once. Erasing a line by redrawing it in the background
  colour would nick the head circle and any hexagon it passes near, and bridges
  change seconds apart at worst. This is not the five-a-second rebuild that
  caused the original flicker; the two are different problems.

  TESTED IN SIMULATION, 17 OCTOBER 2026. A fixture drives the real measureRaw()
  through stubbed ADCs that honour gain and DAC polarity, with a per-pair
  impedance table decoded from the actual mux writes. 17 checks, all passing:

    measureRaw recovered 29989 ohm from a 30000 ohm fixture (0.04 percent)
    60 passes of open circuit stayed silent
    a 30k bridge on C4->T4 was found on pass 5, inside one sweep cycle
    exactly one bridge reported, both its sites flagged, uninvolved sites not
    40 further passes raised no second alarm (edge triggered, not nagging)
    65k - inside the dead band - held the bridge latched, no chatter
    100k cleared it and released the blink flags
    two simultaneous bridges both reported, all four sites flagged
    brWorst preferred the steeper tier over the lower impedance
    tier rates measured at 1.0 / 0.5 / 0.33 per cycle as designed

  What simulation cannot tell us: the real threshold values, whether a gel
  bridge's impedance is stable enough for 50k/80k to be the right gap, and
  whether 3.5 s is fast enough to feel responsive to a student. All three need
  U1-U6 and a gel-wetted phantom.

FOOTSWITCH - WIRED UP 17 OCTOBER 2026

  J5 pin 1 reaches U12 GPB1 (expander pin 9), pulled high and pulled low by the
  pedal. The hardware was on the board from the start; until now nothing read
  it. The pin was configured as an input in setup() and then ignored, which is
  worth saying plainly because it looked connected.

  WHAT IT DOES. A short press moves the focus to the next site still above
  5 kOhm. A hold of one second toggles Mode A/B. One pedal, both jobs, and the
  common one is the short press: mode changes maybe twice a session while the
  focus moves twenty-two times, every one of them with gel on your fingers.

  SKIPPING PASSED SITES is the point of it. The pedal takes you to work that
  remains rather than walking the montage blindly. An OPEN site reads Z_OPEN,
  far above 5k, so it counts as work still to do - correct, since an open site
  is the least prepped of all. With everything passing the focus does not move
  and the panel says ALL SITES PASS rather than appearing to ignore the pedal.

  WHY POLLED, AND WHY FOUR TIMES A PASS. The MCP23017 has interrupt outputs but
  neither is wired to the ESP32 on this board, so the only way to see the pedal
  is to ask. A pass through loop() takes roughly 320 ms with salt-bridge
  scanning on, and a brisk tap is shorter than that, so polling once per pass
  would drop presses outright. footPoll() is called at four points instead:
  about 80 ms granularity for four I2C reads, roughly 600 us per pass.

  NEVER call footPoll() from inside measureRaw(). A long press writes the
  expander to change mode, and doing that mid-reading would reconfigure the
  switch fabric underneath a measurement in progress. All four call sites sit
  between measurements.

  THE HOLD FIRES ON THE THRESHOLD, not on release, so the mode changes while
  the foot is still down and a short chirp confirms it without looking up. From
  400 ms the panel shows HOLD = MODE, so a long press is never a surprise, and
  a release after the hold has fired does NOT also advance the focus.

  If no pedal is fitted the pull-up reads it as released and nothing happens.

  TESTED IN SIMULATION, 17 OCTOBER 2026, 16 checks all passing: idle produces
  nothing over 3 s; a 200 ms press advances one site; a 10 ms blip is rejected
  by the 25 ms debounce; contact chatter followed by a clean press counts once;
  a 1200 ms hold toggles mode and suppresses the short action; the hint appears
  at 500 ms and the mode fires at 1000 ms with the foot still down; focus skips
  passed sites, stops on an OPEN one, wraps past the end of the montage, and
  reports ALL SITES PASS when there is nothing left to do.

  Untestable until the pedal is in hand: whether 1000 ms is a comfortable hold
  and whether 25 ms debounces your particular switch. Both are one constant.

SCREEN REDESIGN - 17 OCTOBER 2026

  BIGGER MAP. HR 150 -> 172, hexagons 20 -> 23, focus ring 27 -> 30, centre
  moved to (260, 276). The map is 31 percent larger in area and each hexagon
  32 percent larger, with the same content in it.

  WHAT SETS THE CEILING. Fz and GND are the closest pair on the 10-20 layout at
  0.300 map units, so at HR=172 their centres are 51.6 px apart and two 23 px
  hexagons leave a 5.6 px gap - about what the old 150/20 pair gave. HR=182 was
  tried and rejected: the ears at 1.28 map units would run 5 px off the left
  edge of the map region once the focus ring is allowed for. HR=176 fits but
  leaves only 3.7 px at the sides, which is not enough for a focused ear's ring.

  COLOURS, CHOSEN BY MEASUREMENT. Background is a deep slate rather than black,
  so the status colours read as colour instead of as glare. Every chip was
  checked two ways: WCAG contrast for the label sitting on it, and separation
  after a deuteranopia simulation, because green against amber is exactly the
  pair a red-green colour blind student would lose. Worst label contrast is
  5.18:1 against a 4.5:1 target; the closest pair is still 105 units apart
  simulated, where a collision would be under about 40. C_OPEN is deliberately
  a cool blue-grey so "no contact at all" reads as a different kind of thing
  from "bad contact".

  ONE RESULT WORTH KNOWING: white on the red chip is 3.85:1, BELOW the line,
  while dark ink on it is 5.18:1. That is the opposite of what "red means
  danger, use white" would suggest, and it is why inkFor() picks the label
  colour per chip rather than using one colour throughout.

  FAUX BOLD, the one readability win that cost no layout change. The built-in
  5x7 glyph sits in a 6 px cell, so drawing each label twice one pixel apart
  thickens every stroke while staying INSIDE the cell - which matters, because
  the opaque first pass still erases the previous label completely and no stale
  ink can survive at the right edge. The second pass must be transparent:
  setTextColor() with one argument sets background equal to foreground, and
  Arduino_GFX skips background pixels when the two are equal. Verified against
  that library's drawChar, which guards every background write with
  "bg != color" - worth checking rather than assuming, since if it had been
  opaque the second pass would have painted solid blocks over every label.

  WHAT WAS TRIED AND REJECTED: the site name at text size 2. It fits
  arithmetically, but the three-character names - Fp1, Fp2, GND - then fill the
  hexagon edge to edge and squeeze the reading against the bottom. Rendered and
  looked at before deciding. Larger text and an in-hexagon reading cannot both
  fit at any map size the focus panel leaves room for; one of them has to go,
  and that is a call for the bench, not the simulator.

  ALSO: C_BG in board_config.h stays pure black, because an all-zeros field is
  part of what pm_panel_test checks. The instrument screen uses its own
  C_SCREEN instead, so the tools stay diagnostic and the UI gets its slate.

SAMPLING FAULT FOUND AND FIXED - 24 OCTOBER 2026

  SYMPTOM. With U1-U6 fitted, self-calibration reported every standard OPEN,
  including the SHORT channel - which is a dead short on the board. No cal
  resistor and no single mux channel can explain that.

  THE MEASUREMENT THAT SETTLED IT. pm_analog_probe on the SHORT channel:

      single-shot   I  hi-lo    -7847 counts     current IS flowing
      continuous    I  pk-pk        0 counts     the converter never moved
      continuous    V  pk-pk       63 counts     the bus was healthy
      samples used / read       50 in 60         the polling loop ran fine

  Exactly zero peak-to-peak across 60 reads is not noise; it is a converter
  handing back the same number every time. The voltage channel on 0x48 varied
  over the same run, so the fault was specific to the current channel in
  continuous mode.

  ROOT CAUSE, AND IT WAS MINE. Replacing the DRDY interrupt on 17 Oct, I put
  both ADCs in CONTINUOUS mode and read them on a fixed 1000 us timer. The
  reasoning was that conversionComplete() cannot work in continuous mode, since
  the config register's OS bit reads busy whenever a conversion is running and
  in continuous mode one always is. That reasoning was right. The conclusion
  drawn from it was wrong: the answer was not "poll on a timer instead", it was
  "do not use continuous mode".

  Because measureRaw decides "open" from the CURRENT it detects, a current
  channel stuck at a constant made every site read OPEN. A firmware fault
  wearing the costume of an open circuit.

  THE FIX. Single-shot, with both converters started together so the two
  conversions overlap instead of queueing: one 1163 us wait covers both. About
  9 sample pairs in the 13 ms post-blanking window, against the 11 the old
  scheme claimed and the 0 it actually delivered. conversionComplete() is
  meaningful in single-shot mode, which is the mode it was designed for, and
  pm_analog_probe had already proved that path works on this board.

  THE GUARD. measureRaw now tracks the current channel's peak-to-peak across a
  whole measurement. If it collected samples but the channel never moved at
  all, adcStuck is set and the calibration report says "ADC returned constant
  data - SAMPLING fault, not an open circuit". No data and open circuit must
  never again be reported as the same thing.

  WHY THE TESTS MISSED IT. The sampling path was verified only against stubs I
  wrote, and the stub returned a signal that tracked the DAC because I told it
  to. It was verifying my assumptions, not the hardware. Two changes followed:
  the stub now adds a few counts of noise, like a real converter, and it has a
  "stuck" mode that reproduces this exact fault. The calibration suite now
  checks both that the fault is caught and that a genuinely open channel is not
  mistaken for it - the false positive the noise change exposed.

  ATTEMPT 2 ALSO FAILED - 24 OCTOBER 2026, SAME EVENING

  The single-shot replacement reported "ADC returned constant data - SAMPLING
  fault" on the next power-up. The guard worked exactly as intended: one
  power-up to a correct diagnosis instead of another round chasing the analog
  path. But the fix itself was wrong again, and wrong in the SAME way.

  "Single-shot works" had been measured through readADC_Differential_0_1(), one
  blocking library call that starts, waits and reads a single converter.
  Attempt 2 did not use that call - it started both converters, polled
  conversionComplete() on each, then read both. A different code path, untested,
  shipped on the strength of a measurement that did not cover it.

  ATTEMPT 3 uses exactly the call that was measured working, sequentially on
  each converter. Likely reason the concurrent version fails: the ADS1115
  reports "not converting" for a short interval AFTER the start command and
  before conversion begins, so a fast poll of a freshly started converter sees
  the previous state as complete and reads a stale register. That is a
  hypothesis and the fix does not rest on it.

  WHAT IT COSTS. Two conversions in series at 860 SPS is about 2.4 ms plus bus
  overhead, so roughly 5 sample pairs land in the 13 ms post-blanking window
  instead of the 9 a concurrent scheme would give. Noise is about 1.4x the
  ideal. That is the right trade: a slightly noisier reading beats a
  confidently zero one.

  THE RULE THAT COMES OUT OF THIS: a measurement covers the code it actually
  ran, and nothing else. If the firmware is going to call X, the probe has to
  call X. pm_analog_probe now times four candidate methods side by side rather
  than one, so a future change can be chosen on evidence instead of reasoning.

  THE STUB GAP, ROUND TWO: switching measureRaw to readADC_Differential_0_1()
  made nine tests fail, because the stub returned a constant 1234 from that
  call - the same class of fixture gap that let the original fault through. The
  stub now routes it through the same model as every other read.

  TOOL ADDED: pm_analog_probe, bring-up tool 5. It measures the same signal two
  independent ways - single-shot, which bypasses the polling code entirely, and
  continuous as the firmware does it - and names the verdict on screen. Keep it:
  it distinguishes "no current" from "current not sampled", and nothing else in
  the kit can.

PROBE RESULTS ACROSS FOUR CHANNELS - 24 OCTOBER 2026

                    method 1   method 2   method 3   method 4
                    SEQUENTIAL CONCURRENT TIMED WAIT CONTINUOUS
    SHORT ch26        5760          0          0          0
    1k    ch22        6449          0          0          0
    100k  ch24        4454       8687       9509       8175
    Cz    ch09          18         23         26         30   (open, correct)

  Method 1 is the only one that behaves like a measurement. Methods 2, 3 and 4
  read exactly zero on two channels and roughly twice method 1 on a third,
  which is the signature of stale or aliased register reads rather than a clean
  failure. They are not worth diagnosing further - nothing uses them.

  THE ANALOG CHAIN IS PROVEN GOOD. Working method 1's raw counts back by hand,
  using only the probe's own gains:

    channel      I pk-pk  V pk-pk     I (uA)   V_load     Z measured   actual
    SHORT ch26      5760       83      0.450   1.30 mV         2882        0
    1k    ch22      6449      157      0.504   2.45 mV         4869     1000
    100k  ch24      4454     2131      0.348  33.30 mV        95689   100000

  The 100 kohm standard computes to 95689 ohm from raw counts with NO
  calibration applied - 4.3 percent. That number exercises the Howland pump,
  R17, the drive mux, the standard, the return mux, SW5, the transimpedance
  amplifier, the sense mux and the sense amplifier, all at once. U1-U6 are in
  correctly and the whole measurement path works.

  The two-point fit then lands 1k and 100k exactly on their standards by
  construction, and the SHORT channel corrects to -1166 ohm, which becomes the
  return-path offset subtracted from every reading.

NEW OPEN ITEM: DRIVE CURRENT IS 5.8x LOW

  Design is 2.5 uA, from 0.25 V across a 100 kohm Howland network. Measured is
  0.434 uA averaged over the three loaded channels. The DAC is not at fault -
  the probe reports it commanding 1743/2363, a 620-count swing, which is
  0.500 V exactly as intended.

  0.25 V / 0.434 uA implies about 576 kohm of effective series resistance where
  there should be 100 kohm. Worth measuring, in this order: R1, R2, R3 and R4,
  which should all be 100k and which set the Howland gain; then R17 (150k) and
  R18 (200k). A wrong value or a bad joint on R4 in particular degenerates the
  pump from a current source toward a voltage source, and the measured ratio
  between the short and the 100k channels does lean that way.

  WHY IT IS NOT BLOCKING. The measurement is ratiometric - Z comes from
  Vsense/Vtia, and the firmware measures the current rather than assuming it, so
  both scale together and Z stays right. That is exactly the property the design
  was given for this reason, and the 95689 figure is it working.

  WHAT IT COSTS. Signal-to-noise, proportionally. Readings will be noisier than
  designed and the mid-scale R21 check is the number to watch, since it is the
  only one the two-point fit does not force. Chase the resistors when the
  instrument is otherwise working, not before.

ONE TAP COUNTED SEVERAL TIMES - FIXED 24 OCTOBER 2026

  Found in pm_analog_probe: one tap skipped two or three channels, so a given
  channel could not be held on screen. Every touch loop in the kit asked "is a
  touch being reported?" instead of "did a finger just come down?", and the
  controller can keep reporting a touch for a moment after the finger lifts.

  The same bug was live in the main firmware's mode toggle, where it was worse:
  a toggle undoes itself, so one tap read an even number of times left the mode
  exactly where it started and looked like the tap had been ignored.

  Now: tapEdge() fires once per finger-down, starts each listening window with
  wasDown = true so a press carried over from the previous screen cannot fire,
  and applies a 400 ms hold-off for a controller that reports up-down-up for one
  physical tap. Sites stay level-triggered on purpose - holding a finger on a
  hexagon just keeps selecting it, which is harmless. touchtest: 7 checks.

  The probe now shows its position, e.g. 3/6, in the header.

PROBE, SECOND SESSION - 24 OCTOBER 2026

    channel       method 1 I pk-pk   current    V pk-pk
    4k7 R21 ch23        8646         0.68 uA      457     alive, in circuit
    OPEN    ch25           0         0.00 uA        2     no leakage
    SHORT   ch26        9082         0.71 uA       92

  Every calibration channel - 1k, 4k7, 100k, short, open - has now been seen
  behaving correctly through method 1. The OPEN channel reading 0 counts with 2
  counts of voltage means no mux is stuck on.

  The probe is a go/no-go tool, not an accuracy check: it takes one high and
  one low reading at the coarse gain. The calibration report, 16 periods at a
  16x finer gain on the low-value standards, is where R21's accuracy is judged.

  DRIVE CURRENT MOVED BETWEEN SESSIONS. SHORT channel: 0.45 uA in the first
  session, 0.71 uA in this one - 58 percent - with the DAC commanding identical
  codes (1743 / 2363) and vref identical (1.655 V) both times. A core or library
  change cannot do that; the DAC voltage is static when it is read. An analog
  current that wanders between sessions with the same command points at an
  intermittent joint, which strengthens the case for checking R1-R4, and R4 in
  particular, before trusting readings to better than a few percent.

STILL OPEN

  Steps 5 and 6: self-calibration against the on-board standards, then a live
  impedance reading. Both need U1-U6.

  Salt-bridge thresholds and sweep rate, once there is a real gel bridge to
  measure.


pm_dac_hold (added 25 Oct 2026) - bring-up tool 6. Parks the DAC at the
firmware's LOW and HIGH levels so the drive chain (M1 -> R7 -> C9 -> U9A) can
be checked point by point with a multimeter. Tap to switch levels, or wait:
it switches itself every 20 s. The header says if the touch controller answers.

28 Oct 2026 - head-map screen redrawn with smooth text.
  phantom_meter_v2 now has a SECOND file beside board_config.h: pm_fonts.h.
  Keep both in the phantom_meter_v2 folder; the sketch will not compile
  without them. pm_fonts.h holds anti-aliased glyphs from Inter Display Bold
  (SIL Open Font License), generated, not hand-edited. Hexagons are now
  flat-top and larger (radius 27, was 23), with labels about twice as tall;
  readings on the hexagons are whole kilohms above 10 k ("12", "149") so they
  fit. The focus panel's site name and reading use 64 px and 72 px type.
  screen_preview.png shows the new layout.
