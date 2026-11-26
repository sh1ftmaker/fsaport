// --pre-js: the page side of the behaviour harness (docs/HARNESS.md,
// platform/harness.cpp). The page awaits Module.fsaHarnessInit() before main().
//
//   ?record=1        write every pad read to the "inputs" diagnostics stream
//   ?inputs=URL      replay that JSONL file (one line per pad read)
//   ?script=URL      closed-loop scripted input (JSON, compiled below, run per read in C)
//   ?lockstep=1      lockstep only (implied by record and inputs; lockstep=0 turns it off)
//   ?fast=1          lockstep without wall-clock pacing (as fast as the game runs)
Module.fsaHarnessInit = async function () {
  const q = new URLSearchParams(location.search);
  const log = (s) => (window.fsaLog || console.log)('[harness] ' + s);
  // ?allocprobe=1: host allocations in the game's JKR heaps (platform/alloc_probe.cpp).
  if (q.get('allocprobe') === '1') Module._fsa_alloc_probe(1);
  const record = q.get('record') === '1';
  const inputs = q.get('inputs');
  // A netplay room's script (docs/NETPLAY.md) is the room's, the same in every browser.
  const sess = Module.fsaNetSession;
  const script = (sess && sess.opts.script) || q.get('script');
  if (inputs && script) throw new Error('harness: ?inputs and ?script exclude each other');
  const fast = q.get('fast') === '1';
  // Netplay (?room=, ?rbtest=; docs/NETPLAY.md) needs lockstep: virtual time.
  const net = !!(sess || q.get('rbtest'));
  let lockstep = record || !!inputs || !!script || fast || net || q.get('lockstep') === '1';
  if (q.get('lockstep') === '0' && !net) lockstep = false;
  if (!record && !inputs && !script && !lockstep) return;
  if (script) {
    const r = await fetch(script, { cache: 'no-store' });
    if (!r.ok) throw new Error('harness: ' + script + ': HTTP ' + r.status);
    const table = compileScript(JSON.parse(await r.text()));
    const ptr = Module._malloc(table.byteLength);
    HEAPU8.set(new Uint8Array(table.buffer), ptr);
    Module._fsa_harness_script(ptr, table.length);
    log(`script ${script}: ${table[0]} steps`);
  }
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
  // A script's input goes to the "inputs" stream like a recording's.
  Module._fsa_harness_config((lockstep ? 1 : 0) | (record || script || net ? 2 : 0) | (inputs ? 4 : 0) | (script ? 8 : 0));
  log(`lockstep ${lockstep}${fast ? ' (fast)' : ''}, record ${record || !!script}, replay ${!!inputs}, script ${!!script}`);
  window.fsaHarnessReads = () => Module._fsa_harness_reads();
};

// The script format (docs/HARNESS.md "Scripted input") compiled into the
// Float64Array platform/harness.cpp reads:
//   [0] step count; [1 + 9 * i ...] step i: kind, buttons, every, reads,
//   stickX, stickY, timeout (-1 none), condition offset (-1 none);
//   a condition: term count, then per term key, value count, values (NaN = null).
// Keys are kGlobals indices (game/src/port/harness_state.cpp), or -1/-2/-3 for
// the scene triple (mode, -1 before GameRoot exists; stage; level).
function compileScript(doc) {
  const fail = (s) => { throw new Error('harness script: ' + s); };
  if (!doc || doc.version !== 1 || !Array.isArray(doc.steps)) fail('need {"version":1,"steps":[...]}');
  const BUTTONS = { LEFT: 0x1, RIGHT: 0x2, DOWN: 0x4, UP: 0x8, Z: 0x10, R: 0x20, L: 0x40,
                    A: 0x100, B: 0x200, X: 0x400, Y: 0x800, START: 0x1000 };
  const KINDS = ['wait', 'wait_reads', 'press', 'hold', 'stick', 'end', 'set'];
  const ALLOWED = { wait: ['wait'], wait_reads: ['wait_reads'], press: ['press', 'every', 'until', 'pads'],
                    hold: ['hold', 'reads', 'pads'], stick: ['stick', 'reads', 'buttons', 'pads'], end: ['end'], set: ['set'] };
  const keys = { mode: -1, stage: -2, level: -3,
                 'gGameRoot_8053AAF8->field_0x4c': -1,
                 'lbl_80529DEC.mStage_0x118': -2, 'lbl_80529DEC.mLevel_0x11c': -3 };
  for (let k = 0, n = Module._fsa_harness_keys(); k < n; k++) {
    const name = UTF8ToString(Module._fsa_harness_key(k));
    if (!(name in keys)) keys[name] = k;
  }
  const int = (v, i, what, lo, hi) => {
    if (!Number.isInteger(v) || v < lo || v > hi) fail('step ' + i + ': ' + what + ' must be an integer in ' + lo + '..' + hi);
    return v;
  };
  const buttons = (v, i) => {
    if (v === undefined) return 0;
    if (!Array.isArray(v)) fail('step ' + i + ': buttons are a list of names');
    return v.reduce((m, b) => {
      const bit = BUTTONS[String(b).toUpperCase()];
      if (!bit) fail('step ' + i + ': unknown button ' + b);
      return m | bit;
    }, 0);
  };
  const steps = [], conds = [];
  const cond = (c, i) => {
    if (c === undefined) return -1;
    if (!c || typeof c !== 'object' || Array.isArray(c)) fail('step ' + i + ': a condition is an object');
    const at = conds.length;
    const terms = Object.entries(c);
    conds.push(terms.length);
    for (const [k, v] of terms) {
      if (!(k in keys)) fail('step ' + i + ': unknown condition key ' + k);
      const vals = Array.isArray(v) ? v : [v];
      conds.push(keys[k], vals.length);
      for (const x of vals) {
        if (x === null) conds.push(NaN);
        else if (typeof x === 'number') conds.push(x);
        else if (typeof x === 'boolean') conds.push(x ? 1 : 0);
        else fail('step ' + i + ': condition values are numbers, booleans or null');
      }
    }
    return at;
  };
  doc.steps.forEach((s, i) => {
    const kinds = KINDS.filter((k) => k in s);
    if (kinds.length !== 1) fail('step ' + i + ': needs exactly one of ' + KINDS.join(' '));
    const kind = kinds[0];
    for (const k of Object.keys(s))
      if (k !== 'timeout' && k !== 'comment' && k !== 'pad' && !ALLOWED[kind].includes(k))
        fail('step ' + i + ': unexpected key ' + k + ' in a ' + kind + ' step');
    const row = [KINDS.indexOf(kind), 0, 1, 0, 0, 0, -1, -1, 0];
    // "pad": 0-3, the pad the step's input goes to (deluxe; default 0).
    if (s.pad !== undefined) row[8] = int(s.pad, i, 'pad', 0, 3);
    if (s.timeout !== undefined) row[6] = int(s.timeout, i, 'timeout', 0, 2 ** 31);
    if (kind === 'wait') row[7] = cond(s.wait, i);
    if (kind === 'wait_reads') row[3] = int(s.wait_reads, i, 'wait_reads', 0, 2 ** 31);
    if (kind === 'press') {
      row[1] = buttons(s.press, i);
      row[2] = int(s.every === undefined ? 30 : s.every, i, 'every', 2, 2 ** 31);
      row[7] = cond(s.until, i);
    }
    if (kind === 'hold') {
      row[1] = buttons(s.hold, i);
      row[3] = int(s.reads, i, 'reads', 0, 2 ** 31);
    }
    if (kind === 'stick') {
      if (!Array.isArray(s.stick) || s.stick.length !== 2) fail('step ' + i + ': stick is [x, y]');
      row[4] = int(s.stick[0], i, 'stick x', -128, 127);
      row[5] = int(s.stick[1], i, 'stick y', -128, 127);
      row[3] = int(s.reads, i, 'reads', 0, 2 ** 31);
      row[1] = buttons(s.buttons, i);
    }
    if (kind === 'end' && s.end !== true) fail('step ' + i + ': end must be true');
    if (s.pads !== undefined) {
      // Diagnostics builds only (docs/HARNESS.md "Diagnostic pokes"): the pads that get the
      // step's input, as bits 16-19 of the buttons word; a normal build feeds port 0 only.
      if (!Array.isArray(s.pads) || !s.pads.length) fail('step ' + i + ': pads is a list of ports 0-3');
      row[1] += 65536 * s.pads.reduce((m, p) => m | (1 << int(p, i, 'pad', 0, 3)), 0);
    }
    if (kind === 'set') {
      // {"set": {NAME: number, ...}}: the GameState fields ?stage= writes
      // (platform/harness.cpp kSet, game/src/port/harness_state.cpp fsa_harness_set).
      // Ids from 100 are the diagnostic pokes (FSA_DIAG builds; fsa_harness_diag_set).
      const SETTABLE = { 'lbl_80529DEC.mStage_0x118': 1, stage: 1, 'lbl_80529DEC.mLevel_0x11c': 2, level: 2,
                         'lbl_80529DEC.field_0x128': 3, 'lbl_80529DEC.field_0x12c': 4,
                         seed: 5, 'gGameRoot_8053AAF8->mRandom.seed': 5, seed_on_stage: 6,
                         coop_join: 7, coop_leave: 8,
                         players_on_stage: 100, kinds_on_stage: 101, pads: 102, gba_link: 103, force: 104,
                         target: 110, hit_type: 111, hit_strength: 112, hit_dy: 113, hit: 114, shake: 115,
                         kill: 116, layer: 117, player_b9c: 120, msg_scan: 130, msg_window: 131, msg_eval: 132 };
      const fourcc = (v) => (typeof v === 'string' && v.length === 4)
        ? [...v].reduce((m, c) => m * 256 + c.charCodeAt(0), 0) : v;
      if (!s.set || typeof s.set !== 'object' || Array.isArray(s.set)) fail('step ' + i + ': set is an object');
      const entries = Object.entries(s.set);
      row[7] = conds.length;
      conds.push(entries.length);
      for (let [k, v] of entries) {
        if (k === 'target') v = fourcc(v);  // an ActorId ("OCTR") or a slot number
        if (!(k in SETTABLE)) fail('step ' + i + ': cannot set ' + k + ' (settable: ' + Object.keys(SETTABLE).join(' ') + ')');
        if (!Number.isInteger(v) || v < -(2 ** 31) || v > 2 ** 32 - 1) fail('step ' + i + ': set values are 32-bit integers');
        conds.push(SETTABLE[k], v);
      }
    }
    steps.push(...row);
  });
  const base = 1 + steps.length;
  const out = new Float64Array(base + conds.length);
  out[0] = doc.steps.length;
  out.set(steps, 1);
  out.set(conds, base);
  for (let i = 0; i < doc.steps.length; i++) if (out[1 + 9 * i + 7] >= 0) out[1 + 9 * i + 7] += base;
  return out;
}
