/* pm_analog_probe - EEG Head Phantom impedance meter, bring-up tool 5 of 5
 *
 * Measures the SAME signal four different ways and prints all four, so the
 * hardware decides which sampling method works instead of anyone reasoning
 * about it. That distinction has already cost this project two rounds:
 *
 *   24 Oct - self-calibration reported every standard OPEN, including the dead
 *            short. This probe showed single-shot reading -7847 counts of
 *            current while continuous mode read 0 pk-pk. Current was flowing;
 *            the converter was handing back a constant.
 *
 *   The firmware was changed to concurrent single-shot on that evidence, and
 *   calibration still reported OPEN. So the evidence was not enough: "single
 *   shot works" was measured through readADC_Differential_0_1(), which is not
 *   the call the firmware makes. Hence this version, which times every
 *   candidate path explicitly.
 *
 * THE FOUR METHODS
 *
 *   1 SEQUENTIAL   readADC_Differential_0_1() on each ADC in turn. One blocking
 *                  library call that starts, waits and reads. Known to work -
 *                  this is what measured -7847 counts.
 *
 *   2 CONCURRENT   start both, poll conversionComplete() on each, then read.
 *                  What the firmware does as of 24 Oct. Suspect: the ADS1115's
 *                  OS bit reads "not converting" for a short while AFTER the
 *                  start command, so a fast poll can see the PREVIOUS
 *                  conversion as complete and return a stale result.
 *
 *   3 TIMED WAIT   start both, wait a flat 1300 us - longer than the 1163 us a
 *                  conversion takes at 860 SPS - then read. No reliance on the
 *                  OS bit at all, so the race in method 2 cannot occur.
 *
 *   4 CONTINUOUS   continuous mode polled on a timer. The original, known
 *                  broken, kept as the control.
 *
 * FOUND 24 Oct 2026, night - the cause of every "constant data" result: on
 * the Rev B PCB, M3's ALRT pin is wired to TIA_OUT. Every Adafruit library
 * read turns ALRT on, and it then drags the current amplifier's output to
 * ground. Rows 1-3 now read the converters through their registers with ALRT
 * kept off; row 4 is the old library read, kept as the control and run last.
 * Expect rows 1-3 green with about 32000 counts of I pk-pk (2.5 uA) on the
 * short, 1k and 4.7k, and row 4 red.
 *
 * EARLIER REVISION, 24 Oct 2026, late: methods 2-4 above were all seen failing and are
 * gone. In their place, the firmware's own sampling routine (12 ms settle, one
 * read pair per half period) run three times, with the VOLTAGE converter at
 * gain 1, 4 and 16. Reason: calibration read the 100k standard (V at gain 1)
 * correctly and the 1k, 4.7k and short (V at gain 16) as constant data, and
 * this probe - which always used gain 1 - read every channel. The rows below
 * test that one difference and nothing else. The two raw lines at the bottom
 * show the lowest and highest count each converter returned: +/-32767 means
 * clipped, 0/0 means no data, any other single value means frozen.
 *
 * Read the I pk-pk column. A method that works shows thousands of counts on the
 * SHORT channel; a method that does not shows zero or near it. Tap to change
 * channel. Nothing here writes to flash.
 */
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MCP4725.h>
#include <Adafruit_MCP23X17.h>
#include "board_config.h"

Arduino_ESP32RGBPanel *rgbpanel = makeRGBPanel();
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(800, 480, rgbpanel, 0, true);
TAMC_GT911 touch(PIN_SDA, PIN_SCL, -1, TOUCH_RST_PIN, 800, 480);

Adafruit_ADS1115 adcV, adcI;
Adafruit_MCP4725 dac;
Adafruit_MCP23X17 expA, expB;

const float R_F = 200000.0f, G_SENSE = 1.0f, I_TEST = 2.5e-6f, DAC_SWING = 0.25f;   // G_SENSE: 1 with R8 bridged (bench board, 5 Nov 2026), 4 stock; not used by this probe's display
const uint32_t HALF_PERIOD_US = 16000, BLANK_US = 3000, SAMPLE_US = 1000;
enum { SW5 = 16 };                       // GPA4 on U12: RETURN -> TIA, Mode A

const struct { const char *name; uint8_t ch; const char *expect; } CHANS[] = {
  {"SHORT    ch26", 26, "a dead short - the strongest signal on the board"},
  {"1k  R20  ch22", 22, "1.00 k standard"},
  {"4k7 R21  ch23", 23, "4.70 k standard"},
  {"100k R22 ch24", 24, "100 k standard"},
  {"OPEN     ch25", 25, "nothing - this one SHOULD read flat"},
  {"Cz       ch09",  9, "a phantom site, open unless the cap is on"},
};
const int NCH = sizeof(CHANS) / sizeof(CHANS[0]);
int sel = 0;

float vref = 1.65f;
uint16_t dacHi, dacLo, dacMid;

void muxSelect(uint8_t chDS, uint8_t chR) {
  uint8_t a = (chDS & 0x0F); a |= (chDS < 16) ? (1 << 5) : (1 << 4);
  if (chDS > 31) a = 0x30;
  uint8_t b = (chR & 0x0F);  b |= (chR  < 16) ? (1 << 5) : (1 << 4);
  if (chR  > 31) b = 0x30;
  expA.writeGPIOA(a); expA.writeGPIOB(b);
}

void row(int y, const char *label, const char *value, uint16_t col) {
  gfx->setTextSize(2);
  gfx->setTextColor(C_DIM, C_BG); gfx->setCursor(30, y);  gfx->print(label);
  gfx->setTextColor(col, C_BG);   gfx->setCursor(360, y); gfx->print(value);
}

// ADS1115 register access with the comparator OFF, copied from phantom_meter_v2.
// See the note there: on Rev B, M3's ALRT pin is wired to TIA_OUT.
const uint8_t ADS_V = 0x48, ADS_I = 0x49;
const uint16_t ADS_MUX_DIFF01 = 0x0000, ADS_MUX_AIN1 = 0x5000;
const uint16_t ADS_CFG_BASE = 0x0100     // single-shot
                            | 0x00E0     // 860 SPS
                            | 0x0003;    // comparator OFF: ALRT stays high-impedance
uint16_t pgaBits(adsGain_t g) {
  switch (g) {
    case GAIN_TWOTHIRDS: return 0x0000;  case GAIN_ONE:   return 0x0200;
    case GAIN_TWO:       return 0x0400;  case GAIN_FOUR:  return 0x0600;
    case GAIN_EIGHT:     return 0x0800;  default:         return 0x0A00;   // GAIN_SIXTEEN
  }
}
void adsWrite16(uint8_t addr, uint8_t reg, uint16_t v) {
  Wire.beginTransmission(addr); Wire.write(reg); Wire.write((uint8_t)(v >> 8)); Wire.write((uint8_t)(v & 0xFF));
  Wire.endTransmission();
}
uint16_t adsRead16(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr); Wire.write(reg); Wire.endTransmission();
  if (Wire.requestFrom(addr, (uint8_t)2) < 2) return 0;
  uint16_t hi = Wire.read(), lo = Wire.read();
  return (uint16_t)((hi << 8) | lo);
}
// Park a converter: powered down, comparator off, ALRT released.
void adsQuiet(uint8_t addr) { adsWrite16(addr, 0x01, ADS_CFG_BASE | 0x0200); }
void adsStart(uint8_t addr, uint16_t mux, adsGain_t g) {
  adsWrite16(addr, 0x01, 0x8000 | ADS_CFG_BASE | mux | pgaBits(g));
}
// A conversion at 860 SPS takes 1.16 ms (+/-10 %). Wait 1 ms, then poll the
// ready bit with a 4 ms ceiling, then read the result.
int16_t adsFinish(uint8_t addr) {
  uint32_t t = micros();
  while (micros() - t < 1000) { }
  while (!(adsRead16(addr, 0x01) & 0x8000) && micros() - t < 4000) { }
  return (int16_t)adsRead16(addr, 0x00);
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

  bool ok = true;
  ok &= adcV.begin(0x48); ok &= adcI.begin(0x49); ok &= dac.begin(findDac());
  ok &= expA.begin_I2C(0x20); ok &= expB.begin_I2C(0x21);
  if (!ok) {
    gfx->setTextSize(3); gfx->setTextColor(0xF800, C_BG);
    gfx->setCursor(30, 200); gfx->print("I2C device missing - run pm_i2c_scan");
    while (1) delay(1000);
  }
  for (int p = 0; p < 16; p++) { expA.pinMode(p, OUTPUT); expB.pinMode(p, OUTPUT); }
  adcV.setDataRate(RATE_ADS1115_860SPS); adcI.setDataRate(RATE_ADS1115_860SPS);

  adsQuiet(ADS_V); adsQuiet(ADS_I); delay(50);
  adcV.setGain(GAIN_ONE);
  adsStart(ADS_V, ADS_MUX_AIN1, GAIN_ONE);
  vref  = adcV.computeVolts(adsFinish(ADS_V));
  dacHi = (uint16_t)((vref + DAC_SWING) / 3.3f * 4095.0f);
  dacLo = (uint16_t)((vref - DAC_SWING) / 3.3f * 4095.0f);
  dacMid = (dacHi + dacLo) / 2;
}

void loop() {
  uint8_t ch = CHANS[sel].ch;
  muxSelect(ch, ch);
  expB.writeGPIOA(SW5);                 // Mode A return path
  expB.writeGPIOB(0);
  adcV.setGain(GAIN_ONE); adcI.setGain(GAIN_FOUR);

  adsQuiet(ADS_V); adsQuiet(ADS_I);    // release ALRT: the library row left it in RDY mode
  delay(50);                            // and let the TIA come back off the ground rail

  long pp[5] = {0,0,0,0,0}; float uA[5] = {0,0,0,0,0}; uint32_t cnt[5] = {0,0,0,0,0};
  int16_t iLoR[5] = {0}, iHiR[5] = {0}, vLoR[5] = {0}, vHiR[5] = {0};
  long ppVm[5] = {0,0,0,0,0};
  const adsGain_t GV[5] = {GAIN_ONE, GAIN_ONE, GAIN_FOUR, GAIN_SIXTEEN, GAIN_ONE};
  // Rows 1-3: the firmware's routine (register reads, comparator off).
  // Row 4: the old Adafruit library read - the control. Run LAST, because it
  // switches ALRT on and drags TIA_OUT to ground for everything after it.
  for (int method = 1; method <= 4; method++) {
    bool lib = (method == 4);
    adcV.setGain(GV[method]); adcI.setGain(GAIN_FOUR);
    int16_t lo = 32767, hi = -32768, vlo = 32767, vhi = -32768;
    int64_t accI = 0; uint32_t n = 0;
    for (uint8_t p = 0; p < 8; p++) {                 // 4 periods, as a site scan
      int sign = (p & 1) ? -1 : +1;
      dac.setVoltage(sign > 0 ? dacHi : dacLo, false);
      uint32_t t0 = micros();
      while (micros() - t0 < 12000) { }
      int16_t vv, iv;
      if (lib) {
        vv = adcV.readADC_Differential_0_1();
        iv = adcI.readADC_Differential_0_1();
      } else {
        adsStart(ADS_V, ADS_MUX_DIFF01, GV[method]);
        adsStart(ADS_I, ADS_MUX_DIFF01, GAIN_FOUR);
        vv = adsFinish(ADS_V);
        iv = adsFinish(ADS_I);
      }
      if (iv < lo) lo = iv;  if (iv > hi) hi = iv;
      if (vv < vlo) vlo = vv; if (vv > vhi) vhi = vv;
      accI += sign * (int64_t)iv; n++;
      while (micros() - t0 < HALF_PERIOD_US) { }
    }
    dac.setVoltage(dacMid, false);
    pp[method] = (long)hi - lo; cnt[method] = n;
    iLoR[method] = lo; iHiR[method] = hi; vLoR[method] = vlo; vHiR[method] = vhi;
    ppVm[method] = (long)vhi - vlo;
    float Vt = n ? ((float)accI / n) * adcI.computeVolts(1) : 0.0f;
    uA[method] = fabsf(Vt) / R_F * 1e6f;
  }
  adcV.setGain(GAIN_ONE);
  adsQuiet(ADS_I);                      // release ALRT again before the idle wait

  char b[48];
  const char *MN[5] = {"", "1 REGISTER V g1", "2 REGISTER V g4",
                       "3 REGISTER V g16", "4 LIBRARY  (old)"};
  gfx->fillScreen(C_BG);
  gfx->fillRect(0, 0, 800, 54, 0x1125); gfx->fillRect(0, 54, 800, 3, 0x0552);
  gfx->setTextColor(C_TXT, 0x1125); gfx->setTextSize(3);
  gfx->setCursor(16, 16); gfx->print("ANALOG PROBE");
  gfx->setTextSize(2); gfx->setCursor(470, 22); gfx->print(CHANS[sel].name);
  gfx->setCursor(730, 22); gfx->printf("%d/%d", sel + 1, NCH);   // where you are
  gfx->setTextSize(2); gfx->setTextColor(C_DIM, C_BG);
  gfx->setCursor(30, 78); gfx->print(CHANS[sel].expect);
  gfx->setCursor(30, 112); gfx->print("method");
  gfx->setCursor(300, 112); gfx->print("I pk-pk");
  gfx->setCursor(450, 112); gfx->print("current");
  gfx->setCursor(620, 112); gfx->print("V pk-pk");

  int best = 0;
  for (int m = 1; m <= 4; m++) {
    int y = 148 + (m - 1) * 38;
    bool good = pp[m] > 300;
    if (good && !best) best = m;
    uint16_t c = good ? 0x2E6E : 0xC985;
    gfx->setTextSize(2);
    gfx->setTextColor(C_TXT, C_BG); gfx->setCursor(30, y);  gfx->print(MN[m]);
    snprintf(b, sizeof b, "%7ld", pp[m]);
    gfx->setTextColor(c, C_BG);     gfx->setCursor(300, y); gfx->print(b);
    snprintf(b, sizeof b, "%6.2fuA", uA[m]);
    gfx->setTextColor(c, C_BG);     gfx->setCursor(450, y); gfx->print(b);
    snprintf(b, sizeof b, "%7ld", ppVm[m]);
    bool vclip = vLoR[m] <= -32767 || vHiR[m] >= 32767;
    gfx->setTextColor(vclip ? 0xF5A5 : C_DIM, C_BG); gfx->setCursor(620, y); gfx->print(b);
  }

  gfx->setTextSize(2);
  if (ch == 25) { gfx->setTextColor(C_DIM, C_BG); gfx->setCursor(30, 318);
                  gfx->print("OPEN channel - all four reading flat is correct here"); }
  else if (!best) { gfx->setTextColor(0xC985, C_BG); gfx->setCursor(30, 318);
                  gfx->print("NO method sees current - the analog path is open"); }
  else { gfx->setTextColor(0x2E6E, C_BG); gfx->setCursor(30, 318);
         gfx->print("green = current seen; red = constant data"); }

  gfx->setTextColor(C_DIM, C_BG);
  gfx->setCursor(30, 352); gfx->printf("I lo/hi 1:%d/%d 2:%d/%d 4:%d/%d",
                                       iLoR[1], iHiR[1], iLoR[2], iHiR[2], iLoR[4], iHiR[4]);
  gfx->setCursor(30, 382); gfx->printf("V lo/hi 1:%d/%d 2:%d/%d 3:%d/%d",
                                       vLoR[1], vHiR[1], vLoR[2], vHiR[2], vLoR[3], vHiR[3]);
  gfx->setCursor(30, 412); gfx->printf("vref %.3f V   dac %u / %u", vref, dacLo, dacHi);
  gfx->setCursor(30, 446); gfx->print("tap the screen for the next channel");

  Serial.printf("%s | g1 %ld | g4 %ld | g16 %ld | lib %ld | best %d\n",
                CHANS[sel].name, pp[1], pp[2], pp[3], pp[4], best);

  // One tap, one channel. The first version advanced whenever a touch was
  // REPORTED, and the touch controller can go on reporting one for a moment
  // after the finger lifts - so a single tap was counted in two or three
  // windows in a row and skipped straight past the channel you wanted.
  // Starting with wasDown = true means a touch that is already being reported
  // when this window opens has to lift before it can count; only a finger
  // coming DOWN advances.
  bool wasDown = true;
  uint32_t t0 = millis();
  while (millis() - t0 < 2500) {
    touch.read();
    bool down = touch.isTouched;
    if (down && !wasDown) { sel = (sel + 1) % NCH; break; }
    wasDown = down;
    delay(20);
  }
}
