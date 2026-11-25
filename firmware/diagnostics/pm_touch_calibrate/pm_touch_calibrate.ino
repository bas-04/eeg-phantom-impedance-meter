/* pm_touch_calibrate - EEG Head Phantom impedance meter, bring-up tool 3 of 3
 *
 * Determines the exact transform between raw GT911 coordinates and screen
 * pixels. Draws a crosshair at five known positions; tap each and the raw
 * reading is recorded. After the fifth tap the screen lists all five pairs as
 * "target -> raw". Board settings are listed in board_config.h.
 *
 * Five points, not one. A single corner tap CANNOT distinguish a plain
 * 180-degree inversion from an inverted-and-scaled mapping, because near the
 * corner the two agree - they only diverge mid-screen. That is exactly how the
 * CYD's transform was initially misdiagnosed.
 *
 * Tap precisely: the centre of your fingertip on the circle, not the edge. A
 * sloppy tap produces a sloppy transform.
 *
 * Using the result: fit raw = a + b*screen for each axis, then put the inverse
 * into mapTouch() in board_config.h. On the CrowPanel 7.0 (15 Oct 2026) that
 * came out as
 *
 *     x = 781.56 - 0.95296 * rawx
 *     y = 470.96 - 0.97204 * rawy
 *
 * i.e. inverted on both axes at nearly full scale. Re-run this only if taps
 * land on the wrong hexagon, or for a different panel.
 *
 * Raw arithmetic is used instead of the library's setRotation() because a
 * subtract-and-scale handles inversion AND scaling, which a rotation cannot.
 *
 * NOTE: do not name these arrays TX and TY. The ESP32 core already defines
 * those as UART pin numbers in pins_arduino.h and the sketch will not compile.
 */

#include <Wire.h>
// Included here as well as in board_config.h so the IDE's library discovery
// does not depend on it following a local header. Harmless duplication.
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include "board_config.h"

Arduino_ESP32RGBPanel *rgbpanel = makeRGBPanel();
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(800, 480, rgbpanel, 0, true);

TAMC_GT911 touch(PIN_SDA, PIN_SCL, -1, TOUCH_RST_PIN, 800, 480);

// The GT911 answers at 0x5D or 0x14 depending on board wiring, and a header
// that disagrees with the hardware would leave this sketch seeing no taps at
// all - a silent failure that wastes a session. So find it on the bus, exactly
// as pm_touch_probe does, and fall back to the header only if neither answers.
uint8_t usedAddr = TOUCH_ADDR;

bool present(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

const int NT = 5;
const int CROSS_X[NT] = { 40, 760,  40, 760, 400 };
const int CROSS_Y[NT] = { 40,  40, 440, 440, 240 };
int idx = 0;
int rawX[NT], rawY[NT];
bool wasTouched = false;

void drawTarget() {
  gfx->fillScreen(C_BG);
  gfx->setTextColor(C_TXT, C_BG);
  gfx->setTextSize(2);
  gfx->setCursor(230, 200);
  gfx->printf("TAP THE CROSS  (%d of %d)", idx + 1, NT);
  gfx->setTextColor(C_DIM, C_BG);
  gfx->setCursor(230, 230);
  gfx->print(BOARD_NAME);
  int px = CROSS_X[idx];
  int py = CROSS_Y[idx];
  gfx->drawLine(px - 20, py, px + 20, py, C_TXT);
  gfx->drawLine(px, py - 20, px, py + 20, C_TXT);
  gfx->drawCircle(px, py, 10, C_TXT);
}

void showResults() {
  gfx->fillScreen(C_BG);
  gfx->setTextColor(C_TXT, C_BG);
  gfx->setTextSize(2);
  gfx->setCursor(20, 20);
  gfx->print("target  ->  raw");
  for (int i = 0; i < NT; i++) {
    gfx->setCursor(20, 70 + i * 40);
    gfx->printf("%3d,%3d  ->  %4d,%4d",
                CROSS_X[i], CROSS_Y[i], rawX[i], rawY[i]);
  }
  gfx->setTextColor(C_DIM, C_BG);
  gfx->setCursor(20, 300);
  gfx->printf("%s   touch 0x%02X", BOARD_NAME, usedAddr);
}

void setup() {
  Serial.begin(115200);
  gfx->begin();
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  if      (present(GT911_ADDR1)) usedAddr = GT911_ADDR1;   // 0x5D
  else if (present(GT911_ADDR2)) usedAddr = GT911_ADDR2;   // 0x14
  touch.begin(usedAddr);
  Serial.printf("touch at 0x%02X\n", usedAddr);
  drawTarget();
}

void loop() {
  touch.read();
  if (touch.isTouched && !wasTouched && idx < NT) {
    rawX[idx] = touch.points[0].x;
    rawY[idx] = touch.points[0].y;
    Serial.printf("%d,%d -> %d,%d\n",
                  CROSS_X[idx], CROSS_Y[idx], rawX[idx], rawY[idx]);
    idx++;
    delay(250);
    if (idx < NT) drawTarget(); else showResults();
  }
  wasTouched = touch.isTouched;
  delay(20);
}
