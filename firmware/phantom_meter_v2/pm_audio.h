/* pm_audio.h - WAV sound effects through the Elecrow CrowPanel 7.0's speaker port
 *
 * Added 22 Nov 2026. Elecrow only (PM_BOARD 2): the CrowPanel has an NS4168
 * class-D amplifier on its I2S bus driving the 2-pin PH2.0 SPK socket, and a
 * microSD (TF) slot on SPI. On the CYD (PM_BOARD 1) this whole file compiles to
 * nothing and the piezo on the carrier board keeps doing every sound.
 *
 *   I2S:   BCLK GPIO42   LRCLK GPIO18   DATA GPIO17     (NS4168, no MCLK needed)
 *   SD:    CS GPIO10     MOSI GPIO11    SCK GPIO12      MISO GPIO13
 *
 * HOW IT WORKS
 *   At start-up the four WAV files are read from the card's top folder into
 *   PSRAM, converted to 16-bit mono on the way in. Playback then runs from
 *   memory in its own task on core 0, so the SD card is never touched while
 *   measuring, a click starts within ~25 ms of the tap, and nothing in the
 *   measurement loop ever waits for audio.
 *
 *   Two voices are mixed: one for one-shot effects (opening, click, mode
 *   change) and one for the looping salt-bridge alarm. So a click while the
 *   alarm is sounding is heard on top of it rather than cutting it off. Each
 *   clip is resampled to the 44.1 kHz output with linear interpolation, so
 *   files at any common sample rate work and can be mixed.
 *
 * FILES ACCEPTED
 *   Uncompressed WAV ("RIFF/WAVE", PCM): 8, 16, 24 or 32-bit integer, or 32-bit
 *   float; mono or stereo (stereo is mixed down to mono - the SPK port has one
 *   speaker); 8 kHz to 96 kHz. NOT accepted: MP3 or AAC renamed to .wav, ADPCM,
 *   or other compressed WAV. In Audacity: File > Export > Export as WAV,
 *   "Signed 16-bit PCM" always works.
 *
 *   A file longer than SOUND_MAX_SECONDS, or bigger than the PSRAM left over,
 *   is cut short rather than refused, and the calibration screen says so.
 *
 * The WAV parser and the mixer at the top of this file are plain C++ with no
 * ESP32 dependency, so they can be tested on a desktop compiler: define
 * PM_AUDIO_HOST_TEST before including it.
 */
#pragma once
#include <stdint.h>
#include <string.h>

namespace pmaudio {

// ------------------------------------------------------------------ portable core
enum ClipStatus : uint8_t {
  CLIP_NONE = 0,      // not loaded yet / no card
  CLIP_OK,            // loaded in full
  CLIP_TRUNCATED,     // loaded, but cut to the length limit or the memory left
  CLIP_MISSING,       // no such file on the card
  CLIP_BADFORMAT,     // not an uncompressed PCM / float WAV
  CLIP_NOMEM,         // could not get any PSRAM for it
  CLIP_READERR        // file found but nothing could be read
};

struct Clip {
  int16_t  *pcm  = nullptr;   // mono samples at 'rate'
  uint32_t  n    = 0;         // number of samples
  uint32_t  rate = 0;         // source sample rate, Hz
  uint8_t   status = CLIP_NONE;
  // As found in the file, for the status line.
  uint16_t  fmtTag = 0, channels = 0, bits = 0;
  float     seconds = 0;      // length actually loaded
};

static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

// One frame (all channels) -> one mono 16-bit sample.
static inline int16_t frameToMono(const uint8_t *p, uint16_t tag, uint16_t ch, uint16_t bits) {
  int32_t sum = 0;
  const uint16_t bps = bits / 8;
  for (uint16_t c = 0; c < ch; c++, p += bps) {
    int32_t s;
    if (tag == 3) {                                  // IEEE float, 32-bit
      float f; memcpy(&f, p, 4);
      if (!(f == f)) f = 0;                          // NaN
      if (f > 1.0f) f = 1.0f; else if (f < -1.0f) f = -1.0f;
      s = (int32_t)(f * 32767.0f);
    } else switch (bits) {
      case 8:  s = ((int32_t)p[0] - 128) << 8; break;          // 8-bit WAV is unsigned
      case 16: s = (int16_t)rd16(p); break;
      case 24: s = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 16; break;
      default: s = (int32_t)rd32(p) >> 16; break;               // 32-bit integer
    }
    sum += s;
  }
  return (int16_t)(sum / (int32_t)ch);
}

// Reads a WAV through any file object offering read(uint8_t*, size_t),
// seek(uint32_t), position() and size() - Arduino's File does. 'alloc' supplies
// memory for the samples (ps_malloc on the ESP32), 'maxSamples' caps the length,
// and 'avail' (bytes) is how much memory may be asked for at most.
template <class F>
uint8_t wavLoad(F &f, Clip &c, uint32_t maxSeconds, uint32_t avail, void *(*alloc)(size_t)) {
  uint8_t h[40];
  const uint32_t fsize = (uint32_t)f.size();
  if ((uint32_t)f.read(h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return c.status = CLIP_BADFORMAT;
  uint16_t tag = 0, ch = 0, bits = 0, align = 0; uint32_t rate = 0;
  uint32_t dataPos = 0, dataLen = 0; bool haveFmt = false, haveData = false;
  uint32_t pos = 12;
  while (pos + 8 <= fsize && !(haveFmt && haveData)) {
    f.seek(pos);
    if ((uint32_t)f.read(h, 8) != 8) break;
    uint32_t len = rd32(h + 4);
    if (!memcmp(h, "fmt ", 4)) {
      uint32_t k = len < 40 ? len : 40;
      if (k < 16 || (uint32_t)f.read(h, k) != k) return c.status = CLIP_BADFORMAT;
      tag = rd16(h); ch = rd16(h + 2); rate = rd32(h + 4); align = rd16(h + 12); bits = rd16(h + 14);
      if (tag == 0xFFFE && k >= 26) tag = rd16(h + 24);   // WAVE_FORMAT_EXTENSIBLE: sub-format GUID starts with the real tag
      haveFmt = true;
    } else if (!memcmp(h, "data", 4)) {
      dataPos = pos + 8;
      dataLen = (len == 0 || len == 0xFFFFFFFFu || dataPos + len > fsize) ? fsize - dataPos : len;  // streamed / damaged headers
      haveData = true;
    }
    pos += 8 + len + (len & 1);                       // chunks are padded to even length
    if (len > fsize) break;
  }
  c.fmtTag = tag; c.channels = ch; c.bits = bits;
  if (!haveFmt || !haveData) return c.status = CLIP_BADFORMAT;
  bool pcmOk   = (tag == 1) && (bits == 8 || bits == 16 || bits == 24 || bits == 32);
  bool floatOk = (tag == 3) && bits == 32;
  if (!(pcmOk || floatOk) || ch < 1 || ch > 8 || rate < 8000 || rate > 96000 || align != ch * (bits / 8))
    return c.status = CLIP_BADFORMAT;

  uint32_t frames = dataLen / align;
  bool cut = false;
  uint64_t capT = (uint64_t)maxSeconds * rate;
  if (frames > capT) { frames = (uint32_t)capT; cut = true; }
  if ((uint64_t)frames * 2 > avail) { frames = avail / 2; cut = true; }
  if (frames == 0) return c.status = (dataLen < align) ? CLIP_READERR : CLIP_NOMEM;
  int16_t *pcm = (int16_t *)alloc((size_t)frames * 2);
  if (!pcm) return c.status = CLIP_NOMEM;

  f.seek(dataPos);
  static uint8_t buf[4096];
  const uint32_t perRead = sizeof(buf) / align;     // whole frames only
  uint32_t got = 0;
  while (got < frames) {
    uint32_t want = frames - got; if (want > perRead) want = perRead;
    int r = f.read(buf, want * align);
    if (r <= 0) break;
    uint32_t fr = (uint32_t)r / align;
    for (uint32_t i = 0; i < fr; i++) pcm[got + i] = frameToMono(buf + i * align, tag, ch, bits);
    got += fr;
    if (fr < want) break;                            // file ended early
  }
  if (got == 0) return c.status = CLIP_READERR;      // (memory is kept: tiny, and never reused)
  c.n = got; c.rate = rate; c.seconds = (float)got / rate;
  c.pcm = pcm;                                       // publish last: the audio task checks pcm
  return c.status = (cut || got < frames) ? CLIP_TRUNCATED : CLIP_OK;
}

// ------------------------------------------------------------------ mixer
struct Voice {
  const Clip *c = nullptr;
  uint32_t idx = 0, frac = 0, step = 0;   // position in 16.16 fixed point
  int32_t  gain = 0;                      // Q12: 4096 = as recorded
  int32_t  fadeStep = 0;                  // > 0 while fading out
  bool     loop = false, active = false;
  int8_t   id = -1;
};

inline void voiceStart(Voice &v, const Clip *c, int8_t id, int32_t gainQ12, bool loop, uint32_t outRate) {
  v.active = false;
  if (!c || !c->pcm || !c->n || gainQ12 <= 0) return;
  v.c = c; v.id = id; v.idx = 0; v.frac = 0;
  v.step = (uint32_t)(((uint64_t)c->rate << 16) / outRate);
  v.gain = gainQ12; v.fadeStep = 0; v.loop = loop; v.active = true;
}

// A 10 ms fade instead of an abrupt stop: cutting a waveform mid-cycle clicks.
inline void voiceStop(Voice &v, uint32_t outRate) {
  if (!v.active) return;
  int32_t frames = (int32_t)(outRate / 100); if (frames < 1) frames = 1;
  v.fadeStep = v.gain / frames + 1;
}

// Renders 'frames' stereo frames (L = R) into out[2*frames]. Returns the number
// of voices still active afterwards.
inline int mixRender(Voice *vs, int nv, int16_t *out, int frames) {
  for (int k = 0; k < frames; k++) {
    int32_t acc = 0;
    for (int j = 0; j < nv; j++) {
      Voice &v = vs[j];
      if (!v.active) continue;
      const int16_t *p = v.c->pcm; const uint32_t n = v.c->n;
      int32_t a = p[v.idx];
      int32_t b = (v.idx + 1 < n) ? p[v.idx + 1] : (v.loop ? p[0] : a);
      int32_t s = a + (((b - a) * (int32_t)v.frac) >> 16);
      acc += (s * v.gain) >> 12;
      if (v.fadeStep) { v.gain -= v.fadeStep; if (v.gain <= 0) { v.active = false; continue; } }
      v.frac += v.step; v.idx += v.frac >> 16; v.frac &= 0xFFFF;
      if (v.idx >= n) {
        if (v.loop) v.idx %= n;
        else v.active = false;
      }
    }
    if (acc > 32767) acc = 32767; else if (acc < -32768) acc = -32768;
    out[2 * k] = out[2 * k + 1] = (int16_t)acc;
  }
  int live = 0;
  for (int j = 0; j < nv; j++) live += vs[j].active;
  return live;
}

} // namespace pmaudio

// ==================================================================== ESP32 part
#ifndef PM_AUDIO_HOST_TEST
#if PM_BOARD == 2
#define PM_HAS_SPEAKER 1
#include <SPI.h>
#include <SD.h>
#include "driver/i2s.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"

const uint8_t PIN_I2S_BCLK = 42, PIN_I2S_LRCK = 18, PIN_I2S_DOUT = 17;
const uint8_t PIN_SD_CS = 10, PIN_SD_MOSI = 11, PIN_SD_SCK = 12, PIN_SD_MISO = 13;
const uint32_t AUDIO_RATE = 44100;

enum SoundId : uint8_t { SND_OPENING = 0, SND_CLICK, SND_MODE, SND_BRIDGE, SND_COUNT };
static const char *const SND_FILE[SND_COUNT] = { "/opening.wav", "/click.wav", "/modechange.wav", "/saltbridge.wav" };

namespace pmaudio {
Clip     clips[SND_COUNT];
bool     sdOk = false, i2sOk = false;
QueueHandle_t q = nullptr;
volatile int8_t fxPlaying = -1, loopPlaying = -1;   // what the audio task is sounding now

struct Cmd { uint8_t op; int8_t id; int32_t gain; };   // op: 1 fx, 2 loop on, 3 loop off, 4 all off
enum { OP_FX = 1, OP_LOOP_ON, OP_LOOP_OFF, OP_ALL_OFF };

void audioTask(void *) {
  static Voice v[2];                                 // [0] one-shot effects, [1] looping alarm
  static int16_t buf[256 * 2];
  Cmd cmd;
  for (;;) {
    bool any = v[0].active || v[1].active;
    // Idle: sleep on the queue. Playing: just drain it between buffers.
    TickType_t wait = any ? 0 : portMAX_DELAY;
    while (xQueueReceive(q, &cmd, wait) == pdTRUE) {
      wait = 0;
      switch (cmd.op) {
        case OP_FX:       voiceStart(v[0], &clips[cmd.id], cmd.id, cmd.gain, false, AUDIO_RATE); break;
        case OP_LOOP_ON:  if (!(v[1].active && v[1].id == cmd.id && !v[1].fadeStep))
                            voiceStart(v[1], &clips[cmd.id], cmd.id, cmd.gain, true, AUDIO_RATE);
                          break;
        case OP_LOOP_OFF: voiceStop(v[1], AUDIO_RATE); break;
        case OP_ALL_OFF:  voiceStop(v[0], AUDIO_RATE); voiceStop(v[1], AUDIO_RATE); break;
      }
    }
    if (!(v[0].active || v[1].active)) { fxPlaying = loopPlaying = -1; continue; }
    mixRender(v, 2, buf, 256);
    fxPlaying   = v[0].active ? v[0].id : -1;
    loopPlaying = v[1].active ? v[1].id : -1;
    size_t wr;
    i2s_write(I2S_NUM_0, buf, sizeof buf, &wr, portMAX_DELAY);   // paces the task to the output rate
    // When the last voice ends, the DMA buffers run out and tx_desc_auto_clear
    // sends silence, so the amplifier is never left holding a stale sample.
  }
}

void *psAlloc(size_t n) { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
} // namespace pmaudio

// Mount the card and start the I2S output. Safe to call with no card: sound
// is then simply off, and the piezo covers every event as before.
bool audioBegin() {
  using namespace pmaudio;
  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  sdOk = SD.begin(PIN_SD_CS, SPI, 20000000) || SD.begin(PIN_SD_CS, SPI, 4000000);  // some cards want it slow
  Serial.printf("audio: SD card %s\n", sdOk ? "mounted" : "NOT FOUND");

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = AUDIO_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;   // same sample on both: the NS4168 picks one
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = 0;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;                             // 4 x 256 frames = 23 ms of latency
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  cfg.fixed_mclk = 0;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num = PIN_I2S_LRCK;
  pins.data_out_num = PIN_I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  i2sOk = i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) == ESP_OK && i2s_set_pin(I2S_NUM_0, &pins) == ESP_OK;
  if (i2sOk) i2s_zero_dma_buffer(I2S_NUM_0);
  Serial.printf("audio: I2S %s\n", i2sOk ? "running" : "FAILED");
  if (!i2sOk) return false;
  q = xQueueCreate(8, sizeof(Cmd));
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, NULL, 3, NULL, 0);   // core 0: the sketch runs on core 1
  return sdOk;
}

// Load one file into PSRAM. Leaves 512 kB of PSRAM free for everything else.
void audioLoad(SoundId id, uint32_t maxSeconds) {
  using namespace pmaudio;
  Clip &c = clips[id];
  if (!sdOk) { c.status = CLIP_NONE; return; }
  File f = SD.open(SND_FILE[id], FILE_READ);
  if (!f) { c.status = CLIP_MISSING; Serial.printf("audio: %s not found\n", SND_FILE[id]); return; }
  size_t freePs = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  uint32_t avail = freePs > 524288 ? (uint32_t)(freePs - 524288) : 0;
  uint32_t t0 = millis();
  wavLoad(f, c, maxSeconds, avail, psAlloc);
  f.close();
  Serial.printf("audio: %s  status %u  tag %u  %u ch  %u bit  %lu Hz  %.2f s  (%lu ms)\n",
                SND_FILE[id], c.status, c.fmtTag, c.channels, c.bits, (unsigned long)c.rate,
                c.seconds, (unsigned long)(millis() - t0));
}

bool audioHas(SoundId id) { return pmaudio::i2sOk && pmaudio::clips[id].pcm != nullptr; }

static inline int32_t volQ12(uint16_t master, uint16_t each) { return (int32_t)master * each * 4096 / 10000; }

void audioPlay(SoundId id, uint16_t master, uint16_t each) {
  if (!audioHas(id)) return;
  pmaudio::Cmd c = { pmaudio::OP_FX, (int8_t)id, volQ12(master, each) };
  xQueueSend(pmaudio::q, &c, 0);
}
void audioLoop(SoundId id, bool on, uint16_t master, uint16_t each) {
  if (!pmaudio::i2sOk) return;
  if (on && !audioHas(id)) return;
  pmaudio::Cmd c = { on ? pmaudio::OP_LOOP_ON : pmaudio::OP_LOOP_OFF, (int8_t)id, volQ12(master, each) };
  xQueueSend(pmaudio::q, &c, 0);
}
void audioStopAll() {
  if (!pmaudio::i2sOk) return;
  pmaudio::Cmd c = { pmaudio::OP_ALL_OFF, -1, 0 };
  xQueueSend(pmaudio::q, &c, 0);
}
bool audioFxBusy(SoundId id) { return pmaudio::fxPlaying == (int8_t)id; }

// One line for the calibration screen, e.g.
//   "speaker: opening 4.2s  click 0.1s  modechange MISSING  saltbridge 1.5s"
void audioStatusLine(char *out, size_t n) {
  using namespace pmaudio;
  if (!i2sOk) { snprintf(out, n, "speaker: I2S output failed to start - piezo only"); return; }
  if (!sdOk)  { snprintf(out, n, "speaker: no TF card found - piezo only"); return; }
  size_t k = snprintf(out, n, "speaker:");
  for (int i = 0; i < SND_COUNT && k < n; i++) {
    const char *name = SND_FILE[i] + 1;              // without the '/'
    char nm[16]; snprintf(nm, sizeof nm, "%s", name);
    char *dot = strchr(nm, '.'); if (dot) *dot = 0;
    const Clip &c = clips[i];
    switch (c.status) {
      case CLIP_OK:        k += snprintf(out + k, n - k, "  %s %.1fs", nm, c.seconds); break;
      case CLIP_TRUNCATED: k += snprintf(out + k, n - k, "  %s CUT TO %.1fs", nm, c.seconds); break;
      case CLIP_MISSING:   k += snprintf(out + k, n - k, "  %s MISSING", nm); break;
      case CLIP_BADFORMAT: k += snprintf(out + k, n - k, "  %s NOT PCM WAV", nm); break;
      case CLIP_NOMEM:     k += snprintf(out + k, n - k, "  %s NO MEMORY", nm); break;
      case CLIP_READERR:   k += snprintf(out + k, n - k, "  %s READ ERROR", nm); break;
      default:             k += snprintf(out + k, n - k, "  %s -", nm); break;
    }
  }
}
// True if any file failed: the line is then drawn in the warning colour.
bool audioAnyProblem() {
  using namespace pmaudio;
  if (!i2sOk || !sdOk) return true;
  for (int i = 0; i < SND_COUNT; i++) if (clips[i].status != CLIP_OK) return true;
  return false;
}

#else
// CYD (PM_BOARD 1): no speaker path. These stubs let the sketch call the same
// functions on both boards; every sound then falls back to the piezo.
#define PM_HAS_SPEAKER 0
enum SoundId : uint8_t { SND_OPENING = 0, SND_CLICK, SND_MODE, SND_BRIDGE, SND_COUNT };
inline bool audioBegin() { return false; }
inline void audioLoad(SoundId, uint32_t) {}
inline bool audioHas(SoundId) { return false; }
inline void audioPlay(SoundId, uint16_t, uint16_t) {}
inline void audioLoop(SoundId, bool, uint16_t, uint16_t) {}
inline void audioStopAll() {}
inline bool audioFxBusy(SoundId) { return false; }
inline void audioStatusLine(char *out, size_t n) { if (n) out[0] = 0; }
inline bool audioAnyProblem() { return false; }
#endif  // PM_BOARD == 2
#endif  // PM_AUDIO_HOST_TEST
