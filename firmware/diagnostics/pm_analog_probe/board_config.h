/* board_config.h - display board selection for the EEG phantom impedance meter
 *
 * Both supported boards are ESP32-S3 with an 800x480 RGB panel, a GT911 touch
 * controller, and I2C on GPIO19/20 - which is why the carrier board's SDA/SCL
 * wiring is identical on either one, and why all the drawing code is unchanged.
 * What differs is the RGB pin map, the panel timing, the touch I2C address, the
 * flash size, and the touch coordinate transform.
 *
 * SELECT A BOARD: set PM_BOARD below to 1 or 2. One line, one number - there
 * is deliberately no way to select none or both.
 *
 * NOTE: every sketch folder carries its OWN copy of this file. Edit the copy
 * in the folder you are compiling. Editing the wrong copy is the single most
 * likely mistake here, and it produces a compile error rather than silence.
 *
 * ---------------------------------------------------------------------------
 * PM_BOARD 1  Sunton ESP32-8048S043C, 4.3 inch, 16MB flash / 8MB PSRAM
 *             (silkscreen may say ESP32-4827S043 - same PCB, two products)
 *   Board:            ESP32S3 Dev Module
 *   Flash Size:       16MB (128Mb)
 *   Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)
 *   PSRAM:            OPI PSRAM
 *   USB CDC On Boot:  Disabled
 *
 * PM_BOARD 2  Elecrow CrowPanel ESP32 HMI 7.0 inch, ESP32-S3-WROOM-1-N4R8,
 *             4MB flash / 8MB PSRAM
 *   Board:            ESP32S3 Dev Module
 *   Flash Size:       4MB (32Mb)
 *   Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)
 *   PSRAM:            OPI PSRAM
 *   Flash Mode:       QIO 80MHz
 *   USB CDC On Boot:  Disabled
 *
 * USB CDC On Boot must be Disabled on BOTH boards. GPIO19/20 are the ESP32-S3's
 * native USB D-/D+ pins on both, so leaving CDC enabled hands those pins to the
 * USB peripheral and the I2C bus dies. For the same reason, a USB cable running
 * to a computer loads those pins: when testing I2C, power from a plain charger.
 *
 * The three #if checks below catch the board settings that actually bite. They
 * are compile-time, so a wrong setting fails in seconds instead of presenting
 * as a blank screen or a dead I2C bus.
 * ---------------------------------------------------------------------------
 */
#pragma once

#define PM_BOARD 2

#if PM_BOARD != 1 && PM_BOARD != 2
#error "board_config.h: PM_BOARD must be 1 (Sunton CYD 4.3in) or 2 (Elecrow CrowPanel 7.0in)"
#endif

// PSRAM. The core defines BOARD_HAS_PSRAM only when PSRAM is enabled in Tools.
// An 800x480 16-bit framebuffer is 768 kB; internal SRAM is about 512 kB, so
// without PSRAM the panel cannot allocate and the screen stays dark with no
// other symptom. This is the most expensive setting to get wrong.
#ifndef BOARD_HAS_PSRAM
#error "PSRAM is disabled. Set Tools > PSRAM to 'OPI PSRAM'. An 800x480 framebuffer needs 768 kB and will not fit in internal SRAM."
#endif

// USB CDC On Boot. Enabled hands GPIO19/20 to the USB peripheral, and those
// are the I2C pins on both boards.
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
#error "Set Tools > USB CDC On Boot to 'Disabled'. GPIO19/20 are the native USB pins and carry the I2C bus."
#endif

// Partition scheme. Not fatal for the small tool sketches, but the default
// scheme leaves a 1.2 MB app partition on 4 MB flash, which the main firmware
// may outgrow. A warning, not an error.
#if defined(ARDUINO_PARTITION_default)
#warning "Partition Scheme is 'default'. Prefer 'Huge APP (3MB No OTA/1MB SPIFFS)' on 4MB flash, or '16M Flash (3MB APP/9.9MB FATFS)' on 16MB."
#endif

#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>

// ------------------------------------------------------------- common to both
#define PIN_SDA 19
#define PIN_SCL 20
#define GFX_BL   2          // backlight; GPIO2 on both boards, by coincidence

// Raw RGB565 values, not colour names: the bare BLACK/WHITE constants are not
// defined in GFX Library for Arduino 1.6.9.
static const uint16_t C_BG = 0x0000, C_TXT = 0xFFFF, C_DIM = 0x7BEF;

// ------------------------------------------------------------------ per board
#if PM_BOARD == 1
  #define BOARD_NAME        "CYD 4.3in (ESP32-8048S043C)"
  #define TOUCH_ADDR        GT911_ADDR1     // 0x5D
  #define TOUCH_RST_PIN     38
#else
  #define BOARD_NAME        "Elecrow CrowPanel 7.0in"
  // 0x5D, VERIFIED on this board by pm_touch_probe, 14 Oct 2026. A third-party
  // LovyanGFX config for the WZ8048C070 claims 0x14; that is wrong for this
  // unit - either a hardware revision difference or an error in that project.
  // The bus is the authority.
  #define TOUCH_ADDR        GT911_ADDR1
  // GPIO38 is the GPIO_D connector pin on this board. It is NOT the GT911's
  // reset - the panel holds that high itself - so driving it in begin() is
  // harmless. Do not also use GPIO38 for DRDY; use GPIO44 (UART0 RX, an input
  // by default) or switch the firmware to polling the ADS1115.
  #define TOUCH_RST_PIN     38
#endif

// Panel construction. The Elecrow numbers came from two agreeing sources - a
// working Arduino_GFX config from that library's discussion #367 and the
// LovyanGFX config in the WZ8048C070 project - and are now VERIFIED on
// hardware by pm_panel_test, 14 Oct 2026: correct colour-bar order, border on
// all four edges, no rotation, and uniform white/grey fields at 16 MHz (so no
// need to fall back to 12000000).
static inline Arduino_ESP32RGBPanel *makeRGBPanel() {
#if PM_BOARD == 1
  return new Arduino_ESP32RGBPanel(
      40 /*DE*/, 41 /*VSYNC*/, 39 /*HSYNC*/, 42 /*PCLK*/,
      45, 48, 47, 21, 14,              /* R0-R4 */
      5, 6, 7, 15, 16, 4,              /* G0-G5 */
      8, 3, 46, 9, 1,                  /* B0-B4 */
      0, 8, 4, 8,                      /* hsync: pol, front, pulse, back */
      0, 8, 4, 8,                      /* vsync: pol, front, pulse, back */
      1 /*pclk_active_neg*/, 16000000);
#else
  return new Arduino_ESP32RGBPanel(
      41 /*DE*/, 40 /*VSYNC*/, 39 /*HSYNC*/, 0 /*PCLK*/,
      14, 21, 47, 48, 45,              /* R0-R4 */
      9, 46, 3, 8, 16, 1,              /* G0-G5 */
      15, 7, 6, 5, 4,                  /* B0-B4 */
      0, 40, 48, 40,                   /* hsync: pol, front, pulse, back */
      0, 1, 31, 13,                    /* vsync: pol, front, pulse, back */
      1 /*pclk_active_neg*/, 16000000);
#endif
}

// Touch coordinate transform, raw controller units -> screen pixels.
//
// CYD: inverted on both axes AND compressed, because its GT911 carries the
// 480x272 configuration of the sibling ESP32-4827S043 that shares the PCB.
// Five-point fit, 13 Oct 2026, residuals under 13 px.
//
// Elecrow: inverted on both axes at very nearly full scale - its GT911 really
// is configured for 800x480, unlike the CYD's. Five-point fit, 15 Oct 2026,
// residuals under 13 px:
//
//     raw_x = 819.8 - 1.0486 * x      ->   x = 781.56 - 0.95296 * raw_x
//     raw_y = 483.9 - 1.0263 * y      ->   y = 470.96 - 0.97204 * raw_y
//
// Written in that second form below - intercept minus slope times raw. Note
// that it is NOT the same as (781.56 - raw_x) * 0.95296, which moves the
// intercept by 37 px; the slope multiplies only the raw term.
//
// A full affine fit with cross terms (panel skew) halved the rms error but
// barely moved the worst case, and the cross terms it found - 14 px of x shift
// across the screen's height, 17 px of y shift across its width - are the same
// size as the scatter between repeated taps at one target, 16 to 20 counts. So
// the skew is not distinguishable from tapping imprecision with five points
// and was deliberately left out. Two parameters per axis, max error ~13 px,
// inside a 24 px hexagon hit radius.
static inline void mapTouch(int rawx, int rawy, int &x, int &y) {
#if PM_BOARD == 1
  x = (int)((785.1f - rawx) * 1.7143f);
  y = (int)((471.7f - rawy) * 1.8223f);
#else
  x = (int)(781.56f - 0.95296f * rawx);
  y = (int)(470.96f - 0.97204f * rawy);
#endif
  x = constrain(x, 0, 799);
  y = constrain(y, 0, 479);
}
