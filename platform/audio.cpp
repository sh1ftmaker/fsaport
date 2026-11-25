// AI: the audio interface's DMA, fed to Web Audio (platform/web/fsa_audio.js).
//
// On the GameCube the AI streams 32 kHz stereo s16 from main RAM by DMA, a
// block at a time; when a block starts it raises the DMA interrupt, and
// JAudio's handler (JASAudioThread::DMACallback) has the audio thread prepare
// the next one (JASDriver::updateDac -> the DSP mix, see audio_dsp.cpp) and
// program it with AIInitDMA. Here an OS alarm stands in for that interrupt:
// each "DMA" hands the block programmed last to the page, which schedules it
// on its AudioContext, then calls the game's callback. The alarm is paced by
// the page's audio clock, keeping about kLeadSec of sound queued ahead of
// what is playing; until the page may start audio (a user gesture), the
// page's clock is wall time and the blocks are dropped, so JAudio and the
// movie player (which times video by its audio) run at the right rate anyway.
#include "platform.h"

#include <emscripten.h>

#include <cstdint>

namespace {

typedef void (*AIDCallback)(void);

AIDCallback g_dmaCallback = nullptr;
u32 g_dmaAddr = 0, g_dmaLen = 0;
bool g_dmaRunning = false;
u32 g_dspRate = 1;  // AI_SAMPLERATE_48KHZ until JAudio sets 32 kHz
u32 g_dmaInits = 0;  // AIInitDMA calls
u32 g_dmaTicks = 0;  // DMA "interrupts" raised
OSAlarm g_alarm;
bool g_alarmSet = false;
OSTime g_netDue = 0;  // netplay: when the next block is due (virtual time)

constexpr double kLeadSec = 0.05;

}  // namespace

// The page side: queue one block (interleaved R, L s16, as the AI reads it);
// returns how many seconds of sound are now queued ahead of the audio clock.
EM_JS(double, fsa_js_audio_push, (const s16* samples, int frames, int rate), {
  return Module.fsaAudio ? Module.fsaAudio.push(samples, frames, rate) : 0;
});

namespace {

void arm(OSTime at);

void dma_interrupt(OSAlarm*, OSContext*) {
  g_alarmSet = false;
  if (!g_dmaRunning || !g_dmaCallback)
    return;
  OSTime now = OSGetTime();
  // The audio thread has not run since the last interrupt (it programs the
  // next block from it): wait for it rather than replay a block.
  if (g_dmaTicks > g_dmaInits) {
    arm(now + OSMillisecondsToTicks(1));
    return;
  }
  double lead = 0;
  u32 frames = g_dmaLen / 4;
  // Netplay (docs/NETPLAY.md): the DMA is paced by virtual time alone, one
  // block per block length, so the audio thread runs at the same reads in
  // every browser and in a re-simulation; re-simulated blocks are not played.
  bool net = platform_net_on();
  if (g_dmaAddr && frames && !platform_net_resim())
    lead = fsa_js_audio_push((const s16*)(uintptr_t)g_dmaAddr, (int)frames, g_dspRate == 0 ? 32000 : 48000);
  g_dmaTicks++;
  g_dmaCallback();
  if (net) {
    // From the previous due time, not from now: alarms fire at retraces only.
    if (!g_netDue)
      g_netDue = now;
    g_netDue += (OSTime)((double)frames * OS_TIMER_CLOCK / (g_dspRate == 0 ? 32000.0 : 48000.0));
    arm(g_netDue);
    return;
  }
  double wait = lead - kLeadSec;
  arm(wait > 0 ? now + (OSTime)(wait * OS_TIMER_CLOCK) : now);
}

void arm(OSTime at) {
  OSSetAbsAlarm(&g_alarm, at, dma_interrupt);
  g_alarmSet = true;
}

}  // namespace

extern "C" {

void AIInit(u8*) {}

void AIInitDMA(u32 addr, u32 length) {
  g_dmaAddr = addr;
  g_dmaLen = length;
  g_dmaInits++;
}

void AIStartDMA() {
  if (g_dmaRunning)
    return;
  g_dmaRunning = true;
  g_dmaTicks = 0;
  g_dmaInits = g_dmaInits ? 1 : 0;
  platform_log("[audio] AI DMA started (%u bytes per block)\n", g_dmaLen);
  if (!g_alarmSet) {
    OSCreateAlarm(&g_alarm);
    arm(OSGetTime());
  }
}

void AIStopDMA() {
  g_dmaRunning = false;
  if (g_alarmSet) {
    OSCancelAlarm(&g_alarm);
    g_alarmSet = false;
  }
}

AIDCallback AIRegisterDMACallback(AIDCallback cb) {
  AIDCallback old = g_dmaCallback;
  g_dmaCallback = cb;
  return old;
}

u32 AIGetDSPSampleRate() { return g_dspRate; }
void AISetDSPSampleRate(u32 rate) { g_dspRate = rate; }

// For the page / debugging: blocks handed out so far.
EMSCRIPTEN_KEEPALIVE u32 fsa_audio_blocks() { return g_dmaTicks; }

}  // extern "C"
