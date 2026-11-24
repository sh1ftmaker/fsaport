// The DSP's mixing, done on the CPU.
//
// On the GameCube JAudio hands the DSP a table of 64 voice blocks
// (JASDsp::TChannel, in main RAM) and, once per audio frame, two output
// buffers; the DSP then renders the frame one 80-sample subframe at a time,
// mailing the CPU after each so JAudio can update the voices for the next.
// Here JASDriver::finishDSPFrame (FSA_PORT) calls fsa_dsp_render_subframe()
// after each of those updates instead, and this file does the DSP's part:
// read each active voice's wave data from ARAM (ADPCM or PCM), resample it to
// 32 kHz by its pitch, run its filters, apply its mixer volumes and add it to
// the left/right buffers, writing back the feedback fields JAudio reads
// (finished flag, remaining samples, stream position, reset flag).
//
// Ported from Dusklight's CPU DSP (src/dusk/audio/DuskDsp.cpp and Adpcm.cpp,
// the Twilight Princess PC port, CC0), which drives the same JAudio voice
// layout; adapted to FSA's JAudio and to s16 output, without its reverb and
// surround extras. The ADPCM tables follow vgmtrans / JaiSeqX, as Dusklight's.
#include "platform.h"

#include <emscripten.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" void* ARGetStorageAddress();

namespace {

constexpr int kSampleRate = 32000;
constexpr int kSubframe = 0x50;   // samples per DSP subframe
constexpr int kChannels = 64;
constexpr int kOutputs = 6;
constexpr f32 kChannelLevel = 0x2ee0;  // JASDriver::getChannelLevel_dsp()

// The DSP's view of JASDsp::TChannel (game/libs/sdk/include/JSystem/JAudio/
// System/JASDsp.h): the same 0x180 bytes, with Dusklight's names for the
// fields the decomp leaves numbered.
struct Mixer {
  u16 bus;
  u16 target;
  u16 current;
  u16 progress;
};
struct Voice {
  u16 active;        // 0x000
  u16 finished;      // 0x002  written by the DSP
  u16 pitch;         // 0x004  0x1000 = 1:1 at 32 kHz
  u16 _06;
  u16 reset;         // 0x008  set by playStart, cleared by the DSP
  u16 _0a;
  s16 pause;         // 0x00C
  u16 _0e;
  Mixer mixer[kOutputs];  // 0x010
  u8 _40[0x10];
  u16 amPanDolby;    // 0x050
  u16 amFxMix;       // 0x052
  u16 amInitVolume;  // 0x054
  u16 amVolume;      // 0x056
  u16 amSet;         // 0x058
  u8 _5a[6];
  u16 _60;
  u16 _62;
  u16 samplesPerBlock;  // 0x064
  u16 _66;
  u32 samplePosition;   // 0x068
  u32 _6c;
  u32 aramPosition;     // 0x070  written by the DSP
  u32 samplesLeft;      // 0x074  written by the DSP and by JASAramStream
  u8 _78[0x100 - 0x78];
  u16 bytesPerBlock;    // 0x100  (oscillator type when waveAram is 0)
  u16 loop;             // 0x102
  s16 loopLast;         // 0x104
  s16 loopPenult;       // 0x106
  u16 filterMode;       // 0x108
  u16 forcedStop;       // 0x10A
  u32 _10c;
  u32 loopStart;        // 0x110
  u32 endSample;        // 0x114
  u32 waveAram;         // 0x118
  u32 sampleCount;      // 0x11C
  s16 fir[8];           // 0x120
  u8 _130[0x18];
  s16 iir[8];           // 0x148
  u8 _158[0x180 - 0x158];
};
static_assert(sizeof(Voice) == 0x180, "TChannel layout");
static_assert(offsetof(Voice, amPanDolby) == 0x50, "TChannel layout");
static_assert(offsetof(Voice, samplesPerBlock) == 0x64, "TChannel layout");
static_assert(offsetof(Voice, aramPosition) == 0x70, "TChannel layout");
static_assert(offsetof(Voice, bytesPerBlock) == 0x100, "TChannel layout");
static_assert(offsetof(Voice, loopStart) == 0x110, "TChannel layout");
static_assert(offsetof(Voice, fir) == 0x120, "TChannel layout");
static_assert(offsetof(Voice, iir) == 0x148, "TChannel layout");

constexpr u16 kBusLeft = 0x0D00;
constexpr u16 kBusRight = 0x0D60;

// Decoded-but-unplayed samples and filter state the DSP keeps per voice.
struct VoiceState {
  s16 hist1, hist0;  // ADPCM history: hist1 = older, hist0 = newer
  f32 prevVolume[2]; // NaN after a reset: start from the voice's init volume
  static constexpr int kDecodeBuf = 2048;
  s16 decode[kDecodeBuf];
  int decodeCount;
  f32 resamplePos;
  s16 resamplePrev;
  u16 oscPhase;
  f32 lpOut, lpIn;
  f32 bIn1, bIn2, bOut1, bOut2;
};

Voice* g_voices = nullptr;
VoiceState g_state[kChannels];
s16* g_outL = nullptr;
s16* g_outR = nullptr;
u32 g_subframes = 0;
f32 g_gain = 1.0f;
s16 g_evolving[64];
bool g_warned[16];

// Stats for the page's audio meter (fsa_audio_stats).
u32 g_framesRendered = 0;
u32 g_voicesPlayed = 0;
double g_renderMs = 0;  // time spent mixing

inline s16 clamp16(s32 v) { return (s16)std::clamp<s32>(v, -0x8000, 0x7FFF); }

const s16 kCoef0[16] = {0,      0x0800, 0,      0x0400, 0x1000, 0x0e00, 0x0c00, 0x1200,
                        0x1068, 0x12c0, 0x1400, 0x0800, 0x0400, (s16)0xfc00, (s16)0xfc00, (s16)0xf800};
const s16 kCoef1[16] = {0,      0,      0x0800, 0x0400, (s16)0xf800, (s16)0xfa00, (s16)0xfc00, (s16)0xf600,
                        (s16)0xf738, (s16)0xf704, (s16)0xf400, (s16)0xf800, (s16)0xfc00, 0x0400, 0, 0};

// JAudio ADPCM: frames of a header byte (scale << 4 | predictor) and 16 4-bit
// (9-byte frames) or 2-bit (5-byte frames) samples.
void decodeAdpcm(const u8* src, size_t bytes, int frameBytes, s16* pcm, size_t count, s16& hist1, s16& hist0) {
  s16* end = pcm + count;
  int bits = frameBytes == 9 ? 4 : 2;
  for (size_t i = 0; i + frameBytes <= bytes; i += frameBytes) {
    u8 header = src[i];
    s32 scale = 1 << (header >> 4);
    s16 c0 = kCoef0[header & 0xF], c1 = kCoef1[header & 0xF];
    for (int s = 0; s < 16; s++) {
      s32 v;
      if (bits == 4) {
        u8 b = src[i + 1 + s / 2];
        v = (s8)((s & 1 ? b << 4 : b) & 0xF0) >> 4;
        v = (v * scale) << 11;
      } else {
        u8 b = src[i + 1 + s / 4];
        v = (s8)((b << ((s & 3) * 2)) & 0xC0) >> 6;
        v = (v * scale) << 13;
      }
      s16 sample = clamp16((v + c0 * hist0 + c1 * hist1) >> 11);
      hist1 = hist0;
      hist0 = sample;
      *pcm++ = sample;
      if (pcm == end)
        return;
    }
  }
}

inline u32 blockBytes(const Voice& v) {
  if (v.samplesPerBlock == 1)
    return v.bytesPerBlock == 16 ? 2 : 1;
  return v.bytesPerBlock;
}

inline bool knownFormat(const Voice& v) {
  if (v.samplesPerBlock == 16)
    return v.bytesPerBlock == 9 || v.bytesPerBlock == 5;
  if (v.samplesPerBlock == 1)
    return v.bytesPerBlock == 16 || v.bytesPerBlock == 8;
  return false;
}

inline u32 samplesToBytes(const Voice& v, u32 samples) {
  if (samples % v.samplesPerBlock)
    samples += v.samplesPerBlock;
  return samples / v.samplesPerBlock * blockBytes(v);
}

int g_logStarts = 0;  // voice starts left to log (fsa_debug_dsp_starts)

void resetVoice(Voice& v, VoiceState& st) {
  if (g_logStarts > 0) {
    g_logStarts--;
    platform_log("[audio] start v%02d t=%u fmt %u/%u aram %06x end %u loop %u pitch %04x mix %04x:%u %04x:%u am %u\n",
                 (int)(&v - g_voices), g_framesRendered, v.samplesPerBlock, v.bytesPerBlock, v.waveAram, v.endSample, v.loop,
                 v.pitch, v.mixer[0].bus, v.mixer[0].current, v.mixer[1].bus, v.mixer[1].current, v.amSet);
  }
  v.samplesLeft = v.endSample - v.samplePosition;
  st.hist0 = st.hist1 = 0;
  st.decodeCount = 0;
  st.resamplePos = 0;
  st.resamplePrev = 0;
  st.oscPhase = 0;
  st.lpOut = st.lpIn = 0;
  st.bIn1 = st.bIn2 = st.bOut1 = st.bOut2 = 0;
  st.prevVolume[0] = st.prevVolume[1] = NAN;
  v.reset = 0;
}

void readSamples(const Voice& v, VoiceState& st, const u8* data, size_t bytes, s16* pcm, size_t count) {
  if (v.samplesPerBlock == 1) {
    if (v.bytesPerBlock == 16) {
      for (size_t i = 0; i < count; i++)
        pcm[i] = (s16)(data[i * 2] << 8 | data[i * 2 + 1]);  // big-endian in ARAM
    } else {
      for (size_t i = 0; i < count; i++)
        pcm[i] = (s16)((s8)data[i] << 8);
    }
  } else {
    decodeAdpcm(data, bytes, v.bytesPerBlock, pcm, count, st.hist1, st.hist0);
  }
}

// Decode one contiguous run of the voice's samples; returns how many landed in out.
int readChunk(Voice& v, VoiceState& st, int wanted, s16* out, int outSize) {
  const u8* aram = (const u8*)ARGetStorageAddress() + v.waveAram;
  u32 pos = v.endSample - v.samplesLeft;
  u32 skip = pos % v.samplesPerBlock;
  if (skip) {
    // Mid-block (after a loop): decode from the block start, drop the head.
    wanted += (int)skip;
    pos -= skip;
    v.samplesLeft += skip;
    v.samplePosition -= skip;
  }
  wanted = (wanted + v.samplesPerBlock - 1) / v.samplesPerBlock * v.samplesPerBlock;
  u32 n = std::min(v.samplesLeft, (u32)wanted);
  s16 tmp[VoiceState::kDecodeBuf + 32];
  n = std::min<u32>(n, sizeof(tmp) / sizeof(tmp[0]));
  memset(tmp, 0, n * sizeof(s16));
  readSamples(v, st, aram + samplesToBytes(v, pos), samplesToBytes(v, n), tmp, n);
  v.samplesLeft -= n;
  v.samplePosition += n;
  int got = std::min((int)n - (int)skip, outSize);
  if (got > 0)
    memcpy(out, tmp + skip, got * sizeof(s16));
  return std::max(got, 0);
}

void fillDecode(Voice& v, VoiceState& st, int needed) {
  for (int guard = 0; st.decodeCount < needed && guard < 64; guard++) {
    if (v.samplesLeft == 0) {
      if (!v.loop)
        break;
      v.samplesLeft = v.endSample - v.loopStart;
      v.samplePosition = v.loopStart;
      st.hist1 = v.loopPenult;
      st.hist0 = v.loopLast;
      if (v.samplesLeft == 0)
        break;
    }
    int space = VoiceState::kDecodeBuf - st.decodeCount;
    if (space == 0)
      break;
    st.decodeCount += readChunk(v, st, std::min(space, needed - st.decodeCount), st.decode + st.decodeCount, space);
  }
  v.aramPosition = v.waveAram + samplesToBytes(v, v.samplePosition);
}

struct Vol {
  f32 target, init;
};

Vol outputVolume(const Voice& v, int side) {
  u16 vol, init;
  f32 pan = 1;
  if (v.amSet) {
    vol = v.amVolume;
    init = v.amInitVolume;
    f32 p = (f32)(v.amPanDolby >> 8) / 127.0f;
    pan = side == 0 ? 1 - p : p;
  } else {
    u16 bus = side == 0 ? kBusLeft : kBusRight;
    const Mixer* m = nullptr;
    for (const Mixer& mx : v.mixer)
      if (mx.bus == bus) {
        m = &mx;
        break;
      }
    if (!m)
      return {0, 0};
    vol = m->target;
    init = m->current;
  }
  return {vol / kChannelLevel * pan, init / kChannelLevel * pan};
}

void mixOut(const Voice& v, VoiceState& st, const f32* in, f32* acc[2]) {
  for (int side = 0; side < 2; side++) {
    Vol vol = outputVolume(v, side);
    f32& prev = st.prevVolume[side];
    if (std::isnan(prev))
      prev = vol.init;
    if (prev == 0 && vol.target == 0)
      continue;
    f32 step = (vol.target - prev) / kSubframe;
    f32 g = prev;
    for (int i = 0; i < kSubframe; i++, g += step)
      acc[side][i] += in[i] * g;
    prev = vol.target;
  }
}

void updateEvolving() {
  static bool init = false;
  if (!init) {
    g_evolving[62] = 8191;
    g_evolving[63] = 16383;
    init = true;
  }
  u32 p2 = (u32)g_evolving[62], p1 = (u32)g_evolving[63];
  for (int i = 0; i < 64; i += 2) {
    u32 cur = (u32)g_evolving[i];
    g_evolving[i] = (s16)((s32)(p2 * p1 - (cur << 16)) >> 16);
    p2 = p1;
    p1 = cur;
    cur = (u32)g_evolving[i + 1];
    g_evolving[i + 1] = (s16)((s32)(2u * (p2 * p1 + (cur << 16))) >> 16);
    p2 = p1;
    p1 = cur;
  }
}

void renderOsc(Voice& v, VoiceState& st, f32* acc[2]) {
  if (v.reset)
    resetVoice(v, st);
  f32 buf[kSubframe];
  u32 inc = v.pitch >> 1;
  for (int i = 0; i < kSubframe; i++) {
    u16 ph = st.oscPhase;
    f32 s;
    switch (v.bytesPerBlock) {
    case 0: s = ph < 0x8000u ? 0.5f : -0.5f; break;       // square 50%
    case 3: s = ph < 0x4000u ? 0.5f : -0.5f; break;       // square 25%
    case 1: case 12: s = (f32)(s16)ph / 32768.0f; break;  // saw / ramp
    case 4: s = 0.5f - fabsf((f32)(s16)ph / 32768.0f); break;  // triangle
    case 7: case 10: s = sinf((f32)ph * (2.0f * (f32)M_PI / 65536.0f)) * 0.5f; break;
    case 11: s = g_evolving[ph >> 10] / 32768.0f; break;
    default: s = 0; break;
    }
    buf[i] = s;
    st.oscPhase = ph + inc;
  }
  mixOut(v, st, buf, acc);
}

void renderWave(Voice& v, VoiceState& st, f32* acc[2]) {
  if (v.reset)
    resetVoice(v, st);
  f32 step = (f32)v.pitch / 4096.0f;
  int needed = (int)(st.resamplePos + kSubframe * step) + 2;
  needed = std::min(needed, VoiceState::kDecodeBuf);
  fillDecode(v, st, needed);
  if (st.decodeCount < needed)
    v.finished = 1;  // ran dry: the voice ends

  f32 buf[kSubframe];
  f32 pos = st.resamplePos;
  s16 prev = st.resamplePrev;
  s16 next = st.decodeCount > 0 ? st.decode[0] : prev;
  int idx = 0;
  for (int i = 0; i < kSubframe; i++) {
    buf[i] = (prev + pos * (next - prev)) / 32768.0f;
    pos += step;
    while (pos >= 1.0f) {
      pos -= 1.0f;
      prev = next;
      idx++;
      next = idx < st.decodeCount ? st.decode[idx] : prev;
    }
  }
  st.resamplePos = pos;
  st.resamplePrev = prev;

  // One-pole low-pass ("distortion filter", iir[4]), then the biquad.
  if (s16 k = v.iir[4]) {
    for (f32& s : buf) {
      f32 o = std::clamp((s - st.lpIn) * ((f32)k / 128.0f) + st.lpOut, -1.0f, 1.0f);
      st.lpIn = s;
      s = st.lpOut = o;
    }
  }
  if (v.filterMode & 0x20) {
    for (f32& s : buf) {
      f32 o = std::clamp((v.iir[0] * st.bIn1 + v.iir[1] * st.bIn2 + v.iir[2] * st.bOut1 + v.iir[3] * st.bOut2) / 32768.0f,
                         -1.0f, 1.0f);
      st.bIn2 = st.bIn1;
      st.bIn1 = s;
      st.bOut2 = st.bOut1;
      s = st.bOut1 = o;
    }
  }

  int left = st.decodeCount - idx;
  if (left > 0)
    memmove(st.decode, st.decode + idx, left * sizeof(s16));
  st.decodeCount = std::max(0, left);
  mixOut(v, st, buf, acc);
}

}  // namespace

extern "C" {

// DsetupTable: the voice table (JASDsp::initBuffer).
void fsa_dsp_setup(u32 count, u32 voices) {
  g_voices = (Voice*)(uintptr_t)voices;
  memset(g_state, 0, sizeof(g_state));
  platform_log("[audio] DSP voice table at %08x (%u voices)\n", voices, count);
}

// DsyncFrame: start a frame of `subframes` subframes into the two buffers.
void fsa_dsp_sync_frame(u32 subframes, u32 left, u32 right, u16 mixerLevel) {
  g_subframes = subframes;
  g_outL = (s16*)(uintptr_t)left;
  g_outR = (s16*)(uintptr_t)right;
  g_gain = mixerLevel / (f32)0x4000;  // DSP_MIXERLEVEL: 0x4000 at the default level 1.0
}

// Render subframe `index` of the current frame with the voices as they are now.
void fsa_dsp_render_subframe(u32 index) {
  if (!g_voices || !g_outL || index >= g_subframes)
    return;
  double t0 = emscripten_get_now();
  f32 l[kSubframe] = {}, r[kSubframe] = {};
  f32* acc[2] = {l, r};
  updateEvolving();
  for (int i = 0; i < kChannels; i++) {
    Voice& v = g_voices[i];
    if (!v.active || v.pause)
      continue;
    if (v.forcedStop) {
      v.finished = 1;
      continue;
    }
    VoiceState& st = g_state[i];
    if (v.waveAram == 0) {
      renderOsc(v, st, acc);
    } else if (knownFormat(v)) {
      renderWave(v, st, acc);
    } else {
      u32 key = (v.samplesPerBlock & 3) << 2 | (v.bytesPerBlock & 3);
      if (!g_warned[key]) {
        g_warned[key] = true;
        platform_log("[audio] voice %d: unknown wave format (%u samples / %u bytes per block)\n", i, v.samplesPerBlock,
                     v.bytesPerBlock);
      }
      v.finished = 1;
      continue;
    }
    g_voicesPlayed++;
  }
  s16* outL = g_outL + index * kSubframe;
  s16* outR = g_outR + index * kSubframe;
  f32 g = g_gain * 32767.0f;
  for (int i = 0; i < kSubframe; i++) {
    outL[i] = clamp16((s32)lrintf(l[i] * g));
    outR[i] = clamp16((s32)lrintf(r[i] * g));
  }
  g_renderMs += emscripten_get_now() - t0;
  if (index + 1 == g_subframes)
    g_framesRendered++;
}

// Debugging: the voices playing now (Module._fsa_debug_dsp() from the console).
EMSCRIPTEN_KEEPALIVE void fsa_debug_dsp_starts(int n) { g_logStarts = n; }
EMSCRIPTEN_KEEPALIVE void fsa_debug_dsp() {
  platform_log("[audio] DSP: %u frames rendered (%.3f ms each), %u voice-subframes mixed, gain %.2f\n", g_framesRendered,
               g_framesRendered ? g_renderMs / g_framesRendered : 0.0, g_voicesPlayed,
               g_gain);
  if (!g_voices)
    return;
  for (int i = 0; i < kChannels; i++) {
    const Voice& v = g_voices[i];
    if (!v.active)
      continue;
    platform_log("[audio]  v%02d %s%s%s fmt %u/%u pitch %04x aram %06x pos %u left %u end %u loop %u@%u am %u vol %u pan %04x "
                 "mix %04x:%u %04x:%u %04x:%u %04x:%u\n",
                 i, v.pause ? "P" : "", v.finished ? "F" : "", v.forcedStop ? "S" : "", v.samplesPerBlock, v.bytesPerBlock,
                 v.pitch, v.waveAram, v.samplePosition, v.samplesLeft, v.endSample, v.loop, v.loopStart, v.amSet, v.amVolume,
                 v.amPanDolby, v.mixer[0].bus, v.mixer[0].target, v.mixer[1].bus, v.mixer[1].target, v.mixer[2].bus,
                 v.mixer[2].target, v.mixer[3].bus, v.mixer[3].target);
  }
}

}  // extern "C"
