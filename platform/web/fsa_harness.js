// --pre-js: the page side of the behaviour harness (docs/HARNESS.md,
// platform/harness.cpp). The page awaits Module.fsaHarnessInit() before main().
//
//   ?record=1        write every pad read to the "inputs" diagnostics stream
//   ?inputs=URL      replay that JSONL file (one line per pad read)
//   ?lockstep=1      lockstep only (implied by record and inputs; lockstep=0 turns it off)
//   ?fast=1          lockstep without wall-clock pacing (as fast as the game runs)
Module.fsaHarnessInit = async function () {
  const q = new URLSearchParams(location.search);
  const log = (s) => (window.fsaLog || console.log)('[harness] ' + s);
  const record = q.get('record') === '1';
  const inputs = q.get('inputs');
  const fast = q.get('fast') === '1';
  let lockstep = record || !!inputs || fast || q.get('lockstep') === '1';
  if (q.get('lockstep') === '0') lockstep = false;
  if (!record && !inputs && !lockstep) return;
  if (inputs) {
    const r = await fetch(inputs, { cache: 'no-store' });
    if (!r.ok) throw new Error('harness: ' + inputs + ': HTTP ' + r.status);
    const lines = (await r.text()).split('\n').filter((l) => l.trim());
    const rows = lines.map((l) => JSON.parse(l));
    // Index by "n"; a gap repeats nothing: missing reads are neutral, port 0 connected.
    const count = rows.reduce((m, o) => Math.max(m, o.n + 1), 0);
    const per = 4 * 8;
    const buf = new Int16Array(count * per);
    for (let n = 0; n < count; n++) buf[n * per] = 1;
    for (const o of rows) {
      const base = o.n * per;
      for (let i = 0; i < 4; i++) {
        const p = o.pads ? o.pads[i] : undefined;
        const at = base + i * 8;
        // An array is a connected pad; null is an unplugged one; a pad left
        // out of a short array is unplugged too, except port 0 (connected, neutral).
        buf[at] = Array.isArray(p) ? 1 : (p === undefined && i === 0 ? 1 : 0);
        if (Array.isArray(p)) for (let k = 0; k < 7; k++) buf[at + 1 + k] = p[k] | 0;
        else for (let k = 0; k < 7; k++) buf[at + 1 + k] = 0;
      }
    }
    const ptr = Module._malloc(buf.byteLength);
    HEAPU8.set(new Uint8Array(buf.buffer), ptr);
    Module._fsa_harness_load(ptr, count);
    log(`replaying ${inputs}: ${count} reads`);
  }
  Module.fsaClock.lockstep = lockstep;
  Module.fsaClock.fast = fast && lockstep;
  Module._fsa_harness_config((lockstep ? 1 : 0) | (record ? 2 : 0) | (inputs ? 4 : 0));
  log(`lockstep ${lockstep}${fast ? ' (fast)' : ''}, record ${record}, replay ${!!inputs}`);
  window.fsaHarnessReads = () => Module._fsa_harness_reads();
};
