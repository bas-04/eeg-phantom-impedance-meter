/* pm_touch_probe - EEG Head Phantom impedance meter, bring-up tool 2 of 3
 *
 * Confirms the GT911 touch controller is alive and reports RAW controller
 * coordinates live. Select your board in board_config.h.
 *
 * The GT911 answers at one of two addresses, latched during reset from the
 * level of its INT pin: 0x5D or 0x14. Neither board breaks INT out, so the
 * address is whatever the board's wiring produces - 0x5D on the CYD, 0x14 on
 * the Elecrow CrowPanel. This sketch does not trust board_config.h on that
 * point: it scans first and uses whichever address actually answered, then
 * tells you which one it picked. If the scan shows one address and the header
 * expects the other, the header is wrong and should be corrected.
 *
 * Three things to read off it:
 *   1. Does the address move when touch.begin() resets the chip? The scan runs
 *      before and after, so you can see it shift. If it does, the library is
 *      asking the wrong address and will never report a touch.
 *   2. Does a press register? The bottom line flips between "no touch" and
 *      "TOUCH n=... x=... y=...".
 *   3. Are coordinates oriented correctly? Tap the top-left corner. Both values
 *      should read near zero. Near 800/480 means both axes are inverted; x
 *      tracking your finger's vertical movement means the axes are swapped.
 *      Use pm_touch_calibrate to pin down the exact mapping.
 *   4. Does the transform in board_config.h actually work? The second line
 *      shows the same touch after mapTouch(), in screen pixels. Press a corner
 *      or the middle of an edge and the mapped numbers should agree with where
 *      your finger is. This is the check that the calibration fit was applied
 *      correctly - raw numbers alone cannot tell you that.
 */

#include <Wire.h>
// Included here as well as in board_config.h so the IDE's library discovery
// does not depend on it following a local header. Harmless duplication.
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include "board_config.h"

Arduino_ESP32RGBPanel *rgbpanel = makeRGBPanel();
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(800, 480, rgbpanel, 0, true);

// INT is not broken out on either board, so it is passed as -1.
TAMC_GT911 touch(PIN_SDA, PIN_SCL, -1, TOUCH_RST_PIN, 800, 480);

uint8_t usedAddr = TOUCH_ADDR;

// Returns true if anything answered at addr.
bool present(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void scanTo(int y, const char *label) {
  gfx->setTextColor(C_TXT, C_BG);
  gfx->setCursor(20, y);
  gfx->print(label);
  int x = 360;
  for (uint8_t a = 1; a < 127; a++) {
    if (present(a)) {
      gfx->setCursor(x, y);
      gfx->printf("%02X", a);
      x += 70;
      if (x > 760) break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  gfx->begin();
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
  gfx->fillScreen(C_BG);
  gfx->setTextSize(2);

  Wire.begin(PIN_SDA, PIN_SCL, 400000);

  gfx->setTextColor(C_DIM, C_BG);
  gfx->setCursor(20, 20);
  gfx->println(BOARD_NAME);

  scanTo(55, "before touch.begin:");

  // Believe the bus, not the header.
  if      (present(GT911_ADDR2)) usedAddr = GT911_ADDR2;   // 0x14
  else if (present(GT911_ADDR1)) usedAddr = GT911_ADDR1;   // 0x5D

  touch.begin(usedAddr);
  delay(100);
  scanTo(90, "after  touch.begin:");

  gfx->setTextColor(C_DIM, C_BG);
  gfx->setCursor(20, 130);
  gfx->printf("using touch address 0x%02X   (header expects 0x%02X)",
              usedAddr, (uint8_t)TOUCH_ADDR);
  gfx->setTextColor(C_TXT, C_BG);
  gfx->setCursor(20, 170);
  gfx->print("tap the screen - try the top-left corner");
}

void loop() {
  touch.read();
  gfx->fillRect(0, 210, 800, 80, C_BG);
  gfx->setTextColor(C_TXT, C_BG);
  gfx->setCursor(20, 220);
  if (touch.isTouched) {
    int rx = touch.points[0].x, ry = touch.points[0].y;
    gfx->printf("RAW     n=%d  x=%4d  y=%4d", touch.touches, rx, ry);
    int mx, my;
    mapTouch(rx, ry, mx, my);
    gfx->setCursor(20, 250);
    gfx->printf("MAPPED       x=%4d  y=%4d", mx, my);
  } else {
    gfx->print("no touch");
  }
  delay(50);
}
