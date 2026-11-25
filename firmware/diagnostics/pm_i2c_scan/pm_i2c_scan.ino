/* pm_i2c_scan - EEG Head Phantom impedance meter, bring-up tool 1 of 3
 *
 * Scans the I2C bus and lists every address that answers, on the DISPLAY as
 * well as the serial monitor. The display output is the important one:
 * GPIO19/20 are the ESP32-S3's native USB pins, so serial is not reliably
 * usable while I2C is in play.
 *
 * Board settings are listed in board_config.h.
 *
 * Expected on a healthy system:
 *   0x20 / 0x21  MCP23017 port expanders U11 / U12 (carrier board)
 *   0x48 / 0x49  ADS1115 ADC modules in M2 / M3   (carrier board)
 *   0x62 or 0x60 MCP4725 DAC module in M1         (carrier board)
 *                (0x62 = Adafruit module, 0x60 = most generic modules)
 *   0x5D         GT911 touch controller (display) - may only appear after a
 *                sketch has initialised touch; that is normal
 *   0x18         PCA9557 expander (display), handles panel reset.
 *                Expected; not a fault.
 *
 * Reading the result:
 *   touch address only    -> bus is not reaching the carrier board. Check that
 *                            BOTH connections exist: power, and SDA/SCL from
 *                            GPIO19/20 to J3 pins 3 and 4.
 *   touch address missing -> fault on the display side of the bus, or a USB
 *                            host is loading GPIO19/20. Power from a charger.
 *   0x60-0x63 missing     -> MCP4725 unpowered or its SDA/SCL not reaching the
 *                            bus. Measure 3.3V on the module's VIN pad first.
 *   0x20 or 0x21 missing  -> that MCP23017 is backwards or not seated.
 *   0x48 or 0x49 missing  -> that ADS1115 is backwards, or its ADDR strap is off.
 *
 * NOTE for the Elecrow board: its I2C/GPIO/UART connectors all supply 3V3, not
 * 5V. The carrier board's J3 pin 1 wants 5V, which on that board feeds only the
 * MCP1700 3.3V LDO. Give the carrier its own 5V supply and tie grounds. The BAT
 * connector is a single-cell LiPo input wired to the charger, NOT a 5V rail -
 * do not tap it as a source.
 */

#include <Wire.h>
// Included here as well as in board_config.h so the IDE's library discovery
// does not depend on it following a local header. Harmless duplication.
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include "board_config.h"

Arduino_ESP32RGBPanel *rgbpanel = makeRGBPanel();
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(800, 480, rgbpanel, 0, true);

void setup() {
  Serial.begin(115200);
  delay(300);
  gfx->begin();
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
  Wire.begin(PIN_SDA, PIN_SCL, 100000);
}

void loop() {
  gfx->fillScreen(C_BG);
  gfx->setTextColor(C_TXT, C_BG);
  gfx->setTextSize(3);
  gfx->setCursor(20, 20);
  gfx->println("I2C SCAN");
  gfx->setTextSize(2);
  gfx->setTextColor(C_DIM, C_BG);
  gfx->setCursor(20, 60);
  gfx->println(BOARD_NAME);

  Serial.println("--- I2C scan --- " BOARD_NAME);
  gfx->setTextSize(3);
  gfx->setTextColor(C_TXT, C_BG);
  int n = 0, y = 110;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      n++;
      Serial.printf("found 0x%02X\n", a);
      gfx->setCursor(20, y);
      gfx->printf("0x%02X", a);
      y += 40;
    }
  }
  if (n == 0) {
    Serial.println("none");
    gfx->setCursor(20, y);
    gfx->println("NONE");
  }
  Serial.printf("%d device(s)\n\n", n);
  delay(3000);
}
