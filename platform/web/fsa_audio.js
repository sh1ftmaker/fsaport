// --pre-js: the page side of the AI (platform/audio.cpp).
//
// fsaAudio.push() takes one AI DMA block (interleaved right, left s16 at 32 kHz,
// as the GameCube's AI reads it) and schedules it on an AudioContext right
// after the previous one, returning how far ahead of the audio clock the queue
// now reaches; audio.cpp paces the game's DMA interrupts by that. Browsers only
// start an AudioContext after a user gesture: until then (or with ?audio=0)
// blocks are dropped and the clock is wall time, so the game's audio still
// runs at its rate.
//
// Measuring (there is nothing to hear in a headless browser):
// fsaAudio.stats() returns block counts, the RMS/peak of what the game sent
// and of what the AudioContext is playing (an AnalyserNode after the output),
// underruns and the queue lead; ?audiolog=1 logs it every 5 s.
Module.fsaAudio = {
  ctx: null,
  out: null,
  analyser: null,
  next: 0,
  running: false,
  disabled: /[?&]audio=0\b/.test(location.search),
  blocks: 0,
  played: 0,
  underruns: 0,
  sumSq: 0,
  sumN: 0,
  peak: 0,
  lastLead: 0,
  scratch: null,

  create() {
    if (this.ctx || this.disabled) return;
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) { this.disabled = true; return; }
    try {
      this.ctx = new AC({ sampleRate: 32000, latencyHint: 'interactive' });
    } catch (e) {
      this.ctx = new AC();  // no 32 kHz context here: the buffers are resampled
    }
    this.out = this.ctx.createGain();
    this.analyser = this.ctx.createAnalyser();
    this.analyser.fftSize = 2048;
    this.out.connect(this.analyser);
    this.analyser.connect(this.ctx.destination);
    this.ctx.onstatechange = () => console.log('[audio] AudioContext ' + this.ctx.state + ' at ' + this.ctx.sampleRate + ' Hz');
  },

  // From a user gesture: create/resume the context (iOS also wants a sound
  // started inside the gesture).
  unlock() {
    if (this.disabled) return;
    this.create();
    if (!this.ctx) return;
    if (this.ctx.state !== 'running') this.ctx.resume().catch(() => {});
    if (!this.unlocked) {
      this.unlocked = true;
      const b = this.ctx.createBuffer(1, 1, this.ctx.sampleRate);
      const s = this.ctx.createBufferSource();
      s.buffer = b;
      s.connect(this.ctx.destination);
      s.start(0);
    }
  },

  push(ptr, frames, rate) {
    if (!this.ctx) this.create();
    const ctx = this.ctx;
    const running = !!ctx && ctx.state === 'running';
    if (running !== this.running) { this.running = running; this.next = 0; }  // clock changed
    const now = running ? ctx.currentTime : performance.now() / 1000;
    if (this.next < now) {
      if (this.next && running) this.underruns++;
      this.next = now + 0.005;
    }
    this.blocks++;
    const src = new Int16Array(HEAPU8.buffer, ptr, frames * 2);
    let sq = 0, pk = 0;
    for (let i = 0; i < src.length; i++) {
      const v = src[i];
      sq += v * v;
      if (v > pk) pk = v; else if (-v > pk) pk = -v;
    }
    this.sumSq += sq; this.sumN += src.length;
    if (pk > this.peak) this.peak = pk;
    if (this.rec && this.recPos < this.rec.length) {
      for (let j = 0; j + 1 < src.length && this.recPos < this.rec.length; j += 2) {
        this.rec[this.recPos++] = src[j + 1];  // left
        this.rec[this.recPos++] = src[j];      // right
      }
    }
    if (running) {
      const buf = ctx.createBuffer(2, frames, rate);
      const l = buf.getChannelData(0), r = buf.getChannelData(1);
      for (let i = 0, j = 0; i < frames; i++, j += 2) {
        r[i] = src[j] / 32768;
        l[i] = src[j + 1] / 32768;
      }
      const node = ctx.createBufferSource();
      node.buffer = buf;
      node.connect(this.out);
      node.start(this.next);
      this.played++;
    }
    this.next += frames / rate;
    return (this.lastLead = this.next - now);
  },

  // RMS / peak of what the AudioContext is playing right now (0..1).
  outputLevel() {
    if (!this.analyser) return { rms: 0, peak: 0 };
    const n = this.analyser.fftSize;
    if (!this.scratch) this.scratch = new Float32Array(n);
    this.analyser.getFloatTimeDomainData(this.scratch);
    let sq = 0, pk = 0;
    for (let i = 0; i < n; i++) { const v = this.scratch[i]; sq += v * v; pk = Math.max(pk, Math.abs(v)); }
    return { rms: Math.sqrt(sq / n), peak: pk };
  },

  // Counters since the last call (rates per second of wall time).
  stats() {
    const t = performance.now();
    const dt = this.lastStats ? (t - this.lastStats.t) / 1000 : 0;
    const o = this.outputLevel();
    const s = {
      state: this.ctx ? this.ctx.state : 'none',
      ctxRate: this.ctx ? this.ctx.sampleRate : 0,
      blocks: this.blocks,
      played: this.played,
      blocksPerSec: dt ? +((this.blocks - this.lastStats.blocks) / dt).toFixed(2) : 0,
      gameRms: this.sumN ? +(Math.sqrt(this.sumSq / this.sumN) / 32768).toFixed(4) : 0,
      gamePeak: +(this.peak / 32768).toFixed(4),
      outRms: +o.rms.toFixed(4),
      outPeak: +o.peak.toFixed(4),
      underruns: this.underruns,
      leadMs: +(this.lastLead * 1000).toFixed(1),
    };
    this.lastStats = { t, blocks: this.blocks };
    this.sumSq = this.sumN = this.peak = 0;
    return s;
  },
};

(() => {
  const a = Module.fsaAudio;
  const unlock = () => a.unlock();
  for (const ev of ['pointerdown', 'mousedown', 'touchend', 'keydown'])
    window.addEventListener(ev, unlock, { capture: true, passive: true });
  // ?audiorec=S: keep the first S seconds the game sends (16-bit stereo, left
  // first, 32 kHz) for tools/web_run.mjs --wav.
  const rec = /[?&]audiorec=(\d+)/.exec(location.search);
  if (rec) { a.rec = new Int16Array(Math.min(+rec[1], 600) * 32000 * 2); a.recPos = 0; }
  a.recChunk = (start, count) => {
    const end = Math.min(a.recPos, start + count);
    if (!a.rec || start >= end) return '';
    const b = new Uint8Array(a.rec.buffer, start * 2, (end - start) * 2);
    let s = '';
    for (let i = 0; i < b.length; i += 0x8000) s += String.fromCharCode.apply(null, b.subarray(i, i + 0x8000));
    return btoa(s);
  };
  if (/[?&]audiolog=1\b/.test(location.search))
    setInterval(() => console.log('[audio] ' + JSON.stringify(a.stats())), 5000);
})();
