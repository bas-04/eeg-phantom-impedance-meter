/* pm_panel_test - EEG Head Phantom impedance meter, panel smoke test
 *
 * RUN THIS FIRST on any new display board. It touches nothing but the RGB
 * panel: no I2C, no touch controller, no carrier board. So if it fails, the
 * fault is in the pin map or the panel timing and nowhere else. The other
 * three tools all bring up touch I2C as well, which makes a blank screen
 * ambiguous.
 *
 * Select your board in board_config.h. Board settings are listed there.
 *
 * WHAT YOU SHOULD SEE, cycling every few seconds:
 *
 *   1. TEST CARD. Eight vertical colour bars, labelled, left to right:
 *         BLK  RED  GRN  BLU  YEL  CYN  MAG  WHT
 *      Plus a one-pixel white border hugging all four edges, corner labels
 *      TL / TR / BL / BR, and a crosshair at the exact centre marked 400,240.
 *   2. FULL WHITE field.
 *   3. FULL MID-GREY field.
 *
 * HOW TO READ IT:
 *
 *   Bars in the wrong order - specifically RED and BLU swapped - means the R
 *     and B pin groups are transposed in makeRGBPanel(). This is the classic
 *     RGB pin-map error and the reason this sketch leads with colour bars.
 *   YEL/CYN/MAG wrong while RED/GRN/BLU look right means a single data bit is
 *     on the wrong GPIO. Note which composite colour is off: yellow is R+G,
 *     cyan is G+B, magenta is R+B.
 *   Border not touching all four edges, or a black margin on one side, means
 *     the porch or sync-width timing is off, or the panel is not really
 *     800x480.
 *   Corner labels in the wrong corners means the panel is mounted rotated;
 *     fix it in the Arduino_RGB_Display rotation argument, not in the drawing
 *     code.
 *   Horizontal tearing or banding sweeping across the solid white and grey
 *     fields means the pixel clock is too fast for PSRAM to keep up. Lower
 *     the final prefer_speed argument in makeRGBPanel() from 16000000 to
 *     12000000 and retest. Note that this is a DIFFERENT fault from the
 *     flicker we fixed in the firmware: that was erase-then-redraw in the
 *     drawing code, and showed only on the elements being repainted. This
 *     one affects everything, including static fields.
 *   Nothing at all, backlight clearly on: pin map or timing is wrong.
 *   Nothing at all, backlight off: check GFX_BL, and on the Elecrow board
 *     see the PCA9557 note below.
 *
 * ELECROW BLANK-SCREEN FALLBACK
 *
 * Elecrow's wiki says a PCA9557 I2C expander handles the display reset on the
 * CrowPanel 7.0. If this sketch shows nothing, that expander may be holding
 * the panel in reset. Uncomment TRY_PCA9557 below to drive all of its pins
 * high before the panel starts, which should release an active-low reset and
 * enable the backlight.
 *
 * This is deliberately OFF by default because it writes blind to an expander
 * whose wiring is not documented. Enable it only if the panel is blank, and
 * only on the Elecrow board.
 */

//#define TRY_PCA9557

#include <Wire.h>
// Included here as well as in board_config.h so the IDE's library discovery
// does not depend on it following a local header. Harmless duplication.
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include "board_config.h"

Arduino_ESP32RGBPanel *rgbpanel = makeRGBPanel();
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(800, 480, rgbpanel, 0, true);

const uint16_t BAR_COL[8] = {
  0x0000, 0xF800, 0x07E0, 0x001F, 0xFFE0, 0x07FF, 0xF81F, 0xFFFF };
const char *BAR_NAME[8] = { "BLK","RED","GRN","BLU","YEL","CYN","MAG","WHT" };

#ifdef TRY_PCA9557
// PCA9557: reg 1 = output port, reg 3 = configuration (1 = input).
// All outputs, all high.
void pokePCA9557() {
  for (uint8_t addr = 0x18; addr <= 0x1B; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() != 0) continue;
    Wire.beginTransmission(addr); Wire.write(0x03); Wire.write(0x00); Wire.endTransmission();
    Wire.beginTransmission(addr); Wire.write(0x01); Wire.write(0xFF); Wire.endTransmission();
    Serial.printf("poked PCA9557 at 0x%02X\n", addr);
  }
}
#endif

void drawTestCard() {
  for (int i = 0; i < 8; i++) {
    gfx->fillRect(i * 100, 0, 100, 480, BAR_COL[i]);
  }
  // Labels: dark text on the light bars, light text on the dark ones.
  gfx->setTextSize(2);
  for (int i = 0; i < 8; i++) {
    uint16_t fg = (i == 0 || i == 3) ? 0xFFFF : 0x0000;
    gfx->setTextColor(fg, BAR_COL[i]);
    gfx->setCursor(i * 100 + 26, 300);
    gfx->print(BAR_NAME[i]);
  }
  // One-pixel border on the extreme edges: proves the full raster is addressable.
  gfx->drawRect(0, 0, 800, 480, 0xFFFF);
  // Corner labels, inset slightly so they are readable.
  gfx->setTextSize(2);
  gfx->setTextColor(0xFFFF, 0x0000);
  gfx->setCursor(8, 8);     gfx->print("TL");
  gfx->setCursor(764, 8);   gfx->print("TR");
  gfx->setCursor(8, 456);   gfx->print("BL");
  gfx->setCursor(764, 456); gfx->print("BR");
  // Exact centre.
  gfx->drawLine(400 - 40, 240, 400 + 40, 240, 0xFFFF);
  gfx->drawLine(400, 240 - 40, 400, 240 + 40, 0xFFFF);
  gfx->drawCircle(400, 240, 20, 0xFFFF);
  gfx->setTextColor(0xFFFF, 0x0000);
  gfx->setCursor(340, 268);
  gfx->print("400,240");
  // Board name, bottom centre.
  gfx->setTextColor(0xFFFF, 0x0000);
  gfx->setCursor(240, 430);
  gfx->print(BOARD_NAME);
}

void setup() {
  Serial.begin(115200);
  delay(300);
#ifdef TRY_PCA9557
  Wire.begin(PIN_SDA, PIN_SCL, 100000);
  pokePCA9557();
#endif
  gfx->begin();
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
  Serial.println("panel test running: " BOARD_NAME);
}

void loop() {
  drawTestCard();
  delay(6000);
  gfx->fillScreen(0xFFFF);      // full white: reveals banding and tearing
  delay(2500);
  gfx->fillScreen(0x8410);      // mid grey: reveals it more subtly
  delay(2500);
}
