/*
 * EEG Phantom Impedance Meter — Rev B firmware (Arduino-ESP32, ESP32-S3)
 *
 * Runs on EITHER display. Pick one in board_config.h, which must sit in this
 * folder beside this file:
 *
 *     #define PM_BOARD 1     Sunton ESP32-8048S043C, 4.3 inch
 *     #define PM_BOARD 2     Elecrow CrowPanel ESP32 HMI, 7.0 inch
 *
 * That one line carries the RGB pin map and timing, the panel's touch address,
 * and the measured touch transform for that panel. board_config.h also turns
 * the two board settings that cost hours - PSRAM disabled, USB CDC On Boot
 * enabled - into compile-time errors, so if this builds, those two are right.
 *
 * Board settings in Arduino IDE:
 *   Board: "ESP32S3 Dev Module"       USB CDC On Boot: Disabled
 *   PSRAM: "OPI PSRAM"
 *   CYD (16MB flash):    Flash Size 16MB, Partition "16M Flash (3MB APP/9.9MB FATFS)"
 *   Elecrow (4MB flash): Flash Size 4MB,  Partition "Huge APP (3MB No OTA/1MB SPIFFS)"
 *
 * Libraries (Library Manager):  Adafruit ADS1X15, Adafruit MCP4725, Adafruit MCP23017 (MCP23X17),
 *                               GFX Library for Arduino (moononournation), TAMC_GT911
 *
 * Wiring - FOUR wires from J3 on the carrier board to the display:
 *   J3.1 5V   -> 5V          J3.2 GND -> GND
 *   J3.3 SDA  -> GPIO19      J3.4 SCL -> GPIO20
 *   J3.5 DRDY   NOT CONNECTED - see "sample pacing" in measureRaw() below.
 *
 * On the Elecrow, GPIO19/20 appear on the connector marked I2C, and its 3V3 pin
 * must be left disconnected: J3 pin 1 is a 5V input with no blocking diode, so
 * tying the display's 3V3 to it back-feeds 5V onto the ESP32-S3's rail.
 *
 * Footswitch (J5, optional): a short press moves the focus to the next site
 * still above 5 kOhm; holding it for a second toggles Mode A/B. Nothing is
 * harmed if no pedal is fitted - the pull-up simply reads it as released.
 *
 * Speaker (Elecrow only, added 22 Nov 2026): an 8 ohm speaker on the CrowPanel's
 * SPK socket plays WAV files from the TF card's top folder - opening.wav at
 * power-up, click.wav when a site is tapped, modechange.wav on Mode A/B, and
 * saltbridge.wav looped while a bridge is present until MUTE. See pm_audio.h
 * and the SOUND_ settings below. Missing files fall back to the piezo. The SD
 * and SPI libraries come with the ESP32 core: nothing extra to install.
 *
 * I2C map: GT911 touch 0x5D | MCP4725 DAC 0x62 (Adafruit) | ADS1115 #1 0x48 (V) | ADS1115 #2 0x49 (I)
 *          MCP23017 #1 0x20 (mux select) | MCP23017 #2 0x21 (switches, LEDs, buzzer)
 *          PCA9557 0x18 is the Elecrow's own panel-reset expander. Expected, not a fault.
 */
#include <Wire.h>
#include <Preferences.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MCP4725.h>
#include <Adafruit_MCP23X17.h>
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include "board_config.h"   // PM_BOARD, pin map, panel timing, touch transform
#include "pm_fonts.h"       // anti-aliased Inter glyphs for the head-map screen
#include "pm_audio.h"       // WAV sounds on the Elecrow speaker port (nothing on the CYD)

// ------------------------------------------------------------------ analog constants (Rev B)
const float G_SENSE = 1.0f;       // U10A at gain 1: R8 bridged with a wire link (5 Nov 2026)
// Was 4.0f (1 + R8/R9 = 1 + 300k/100k). With tin electrodes and Electro-Gel on
// the carbon-filled inserts the electrode DC offset is about 0.5 V, and at a
// gain of 4 that drove the sense amplifier to its rails ("DC" on every site, in
// both modes). With R8 bridged the gain is 1 whether or not R9 is fitted, and
// the meter tolerates about 1.45 V of DC at the electrode. On an UNMODIFIED
// board this line must read 4.0f again, or every reading is four times high
// until self-calibration - and the "DC" limit is four times too generous.

// Shown on the calibration screen, so the version running on the display can
// be checked at a glance against the file that was meant to be uploaded.
const char *FW_VERSION = "2026-11-22 a";
const float R_F     = 200000.0f;  // U9C transimpedance resistor
const float I_TEST  = 2.5e-6f;    // nominal test current, amps (square wave amplitude)
const float DAC_SWING = 0.25f;    // volts about VREF -> I = 0.25 V / 100k = 2.5 uA
const uint32_t HALF_PERIOD_US = 16000;   // 31.25 Hz
const uint32_t SETTLE_US      = 12000;   // wait after each edge before the ONE read pair (as pm_analog_probe method 1)
const float Z_OPEN = 150000.0f;

// ------------------------------------------------------------------ channel map (mux channel index 0..31)
struct Site { const char* name; uint8_t db25; float x, y; };
const Site SITES[22] = {
  {"Fp1",1,-0.31f,-0.95f},{"Fp2",14,0.31f,-0.95f},{"F7",6,-0.81f,-0.59f},{"F3",2,-0.40f,-0.53f},
  {"Fz",10,0.00f,-0.50f},{"F4",15,0.40f,-0.53f},{"F8",19,0.81f,-0.59f},{"T3",7,-1.00f,0.00f},
  {"C3",3,-0.50f,0.00f},{"Cz",22,0.00f,0.00f},{"C4",16,0.50f,0.00f},{"T4",20,1.00f,0.00f},
  {"T5",8,-0.81f,0.59f},{"P3",4,-0.40f,0.53f},{"Pz",23,0.00f,0.50f},{"P4",17,0.40f,0.53f},
  {"T6",21,0.81f,0.59f},{"O1",5,-0.31f,0.88f},{"O2",18,0.31f,0.88f},{"GND",9,0.00f,-0.80f},
  {"A1",11,-1.28f,0.30f},{"A2",24,1.28f,0.30f}};
enum { CH_A1 = 20, CH_A2 = 21, CH_CAL1K = 22, CH_CAL5K = 23, CH_CAL100K = 24, CH_OPEN = 25, CH_SHORT = 26 };

// ------------------------------------------------------------------ expander bit map
// U11 (0x20): GPA0-3 SEL_DS, GPA4 nEN_DSLO, GPA5 nEN_DSHI | GPB0-3 SEL_R, GPB4 nEN_RLO, GPB5 nEN_RHI
// U12 (0x21): GPA0..5 SW1..SW6, GPA6 BUZ, GPA7 LED_MODE | GPB0 LED_REF, GPB1 FOOTSW (input)
enum { SW1=1, SW2=2, SW3=4, SW4=8, SW5=16, SW6=32 };   // mask bits for the mode matrix
const uint8_t MODE_A_MASK      = SW5;               // return through the same cap channel
const uint8_t MODE_B_A1A2_MASK = SW1|SW2|SW3|SW4;   // linked ears
const uint8_t MODE_B_A1_MASK   = SW1|SW3;
const uint8_t MODE_B_A2_MASK   = SW2|SW4;

Adafruit_ADS1115 adcV, adcI;
Adafruit_MCP4725 dac;
Adafruit_MCP23X17 expA, expB;
Preferences prefs;

// ------------------------------------------------------------------ switch fabric
void muxSelect(uint8_t chDS, uint8_t chR) {
  // Drive and sense banks always follow the same channel; return bank can differ.
  uint8_t a = (chDS & 0x0F);
  a |= (chDS < 16) ? (1 << 5) : (1 << 4);      // enable lo (nEN_DSLO=0) or hi (nEN_DSHI=0); other high
  if (chDS > 31) a = 0x30;                     // all off
  uint8_t b = (chR & 0x0F);
  b |= (chR < 16) ? (1 << 5) : (1 << 4);
  if (chR > 31) b = 0x30;
  expA.writeGPIOA(a);
  expA.writeGPIOB(b);
}
void modeSwitches(uint8_t mask, bool ledMode, bool ledRef, bool buzz) {
  uint8_t a = (mask & 0x3F) | (buzz ? 0x40 : 0) | (ledMode ? 0x80 : 0);
  expB.writeGPIOA(a);
  expB.writeGPIOB(ledRef ? 0x01 : 0x00);
}

// LS1 (PS1240) is a bare PIEZO ELEMENT, not a self-oscillating buzzer: a steady high
// only clicks once. The pin is toggled to make a tone.
//
// VOLUME (8 Nov 2026). A piezo element is loudest at its resonance - about
// 4 kHz for the PS1240 - and much quieter away from it. The original code
// toggled through the Adafruit library with two 250 us waits, about 1.4-2 kHz,
// well below resonance. Now each half-cycle is one short direct I2C write timed
// against micros(), so the tone can sit at resonance, and the volume setting
// works by moving the pitch toward or away from it:
//     BEEP_VOLUME 0 = silent, 1 = soft, 2 = medium, 3 = loud
// Pitch therefore rises with volume. If the loud setting is not the loudest
// the element can do, its resonance is a little off 4 kHz: try BEEP_HZ_LOUD
// values from 3600 to 4400 in steps of 200 and keep the loudest.
// At 400 kHz each I2C write takes roughly 80-100 us, so about 5 kHz is the
// ceiling; if a write is slow the tone simply lands a little lower.
const uint8_t  BEEP_VOLUME  = 3;
const uint16_t BEEP_HZ_LOUD = 4000;

// WHICH EVENTS BEEP (9 Nov 2026). Each can be switched on or off on its own.
// For "beep only for salt bridges": PASS false, FAIL false, BRIDGE true.
const bool BEEP_ON_PASS   = true;   // focused site drops to 5 k or below: one chime
const bool BEEP_ON_FAIL   = false;  // focused site rises back above 5 k: two short low beeps
const bool BEEP_ON_BRIDGE = true;   // bridge appears (triple beep), then once a second
const bool BEEP_ON_MODE   = true;   // footswitch hold changes Mode A/B: short confirmation
// A reading hovering at the line must not chime over and over: a site counts as
// passed at 5.00 k and only counts as failed again above this.
const float BEEP_FAIL_ABOVE = 5250.0f;

// SPEAKER SOUNDS (22 Nov 2026) - Elecrow CrowPanel 7.0 only. WAV files in the
// TF card's top folder, played through the speaker on the SPK socket:
//     opening.wav      once, when the screen first comes on
//     click.wav        each time an electrode site on the screen is tapped
//     modechange.wav   Mode A -> B or B -> A (header tap or footswitch hold)
//     saltbridge.wav   looped while a salt bridge is present, until MUTE
// A sound that is switched on here and whose file loaded REPLACES the piezo
// for that event. If the file is missing or unreadable, the piezo does what it
// did before (BEEP_ON_MODE, BEEP_ON_BRIDGE). Pass/fail beeps stay on the piezo.
// The calibration screen lists what loaded, just above the firmware line.
const bool     SOUND_ON_OPENING = true;
const bool     SOUND_ON_CLICK   = true;
const bool     SOUND_ON_MODE    = true;
const bool     SOUND_ON_BRIDGE  = true;
// Volume in percent: 100 = as recorded, 0 = silent, up to 200 to lift a quiet
// file (anything driven past full scale is clipped, which sounds harsh). Each
// sound's own level is a percent of the master.
const uint16_t SOUND_VOLUME = 80;
const uint16_t VOL_OPENING  = 100;
const uint16_t VOL_CLICK    = 60;
const uint16_t VOL_MODE     = 100;
const uint16_t VOL_BRIDGE   = 100;
// Files longer than this are cut to it (they are held in PSRAM for instant
// playback; 30 s at 44.1 kHz mono is 2.6 MB).
const uint32_t SOUND_MAX_SECONDS = 30;
// Let opening.wav finish before self-calibration starts, so its amplifier load
// is not on the supply while the reference is measured. Tap the screen to cut
// it short. false = calibrate while it plays.
const bool     SOUND_WAIT_FOR_OPENING = true;
uint16_t beepHz() {
  switch (BEEP_VOLUME) {
    case 1:  return 1500;               // well below resonance: soft
    case 2:  return 2700;               // medium
    default: return BEEP_HZ_LOUD;       // at resonance: loud
  }
}
void chirp(uint16_t ms, uint8_t mask, bool ledMode, bool ledRef);
void chirpHz(uint16_t hz, uint16_t ms, uint8_t mask, bool ledMode, bool ledRef);
static inline void buzWrite(uint8_t v) {     // U12 (0x21) GPIOA, one transaction
  Wire.beginTransmission(0x21); Wire.write(0x12); Wire.write(v); Wire.endTransmission();
}
void chirp(uint16_t ms, uint8_t mask, bool ledMode, bool ledRef) {
  chirpHz(beepHz(), ms, mask, ledMode, ledRef);
}
void chirpHz(uint16_t hz, uint16_t ms, uint8_t mask, bool ledMode, bool ledRef) {
  uint8_t base = (mask & 0x3F) | (ledMode ? 0x80 : 0);
  if (BEEP_VOLUME == 0 || hz == 0) { delay(ms); return; }   // silent, but keep the same timing
  uint32_t half = 500000UL / hz;
  uint32_t t0 = micros(), next = t0;
  bool hi = false;
  while (micros() - t0 < (uint32_t)ms * 1000UL) {
    while ((int32_t)(micros() - next) < 0) { }
    hi = !hi;
    buzWrite(hi ? (base | 0x40) : base);
    next += half;
    if ((int32_t)(micros() - next) > (int32_t)half) next = micros();   // fell behind: no backlog
  }
  buzWrite(base);                                 // BUZ low: no standing DC on the element
  expB.writeGPIOB(ledRef ? 0x01 : 0x00);
}

// ------------------------------------------------------------------ ADS1115, direct register access
// THE FAULT BEHIND EVERY "CONSTANT DATA" SCREEN (found 24 Oct 2026, late).
// On the Rev B PCB, M3's ALRT pin (header pin 6) is joined to TIA_OUT - the
// same net as its A0 input. ALRT is an open-drain output. The Adafruit library
// switches it to "conversion ready" mode on EVERY read, so from then on the
// ADC pulls the transimpedance amplifier's output to ground whenever it is not
// converting. The probe showed the result: the current channel stuck at
// +32767 (clipped) on the short, 1k and 4.7k, and a 0.8 V false offset on the
// 100k. Every earlier "sampling" theory was this.
//
// The fix needs no soldering. The ADS1115 powers up with the comparator OFF,
// which leaves ALRT high-impedance, and these routines never turn it on
// (COMP_QUE = 11 in every config written). No Adafruit read call may be made
// on adcI anywhere in this file; the objects are kept only for begin() and
// computeVolts(). The permanent fix is to cut M3 pin 6 - see ERRATA.txt.
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

// ------------------------------------------------------------------ DAC / VREF
float vref = 1.65f;
uint16_t dacHi, dacLo;
float readVref() {
  // ADC #1 single-ended on A1 (VREF) with the 4.096 V range
  adcV.setGain(GAIN_ONE);                      // sets the scale computeVolts() uses
  adsStart(ADS_V, ADS_MUX_AIN1, GAIN_ONE);
  return adcV.computeVolts(adsFinish(ADS_V));
}

// VREF must have SETTLED before it is measured, because the DAC's square wave
// is centred on the value read here, once. Found 31 Oct 2026 on the CYD: VREF
// comes from a 100k/100k divider filtered by C10 (10 uF), a 0.5 s time constant,
// so it needs about 2.5 s to reach its final value. The Elecrow's carrier had
// its own supply and was usually powered long before the display booted; the
// CYD powers both boards at the same instant and the ESP32 got here first. The
// DAC was centred below the real VREF, a DC current jammed the current pump,
// calibration scraped a pass, and the sites read OPEN or random high values
// until the RESET button was pressed with VREF already up.
//
// So: read every 100 ms until three successive readings agree within 2 mV and
// the value is plausible, with a 6 s ceiling. A board that is already settled
// passes in about 0.3 s.
void measureVref() {
  float prev = readVref();
  int steady = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < 6000) {
    delay(100);
    float v = readVref();
    bool plausible = v > 1.50f && v < 1.80f;
    steady = (plausible && fabsf(v - prev) < 0.002f) ? steady + 1 : 0;
    prev = v;
    if (steady >= 3) break;
  }
  vref = prev;
  dacHi = (uint16_t)((vref + DAC_SWING) / 3.3f * 4095.0f);
  dacLo = (uint16_t)((vref - DAC_SWING) / 3.3f * 4095.0f);
}

// ------------------------------------------------------------------ one synchronous-detection reading
struct Cal { float a = 1.0f, b = 0.0f; float rOffsetA = 0, rOffsetB = 0; } cal;

// UI state lives in the display section further down. Declared here so the
// measurement path can PRESERVE the LED states rather than blanking them:
// passing false/false blanked both LEDs on every reading, many times a second,
// so they could only ever flicker and never indicate anything.
extern bool modeB;
extern bool refsValid;
extern float zSite[22];

// Sampling: one blocking library call per converter, in turn.
//
// THIS IS THE THIRD ATTEMPT. The first two were wrong in the same way, and the
// history is kept because the mistake is easy to repeat.
//
// Attempt 1 (17 Oct) put both ADCs in CONTINUOUS mode and read them on a fixed
// 1000 us timer, reasoning that conversionComplete() cannot work in continuous
// mode - the OS bit reads busy whenever a conversion is running, and in
// continuous mode one always is. That reasoning was right; the conclusion was
// not. On hardware (24 Oct) pm_analog_probe measured the SHORT channel, a dead
// short and the strongest signal this instrument can make:
//
//     single-shot  I hi-lo   -7847 counts    current IS flowing
//     continuous   I pk-pk       0 counts    the converter never moved
//
// Attempt 2 (24 Oct) therefore switched to single-shot - but to a DIFFERENT
// single-shot arrangement than the one that had just been measured: start both
// converters, poll conversionComplete() on each, read both. On hardware that
// also returned constant data, and the new guard below caught it and said so.
//
// The error both times was the same. "Single-shot works" had been measured
// through readADC_Differential_0_1(), one blocking library call that starts,
// waits and reads a single converter. Neither replacement was that call. A
// measurement only covers the code it actually ran.
//
// So this version uses exactly the call that was measured working, sequentially
// on each converter. The likely reason the concurrent version fails: the
// ADS1115 reports "not converting" for a short interval AFTER the start command
// and before conversion actually begins, so a fast poll of a freshly started
// converter sees the previous state as complete and reads a stale register.
// That is a hypothesis, not a finding - the fix here does not depend on it.
//
// WHAT IT COSTS. Two conversions in series at 860 SPS is about 2.4 ms plus bus
// overhead, so roughly 5 sample pairs land in the 13 ms post-blanking window
// instead of the 9 a concurrent scheme would give. Noise is therefore about
// 1.4x the ideal. That is the right trade: a reading that is slightly noisier
// beats a reading that is confidently zero. If pm_analog_probe shows the timed
// variant (method 3) working on your board, switching to it recovers the
// difference - but only on that evidence, not on this reasoning.

// Set when a measurement collected samples but the current channel never moved
// at all. That is not an open circuit, it is no data, and the two must never
// again be reported as the same thing.
bool adcStuck = false;

// OPEN means "no current flowed", and the test for that must not depend on how
// much current a healthy board makes. The first version called anything under
// 15 % of the 2.5 uA design current open (0.375 uA). Bench, 24 Oct 2026: this
// board drives about 0.43-0.7 uA, so every standard - the short included - fell
// under the line and calibration reported five OPENs on a working analog chain.
// The measurement is ratiometric (V over I), so a low drive current changes the
// noise, not the answer. The floor is now absolute and tiny: 0.02 uA is 4 mV at
// the TIA, about 125 counts at GAIN_FOUR. A genuinely open channel is still
// caught twice: almost no current, and a huge V/I ratio (>= Z_OPEN).
const float I_OPEN_FLOOR = 0.02e-6f;
// Drive current of the most recent measureRaw(), microamps, for the cal screen.
float lastIuA = 0;
// What the voltage converter saw in the most recent measureRaw(): whether any
// sample hit the end of its range, the largest magnitude (volts at SENSE_OUT,
// DC included), and the steady DC at the electrode in millivolts.
bool  lastVClip = false;
float lastVPeak = 0, lastDcMv = 0;
// Above this at SENSE_OUT the sense amplifier is near its rails (VREF is 1.65 V
// on a 3.3 V supply), so the AC signal is being clipped by the amplifier itself.
const float SENSE_SAT_V = 1.45f;
// Returned when that happens. Larger than Z_OPEN, so everything that treats
// ">= Z_OPEN" as "no usable reading" still does, but it is shown as "DC".
const float Z_SAT = 2.0f * Z_OPEN;

// Returns loop impedance in ohms (raw, before calibration). Z_OPEN if no current flows.
float measureRaw(uint8_t chDS, uint8_t chR, uint8_t swMask, uint8_t periods, adsGain_t gainV = GAIN_SIXTEEN) {
  muxSelect(chDS, chR);
  modeSwitches(swMask, modeB, refsValid, false);
  adcV.setGain(gainV);                // scale only - autorange handled by caller
  adcI.setGain(GAIN_FOUR);            // +/-1.024 V: TIA output swings +/-0.5 V
  int64_t accV = 0, accI = 0, sumV = 0; uint32_t n = 0;
  int16_t iMin = 32767, iMax = -32768, vMin = 32767, vMax = -32768;
  // ATTEMPT 4 (24 Oct 2026, evening). Attempt 3 called the right library
  // function but in a tight loop - read, read, read with no gap - and on the
  // bench the current converter again returned one constant value. The probe
  // that works does something narrower: step the DAC, WAIT 12 ms, take ONE
  // read pair. So that is now exactly what happens here, once per half period.
  // Nothing faster goes back in until the probe has shown it working.
  //
  // One pair per half period means fewer samples than attempt 3 hoped for
  // (32 over a 16-period measurement instead of ~160), so noise is about
  // 2x higher. The half period is unchanged, so the scan is no slower.
  //
  // Both channels are rectified (fabsf) below, so this is correct even if the
  // converter turns out to hand back the PREVIOUS conversion: that flips the
  // sign of V and I together and leaves their ratio alone.
  for (uint8_t p = 0; p < periods * 2; p++) {
    int sign = (p & 1) ? -1 : +1;
    dac.setVoltage(sign > 0 ? dacHi : dacLo, false);
    uint32_t t0 = micros();
    while (micros() - t0 < SETTLE_US) { }
    adsStart(ADS_V, ADS_MUX_DIFF01, gainV);      // both convert together
    adsStart(ADS_I, ADS_MUX_DIFF01, GAIN_FOUR);
    int16_t v = adsFinish(ADS_V);
    int16_t i = adsFinish(ADS_I);
    if (i < iMin) iMin = i;
    if (i > iMax) iMax = i;
    if (v < vMin) vMin = v;
    if (v > vMax) vMax = v;
    accV += sign * (int64_t)v; accI += sign * (int64_t)i; sumV += v; n++;
    while (micros() - t0 < HALF_PERIOD_US) { }      // keep 31.25 Hz
  }
  dac.setVoltage((dacHi + dacLo) / 2, false);
  // A converter that never moved is not a measurement. This guard is what
  // caught attempt 2 within one power-up instead of another round of chasing
  // the analog path, and it is the most valuable line in this function.
  if (n > 0 && iMax == iMin) adcStuck = true;
  lastIuA = 0;
  if (n == 0) return Z_OPEN;
  {
    float lsbV = adcV.computeVolts(1);
    lastVClip = (vMax >= 32760 || vMin <= -32760);
    lastVPeak = fmaxf(fabsf((float)vMin), fabsf((float)vMax)) * lsbV;   // volts at SENSE_OUT
    lastDcMv  = (float)sumV / n * lsbV / G_SENSE * 1000.0f;             // at the electrode
  }
  float Vs = (float)accV / n * adcV.computeVolts(1);   // volts at SENSE_OUT relative to VREF
  float Vt = (float)accI / n * adcI.computeVolts(1);   // volts at TIA_OUT relative to VREF
  float I  = fabsf(Vt) / R_F;
  lastIuA = I * 1e6f;
  if (I < I_OPEN_FLOOR) return Z_OPEN;                  // open circuit: no current could flow
  float z = (fabsf(Vs) / G_SENSE) / I;
  return (z >= Z_OPEN) ? Z_OPEN : z;
}

// Autoranging wrapper.
//
// 1 Nov 2026, first test with a gelled cap: every Mode A site read 0.0. A tin
// electrode and a carbon-filled insert in salty gel are a small battery, so
// each site loop carries a steady DC voltage on top of the 31.25 Hz signal.
// The range used to be chosen from the IMPEDANCE alone, so a low-impedance site
// got the +/-0.256 V range - and the DC, times the sense gain of 4, pushed the
// signal off the end of it. The AC part was clipped away and the site read 0.
// The range is now chosen from the PEAK voltage the converter actually sees,
// DC included, and a reading that still clipped is repeated one range lower.
//
// If the DC is so large that the sense amplifier itself is near its supply
// rails (about 0.4 V at the electrode), no range can recover the signal. That
// site reads Z_SAT and shows "DC" instead of a number.
float measureZ(uint8_t chDS, uint8_t chR, uint8_t swMask, uint8_t periods) {
  float z = measureRaw(chDS, chR, swMask, 1, GAIN_ONE);          // coarse, +/-4.096 V
  if (lastVPeak > SENSE_SAT_V) return Z_SAT;
  if (z >= Z_OPEN) return Z_OPEN;
  adsGain_t g = (lastVPeak < 0.20f) ? GAIN_SIXTEEN : (lastVPeak < 0.80f) ? GAIN_FOUR : GAIN_ONE;
  z = measureRaw(chDS, chR, swMask, periods, g);
  if (lastVClip && g != GAIN_ONE) {                               // DC wandered up: one range down
    g = (g == GAIN_SIXTEEN) ? GAIN_FOUR : GAIN_ONE;
    z = measureRaw(chDS, chR, swMask, periods, g);
  }
  return z;
}

// Unclamped: a path can legitimately come out slightly negative after the
// fit, and the site-zero measurement needs to see that sign.
float applyCalRaw(float zraw, bool modeB) {
  return cal.a * zraw + cal.b - (modeB ? cal.rOffsetB : cal.rOffsetA);
}
float applyCal(float zraw, bool modeB) {
  if (zraw >= Z_SAT)  return Z_SAT;
  if (zraw >= Z_OPEN) return Z_OPEN;
  float z = applyCalRaw(zraw, modeB);
  return z < 0 ? 0 : z;
}

// ------------------------------------------------------------------ per-site zero
// Self-calibration removes the resistance of the return path it can see: the
// on-board standards all come home through U6 and SW5. A real site comes home
// through its OWN return-mux channel, and for sites 0-15 that is a different
// chip, U5. At 3.3 V with the signal at mid-supply an HC4067 switch is at its
// highest resistance and two chips can differ by a few hundred ohms. Found
// 27 Oct 2026: a 4.72 k resistor on Fp1 (pin 1 to pin 1) read 5.2 k while the
// on-board 4.7 k standard read 4,708. A per-site zero, measured once through a
// straight-through DB25 cable, removes whatever each site's own path adds -
// return switch, connector contacts and all. Mode A only: in Mode B the
// current returns through the ear channel, not the site's own return switch.
float zSiteZero[22] = {0};
bool  siteZeroValid = false;
const float SITE_ZERO_MAX = 3000.0f;     // beyond this the cable is not there

float applySite(float zraw, int site, bool modeB) {
  if (zraw >= Z_SAT)  return Z_SAT;
  if (zraw >= Z_OPEN) return Z_OPEN;
  float z = applyCalRaw(zraw, modeB) - (modeB ? 0.0f : zSiteZero[site]);
  return z < 0 ? 0 : z;
}

// Measures every site through the cable. Returns the number of sites that did
// not read like a cable (open, or implausibly high); those entries are 0.
int measureSiteZero(float out[22], bool ok[22]) {
  int bad = 0;
  for (int i = 0; i < 22; i++) {
    float zraw = measureZ(i, i, MODE_A_MASK, 4);
    float z = (zraw >= Z_OPEN) ? Z_OPEN : applyCalRaw(zraw, false);
    ok[i] = !(zraw >= Z_OPEN || fabsf(z) > SITE_ZERO_MAX);
    out[i] = ok[i] ? z : 0;
    if (!ok[i]) bad++;
  }
  return bad;
}

// ------------------------------------------------------------------ self-calibration against the on-board standards
// Self-calibration results, kept so the SCREEN can show them.
//
// These numbers only ever went to Serial, which is the wrong channel on this
// hardware - the whole reason pm_i2c_scan reports to the display is that
// GPIO19/20 are the ESP32-S3's native USB pins. Worse, a failed calibration
// drew nothing at all: the map came up with all 22 sites OPEN, which looks
// exactly like a board with no multiplexers fitted. At the one moment you most
// need to know whether the measurement path works, the screen said nothing.
struct CalReport {
  float z1, z5, z100, zs, zo;       // raw, before the fit
  float r1, r5, r100, rShort;       // ohms, after the fit
  float i1, i5, i100, is, io;       // drive current during each, microamps
  bool  ok, lowDrive;
  const char *why;
} calRep;

bool selfCalibrate() {
  adcStuck = false;      // BEFORE the measurements, which are what set it
  float z1 = measureRaw(CH_CAL1K, CH_CAL1K, MODE_A_MASK, 16, GAIN_SIXTEEN);     calRep.i1 = lastIuA;
  float z5 = measureRaw(CH_CAL5K, CH_CAL5K, MODE_A_MASK, 16, GAIN_SIXTEEN);     calRep.i5 = lastIuA;
  float z100 = measureRaw(CH_CAL100K, CH_CAL100K, MODE_A_MASK, 16, GAIN_ONE);   calRep.i100 = lastIuA;
  float zs = measureRaw(CH_SHORT, CH_SHORT, MODE_A_MASK, 16, GAIN_SIXTEEN);     calRep.is = lastIuA;
  bool stuck = adcStuck;   // judged on the four channels that carry current only:
                           // an open channel can legitimately repeat one value
  float zo = measureRaw(CH_OPEN, CH_OPEN, MODE_A_MASK, 4, GAIN_ONE);            calRep.io = lastIuA;
  adcStuck = stuck;
  // The short channel's current is the cleanest look at the pump itself: no
  // load to speak of. Under half the design value works, but is worth fixing.
  calRep.lowDrive = calRep.is < 0.5f * I_TEST * 1e6f;
  calRep.z1=z1; calRep.z5=z5; calRep.z100=z100; calRep.zs=zs; calRep.zo=zo;
  calRep.ok=false; calRep.why="";
  calRep.r1=calRep.r5=calRep.r100=calRep.rShort=0;

  // Name the fault instead of returning a bare false. Each of these points at a
  // different part of the board, and telling them apart on the bench otherwise
  // means a meter and a guess.
  if (adcStuck)            { calRep.why = "ADC returned constant data - SAMPLING fault, not an open circuit"; return false; }
  if (z1 >= Z_OPEN && z5 >= Z_OPEN && z100 >= Z_OPEN && zs >= Z_OPEN)
                           { calRep.why = "No current on any standard: U9B, R1-R4, R17, or U8"; return false; }
  if (z1 >= Z_OPEN)        { calRep.why = "1k standard open - R20, or drive/sense mux"; return false; }
  if (z5 >= Z_OPEN)        { calRep.why = "4.7k standard open - R21"; return false; }
  if (z100 >= Z_OPEN)      { calRep.why = "100k standard open - R22"; return false; }
  if (zo < 100000.0f)      { calRep.why = "open channel is not open - a mux is stuck on"; return false; }
  if (z100 <= z1)          { calRep.why = "100k reads below 1k - standards swapped?"; return false; }
  // two-point fit through 1k and 100k standards
  cal.a = (100000.0f - 1000.0f) / (z100 - z1);
  cal.b = 1000.0f - cal.a * z1;
  cal.rOffsetA = 0;                     // the short channel reading after fit is the residual return-path resistance
  float resid = cal.a * zs + cal.b;
  cal.rOffsetA = resid;
  // Mode B adds S1..S4 and one more mux in the loop: measure the short through that path if you add a
  // jumper; otherwise use the same offset plus a nominal 2x switch resistance (~200 ohm at 3.3 V).
  cal.rOffsetB = resid + 200.0f;
  prefs.putFloat("a", cal.a); prefs.putFloat("b", cal.b); prefs.putFloat("ra", cal.rOffsetA); prefs.putFloat("rb", cal.rOffsetB);
  // R21 is a mid-scale CHECK, not part of the fit, which is what makes it
  // informative: the fit is forced through R20 and R22, so those two always
  // read right. R21 is the only number here that can be wrong, and it is
  // therefore the only one that tells you the chain is linear in between.
  // Expect about 4700 with the Yageo MFP-25BRD52-4K7 that the BOM now calls
  // for. Earlier boards used a 4.99k part and some documentation still says
  // 4990; that is stale, and 4700 is correct for the part you have.
  calRep.r1     = cal.a * z1   + cal.b;
  calRep.r5     = cal.a * z5   + cal.b;
  calRep.r100   = cal.a * z100 + cal.b;
  calRep.rShort = resid;
  calRep.ok     = true;
  calRep.why    = "";
  Serial.printf("CAL a=%.5f b=%.1f short=%.1f R21 (4.7k) reads %.1f\n", cal.a, cal.b, resid, calRep.r5);
  return true;
}

// ------------------------------------------------------------------ Mode B decomposition
float zA1A2 = 0;
// The reference rule. 30 Oct 2026: each ear must be under 5 k on its own, the
// same standard as a scalp site. The earlier rule tested A1 and A2 IN SERIES
// against 20 k, which let either ear sit near 15 k while the other was good.
// Testing the series pair against 5 k would have been wrong the other way - it
// would demand each ear average 2.5 k - so the ears are now measured one at a
// time, directly, and each is held to 5 k.
const float REF_MAX = 5000.0f;
bool refsOk() { return zSite[CH_A1] < REF_MAX && zSite[CH_A2] < REF_MAX; }

// Measures site i in the current mode. The ears are always measured directly
// (their own insert to their own electrode, as in Mode A), in BOTH modes.
// Found 30 Oct 2026: run through the Mode B decomposition, an ear site was
// driven from the same phantom insert that the decomposition connects straight
// to the amplifier, so one of its two readings was a short circuit and its
// hexagon always showed 0.0 in Mode B, whatever the ear contact was like.
float siteZ_modeB(uint8_t ch);
// PHANTOM SCALE (8 Nov 2026). On a head, prep lowers impedance by abrading
// skin. The phantom has no skin: once the gel is worked in, each site sits on a
// floor set by its tin/Electro-Gel and gel/carbon junctions - measured at about
// 4-9 k after careful prep, which made the 5 k standard nearly unreachable.
// Readings at the scalp sites (and GND) are multiplied by this factor so a
// careful prep lands where it would on a head. A SCALE, not a subtraction:
// a poorly wetted site has less contact area and reads proportionally higher,
// and scaling keeps that difference; subtracting would squash it and could show
// a mediocre site as 0. The ears (A1, A2) are left as measured, and so is the
// reference check. Set to 1.0f for true ohms. Shown on the calibration screen.
// 0.5 was chosen from the bench session of 8 Nov 2026: after a touch-up, Mode A
// read 4.3-13 k; scaled, careful sites land at 2-4.5 k and the four weakest at
// 5.5-6.5 k - most pass, a few still need work, as on a real head.
const float PHANTOM_SCALE = 0.5f;

float phantomScaled(int i, float z) {
  if (i == CH_A1 || i == CH_A2 || z >= Z_OPEN) return z;     // ears, OPEN and DC untouched
  return z * PHANTOM_SCALE;
}

float siteZ(int i, uint8_t periods) {
  if (modeB && i != CH_A1 && i != CH_A2) return phantomScaled(i, siteZ_modeB(i));
  return phantomScaled(i, applySite(measureZ(i, i, MODE_A_MASK, periods), i, false));
}

float siteZ_modeB(uint8_t ch) {
  float za1 = applyCal(measureZ(ch, ch, MODE_B_A1_MASK, 4), true);
  float za2 = applyCal(measureZ(ch, ch, MODE_B_A2_MASK, 4), true);
  if (za1 >= Z_SAT  || za2 >= Z_SAT)  return Z_SAT;
  if (za1 >= Z_OPEN || za2 >= Z_OPEN) return Z_OPEN;
  float z = 0.5f * (za1 + za2 - zA1A2);
  return z < 0 ? 0 : z;
}

// ------------------------------------------------------------------ salt-bridge detection
//
// Set to 0 to compile the feature out entirely - worth doing during bring-up
// steps 5 and 6, when 31 extra measurements per cycle are just latency you have
// to wait through while chasing something else.
#define SALT_BRIDGE 1        // on. Shipped as 0 through bring-up, and every update
                             // sent during that time reset it - which is why the
                             // first gel test (1 Nov 2026) saw no bridges at all.

// Guard the switch itself. If the value is deleted, leaving just
// "#define SALT_BRIDGE", every "#if SALT_BRIDGE" below fails with "#if with no
// expression" - three errors on lines that look correct, and none on the line
// that is actually wrong. That happened on 24 Oct 2026. The test below is the
// standard preprocessor idiom for an empty macro: with no value, (1 - X - 1)
// becomes (1 - - 1), which is 2, so the error message lands HERE, on the right
// line, and says what to type.
#if (1 - SALT_BRIDGE - 1) == 2
#error "SALT_BRIDGE has no value. The line must read  #define SALT_BRIDGE 0  (off) or  #define SALT_BRIDGE 1  (on)."
#elif (SALT_BRIDGE + 0) != 0 && (SALT_BRIDGE + 0) != 1
#error "SALT_BRIDGE must be 0 (off) or 1 (on)."
#endif

#if (SALT_BRIDGE + 0)
// HOW A BRIDGE IS MEASURED, since it is not obvious from the schematic.
//
// The drive and sense banks (U1-U4) reach the PHANTOM contacts through J1, and
// the return bank (U5/U6) reaches the ELECTRO-CAP electrodes through J2. Those
// two banks have independent 4-bit selects and independent enables, so the
// firmware can drive site X and return through site Y. That costs nothing in
// hardware - it is the same switch fabric a normal reading uses, pointed at two
// different sites.
//
// Drive X, return Y. Current has to leave the phantom contact at X, cross into
// the gel, travel to electrode Y and return. With bare PLA between the sites
// there is no other path, so with no bridge the reading is open. With gel
// spanning X to Y the loop closes and the reading collapses to the two contact
// impedances plus the bridge itself.
//
// So no extra hardware, no extra wiring, and nothing about the normal
// measurement path changes. The whole feature is firmware.
struct BridgePair { uint8_t up, down, tier; };

// 31 pairs, ordered by descending downhill gradient (height drop / arc length on
// a hemisphere model). Direction is not cosmetic: gel runs downhill and pools at
// the DOWN site, so that is the end the student should clean first, and that is
// where the arrow points.
//
// Fp1-Fp2 and O1-O2 are deliberately absent. Both sites of each sit at the same
// height, so the gradient is zero and the only mechanism is sideways smearing;
// the midline routes they would cover are already watched by GND-Fp1/GND-Fp2 and
// Pz-O1/Pz-O2. Do not re-add them without a reason.
const BridgePair BRIDGES[] = {
  {10,11,1},   // C4  -> T4    grad 0.90
  { 8, 7,1},   // C3  -> T3    grad 0.90
  { 7,20,1},   // T3  -> A1    grad 0.88
  {11,21,1},   // T4  -> A2    grad 0.88
  { 4, 0,1},   // Fz  -> Fp1   grad 0.85
  { 4, 1,1},   // Fz  -> Fp2   grad 0.85
  { 4,19,1},   // Fz  -> GND   grad 0.84
  { 5, 6,1},   // F4  -> F8    grad 0.84
  {15,16,1},   // P4  -> T6    grad 0.84
  { 3, 2,1},   // F3  -> F7    grad 0.84
  {13,12,1},   // P3  -> T5    grad 0.84
  { 5, 1,1},   // F4  -> Fp2   grad 0.82
  { 3, 0,1},   // F3  -> Fp1   grad 0.82
  {14,17,1},   // Pz  -> O1    grad 0.81
  {14,18,1},   // Pz  -> O2    grad 0.81
  {15,18,2},   // P4  -> O2    grad 0.78
  {13,17,2},   // P3  -> O1    grad 0.78
  {12,20,2},   // T5  -> A1    grad 0.76
  {16,21,2},   // T6  -> A2    grad 0.76
  { 8,12,2},   // C3  -> T5    grad 0.74
  {10,16,2},   // C4  -> T6    grad 0.74
  {19, 1,2},   // GND -> Fp2   grad 0.70
  {19, 0,2},   // GND -> Fp1   grad 0.70
  { 9, 3,3},   // Cz  -> F3    grad 0.48
  { 9, 5,3},   // Cz  -> F4    grad 0.48
  { 9,15,3},   // Cz  -> P4    grad 0.48
  { 9,13,3},   // Cz  -> P3    grad 0.48
  { 9, 4,3},   // Cz  -> Fz    grad 0.37
  { 9,10,3},   // Cz  -> C4    grad 0.37
  { 9,14,3},   // Cz  -> Pz    grad 0.37
  { 9, 8,3},   // Cz  -> C3    grad 0.37
};
const uint8_t N_BRIDGE = sizeof(BRIDGES) / sizeof(BRIDGES[0]);

// Hysteresis is mandatory, not a refinement. A bridge sitting near the threshold
// crosses it on measurement noise alone, and without a gap the hexagons would
// chatter and the alarm would retrigger every few seconds. Trip low, clear high.
// Tune both against a real gel bridge on the finished phantom - the right values
// depend on contact geometry, and these are a starting point, not a measurement.
const float BR_TRIP  = 50000.0f;
const float BR_CLEAR = 80000.0f;

// Pairs checked per pass through loop(). Each check is one 32 ms period, so two
// costs about 64 ms on top of a ~256 ms loop and a full cycle lands near 3.5 s.
const uint8_t BR_PER_PASS = 2;

bool     bridged[sizeof(BRIDGES)/sizeof(BRIDGES[0])];
float    bridgeZ[sizeof(BRIDGES)/sizeof(BRIDGES[0])];
bool     siteBridged[22];
uint8_t  bridgeCount = 0;
uint8_t  brScan = 0, brCycle = 0;
bool     brAlarm = false;

// Tier gating: tier 1 every cycle, tier 2 every second, tier 3 every third.
// 15 + 8/2 + 8/3 = 21.7 measurements per cycle instead of 31, with the steepest
// pairs - the ones gel actually runs down - sampled at full rate.
// GND is measured and prepped like any site, but left OUT of bridge checking
// (decided 5 Nov 2026). On this phantom the GND and Fz inserts are only 4 mm
// apart, so gel from Fz reaching the GND insert is close to unavoidable, and the
// conductive insert itself completes the bridge - on a real head the gel would
// have to reach the other electrode's gel, with the cups about 3 cm apart.
// Shrinking or masking the GND insert was rejected: it would take away the
// alignment leeway (about a quarter's diameter) that real cap placement has.
// Set to true to check the three GND pairs again (Fz-GND, GND-Fp1, GND-Fp2).
const bool BRIDGE_CHECK_GND = false;
const uint8_t SITE_GND = 19;

static inline bool brDue(uint8_t i, uint8_t cycle) {
  if (!BRIDGE_CHECK_GND && (BRIDGES[i].up == SITE_GND || BRIDGES[i].down == SITE_GND)) return false;
  uint8_t tier = BRIDGES[i].tier;
  return tier == 1 || (tier == 2 && (cycle % 2) == 0) || (tier == 3 && (cycle % 3) == 0);
}

void brRecount() {
  for (int i = 0; i < 22; i++) siteBridged[i] = false;
  bridgeCount = 0;
  for (uint8_t i = 0; i < N_BRIDGE; i++) {
    if (!bridged[i]) continue;
    bridgeCount++;
    siteBridged[BRIDGES[i].up] = true;
    siteBridged[BRIDGES[i].down] = true;
  }
}

// Returns true if a NEW bridge appeared this call, so the alarm is edge
// triggered rather than nagging for as long as the gel is there.
bool brStep(uint8_t checks) {
  bool fresh = false;
  for (uint8_t c = 0; c < checks; c++) {
    uint8_t guard = 0;
    while (!brDue(brScan, brCycle) && guard++ < N_BRIDGE) {
      if (++brScan >= N_BRIDGE) { brScan = 0; brCycle++; }
    }
    uint8_t i = brScan;
    if (++brScan >= N_BRIDGE) { brScan = 0; brCycle++; }

    // Screen cheaply, then confirm. One period is noisy for an absolute reading
    // but ample for "is this under 50k", and spending four more periods only on
    // the handful that trip keeps a noise spike from raising a false alarm.
    float z = applyCal(measureRaw(BRIDGES[i].up, BRIDGES[i].down, MODE_A_MASK, 1, GAIN_ONE), false);
    if (z < BR_TRIP && !bridged[i])
      z = applyCal(measureRaw(BRIDGES[i].up, BRIDGES[i].down, MODE_A_MASK, 4, GAIN_ONE), false);
    bridgeZ[i] = z;

    if (!bridged[i] && z < BR_TRIP)       { bridged[i] = true; fresh = true; }
    else if (bridged[i] && z > BR_CLEAR)  { bridged[i] = false; }
  }
  brRecount();
  return fresh;
}

// Identifies the current set of bridges, so the map knows when to rebuild.
uint32_t brSig() {
  uint32_t h = 2166136261u;
  for (uint8_t i = 0; i < N_BRIDGE; i++) if (bridged[i]) h = (h ^ (i + 1)) * 16777619u;
  return h;
}

// The worst active bridge: steepest tier first, then lowest impedance. This is
// the one named on the panel when several are up at once.
int brWorst() {
  int best = -1;
  for (uint8_t i = 0; i < N_BRIDGE; i++) {
    if (!bridged[i]) continue;
    if (best < 0 || BRIDGES[i].tier < BRIDGES[best].tier ||
        (BRIDGES[i].tier == BRIDGES[best].tier && bridgeZ[i] < bridgeZ[best])) best = i;
  }
  return best;
}
#else
const uint8_t N_BRIDGE = 0;
const uint8_t BR_PER_PASS = 0;
bool siteBridged[22];
uint8_t bridgeCount = 0;
inline bool brStep(uint8_t) { return false; }
inline uint32_t brSig() { return 0; }
inline int brWorst() { return -1; }
#endif

// ------------------------------------------------------------------ display
Arduino_ESP32RGBPanel *rgbpanel = makeRGBPanel();      // board_config.h
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(800, 480, rgbpanel, 0, true);
TAMC_GT911 touch(PIN_SDA, PIN_SCL, -1 /*INT*/, TOUCH_RST_PIN, 800, 480);

// The GT911 latches its address from the INT pin level during reset, and neither
// board breaks INT out, so it is fixed by board wiring. Published configs got
// this wrong for the Elecrow - they claim 0x14, the hardware answers at 0x5D -
// so find it on the bus rather than trusting any header, exactly as the bring-up
// tools do. A header that disagrees with the hardware means no taps at all,
// which is a silent failure rather than an error.
uint8_t touchAddr = TOUCH_ADDR;
bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// ---- colour scheme, 17 Oct 2026 ----------------------------------------
//
// C_TXT and C_DIM come from board_config.h. C_SCREEN there stays pure black for the
// bring-up tools, where an all-zeros field is part of the panel test; the
// instrument screen uses a deep slate instead, which lets the status colours
// read as colour rather than as glare against black.
const uint16_t C_SCREEN = 0x0862;   // deep slate background
const uint16_t C_RULE   = 0x0552;   // teal accent rule under the header
const uint16_t C_HEAD   = 0x4B0E;   // head outline and panel divider
const uint16_t C_INK    = 0x0881;   // dark label, for use on the light chips

// Status colours. Every pair was checked two ways: WCAG contrast for the label
// sitting on the chip, and separation after a deuteranopia simulation, because
// green against amber is exactly the pair a red-green colour blind student
// would lose. Worst label contrast is 5.18:1 against a 4.5:1 target, and the
// closest pair is still 105 units apart simulated - a collision would be under
// about 40. C_OPEN is deliberately a cool blue-grey so "no contact at all"
// reads as a different kind of thing from "bad contact".
const uint16_t C_OK = 0x2E6E, C_WARN = 0xF5A5, C_BAD = 0xC985, C_OPEN = 0x428B;
float zSite[22]; bool modeB = false; int focus = 18; bool refsValid = true;
uint32_t lastDraw = 0;

// Head-map geometry. File scope so the touch hit-test uses the same numbers the
// drawing code does - they must never drift apart.
const int HX = 260, HY = 276, HR = 172;
// Hexagon and focus-ring radii. The ceiling is set by Fz and GND, the closest
// pair on the layout at 0.300 map units (51.6 px between centres, one directly
// above the other). HR=182 was tried and fails: the ears at 1.28 map units
// would run off the left edge of the map region once the focus ring is added.
//
// 28 Oct 2026: hexagons turned FLAT-TOP. Fz and GND are stacked vertically, and
// a flat-top hexagon is only 0.87 x its radius tall, so the radius could grow
// from 23 to 27 with the Fz/GND gap still 4.8 px (it was 5.6). That also widens
// the label area across the middle from 40 px to 54 px, which is what let the
// site names and readings roughly double in height.
const int HEX_R = 27, RING_R = 33;

// Incremental-redraw state. The first version of this code blanked the whole map
// with fillRect() and repainted all 22 hexagons five times a second; the RGB
// panel scans its framebuffer continuously, so it kept catching the display
// mid-rebuild and the map visibly flickered. Nothing is ever blanked now: text is
// drawn opaque (foreground + background) over fixed-width fields, so a glyph
// paints out whatever it replaces, and only elements whose content actually
// changed are touched. In steady state that is one hexagon per pass.
bool     mapInit = false, panelInit = false;
uint16_t lastCol[22];
char     lastVal[22][8];
int      lastFocus = -1;
uint32_t lastBridgeSig = 0;
char     lastPanelName[8] = "", lastPanelVal[8] = "", lastModeTxt[24] = "";
char     lastBrTxt[32] = "", lastFootTxt[28] = "";
// Transient one-line notices from the footswitch, shown under the mode line.
char     footMsg[24] = "";
uint32_t footMsgMs = 0;
const uint32_t FOOT_MSG_MS = 1500;
uint16_t lastPanelValCol = 0, lastModeCol = 0;
char     lastDcTxt[24] = "";
uint16_t lastDcCol = 0;
float    focusDcMv = 0;                 // DC at the focused site's electrode, mV

// Die-temperature readout. This is the ESP32-S3's on-chip sensor, so it reads
// warm of the air inside the enclosure by an unknown offset - the useful figure
// is the CHANGE between running open on the bench and running sealed in the
// phantom base. Sampled once a second and lightly smoothed, because the sensor
// is noisy at the degree level and a jittering display is just distracting.
const uint16_t C_HDR = 0x18E3;        // header bar fill
float    dieTempF = 0;
bool     tempInit = false;
uint32_t lastTempMs = 0;
char     lastTempTxt[12] = "";

uint16_t colorFor(float z) {
  if (z >= Z_SAT)  return C_BAD;      // DC offset too large to measure through
  if (z >= Z_OPEN) return C_OPEN;
  if (z <= 5000) return C_OK;
  if (z <= 10000) return C_WARN;
  return C_BAD;
}

// Which label colour to put on a chip. Dark ink on the bright green and amber,
// white on the deep red and the grey - decided by measurement, not by taste.
// White on this red is only 3.85:1, below the 4.5:1 line, while dark ink on it
// is 5.18:1. That is the opposite of what you would guess from "red means
// danger, use white".
uint16_t inkFor(uint16_t chip) {
  if (chip == C_TXT) return C_BAD;          // the white half of a bridge flash: red label
  return (chip == C_OK || chip == C_WARN) ? C_INK : C_TXT;
}

// ------------------------------------------------------------------ anti-aliased text
// 28 Oct 2026. The head map and focus panel used the library's built-in 5x7
// font scaled up 5-6x, which is what made the large readings look like blocks.
// Text on this screen is now drawn from pm_fonts.h: Inter Display Bold glyphs
// with 16 levels of edge coverage, blended against the known background colour.
//
// Each piece of text is rendered into a small RAM rectangle - background and
// all - and copied to the panel in ONE draw16bitRGBBitmap() call. That keeps the
// rule this screen already lives by: nothing is ever blanked and redrawn, so a
// changing reading cannot flicker. The rectangle is the field's fixed box, so a
// shorter new value paints out a longer old one with no separate erase.
static uint16_t aaBuf[256 * 72];          // largest field: the panel's site name

static inline uint16_t mix565(uint16_t fg, uint16_t bg, uint8_t a) {   // a = 0..15
  if (a == 0) return bg;
  if (a >= 15) return fg;
  uint32_t r = ((fg >> 11) * a + (bg >> 11) * (15 - a) + 7) / 15;
  uint32_t g = (((fg >> 5) & 63) * a + ((bg >> 5) & 63) * (15 - a) + 7) / 15;
  uint32_t b = ((fg & 31) * a + (bg & 31) * (15 - a) + 7) / 15;
  return (uint16_t)((r << 11) | (g << 5) | b);
}
const AAGlyph *aaGlyph(const AAFont &f, char c) {
  if ((uint8_t)c < f.first || (uint8_t)c > f.last) c = ' ';
  return &f.g[(uint8_t)c - f.first];
}
int aaWidth(const AAFont &f, const char *s) {
  int w = 0;
  for (; *s; s++) w += aaGlyph(f, *s)->adv;
  return w;
}
// Draws s into the box (bx, by, bw, bh). baseline is measured from the top of
// the box; align 0 = left, 1 = centre, 2 = right. Ink outside the box is clipped.
void aaField(const AAFont &f, const char *s, int bx, int by, int bw, int bh,
             int baseline, int align, uint16_t fg, uint16_t bg) {
  if (bw <= 0 || bh <= 0 || bw * bh > (int)(sizeof aaBuf / sizeof aaBuf[0])) return;
  for (int i = 0; i < bw * bh; i++) aaBuf[i] = bg;
  int tw = aaWidth(f, s);
  int x = (align == 0) ? 0 : (align == 1) ? (bw - tw) / 2 : (bw - tw);
  for (; *s; s++) {
    const AAGlyph *g = aaGlyph(f, *s);
    for (int r = 0; r < g->h; r++) {
      int yy = baseline + g->yo + r;
      if (yy < 0 || yy >= bh) continue;
      for (int c = 0; c < g->w; c++) {
        int xx = x + g->xo + c;
        if (xx < 0 || xx >= bw) continue;
        uint32_t k = (uint32_t)r * g->w + c;
        uint8_t byte = f.data[g->off + (k >> 1)];
        uint8_t a = (k & 1) ? (byte & 0x0F) : (byte >> 4);
        if (a) aaBuf[yy * bw + xx] = mix565(fg, bg, a);
      }
    }
    x += g->adv;
  }
  gfx->draw16bitRGBBitmap(bx, by, aaBuf, bw, bh);
}

// A bridged site blinks. The blink runs through the SAME change-detection path
// as everything else - siteColour() simply returns a different answer on
// alternate half-seconds, so drawHeadMap() repaints exactly the affected
// hexagons and nothing else. No separate animation timer touching the panel.
bool     brFlash = false;
uint32_t brFlashMs = 0;
uint16_t siteColour(int i) {
  if (siteBridged[i]) return brFlash ? C_BAD : C_TXT;   // full inversion, label and all
  return colorFor(zSite[i]);
}
void drawHex(int cx, int cy, int r, uint16_t col) {
  int x[6], y[6];   // flat-top: vertices at 0, 60, 120 ... degrees
  for (int i = 0; i < 6; i++) { float a = PI / 3 * i; x[i] = cx + lroundf(r * cosf(a)); y[i] = cy + lroundf(r * sinf(a)); }
  for (int i = 0; i < 6; i++) gfx->fillTriangle(cx, cy, x[i], y[i], x[(i + 1) % 6], y[(i + 1) % 6], col);
}
// The reading on a hexagon, in kilohms, kept to three characters so it fits
// at the larger size: one decimal below 10 k, whole kilohms above.
void siteText(int i, char *out, size_t n) {
  float k = zSite[i] / 1000.0f;
  if (zSite[i] >= Z_SAT)       snprintf(out, n, "DC");
  else if (zSite[i] >= Z_OPEN) snprintf(out, n, "--");
  else if (k < 9.95f)     snprintf(out, n, "%.1f", k);
  else                    snprintf(out, n, "%.0f", k);
}

void siteCentre(int i, int &cx, int &cy) {
  cx = HX + SITES[i].x * HR;
  cy = HY + SITES[i].y * HR;
}

// An arrow, not a line: the student needs to know which END to clean, and gel
// pools at the downhill site. Both ends stop 24 px short of the site centres,
// outside the r=20 hexagons, so a blinking hexagon repaints over its own area
// without ever chewing into the arrow.
void drawArrow(int up, int down) {
  int ux, uy, dx, dy;
  siteCentre(up, ux, uy);
  siteCentre(down, dx, dy);
  float vx = dx - ux, vy = dy - uy;
  float len = sqrtf(vx * vx + vy * vy);
  if (len < 2 * (HEX_R + 2) + 8) return;  // closer than two hexagons: no room
  vx /= len; vy /= len;
  int x0 = ux + (int)(vx * (HEX_R + 2)), y0 = uy + (int)(vy * (HEX_R + 2));
  int x1 = dx - (int)(vx * (HEX_R + 2)), y1 = dy - (int)(vy * (HEX_R + 2));
  gfx->drawLine(x0, y0, x1, y1, C_BAD);
  float px = -vy, py = vx;                 // perpendicular, for the head
  gfx->fillTriangle(x1, y1,
                    x1 - (int)(vx * 11 - px * 5), y1 - (int)(vy * 11 - py * 5),
                    x1 - (int)(vx * 11 + px * 5), y1 - (int)(vy * 11 + py * 5), C_BAD);
}

void drawArrows() {
#if (SALT_BRIDGE + 0)
  for (uint8_t i = 0; i < N_BRIDGE; i++)
    if (bridged[i]) drawArrow(BRIDGES[i].up, BRIDGES[i].down);
#endif
}

// Repaint one site. The filled hexagon covers both the old colour and the old
// label, and the labels are drawn opaque against the hexagon colour, so no part
// of this ever shows background.
// Label boxes, relative to the hexagon centre. Both sit wholly inside a flat-top
// hexagon of radius 27 - checked pixel by pixel against the rasterised hexagon,
// not just on paper. The name box's top row is 14 px up, where the hexagon is
// 37 px wide, exactly GND's width (the widest name); one row higher clips GND's
// corners. The reading box is 32 px wide and ends 18 px down, where the
// hexagon is 33 px wide. 'p' in Fp1/Fp2 descends to 3 px below centre, clear
// of the reading box at 4.
const int SITE_NAME_Y = -14, SITE_NAME_H = 17, SITE_NAME_BASE = 13;
const int SITE_VAL_Y  =   4, SITE_VAL_H  = 15, SITE_VAL_BASE  = 13, SITE_VAL_W = 32;

// Only the reading changed: repaint its box, leave the hexagon alone.
void drawSiteValue(int i, uint16_t col) {
  int cx, cy; siteCentre(i, cx, cy);
  char val[8]; siteText(i, val, sizeof val);
  aaField(PMF_HEX, val, cx - SITE_VAL_W / 2, cy + SITE_VAL_Y, SITE_VAL_W, SITE_VAL_H,
          SITE_VAL_BASE, 1, inkFor(col), col);
  snprintf(lastVal[i], sizeof lastVal[i], "%s", val);
}
void drawSite(int i) {
  int cx, cy; siteCentre(i, cx, cy);
  uint16_t col = siteColour(i);
  drawHex(cx, cy, HEX_R, col);
  int nw = aaWidth(PMF_HEX, SITES[i].name);
  aaField(PMF_HEX, SITES[i].name, cx - nw / 2, cy + SITE_NAME_Y, nw, SITE_NAME_H,
          SITE_NAME_BASE, 0, inkFor(col), col);
  drawSiteValue(i, col);
  lastCol[i] = col;
}

// The focus ring sits at r=27, outside the r=20 hexagon, so repainting a hexagon
// never damages it. Erasing a ring can clip the head-circle outline and just
// nick a close neighbour (GND and Fz are only 45 px apart), so an erase is always
// followed by redrawing the circle and overpainting every site. That overpaint is
// invisible: identical pixels, and nothing is blanked first.
void drawRing(int i, uint16_t col) {
  int cx, cy; siteCentre(i, cx, cy);
  gfx->drawCircle(cx, cy, RING_R, col);
}

// ------------------------------------------------------------------ bridge alarm, buttons
// 4 Nov 2026: the first real bridge (Fz to GND) was easy to miss - two small
// blinking hexagons and one line of text. Now, while ANY bridge is present:
//   - the bridged hexagons invert fully, red/white, label included;
//   - the whole header bar flashes red with the pair named in large type;
//   - the panel's bridge line flashes as a filled red box;
//   - a beep repeats every second until the bridge clears or MUTE is tapped.
// MUTE silences only the bridges already present: a NEW bridge un-mutes it.
bool     alarmMuted = false;
uint32_t lastAlarmBeep = 0;
bool     hdrAlarmShown = false, hdrAlarmFlash = false;
char     lastHdrTxt[40] = "";
bool     recalRequested = false;
bool     bridgeWav();               // is saltbridge.wav standing in for the piezo? (below)

// Panel buttons, below the DC offset line.
const int BTN_Y = 334, BTN_H = 48;
const int BTN_MUTE_X = 540, BTN_MUTE_W = 115;
const int BTN_CAL_X  = 670, BTN_CAL_W  = 115;
int lastMuteState = -1;               // what the MUTE button currently shows

void bridgeText(char *out, size_t n, bool forHeader) {
  out[0] = 0;
#if (SALT_BRIDGE + 0)
  int worst = brWorst();
  if (worst < 0) return;
  char tail[10] = "";
  if (bridgeCount > 1) snprintf(tail, sizeof tail, " +%d", bridgeCount - 1);
  snprintf(out, n, forHeader ? "SALT BRIDGE  %s > %s%s" : "BRIDGE %s>%s%s",
           SITES[BRIDGES[worst].up].name, SITES[BRIDGES[worst].down].name, tail);
#else
  (void)n; (void)forHeader;
#endif
}

// ------------------------------------------------------------------ scan progress bar
// 8 Nov 2026: while prepping it was hard to tell whether every site had been
// re-measured since the last touch-up. The background scan measures one site
// per pass (plus the focused site every pass), so a full scan of all 22 takes
// several seconds. The header now carries a 22-segment bar that fills as the
// scan advances and turns green for a moment when a full scan completes.
const int SCAN_X = 396, SCAN_Y = 22, SCAN_SEG_W = 8, SCAN_GAP = 2, SCAN_H = 14;   // clear of the title (ends x~350) and the temperature (x 654)
uint8_t  scanDone = 0;              // sites measured so far in the current scan
uint32_t scanCompleteMs = 0;        // when the last full scan finished
const uint32_t SCAN_FLASH_MS = 800;
int      lastScanDrawn = -1;        // -1 forces a full redraw
bool     lastScanFlash = false;
uint32_t scanStartMs = 0, lastScanSecs = 0;

void drawScanBar() {
  if (hdrAlarmShown) return;                         // the header is a bridge alarm
  bool flash = scanCompleteMs && (millis() - scanCompleteMs < SCAN_FLASH_MS);
  int filled = flash ? 22 : scanDone;
  if (filled == lastScanDrawn && flash == lastScanFlash) return;
  if (lastScanDrawn < 0) {
    gfx->setTextSize(1); gfx->setTextColor(C_DIM, C_HDR);
    gfx->setCursor(SCAN_X - 34, SCAN_Y + 4); gfx->print("SCAN");
  }
  for (int k = 0; k < 22; k++) {
    int x = SCAN_X + k * (SCAN_SEG_W + SCAN_GAP);
    uint16_t c = (k < filled) ? (flash ? C_OK : C_RULE) : C_HDR;
    gfx->fillRect(x, SCAN_Y, SCAN_SEG_W, SCAN_H, c);
    if (k >= filled) gfx->drawRect(x, SCAN_Y, SCAN_SEG_W, SCAN_H, C_HEAD);
  }
  // Time the last full scan took, so it is clear how long a touch-up takes to show.
  char t[8]; snprintf(t, sizeof t, lastScanSecs ? "%2lus" : "   ", (unsigned long)lastScanSecs);
  gfx->setTextSize(1); gfx->setTextColor(C_DIM, C_HDR);
  gfx->setCursor(SCAN_X + 22 * (SCAN_SEG_W + SCAN_GAP) + 4, SCAN_Y + 4); gfx->print(t);
  lastScanDrawn = filled; lastScanFlash = flash;
}

void drawHeader();
void drawAlarmHeader() {
  if (bridgeCount == 0) {
    if (hdrAlarmShown) { drawHeader(); lastTempTxt[0] = 0; hdrAlarmShown = false; lastHdrTxt[0] = 0; lastScanDrawn = -1; }
    return;
  }
  char t[40]; bridgeText(t, sizeof t, true);
  if (hdrAlarmShown && hdrAlarmFlash == brFlash && strcmp(t, lastHdrTxt) == 0) return;
  uint16_t bg = brFlash ? C_BAD : C_HDR, fg = brFlash ? C_TXT : C_BAD;
  gfx->fillRect(0, 0, 800, 57, bg);
  gfx->fillRect(0, 57, 800, 3, C_BAD);
  int w = aaWidth(PMF_TITLE, t) + 2; if (w > 768) w = 768;
  aaField(PMF_TITLE, t, 16, 12, w, 36, 28, 0, fg, bg);   // 36 tall: room for the p in Fp1
  hdrAlarmShown = true; hdrAlarmFlash = brFlash;
  snprintf(lastHdrTxt, sizeof lastHdrTxt, "%s", t);
}

void calButton(int x, int y, int w, int h, const char *label, uint16_t fill);
void drawButtons(bool force) {
  int st = (bridgeCount == 0 || !(BEEP_ON_BRIDGE || bridgeWav())) ? 0 : alarmMuted ? 2 : 1;   // idle / sounding / muted
  if (force || st != lastMuteState) {
    calButton(BTN_MUTE_X, BTN_Y, BTN_MUTE_W, BTN_H, st == 2 ? "MUTED" : "MUTE",
              st == 1 ? C_WARN : C_OPEN);
    lastMuteState = st;
  }
  if (force) calButton(BTN_CAL_X, BTN_Y, BTN_CAL_W, BTN_H, "RECAL", C_RULE);
}

// Speaker sounds that stand in for the piezo (see SOUND_ON_ above). Each is
// used only when its file actually loaded.
bool bridgeWav() { return SOUND_ON_BRIDGE && audioHas(SND_BRIDGE); }
bool bridgeLoopOn = false;
// saltbridge.wav loops while any bridge is present and MUTE is not on. Called
// every pass and straight after a MUTE tap, so muting is immediate.
void bridgeSoundService() {
  bool want = bridgeWav() && bridgeCount > 0 && !alarmMuted;
  if (want != bridgeLoopOn) { audioLoop(SND_BRIDGE, want, SOUND_VOLUME, VOL_BRIDGE); bridgeLoopOn = want; }
}
// Mode A/B changed. The footswitch has always chirped; the header tap now
// plays the sound too, when there is a file for it.
void modeSound(bool fromPedal) {
  if (SOUND_ON_MODE && audioHas(SND_MODE)) audioPlay(SND_MODE, SOUND_VOLUME, VOL_MODE);
  else if (fromPedal && BEEP_ON_MODE)      chirp(60, 0, modeB, refsValid);
}

// Called from loop(): one beep a second while a bridge is present and not muted
// (the piezo; with saltbridge.wav loaded, bridgeSoundService() does the sound).
void alarmService(bool newBridge) {
  if (newBridge) alarmMuted = false;
  if (bridgeCount == 0) { alarmMuted = false; bridgeSoundService(); return; }
  bridgeSoundService();
  if (bridgeWav()) return;
  if (BEEP_ON_BRIDGE && !alarmMuted && millis() - lastAlarmBeep >= 1000) {
    chirp(150, 0, modeB, refsValid);
    lastAlarmBeep = millis();
  }
}

void drawHeadMap() {
  // A bridge that clears leaves its arrow painted on the map, and erasing a line
  // by redrawing it in the background colour would nick the head circle and any
  // hexagon it passes near. So when the SET of bridges changes, blank the map
  // region and rebuild it once. That is a single repaint seconds apart at worst,
  // not the five-a-second rebuild that caused the original flicker - the two are
  // different problems and only one of them is visible.
  uint32_t sig = brSig();
  if (!mapInit || sig != lastBridgeSig) {
    gfx->fillRect(0, 60, 520, 420, C_SCREEN);
    gfx->drawCircle(HX, HY, HR, C_HEAD);
    drawArrows();                      // under the hexagons, so they sit on top
    for (int i = 0; i < 22; i++) drawSite(i);
    drawRing(focus, C_TXT);
    lastFocus = focus;
    lastBridgeSig = sig;
    mapInit = true;
    return;
  }

  if (focus != lastFocus) {
    drawRing(lastFocus, C_SCREEN);
    gfx->drawCircle(HX, HY, HR, C_HEAD);
    drawArrows();
    for (int i = 0; i < 22; i++) drawSite(i);
    drawRing(focus, C_TXT);
    lastFocus = focus;
  }

  for (int i = 0; i < 22; i++) {
    char val[8]; siteText(i, val, sizeof val);
    uint16_t col = siteColour(i);
    if (col != lastCol[i])                  drawSite(i);           // colour change: whole hexagon
    else if (strcmp(val, lastVal[i]) != 0)  drawSiteValue(i, col); // reading only: its box
  }
}

// Focus panel layout (the panel is x 520-800, divider at 520).
// The card holds FOCUS, the site name, the reading and the DC offset line.
const int CARD_X = 528, CARD_Y = 74, CARD_W = 264, CARD_H = 250;   // 74-323; buttons start at 334
const uint16_t C_CARD = 0x1905;       // a step lighter than C_SCREEN (0x0862)
const int PANEL_X = 540, PANEL_W = 246;
const int PANEL_NAME_Y = 118;                         // below the FOCUS label
const int PANEL_VAL_Y  = 214;
const int PANEL_VAL_X  = 532, PANEL_VAL_W = 212;      // right edge 744
const int PANEL_UNIT_X = 750, PANEL_UNIT_W = 30;

void drawFocus() {
  if (!panelInit) {
    gfx->fillRect(520, 60, 280, 420, C_SCREEN);
    // The focus card: site, reading and DC offset on a lighter panel, so they
    // read as one unit, apart from the controls and status lines below it.
    gfx->fillRoundRect(CARD_X, CARD_Y, CARD_W, CARD_H, 8, C_CARD);
    gfx->drawRoundRect(CARD_X, CARD_Y, CARD_W, CARD_H, 8, C_HEAD);
    gfx->drawLine(520, 70, 520, 470, C_HEAD);   // divider: map on the left, detail on the right
    gfx->setTextColor(C_DIM, C_CARD); gfx->setTextSize(2);
    gfx->setCursor(540, 90); gfx->print("FOCUS");
    drawButtons(true);
    panelInit = true;
  }
  drawButtons(false);

  // Site name, Inter 64 px. The box is the full panel width, so a short name
  // paints out a longer one.
  char name[8]; snprintf(name, sizeof name, "%s", SITES[focus].name);
  if (strcmp(name, lastPanelName) != 0) {
    aaField(PMF_NAME, name, PANEL_X, PANEL_NAME_Y, PANEL_W, PMF_NAME.ascent + PMF_NAME.descent,
            PMF_NAME.ascent, 0, C_TXT, C_CARD);
    snprintf(lastPanelName, sizeof lastPanelName, "%s", name);
  }

  // Big reading, Inter 72 px, right-aligned against the unit so the decimal
  // point stays put while the digits change. OPEN has no unit.
  char val[8];
  bool open = zSite[focus] >= Z_OPEN;
  if (zSite[focus] >= Z_SAT) snprintf(val, sizeof val, "DC");
  else if (open)             snprintf(val, sizeof val, "OPEN");
  else                       snprintf(val, sizeof val, "%.1f", zSite[focus] / 1000.0f);
  uint16_t vcol = colorFor(zSite[focus]);
  if (strcmp(val, lastPanelVal) != 0 || vcol != lastPanelValCol) {
    int vh = PMF_VAL.ascent + PMF_VAL.descent + 1;
    aaField(PMF_VAL, val, PANEL_VAL_X, PANEL_VAL_Y, PANEL_VAL_W, vh, PMF_VAL.ascent, 2, vcol, C_CARD);
    aaField(PMF_UNIT, open ? "" : "k", PANEL_UNIT_X, PANEL_VAL_Y + PMF_VAL.ascent - PMF_UNIT.ascent,
            PANEL_UNIT_W, PMF_UNIT.ascent + 1, PMF_UNIT.ascent, 0, C_DIM, C_CARD);
    snprintf(lastPanelVal, sizeof lastPanelVal, "%s", val);
    lastPanelValCol = vcol;
  }

  // DC offset of the focused site's electrode, in millivolts. A tin electrode on
  // a carbon-filled insert is a small battery; real EEG amplifiers have a limit
  // on how much of this they tolerate, so it is worth seeing. Amber above
  // 150 mV, red when it is too large for an impedance reading at all.
  char dc[24];
  if (zSite[focus] >= Z_SAT)      snprintf(dc, sizeof dc, "%-20s", "DC OFFSET TOO HIGH");
  else if (zSite[focus] >= Z_OPEN) snprintf(dc, sizeof dc, "%-20s", "");
  else                             snprintf(dc, sizeof dc, "DC offset %+5.0f mV  ", focusDcMv);
  uint16_t dcol = (zSite[focus] >= Z_SAT) ? C_BAD : (fabsf(focusDcMv) > 150.0f) ? C_WARN : C_DIM;
  if (strcmp(dc, lastDcTxt) != 0 || dcol != lastDcCol) {
    gfx->setTextColor(dcol, C_CARD); gfx->setTextSize(2);
    gfx->setCursor(540, 300); gfx->print(dc);
    snprintf(lastDcTxt, sizeof lastDcTxt, "%s", dc);
    lastDcCol = dcol;
  }

  // Bridge line. Named rather than just counted: "clean Fz to Fp1, starting at
  // Fp1" is an instruction a student can act on, where "2 bridges" is a puzzle.
  // Padded to 20 like every other field so clearing it needs no erase.
  char br[32], body[28] = "";
  bridgeText(body, sizeof body, false);
  snprintf(br, sizeof br, "%s%c", body, body[0] ? (brFlash ? '1' : '0') : ' ');  // flash state in the key
  if (strcmp(br, lastBrTxt) != 0) {
    if (body[0]) {
      uint16_t bg = brFlash ? C_BAD : C_SCREEN, fg = brFlash ? C_TXT : C_BAD;
      gfx->fillRect(536, 424, 252, 28, bg);
      gfx->drawRect(536, 424, 252, 28, C_BAD);
      gfx->setTextColor(fg, bg); gfx->setTextSize(2);
      gfx->setCursor(536 + (252 - 12 * (int)strlen(body)) / 2, 431); gfx->print(body);
    } else {
      gfx->fillRect(536, 424, 252, 28, C_SCREEN);
    }
    snprintf(lastBrTxt, sizeof lastBrTxt, "%s", br);
  }

  // Footswitch notice. Clears itself after FOOT_MSG_MS; padded like every
  // other field so clearing it needs no erase step.
  char foot[28];
  bool showFoot = footMsg[0] && (millis() - footMsgMs < FOOT_MSG_MS);
  snprintf(foot, sizeof foot, "%-20s", showFoot ? footMsg : "");
  if (strcmp(foot, lastFootTxt) != 0) {
    gfx->setTextColor(C_TXT, C_SCREEN); gfx->setTextSize(2);
    gfx->setCursor(540, 456); gfx->print(foot);
    snprintf(lastFootTxt, sizeof lastFootTxt, "%s", foot);
  }

  // Mode line, padded to 20 so a shorter string cannot leave a tail behind.
  // Mode A does not use the linked-ears reference, so it must not inherit
  // refsValid's warning colour.
  char mode[24];
  snprintf(mode, sizeof mode, "%-20s",
           modeB ? (refsValid ? "MODE B  REFS VALID"
                    : (zSite[CH_A1] >= REF_MAX && zSite[CH_A2] >= REF_MAX) ? "MODE B  FIX A1+A2"
                    : (zSite[CH_A1] >= REF_MAX) ? "MODE B  FIX A1" : "MODE B  FIX A2")
                 : "MODE A  CHANNEL LOOP");
  uint16_t mcol = modeB ? (refsValid ? C_OK : C_BAD) : C_DIM;
  if (strcmp(mode, lastModeTxt) != 0 || mcol != lastModeCol) {
    gfx->setTextColor(mcol, C_SCREEN); gfx->setTextSize(2);
    gfx->setCursor(540, 400); gfx->print(mode);
    snprintf(lastModeTxt, sizeof lastModeTxt, "%s", mode);
    lastModeCol = mcol;
  }
}
void drawHeader() {
  gfx->fillRect(0, 0, 800, 57, C_HDR);
  gfx->fillRect(0, 57, 800, 3, C_RULE);     // accent rule, separates chrome from data
  const char *t = "PHANTOM IMPEDANCE";
  aaField(PMF_TITLE, t, 16, 12, aaWidth(PMF_TITLE, t) + 2, 32, 28, 0, C_TXT, C_HDR);
}

void drawTemp() {
  if (hdrAlarmShown) return;                  // the header is a bridge alarm right now
  if (tempInit && millis() - lastTempMs < 1000) return;
  lastTempMs = millis();
  float f = temperatureRead() * 9.0f / 5.0f + 32.0f;
  if (!tempInit) { dieTempF = f; tempInit = true; }
  else           { dieTempF += 0.25f * (f - dieTempF); }
  char t[12]; snprintf(t, sizeof t, "%5.1f F", dieTempF);
  if (strcmp(t, lastTempTxt) == 0) return;
  gfx->setTextColor(C_TXT, C_HDR); gfx->setTextSize(2);
  gfx->setCursor(654, 22); gfx->print(t);
  snprintf(lastTempTxt, sizeof lastTempTxt, "%s", t);
}

// ------------------------------------------------------------------ footswitch
//
// J5 pin 1 reaches U12 GPB1 (expander pin 9), held high by its pull-up and
// pulled low by the pedal. The hardware was on the board from the start but
// nothing read it until 17 Oct 2026.
//
// WHY POLLED, AND WHY OFTEN. The MCP23017 has interrupt outputs, but neither is
// wired to the ESP32 on this board, so the only way to see the pedal is to ask.
// A pass through loop() takes roughly 320 ms with salt-bridge scanning on, and a
// brisk foot tap can be shorter than that, so polling once per pass would drop
// presses. footPoll() is called at four points instead, which brings granularity
// to about 80 ms at a cost of four I2C reads - roughly 600 us - per pass.
//
// NEVER call footPoll() from inside measureRaw(). A long press writes the
// expander to change mode, and doing that mid-reading would reconfigure the
// switch fabric underneath a measurement in progress.
const uint32_t FOOT_LONG_MS     = 1000;   // hold this long for a mode change
const uint32_t FOOT_HINT_MS     = 400;    // tell the user a hold is building
const uint32_t FOOT_DEBOUNCE_MS = 25;

bool     footRaw = false, footDown = false, footLongFired = false;
bool     footShortPending = false;
uint32_t footEdgeMs = 0, footDownMs = 0;

void footNote(const char *t) {
  snprintf(footMsg, sizeof footMsg, "%s", t);
  footMsgMs = millis();
}

void footPoll() {
  bool raw = (expB.digitalRead(9) == LOW);       // active low against the pull-up
  uint32_t now = millis();
  if (raw != footRaw) { footRaw = raw; footEdgeMs = now; }
  if (now - footEdgeMs < FOOT_DEBOUNCE_MS) return;       // still bouncing

  if (raw == footDown) {
    if (!footDown || footLongFired) return;
    uint32_t held = now - footDownMs;
    if (held >= FOOT_LONG_MS) {
      // Fire on the threshold rather than on release, so the mode changes while
      // the foot is still down and the chirp confirms it without looking up.
      footLongFired = true;
      modeB = !modeB;
      modeSwitches(0, modeB, refsValid, false);
      footNote(modeB ? "MODE B" : "MODE A");
      modeSound(true);
    } else if (held >= FOOT_HINT_MS) {
      footNote("HOLD = MODE");                   // a long press is never a surprise
    }
    return;
  }

  footDown = raw;
  if (footDown) { footDownMs = now; footLongFired = false; }
  else if (!footLongFired) footShortPending = true;      // released before the hold
}

// The pedal should take you to work that remains, so sites already meeting the
// 5 kOhm standard are skipped. Walks the montage in order from the current focus
// and wraps once. An OPEN site reads Z_OPEN, far above 5k, so it counts as work
// still to do - which is right, since an open site is the least prepped of all.
int nextUnpassed(int from) {
  for (int k = 1; k <= 22; k++) {
    int i = (from + k) % 22;
    if (zSite[i] > 5000.0f) return i;
  }
  return from;                                   // everything passes
}

void footService() {
  if (!footShortPending) return;
  footShortPending = false;
  int next = nextUnpassed(focus);
  if (next == focus && zSite[focus] <= 5000.0f) footNote("ALL SITES PASS");
  else                                          focus = next;
}

// ------------------------------------------------------------------ calibration report
//
// Shown once at power-up, then dismissed by a touch, the pedal, or 8 seconds.
// In a classroom it is also a quiet statement that the instrument checked
// itself before anyone trusted a reading off it.
void calRow(int y, const char *what, const char *expect, const char *value, uint16_t vcol) {
  gfx->setTextSize(2);
  gfx->setTextColor(C_TXT, C_SCREEN); gfx->setCursor(40, y);  gfx->print(what);
  gfx->setTextColor(C_DIM, C_SCREEN); gfx->setCursor(330, y); gfx->print(expect);
  gfx->setTextColor(vcol, C_SCREEN);  gfx->setCursor(590, y); gfx->print(value);
}

// Drive current for one row, in its own column, so a low-but-working pump and a
// dead one look different at a glance.
void calDrive(int y, float iuA, bool expectNone = false) {
  char b[12];
  snprintf(b, sizeof b, "%4.2fuA", iuA);
  gfx->setTextSize(2);
  // The open channel: ideally none. With the phantom and cap plugged in during
  // calibration it picks up ~0.2 uA by capacitance through the cables while the
  // pump swings rail to rail (seen 8 Nov 2026); it still reads OPEN, which is
  // what matters. A stuck-on channel would carry the full ~2.5 uA.
  uint16_t c = expectNone ? ((iuA < 0.5f) ? C_OK : C_BAD)
             : (iuA >= 0.5f * I_TEST * 1e6f) ? C_DIM : (iuA >= 0.05f) ? C_WARN : C_BAD;
  gfx->setTextColor(c, C_SCREEN); gfx->setCursor(704, y); gfx->print(b);
}

// A raw reading, shown when the fit never ran. OPEN is spelled out rather than
// printed as 150000, because "this channel is dead" and "this channel reads
// 150k" are different findings.
void calRawRow(int y, const char *what, const char *expect, float z) {
  char b[24];
  if (z >= Z_OPEN) { calRow(y, what, expect, "    OPEN", C_BAD); return; }
  snprintf(b, sizeof b, "%8.0f", z);
  calRow(y, what, expect, b, C_TXT);
}

// One tap, one event, for every touch loop in this file.
//
// All three loops used to ask "is a touch being reported?" rather than "did a
// finger just come down?". The controller can keep reporting a touch for a
// moment after the finger lifts, so a single tap could fire twice - which in
// the main loop meant the mode toggle flipping and flipping back. Found in
// pm_analog_probe on 24 Oct 2026, where one tap skipped two or three channels.
//
// wasDown starts true so that a touch already being reported when a loop
// begins listening - typically the finger that pressed the previous button -
// must lift before it counts. The 400 ms hold-off absorbs a controller that
// reports up-down-up for one physical tap.
const uint32_t TAP_HOLDOFF_MS = 400;
bool tapEdge(bool &wasDown, uint32_t &lastFire, int &tx, int &ty) {
  touch.read();
  bool down = touch.isTouched;
  bool fire = down && !wasDown && (millis() - lastFire >= TAP_HOLDOFF_MS);
  wasDown = down;
  if (fire) {
    lastFire = millis();
    mapTouch(touch.points[0].x, touch.points[0].y, tx, ty);
  }
  return fire;
}

bool calHit(int tx, int ty, int x, int y, int w, int h) {
  return tx >= x && tx < x + w && ty >= y && ty < y + h;
}

void calButton(int x, int y, int w, int h, const char *label, uint16_t fill) {
  gfx->fillRect(x, y, w, h, fill);
  gfx->setTextSize(3);
  gfx->setTextColor(inkFor(fill), fill);
  gfx->setCursor(x + (w - (int)strlen(label) * 18) / 2, y + (h - 24) / 2);
  gfx->print(label);
}

void drawCalReport() {
  char buf[24];
  gfx->fillScreen(C_SCREEN);
  gfx->setTextSize(1); gfx->setTextColor(C_DIM, C_SCREEN);
  gfx->setCursor(8, 470); gfx->printf("firmware %s   sense gain %.0f   phantom scale %.2f (scalp sites)",
                                      FW_VERSION, G_SENSE, PHANTOM_SCALE);
#if PM_HAS_SPEAKER
  {                                   // what the speaker has to play, or why not
    char sl[140]; audioStatusLine(sl, sizeof sl);
    gfx->setTextColor(audioAnyProblem() ? C_WARN : C_DIM, C_SCREEN);
    gfx->setCursor(8, 460); gfx->print(sl);
    gfx->setTextColor(C_DIM, C_SCREEN);
  }
#endif
  gfx->fillRect(0, 0, 800, 57, C_HDR);
  gfx->fillRect(0, 57, 800, 3, C_RULE);
  gfx->setTextColor(C_TXT, C_HDR); gfx->setTextSize(3);
  gfx->setCursor(16, 18); gfx->print("SELF-CALIBRATION");

  uint16_t badge = calRep.ok ? C_OK : C_BAD;
  gfx->fillRect(640, 11, 144, 35, badge);
  gfx->setTextColor(inkFor(badge), badge); gfx->setTextSize(3);
  gfx->setCursor(676, 18); gfx->print(calRep.ok ? "PASS" : "FAIL");

  // On a FAILURE the readings are shown RAW, straight out of measureRaw before
  // any fit. That is the whole diagnostic value: a standard reading OPEN tells
  // you its channel is dead, while one reading a plausible-but-wrong number
  // tells you the channel works and the part or the wiring does not. Hiding
  // them, as the first version of this screen did, threw away the only
  // evidence at exactly the moment it was needed.
  float err = calRep.ok ? fabsf(calRep.r5 - 4700.0f) : 0.0f;
  uint16_t c5 = !calRep.ok ? C_TXT
              : (err <= 150.0f) ? C_OK : (err <= 400.0f) ? C_WARN : C_BAD;
  const char *col3 = calRep.ok ? "fit anchor" : "expect ~1000";

  if (calRep.ok) {
    snprintf(buf, sizeof buf, "%8.1f", calRep.r1);   calRow(110, "1.00 k standard", "fit anchor",     buf, C_TXT);
    snprintf(buf, sizeof buf, "%8.1f", calRep.r5);   calRow(150, "4.70 k check",    "expect ~4700",   buf, c5);
    snprintf(buf, sizeof buf, "%8.0f", calRep.r100); calRow(190, "100 k standard",  "fit anchor",     buf, C_TXT);
    snprintf(buf, sizeof buf, "%8.1f", calRep.rShort);calRow(230,"short channel",   "return path",    buf, C_TXT);
  } else {
    calRawRow(110, "1.00 k standard", "expect ~1000",   calRep.z1);
    calRawRow(150, "4.70 k check",    "expect ~4700",   calRep.z5);
    calRawRow(190, "100 k standard",  "expect ~100000", calRep.z100);
    calRawRow(230, "short channel",   "expect tens",    calRep.zs);
  }
  (void)col3;
  gfx->setTextSize(2); gfx->setTextColor(C_DIM, C_SCREEN);
  gfx->setCursor(590, 80); gfx->print(calRep.ok ? "  fitted" : "     raw");
  gfx->setCursor(704, 80); gfx->print(" drive");
  calDrive(110, calRep.i1); calDrive(150, calRep.i5); calDrive(190, calRep.i100);
  calDrive(230, calRep.is); calDrive(270, calRep.io, true);
  if (calRep.zo >= Z_OPEN) calRow(270, "open channel", "must be open", "    OPEN", C_OK);
  else { snprintf(buf, sizeof buf, "%8.0f", calRep.zo); calRow(270, "open channel", "must be open", buf, C_BAD); }

  gfx->setTextSize(2);
  if (calRep.ok) {
    gfx->setTextColor(C_DIM, C_SCREEN);
    gfx->setCursor(40, 304); gfx->printf("scale %.5f   offset %+.1f ohm", cal.a, cal.b);
    gfx->setCursor(40, 330);
    gfx->print(c5 == C_OK ? "Mid-scale check good: the chain is linear between anchors."
                          : "Mid-scale check is off. Suspect R21, or a mux channel.");
    gfx->setCursor(40, 356);
    if (siteZeroValid) {
      float mx = 0; for (int i = 0; i < 22; i++) if (fabsf(zSiteZero[i]) > mx) mx = fabsf(zSiteZero[i]);
      gfx->printf("Site zero stored: largest path %.0f ohm", mx);
    } else {
      gfx->print("No site zero stored - sites may read a few hundred ohm high");
    }
    if (calRep.lowDrive) {
      gfx->setTextColor(C_WARN, C_SCREEN); gfx->setCursor(40, 380);
      gfx->printf("Drive only %.2f uA (design 2.5)", calRep.is);
      gfx->setTextColor(C_DIM, C_SCREEN);
    }
    gfx->setCursor(310, 414); gfx->print("tap CONTINUE");
    gfx->setCursor(310, 438); gfx->print("or the pedal");
    calButton(40,  400, 250, 56, "ZERO SITES", C_OPEN);
    calButton(470, 400, 290, 56, "CONTINUE",   C_OK);
  } else {
    gfx->setTextColor(C_BAD, C_SCREEN);
    gfx->setCursor(40, 320); gfx->print(calRep.why);
    gfx->setTextColor(C_DIM, C_SCREEN);
    gfx->setCursor(40, 352); gfx->print("Readings are not trustworthy until this passes.");
    // No timeout on a failure. Two explicit targets instead, because the one
    // thing you want here is to fix something and measure again without
    // power-cycling the board.
    calButton(40,  400, 250, 56, "RETRY",    C_OK);
    calButton(470, 400, 290, 56, "CONTINUE", C_OPEN);
  }
}

// The site-zero procedure, run from the ZERO SITES button on the PASS screen.
// Needs a straight-through DB25 male-to-male cable, all 25 pins wired, between
// J1 and J2. Returns when the user leaves it, with the stored values either
// updated (SAVE) or untouched.
void siteZeroWait(int &which) {           // which: 1 = right button, 2 = left button
  bool wd = true; uint32_t lf = 0; int tx, ty;
  for (;;) {
    if (tapEdge(wd, lf, tx, ty)) {
      if (calHit(tx, ty, 470, 400, 290, 56)) { which = 1; return; }
      if (calHit(tx, ty, 40,  400, 250, 56)) { which = 2; return; }
    }
    delay(30);
  }
}
void siteZeroHeader(const char *sub) {
  gfx->fillScreen(C_SCREEN);
  gfx->fillRect(0, 0, 800, 57, C_HDR);
  gfx->fillRect(0, 57, 800, 3, C_RULE);
  gfx->setTextColor(C_TXT, C_HDR); gfx->setTextSize(3);
  gfx->setCursor(16, 18); gfx->print("SITE ZERO");
  gfx->setTextSize(2); gfx->setTextColor(C_DIM, C_HDR); gfx->setCursor(360, 24); gfx->print(sub);
}
void siteZeroScreen() {
  int which;
  siteZeroHeader("Mode A path offsets");
  gfx->setTextSize(2); gfx->setTextColor(C_TXT, C_SCREEN);
  gfx->setCursor(40, 100); gfx->print("Plug a STRAIGHT-THROUGH DB25 male-to-male cable");
  gfx->setCursor(40, 128); gfx->print("between J1 and J2. All 25 pins must be wired");
  gfx->setCursor(40, 156); gfx->print("(not a null-modem or printer cable).");
  gfx->setTextColor(C_DIM, C_SCREEN);
  gfx->setCursor(40, 204); gfx->print("The meter reads each site's own path through the");
  gfx->setCursor(40, 232); gfx->print("cable and subtracts it from then on. Do this once,");
  gfx->setCursor(40, 260); gfx->print("and again only if a multiplexer chip is changed.");
  calButton(40,  400, 250, 56, "CANCEL",  C_OPEN);
  calButton(470, 400, 290, 56, "MEASURE", C_OK);
  siteZeroWait(which);
  if (which == 2) return;

  float z[22]; bool ok[22];
  for (;;) {
    siteZeroHeader("measuring");
    gfx->setTextSize(2); gfx->setTextColor(C_DIM, C_SCREEN);
    gfx->setCursor(40, 200); gfx->print("Measuring 22 sites, about 6 seconds ...");
    int bad = measureSiteZero(z, ok);

    siteZeroHeader(bad ? "check the cable" : "measured");
    gfx->setTextSize(2);
    for (int i = 0; i < 22; i++) {
      int col = i / 8, row = i % 8;
      int x = 40 + col * 260, y = 100 + row * 28;
      gfx->setTextColor(C_TXT, C_SCREEN); gfx->setCursor(x, y); gfx->printf("%-4s", SITES[i].name);
      char b[12];
      if (ok[i]) snprintf(b, sizeof b, "%+6.0f", z[i]); else snprintf(b, sizeof b, "  OPEN");
      gfx->setTextColor(ok[i] ? C_TXT : C_BAD, C_SCREEN); gfx->setCursor(x + 90, y); gfx->print(b);
    }
    gfx->setTextSize(2);
    if (bad == 0) {
      gfx->setTextColor(C_OK, C_SCREEN); gfx->setCursor(40, 340);
      gfx->print("All 22 sites measured. SAVE stores them.");
      calButton(40,  400, 250, 56, "DISCARD", C_OPEN);
      calButton(470, 400, 290, 56, "SAVE",    C_OK);
      siteZeroWait(which);
      if (which == 1) {
        for (int i = 0; i < 22; i++) { char k[8]; snprintf(k, sizeof k, "z0_%d", i); prefs.putFloat(k, z[i]); zSiteZero[i] = z[i]; }
        prefs.putBool("z0ok", true); siteZeroValid = true;
      }
      return;
    }
    gfx->setTextColor(C_BAD, C_SCREEN); gfx->setCursor(40, 340);
    gfx->printf("%d site%s read open - check the cable, then RETRY.", bad, bad == 1 ? "" : "s");
    calButton(40,  400, 250, 56, "CANCEL", C_OPEN);
    calButton(470, 400, 290, 56, "RETRY",  C_WARN);
    siteZeroWait(which);
    if (which == 2) return;
  }
}

// ------------------------------------------------------------------ touch
//
// Sites are LEVEL-triggered on purpose: holding a finger on a hexagon just keeps
// selecting it, which is harmless and forgiving. The mode toggle is
// EDGE-triggered, because a toggle is the one action that undoes itself when
// repeated - one tap read twice used to put you back where you started.
bool     hdrWasDown = true;
uint32_t hdrLast    = 0;
bool muteWasDown = true, calWasDown = true;
uint32_t btnLast = 0;
bool     siteWasDown = true;
uint32_t siteLast = 0;
void touchService() {
  touch.read();
  bool hdrDown = false, muteDown = false, calDown = false, siteDown = false;
  if (touch.isTouched) {
    // Both panels inverted on both axes, but at different scales - the CYD's
    // controller carries the 480x272 config of the sibling panel sharing its
    // PCB, the Elecrow's is genuinely 800x480. Both fits are measured, five
    // points each, and live in mapTouch() in board_config.h.
    int tx, ty;
    mapTouch(touch.points[0].x, touch.points[0].y, tx, ty);
    if (ty < 60) hdrDown = true;
    else if (ty >= BTN_Y && ty < BTN_Y + BTN_H && tx >= BTN_MUTE_X && tx < BTN_MUTE_X + BTN_MUTE_W) muteDown = true;
    else if (ty >= BTN_Y && ty < BTN_Y + BTN_H && tx >= BTN_CAL_X  && tx < BTN_CAL_X  + BTN_CAL_W)  calDown  = true;
    else for (int i = 0; i < 22; i++) {
      int cx, cy; siteCentre(i, cx, cy);
      // 25, not the hexagon's 23: a little forgiveness for a gelled finger,
      // while staying inside the 51.6 px that separates Fz from GND.
      if (abs(tx - cx) < HEX_R && abs(ty - cy) < HEX_R) { focus = i; siteDown = true; }
    }
  }
  if (hdrDown && !hdrWasDown && millis() - hdrLast >= TAP_HOLDOFF_MS) {
    modeB = !modeB;
    modeSwitches(0, modeB, refsValid, false);
    modeSound(false);
    hdrLast = millis();
  }
  hdrWasDown = hdrDown;
  // Click on the press, not while the finger stays down: sites are selected
  // level-triggered (above), but one press is one click.
  if (siteDown && !siteWasDown && millis() - siteLast >= 150) {
    if (SOUND_ON_CLICK) audioPlay(SND_CLICK, SOUND_VOLUME, VOL_CLICK);
    siteLast = millis();
  }
  siteWasDown = siteDown;
  // Buttons are edge-triggered like the header: one tap, one action.
  if (muteDown && !muteWasDown && millis() - btnLast >= TAP_HOLDOFF_MS) {
    if (bridgeCount > 0) alarmMuted = !alarmMuted;     // tap again to un-mute
    btnLast = millis();
    bridgeSoundService();                              // stop (or restart) the alarm sound now
  }
  if (calDown && !calWasDown && millis() - btnLast >= TAP_HOLDOFF_MS) {
    recalRequested = true;
    btnLast = millis();
  }
  muteWasDown = muteDown; calWasDown = calDown;
}

// ------------------------------------------------------------------ setup / loop
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

// The self-calibration report and its buttons. Runs at start-up, and again
// whenever RECAL is tapped on the main screen (added 4 Nov 2026, so a
// recalibration no longer means pulling the USB cable or pressing RESET).
void calibrationScreen() {
  // Report before the map, and wait to be dismissed - PASS as well as FAIL.
  // The first PASS on this board (24 Oct 2026) went straight to the map: the
  // old PASS screen cleared on ANY touch, a pedal press or 8 seconds, and one
  // of those fired before anyone could read it. Now only a tap ON the
  // CONTINUE button (or the pedal) moves on, and nothing counts for the first
  // 600 ms, so a stray touch report at power-up cannot dismiss it. RETRY re-runs the whole calibration in
  // place, so a bent pin can be straightened and retested without a power
  // cycle - which is the loop you are actually in when something is wrong.
  for (;;) {
    drawCalReport();
    if (calRep.ok) {
      uint32_t t0 = millis();
      bool wd = true; uint32_t lf = 0; int tx, ty; int action = 0;
      for (;;) {                                 // no timeout
        bool tap = tapEdge(wd, lf, tx, ty);
        footPoll();
        bool armed = millis() - t0 >= 600;
        if (armed && tap && calHit(tx, ty, 470, 400, 290, 56)) { action = 1; break; }
        if (armed && tap && calHit(tx, ty, 40,  400, 250, 56)) { action = 2; break; }
        if (footShortPending) { footShortPending = false; if (armed) { action = 1; break; } }
        delay(30);
      }
      if (action == 2) { siteZeroScreen(); continue; }   // back to the report
      break;
    }
    bool again = false;
    bool wd = true; uint32_t lf = 0;            // re-armed for every report
    for (;;) {                                   // no timeout on a failure
      int tx, ty;
      if (tapEdge(wd, lf, tx, ty)) {
        bool retry = calHit(tx, ty, 40, 400, 250, 56);
        bool cont  = calHit(tx, ty, 470, 400, 290, 56);
        if (retry || cont) { again = retry; break; }
      }
      delay(30);
    }
    if (!again) break;
    measureVref();                       // conditions may have changed since boot
    selfCalibrate();
  }
}

// Repaint the head-map screen from scratch, e.g. after the calibration screen.
void rebuildMainScreen() {
  gfx->fillScreen(C_SCREEN);
  drawHeader();
  mapInit = false; panelInit = false; hdrAlarmShown = false;
  lastPanelName[0] = lastPanelVal[0] = lastModeTxt[0] = lastBrTxt[0] = 0;
  lastFootTxt[0] = lastDcTxt[0] = lastTempTxt[0] = lastHdrTxt[0] = 0;
  lastMuteState = -1; lastScanDrawn = -1;
  drawHeadMap(); drawFocus();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL, 400000);

  gfx->begin(); pinMode(GFX_BL, OUTPUT); digitalWrite(GFX_BL, HIGH);
  gfx->fillScreen(C_SCREEN); drawHeader();
  if      (i2cPresent(GT911_ADDR1)) touchAddr = GT911_ADDR1;   // 0x5D
  else if (i2cPresent(GT911_ADDR2)) touchAddr = GT911_ADDR2;   // 0x14
  touch.begin(touchAddr);
  Serial.printf("%s, touch at 0x%02X, firmware %s\n", BOARD_NAME, touchAddr, FW_VERSION);

  // Speaker: opening.wav is loaded and started first, so it plays as the screen
  // comes on; the other three load from the card while it plays.
  uint32_t openStart = 0;
  if (audioBegin()) {
    audioLoad(SND_OPENING, SOUND_MAX_SECONDS);
    if (SOUND_ON_OPENING && audioHas(SND_OPENING)) { audioPlay(SND_OPENING, SOUND_VOLUME, VOL_OPENING); openStart = millis(); }
    audioLoad(SND_CLICK,  SOUND_MAX_SECONDS);
    audioLoad(SND_MODE,   SOUND_MAX_SECONDS);
    audioLoad(SND_BRIDGE, SOUND_MAX_SECONDS);
  }

  bool ok = true;
  ok &= adcV.begin(0x48); ok &= adcI.begin(0x49);
  ok &= dac.begin(findDac());       // 0x62 Adafruit, 0x60 generic
  ok &= expA.begin_I2C(0x20); ok &= expB.begin_I2C(0x21);
  if (!ok) { gfx->setTextSize(3); gfx->setTextColor(C_BAD); gfx->setCursor(20, 200); gfx->print("I2C device missing - check J3 wiring"); while (1) delay(1000); }
  for (int p = 0; p < 16; p++) { expA.pinMode(p, OUTPUT); expB.pinMode(p, OUTPUT); }
  expB.pinMode(9, INPUT_PULLUP);      // GPB1 footswitch
  adcV.setDataRate(RATE_ADS1115_860SPS); adcI.setDataRate(RATE_ADS1115_860SPS);
  adsQuiet(ADS_V); adsQuiet(ADS_I);   // release ALRT in case a previous sketch left it in RDY mode
  delay(50);                          // and let the TIA recover from being held at ground
  muxSelect(0xFF, 0xFF);              // everything off
  modeSwitches(0, false, false, false);
  if (openStart && SOUND_WAIT_FOR_OPENING) {
    // The task reports the clip as playing within a buffer or two of the
    // request, so allow a moment for that before waiting on it to end.
    while (!audioFxBusy(SND_OPENING) && millis() - openStart < 300) delay(5);
    while (audioFxBusy(SND_OPENING)) {
      touch.read();
      if (touch.isTouched) { audioStopAll(); delay(50); break; }   // tap = skip
      delay(20);
    }
  }
  measureVref();
  prefs.begin("phantom", false);
  cal.a = prefs.getFloat("a", 1.0f); cal.b = prefs.getFloat("b", 0.0f);
  cal.rOffsetA = prefs.getFloat("ra", 0.0f); cal.rOffsetB = prefs.getFloat("rb", 200.0f);
  siteZeroValid = prefs.getBool("z0ok", false);
  for (int i = 0; i < 22; i++) { char k[8]; snprintf(k, sizeof k, "z0_%d", i); zSiteZero[i] = siteZeroValid ? prefs.getFloat(k, 0.0f) : 0.0f; }
  if (!selfCalibrate()) Serial.println("Self-calibration failed: check R20-R22 and the mux banks");

  calibrationScreen();
  for (int i = 0; i < 22; i++) zSite[i] = Z_OPEN;
  rebuildMainScreen();
}

// Pass / fail beeps for the focused site. The state is per focus: tapping a
// different site never beeps by itself - only a change in the SAME site's
// reading does - and the fail edge has hysteresis (5.00 k in, 5.25 k out).
int  beepFocus = -1;
bool beepPassed = false;
void thresholdBeeps() {
  float z = zSite[focus];
  if (focus != beepFocus) {                       // new focus: adopt its state silently
    beepFocus = focus;
    beepPassed = z <= 5000.0f;
    return;
  }
  if (!beepPassed && z <= 5000.0f) {
    beepPassed = true;
    if (BEEP_ON_PASS) chirp(120, 0, modeB, refsValid);
  } else if (beepPassed && z > BEEP_FAIL_ABOVE) {
    beepPassed = false;
    if (BEEP_ON_FAIL) {                           // lower and doubled: unmistakably not the pass chime
      uint16_t hz = beepHz() / 2;
      chirpHz(hz, 70, 0, modeB, refsValid); delay(60);
      chirpHz(hz, 70, 0, modeB, refsValid);
    }
  }
}

uint8_t sweep = 0;
void loop() {
  // 1. the focused site, 4 periods (128 ms)
  zSite[focus] = siteZ(focus, 4);
  focusDcMv = lastDcMv;
  footPoll();
  // 2. one background site, 2 periods
  zSite[sweep] = siteZ(sweep, 2);
  sweep = (sweep + 1) % 22;
  scanDone = sweep;
  if (sweep == 0) {                     // all 22 measured: a full scan is complete
    uint32_t now = millis();
    if (scanStartMs) lastScanSecs = (now - scanStartMs + 500) / 1000;
    scanStartMs = now;
    scanCompleteMs = now;
  }
  if (sweep == 0 && modeB) {
    // Still needed: the decomposition subtracts the two ears from each site.
    zA1A2 = applyCal(measureZ(CH_A1, CH_A1, MODE_B_A2_MASK, 8), true);   // A1 driven, returns via A2
  }
  refsValid = refsOk();               // each ear under 5 k, from its own reading
  footPoll();
  // 3. touch: tap a hex to pin focus; tap header to toggle mode
  touchService();
  footPoll();
  // 4. salt-bridge sweep, a couple of pairs per pass
  bool newBridge = brStep(BR_PER_PASS);
  if (newBridge && BEEP_ON_BRIDGE && !bridgeWav()) {
    // Three short bursts, deliberately unlike the single chime that means a site
    // has passed. A student should be able to tell good news from bad without
    // looking up from the electrode they are working on.
    for (uint8_t k = 0; k < 3; k++) { chirp(70, 0, modeB, refsValid); delay(70); }
  }
  alarmService(newBridge);            // then one beep a second until it clears or is muted
  if (recalRequested) {
    recalRequested = false;
    audioStopAll(); bridgeLoopOn = false;   // quiet while calibrating; resumes if still bridged
    measureVref();
    selfCalibrate();
    calibrationScreen();
    rebuildMainScreen();
    return;
  }
  footPoll();
  footService();        // act on a completed short press, between measurements
  // 5. threshold beeps for the focused site (see BEEP_ON_PASS / BEEP_ON_FAIL)
  thresholdBeeps();
  // 6. redraw at ~5 Hz, blink bridged sites at ~1.7 Hz
  if (millis() - brFlashMs > 300) { brFlash = !brFlash; brFlashMs = millis(); }
  if (millis() - lastDraw > 200) { drawAlarmHeader(); drawScanBar(); drawHeadMap(); drawFocus(); drawTemp(); lastDraw = millis(); }
}
