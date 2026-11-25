// --pre-js (deluxe): the page side of the native GBA link (platform/gba_link.cpp,
// platform/gba_view.cpp, docs/MULTIPLAYER.md).
//
//   ?gba=N         plug a GBA (local peer) into ports 0..N-1 (N = 1..4)
//   ?gba=0,2       ... or into the listed ports
//   ?gbaview=0     no panel (the GBAs still work); ?gbaview=S: panel scale S (default 2/3)
//
// Each GBA's buttons are the pad of its port (keyboard / gamepad / the harness's
// four pads), plus these keys for ports 1-3 in live play:
//   port 1: IJKL, U = A, O = B, P = Start, 8 = Select, 7 = L, 9 = R
//   port 2: TFGH, R = A, Y = B, V = Start
//   port 3: arrows, Numpad1 = A, Numpad2 = B, Numpad3 = Start
// The panel shows each GBA's native view (240 x 160) and its link state.
Module.fsaGbaInit = function () {
  const q = new URLSearchParams(location.search);
  const arg = q.get('gba');
  if (!arg) return;
  let ports = [];
  if (/^[1-4]$/.test(arg)) for (let i = 0; i < Number(arg); i++) ports.push(i);
  else ports = arg.split(',').map(Number).filter((p) => p >= 0 && p < 4);
  const log = (s) => (window.fsaLog || console.log)('[gba] ' + s);
  for (const p of ports) Module._fsa_gba_attach(p, 1);
  log('GBAs in ports ' + ports.join(', '));
  window.fsaGba = {
    attach: (p, on = 1) => Module._fsa_gba_attach(p, on),
    link: (p) => info(Module._fsa_gba_link_info, p, 9),
    view: (p) => decodeView(p),
    memory: (p, region, len) => HEAPU8.slice(Module._fsa_gba_memory(p, region), Module._fsa_gba_memory(p, region) + len),
    // One line per GBA for logs and scripted runs: link state, record kinds seen,
    // the latest record (hex words), the files placed (address:bytes/sent/flag).
    dump: () => JSON.stringify(ports.map((p) => {
      const v = decodeView(p);
      const hex = (x) => (x >>> 0).toString(16);
      return { port: p, reads: Module._fsa_harness_reads(), link: window.fsaGba.link(p), kinds: v.kinds,
               record: v.record.map(hex), files: v.files.map((f) => hex(f.addr) + ':' + f.size + '/' + f.sent + '/' + f.flag),
               message: v.message.length };
    })),
  };

  const scratch = Module._malloc(4 * 2048);
  function info(fn, p, n) {
    const k = fn(p, scratch);
    return Array.from(new Uint32Array(HEAPU8.buffer, scratch, Math.max(k, n)));
  }
  function decodeView(p) {
    const a = info(Module._fsa_gba_view_info, p, 0);
    let k = 0;
    const v = { records: a[k++], kinds: a.slice(k, k + 16) };
    k += 16;
    const rw = a[k++];
    v.record = a.slice(k, k + rw);
    k += 26;
    const fc = a[k++];
    v.files = [];
    for (let i = 0; i < 32; i++, k += 4)
      if (i < fc) v.files.push({ addr: a[k], size: a[k + 1], sent: a[k + 2], flag: a[k + 3] });
    const ml = a[k++];
    v.message = a.slice(k, k + ml);
    return v;
  }

  // Live keys for ports 1-3 (GBA_* bits).
  const GBA = { A: 1, B: 2, SELECT: 4, START: 8, RIGHT: 0x10, LEFT: 0x20, UP: 0x40, DOWN: 0x80, R: 0x100, L: 0x200 };
  const KEYS = {
    1: { KeyI: 'UP', KeyJ: 'LEFT', KeyK: 'DOWN', KeyL: 'RIGHT', KeyU: 'A', KeyO: 'B', KeyP: 'START', Digit8: 'SELECT', Digit7: 'L', Digit9: 'R' },
    2: { KeyT: 'UP', KeyF: 'LEFT', KeyG: 'DOWN', KeyH: 'RIGHT', KeyR: 'A', KeyY: 'B', KeyV: 'START' },
    3: { ArrowUp: 'UP', ArrowLeft: 'LEFT', ArrowDown: 'DOWN', ArrowRight: 'RIGHT', Numpad1: 'A', Numpad2: 'B', Numpad3: 'START' },
  };
  const held = [0, 0, 0, 0];
  const onKey = (down) => (e) => {
    for (const p in KEYS) {
      const name = KEYS[p][e.code];
      if (!name) continue;
      held[p] = down ? held[p] | GBA[name] : held[p] & ~GBA[name];
      Module._fsa_gba_page_keys(Number(p), held[p]);
    }
  };
  window.addEventListener('keydown', onKey(true));
  window.addEventListener('keyup', onKey(false));

  if (q.get('gbaview') === '0') return;
  const panel = document.createElement('div');
  panel.id = 'gbapanel';
  panel.style.cssText = 'position:fixed;left:4px;bottom:4px;z-index:50;display:flex;flex-wrap:wrap;gap:4px;' +
    'pointer-events:none;font:9px/1.25 ui-monospace,monospace;color:#ddd;opacity:.92';
  const views = ports.map((p) => {
    const box = document.createElement('div');
    box.style.cssText = 'background:rgba(0,0,0,.75);padding:4px;border-radius:4px';
    const c = document.createElement('canvas');
    c.width = 240;
    c.height = 160;
    // ?gbaview=S: the panel's scale (default 2/3; 1 = the GBA's 240 x 160).
    const scale = Number(q.get('gbaview')) || 2 / 3;
    c.style.cssText = `display:block;width:${240 * scale}px;height:${160 * scale}px;image-rendering:pixelated`;
    const t = document.createElement('div');
    t.style.cssText = `white-space:pre;width:${240 * scale}px;overflow:hidden`;
    box.append(c, t);
    panel.append(box);
    return { p, ctx: c.getContext('2d'), img: new ImageData(240, 160), text: t };
  });
  document.body.append(panel);
  const MODES = ['off', 'code', 'disc-id', 'cmd', 'packet', 'frame'];
  setInterval(() => {
    for (const v of views) {
      const ptr = Module._fsa_gba_render(v.p);
      v.img.data.set(HEAPU8.subarray(ptr, ptr + 240 * 160 * 4));
      v.ctx.putImageData(v.img, 0, 0);
      const l = window.fsaGba.link(v.p);
      const d = decodeView(v.p);
      const kind = d.record.length ? (d.record[0] >>> 24) & 15 : -1;
      const valid = d.record.length ? (d.record[0] >>> 19) & 7 : 0;
      const kw = l[7] >>> 0, gk = ((kw >>> 16) & 0xFF) | (kw & 0x300);
      const held = Object.keys(GBA).filter((n) => gk & GBA[n]).join(' ') || '-';
      v.text.textContent =
        `P${v.p + 1} ${l[1] ? 'linked' : 'plugged'} ${MODES[l[2]]} frames ${l[4]} files ${l[5]}\n` +
        `record ${kind}${kind === 5 ? (valid ? ' room' : ' overworld (tile sheet)') : ''}, ${d.record.length} words\n` +
        `keys ${held}`;
    }
  }, 100);
};
