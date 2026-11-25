/* board_config.h - display board definition for the EEG phantom impedance meter
 *
 * Target: Elecrow CrowPanel ESP32 HMI 7.0 inch (SKU DIS08070H),
 *         ESP32-S3-WROOM-1-N4R8: 4 MB flash, 8 MB PSRAM, 800x480 RGB panel,
 *         GT911 touch controller, I2C on GPIO19/20.
 *
 * Earlier development also supported the Sunton ESP32-8048S043C (4.3 inch
 * "CYD") through a PM_BOARD switch. That second board was dropped when the
 * repository was cleaned up; the dual-board version is preserved at the git
 * tag `dual-display-snapshot`.
 *
 * NOTE: every sketch folder carries its OWN identical copy of this file,
 * because the Arduino IDE only compiles files inside the sketch folder. If you
 * change one, change them all (see firmware/README.md).
 *
 * ---------------------------------------------------------------------------
 * Arduino IDE board settings (Tools menu):
 *   Board:            ESP32S3 Dev Module
 *   Flash Size:       4MB (32Mb)
 *   Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)
 *   PSRAM:            OPI PSRAM
 *   Flash Mode:       QIO 80MHz
 *   USB CDC On Boot:  Disabled
 *
 * USB CDC On Boot must be Disabled. GPIO19/20 are the ESP32-S3's native USB
 * D-/D+ pins, so leaving CDC enabled hands those pins to the USB peripheral
 * and the I2C bus dies. For the same reason, a USB cable running to a computer
 * loads those pins: when testing I2C, power the display from a plain charger.
 *
 * The #if checks below catch the board settings that actually bite. They are
 * compile-time, so a wrong setting fails in seconds instead of presenting as a
 * blank screen or a dead I2C bus.
 * ---------------------------------------------------------------------------
 */
#pragma once

// PSRAM. The core defines BOARD_HAS_PSRAM only when PSRAM is enabled in Tools.
// An 800x480 16-bit framebuffer is 768 kB; internal SRAM is about 512 kB, so
// without PSRAM the panel cannot allocate and the screen stays dark with no
// other symptom. This is the most expensive setting to get wrong.
#ifndef BOARD_HAS_PSRAM
#error "PSRAM is disabled. Set Tools > PSRAM to 'OPI PSRAM'. An 800x480 framebuffer needs 768 kB and will not fit in internal SRAM."
#endif

// USB CDC On Boot. Enabled hands GPIO19/20 to the USB peripheral, and those
// are the I2C pins.
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
#error "Set Tools > USB CDC On Boot to 'Disabled'. GPIO19/20 are the native USB pins and carry the I2C bus."
#endif

// Partition scheme. Not fatal for the small diagnostic sketches, but the
// default scheme leaves a 1.2 MB app partition on 4 MB flash, which the main
// firmware may outgrow. A warning, not an error.
#if defined(ARDUINO_PARTITION_default)
#warning "Partition Scheme is 'default'. Use 'Huge APP (3MB No OTA/1MB SPIFFS)' on the CrowPanel's 4MB flash."
#endif

#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>

#define BOARD_NAME   "Elecrow CrowPanel 7.0in"
#define PIN_SDA      19
#define PIN_SCL      20
#define GFX_BL        2     // backlight

// Raw RGB565 values, not colour names: the bare BLACK/WHITE constants are not
// defined in GFX Library for Arduino 1.6.x.
static const uint16_t C_BG = 0x0000, C_TXT = 0xFFFF, C_DIM = 0x7BEF;

// Touch controller. 0x5D, VERIFIED on this board by pm_touch_probe on
// 14 Oct 2026. A third-party LovyanGFX config for this panel (WZ8048C070)
// claims 0x14; that is wrong for this unit. The sketches scan the bus at start
// and use whichever address answers, so this is only the default.
#define TOUCH_ADDR    GT911_ADDR1   // 0x5D
// GPIO38 is the GPIO_D connector pin on this board. It is NOT the GT911's
// reset - the panel holds that high itself - so driving it in begin() is
// harmless. Do not use GPIO38 for anything else while this is set.
#define TOUCH_RST_PIN 38

// RGB panel pin map and timing. Corroborated by two independent sources (a
// working Arduino_GFX config from that library's discussion #367 and the
// LovyanGFX config in the WZ8048C070 project) and VERIFIED on hardware by
// pm_panel_test, 14 Oct 2026: correct colour-bar order, border on all four
// edges, no rotation, and uniform white/grey fields at 16 MHz. If the image is
// ever unstable, lower the last argument from 16000000 to 12000000.
static inline Arduino_ESP32RGBPanel *makeRGBPanel() {
  return new Arduino_ESP32RGBPanel(
      41 /*DE*/, 40 /*VSYNC*/, 39 /*HSYNC*/, 0 /*PCLK*/,
      14, 21, 47, 48, 45,              /* R0-R4 */
      9, 46, 3, 8, 16, 1,              /* G0-G5 */
      15, 7, 6, 5, 4,                  /* B0-B4 */
      0, 40, 48, 40,                   /* hsync: pol, front, pulse, back */
      0, 1, 31, 13,                    /* vsync: pol, front, pulse, back */
      1 /*pclk_active_neg*/, 16000000);
}

// Touch coordinate transform, raw controller units -> screen pixels.
//
// Inverted on both axes at very nearly full scale (the GT911 is configured for
// 800x480). Five-point fit with pm_touch_calibrate, 15 Oct 2026, residuals
// under 13 px against a 24 px hexagon hit radius:
//
//     raw_x = 819.8 - 1.0486 * x      ->   x = 781.56 - 0.95296 * raw_x
//     raw_y = 483.9 - 1.0263 * y      ->   y = 470.96 - 0.97204 * raw_y
//
// Written in that second form below - intercept minus slope times raw. Note
// that it is NOT the same as (781.56 - raw_x) * 0.95296, which moves the
// intercept by 37 px; the slope multiplies only the raw term.
//
// A full affine fit with cross terms (panel skew) halved the rms error but
// barely moved the worst case, and the cross terms it found were the same size
// as the scatter between repeated taps at one target. Two parameters per axis.
static inline void mapTouch(int rawx, int rawy, int &x, int &y) {
  x = (int)(781.56f - 0.95296f * rawx);
  y = (int)(470.96f - 0.97204f * rawy);
  x = constrain(x, 0, 799);
  y = constrain(y, 0, 479);
}
