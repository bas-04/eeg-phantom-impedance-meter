/* pm_dac_hold - EEG Head Phantom impedance meter, bring-up tool 6
 *
 * Holds the DAC still so the drive chain can be checked with a multimeter.
 * The meter measures about a tenth of its design drive current (0.2-0.3 uA
 * instead of 2.5 uA) although R1-R4, R18 and both op-amp chips check out.
 * The square wave starts at the DAC (M1) and passes through R7 (10k) and C9
 * (47 nF) to the buffer U9A before it reaches the current pump. This sketch
 * parks the DAC at the two levels the firmware uses, so you can follow the
 * 0.5 V step from point to point and see where it shrinks.
 *
 * Meter on DC volts, black lead on GND. Tap the screen to switch LOW / HIGH,
 * or just wait: the level also switches by itself every 20 seconds, with a
 * countdown, so the test works even when touch is not responding. The header
 * says whether the touch controller answered on the bus at start-up.
 * Write down every point in BOTH states. Every point except VREF should move
 * by 0.50 V between LOW and HIGH. Nothing here writes to flash or drives a
 * site: all multiplexers are switched off.
 */
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include <Adafruit_MCP4725.h>
#include <Adafruit_MCP23X17.h>
#include "board_config.h"

Arduino_ESP32RGBPanel *rgbpanel = makeRGBPanel();
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(800, 480, rgbpanel, 0, true);
TAMC_GT911 touch(PIN_SDA, PIN_SCL, -1, TOUCH_RST_PIN, 800, 480);
Adafruit_MCP4725 dac;
Adafruit_MCP23X17 expA, expB;

const uint16_t CODE_LO = 1743, CODE_HI = 2363;     // what phantom_meter_v2 uses
const uint32_t AUTO_MS = 20000;                    // self-switch period
bool high = false;
uint32_t stateMs = 0;                              // when the current level began
uint8_t  touchAddr = 0;                            // 0 = touch controller not answering

bool busAck(uint8_t a) { Wire.beginTransmission(a); return Wire.endTransmission() == 0; }

void drawCountdown() {
  int left = (int)((AUTO_MS - (millis() - stateMs) + 999) / 1000);
  if (left < 0) left = 0;
  gfx->fillRect(560, 436, 220, 24, C_BG);
  gfx->setTextSize(2); gfx->setTextColor(C_DIM, C_BG);
  gfx->setCursor(560, 440); gfx->printf("auto-switch %2d s", left);
}

void draw() {
  uint16_t code = high ? CODE_HI : CODE_LO;
  float v = code * 3.3f / 4095.0f;
  gfx->fillScreen(C_BG);
  gfx->fillRect(0, 0, 800, 54, 0x1125); gfx->fillRect(0, 54, 800, 3, 0x0552);
  gfx->setTextColor(C_TXT, 0x1125); gfx->setTextSize(3);
  gfx->setCursor(16, 16); gfx->print("DAC HOLD");
  uint16_t badge = high ? 0xF5A5 : 0x2E6E;
  gfx->fillRect(560, 9, 224, 38, badge);
  gfx->setTextColor(0x0000, badge); gfx->setCursor(590, 16);
  gfx->print(high ? "HIGH" : "LOW"); gfx->printf(" %u", code);

  gfx->setTextSize(2);
  gfx->setTextColor(C_DIM, C_BG);
  gfx->setCursor(30, 78);  gfx->print("meter on DC volts, black lead on GND");
  gfx->setCursor(30, 112); gfx->print("measure at");
  gfx->setCursor(560, 112); gfx->print("expect");

  struct { const char *where; bool moves; } P[] = {
    {"DAC module OUT pin",           true},
    {"M1 socket hole 5 (DAC_OUT)",   true},
    {"U9 pin 3 (after R7)",          true},
    {"U9 pin 1 (buffer out)",        true},
    {"U9 pin 10 (VREF)",             false},
  };
  char b[16];
  for (int k = 0; k < 5; k++) {
    int y = 150 + k * 40;
    gfx->setTextColor(C_TXT, C_BG); gfx->setCursor(30, y); gfx->print(P[k].where);
    snprintf(b, sizeof b, "%.2f V", P[k].moves ? v : 1.655f);
    gfx->setTextColor(P[k].moves ? badge : C_DIM, C_BG); gfx->setCursor(560, y); gfx->print(b);
  }
  gfx->setTextColor(C_DIM, C_BG);
  gfx->setCursor(30, 372); gfx->print("Each point should move 0.50 V between LOW and HIGH.");
  if (touchAddr) { gfx->setTextColor(0x2E6E, C_BG); gfx->setCursor(30, 402); gfx->printf("touch controller answering at 0x%02X", touchAddr); }
  else           { gfx->setTextColor(0xF5A5, C_BG); gfx->setCursor(30, 402); gfx->print("touch controller NOT answering - use the timer"); }
  gfx->setTextColor(C_DIM, C_BG);
  gfx->setCursor(30, 440); gfx->print("tap to switch, or wait");
  drawCountdown();
}

// The MCP4725 answers at 0x60-0x63 depending on the chip variant and its A0
// strap: Adafruit modules ship at 0x62, most generic modules at 0x60. Use
// whichever one is on the bus rather than insisting on one.
uint8_t findDac() {
  for (uint8_t a = 0x60; a <= 0x63; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) return a;
  }
  return 0x62;                      // nothing found: begin() will fail and say so
}

void setup() {
  Serial.begin(115200);
  gfx->begin(); pinMode(GFX_BL, OUTPUT); digitalWrite(GFX_BL, HIGH);
  gfx->fillScreen(C_BG);
  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  uint8_t ta = TOUCH_ADDR;
  Wire.beginTransmission(GT911_ADDR1); if (Wire.endTransmission() == 0) ta = GT911_ADDR1;
  else { Wire.beginTransmission(GT911_ADDR2); if (Wire.endTransmission() == 0) ta = GT911_ADDR2; }
  touch.begin(ta);
  bool ok = dac.begin(findDac()) && expA.begin_I2C(0x20) && expB.begin_I2C(0x21);
  if (!ok) {
    gfx->setTextSize(3); gfx->setTextColor(0xF800, C_BG);
    gfx->setCursor(30, 200); gfx->print("I2C device missing - run pm_i2c_scan");
    while (1) delay(1000);
  }
  for (int p = 0; p < 16; p++) { expA.pinMode(p, OUTPUT); expB.pinMode(p, OUTPUT); }
  expA.writeGPIOA(0x30); expA.writeGPIOB(0x30);   // every multiplexer off
  expB.writeGPIOA(0);    expB.writeGPIOB(0);      // mode switches, LEDs, buzzer off
  dac.setVoltage(CODE_LO, false);
  touchAddr = busAck(GT911_ADDR1) ? GT911_ADDR1 : busAck(0x14) ? 0x14 : 0;
  stateMs = millis();
  draw();
}

void switchLevel() {
  high = !high;
  dac.setVoltage(high ? CODE_HI : CODE_LO, false);
  stateMs = millis();
  draw();
}

void loop() {
  // One tap, one change: only a finger coming DOWN counts.
  static bool wasDown = true;
  static uint32_t last = 0;
  touch.read();
  bool down = touch.isTouched;
  if (down && !wasDown && millis() - last > 400) { last = millis(); switchLevel(); }
  wasDown = down;
  if (millis() - stateMs >= AUTO_MS) switchLevel();
  static uint32_t lastTick = 0;
  if (millis() - lastTick >= 1000) { lastTick = millis(); drawCountdown(); }
  delay(20);
}
