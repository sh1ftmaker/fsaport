// --pre-js (deluxe): local co-op on one screen, the page's half
// (docs/MULTIPLAYER.md "Local co-op"; the game's half is platform/coop.cpp).
//
// An input-device core in the manner of katam-port's web/input_devices.js:
// every device is a potential player and people join and leave at any time.
//
//   devices   keyboard set A (WASD, Space, Left Shift, E, Q, Enter, Z, C, F),
//             keyboard set B (the arrows and . , / ; ' M and Right Shift, or the
//             numpad), the on-screen touch pad, and each Gamepad API pad
//             (standard mapping, as Aurora maps it: A B X Y, Start, Back = Z,
//             shoulders L R, triggers analog, d-pad, both sticks).
//   follow    a device nobody has joined drives player 1. One person with any
//             device plays exactly as before.
//   join      Start on a gamepad that is not the one player 1 is using, set B's
//             Start, a parked device's Start, or Join in the roster: the device
//             becomes the next free player (P2..P4). That press does not reach
//             the game.
//   leave     hold Back (gamepad) / Backspace (set B) / ` (set A) for a
//             second and a half, or the roster's x, or a gamepad unplugged for
//             three seconds. The device follows player 1 again (P1's own
//             device stays with P1).
//   roster    the chip at the top right: P1..P4 in the Links' colours (green,
//             red, blue, purple) with each player's device.
//
// Who drives which player is the page's business; the game only sees the four
// PADStatus (fsa_coop_pad) and join / leave requests (fsa_coop_request), which
// platform/coop.cpp applies at the next pad read. So moving a device between
// players changes nothing but which inputs drive which player.
//
//   ?coop=0        off: Aurora's own keyboard / gamepad routing, as before
//   ?coop=N        start with players 1..N joined (devices still to claim)
//   window.fsaCoop  state(), join(dev|null), leave(slot), assign(dev, slot), devices()
//
// Off by default in harness runs (?script= / ?inputs=: the script drives the
// pads) and in netplay rooms (each browser is one seat; its devices all drive
// it, docs/NETPLAY.md).
Module.fsaCoopInit = function () {
  const q = new URLSearchParams(location.search);
  if (q.get('coop') === '0') return;
  const harness = q.has('script') || q.has('inputs');
  const net = !!Module.fsaNetSession || q.has('rbtest');
  if (!Module._fsa_coop_enable) return;
  const log = (s) => (window.fsaLog || console.log)('[coop] ' + s);

  // PAD_* bits (dolphin/pad.h).
  const P = { LEFT: 1, RIGHT: 2, DOWN: 4, UP: 8, Z: 0x10, R: 0x20, L: 0x40, A: 0x100, B: 0x200, X: 0x400, Y: 0x800, START: 0x1000 };
  const COLORS = ['#3fb950', '#f85149', '#58a6ff', '#bc8cff'];  // the Links: green, red, blue, purple
  const NAMES = ['Green', 'Red', 'Blue', 'Purple'];
  const SLOTS = 4;
  const HOLD_LEAVE_MS = 1500, GRACE_MS = 3000;

  // Keyboard sets (KeyboardEvent.code). Values: a PAD bit, or a stick axis.
  const KEYS_A = {
    KeyW: 'up', KeyS: 'down', KeyA: 'left', KeyD: 'right',
    Space: P.A, ShiftLeft: P.B, KeyE: P.X, KeyQ: P.Y, Enter: P.START, KeyZ: P.L, KeyC: P.R, KeyF: P.Z,
  };
  const KEYS_B = {
    ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right',
    Period: P.A, Comma: P.B, Slash: P.X, KeyM: P.Y, Semicolon: P.START, Quote: P.Z, ShiftRight: P.R, KeyL: P.L,
    Numpad8: 'up', Numpad5: 'down', Numpad4: 'left', Numpad6: 'right',
    Numpad0: P.A, NumpadDecimal: P.B, Numpad1: P.X, Numpad2: P.Y, NumpadEnter: P.START, Numpad3: P.Z,
    NumpadAdd: P.R, NumpadSubtract: P.L,
  };
  const LEAVE_KEY = { kbA: 'Backquote', kbB: 'Backspace' };

  // --- Devices ------------------------------------------------------------------
  // id -> { id, kind: 'kbA'|'kbB'|'touch'|'pad', name, slot (-1 = follows P1),
  //         st: {buttons, sx, sy, cx, cy, tl, tr}, prevStart, startSwallow,
  //         leaveSince, lostAt, last (time of the latest input) }
  const devs = new Map();
  const zero = () => ({ buttons: 0, sx: 0, sy: 0, cx: 0, cy: 0, tl: 0, tr: 0 });
  const device = (id, kind, name) => {
    let d = devs.get(id);
    if (!d) {
      d = { id, kind, name, slot: -1, st: zero(), prevStart: false, swallow: false, leaveSince: 0, lostAt: 0, last: 0 };
      devs.set(id, d);
    }
    return d;
  };
  const kbA = device('kbA', 'kbA', 'Keyboard WASD');
  const kbB = device('kbB', 'kbB', 'Keyboard arrows');
  const touch = device('touch', 'touch', 'Touch pad');

  // Slot owners: slots[s] = device id or null. Slot 0 (player 1) is always in
  // the game; its owner is null while every free device follows it.
  const slots = new Array(SLOTS).fill(null);
  let joined = 1;  // mask of slots the game has (bit 0 always)
  let p1Recent = null;  // the following device player 1 used last
  const startN = Math.max(1, Math.min(SLOTS, Number(q.get('coop')) || 1));
  for (let s = 1; s < startN; s++) joined |= 1 << s;

  // Keyboard state.
  const down = new Set();
  const typing = (e) => {
    const t = e.target;
    return t && (t.tagName === 'INPUT' || t.tagName === 'TEXTAREA' || t.isContentEditable);
  };
  const owns = (code) => code in KEYS_A || code in KEYS_B || code === LEAVE_KEY.kbA || code === LEAVE_KEY.kbB;
  window.addEventListener('keydown', (e) => {
    if (typing(e) || !owns(e.code)) return;
    down.add(e.code);
    e.preventDefault();
    e.stopImmediatePropagation();  // Aurora's SDL keyboard must not see it (we route it)
  }, true);
  window.addEventListener('keyup', (e) => {
    if (!owns(e.code)) return;
    down.delete(e.code);
    e.preventDefault();
    e.stopImmediatePropagation();
  }, true);
  window.addEventListener('blur', () => down.clear());

  function keyState(map) {
    const st = zero();
    let x = 0, y = 0;
    for (const code of down) {
      const v = map[code];
      if (v === undefined) continue;
      if (v === 'up') y += 127; else if (v === 'down') y -= 127;
      else if (v === 'left') x -= 127; else if (v === 'right') x += 127;
      else st.buttons |= v;
    }
    st.sx = Math.max(-127, Math.min(127, x));
    st.sy = Math.max(-127, Math.min(127, y));
    if (st.buttons & P.L) st.tl = 255;
    if (st.buttons & P.R) st.tr = 255;
    return st;
  }

  // Touch: touch.js calls Module._fsa_touch_set(buttons, sx, sy, tl, tr); while
  // co-op is on, the touch pad is a device like the others (the C side's own
  // touch merge into port 0 is bypassed by fsa_coop_enable).
  let touchSt = zero();
  const realTouch = Module._fsa_touch_set;
  Module._fsa_touch_set = (b, sx, sy, tl, tr) => {
    touchSt = { buttons: b & 0xFFFF, sx, sy, cx: 0, cy: 0, tl, tr };
    if (!enabled && realTouch) realTouch(b, sx, sy, tl, tr);
  };

  // Gamepads (standard mapping, as Aurora's SDL mapping).
  const axis = (v) => Math.round(Math.max(-1, Math.min(1, v || 0)) * 127);
  function padState(gp) {
    const st = zero(), b = gp.buttons || [], ax = gp.axes || [];
    const on = (i) => !!(b[i] && (b[i].pressed || b[i].value > 0.5));
    const MAP = [[0, P.A], [1, P.B], [2, P.X], [3, P.Y], [9, P.START], [8, P.Z], [4, P.L], [5, P.R],
                 [12, P.UP], [13, P.DOWN], [14, P.LEFT], [15, P.RIGHT]];
    for (const [i, bit] of MAP) if (on(i)) st.buttons |= bit;
    st.sx = axis(ax[0]); st.sy = -axis(ax[1]); st.cx = axis(ax[2]); st.cy = -axis(ax[3]);
    const trig = (i) => Math.round(Math.max(0, Math.min(1, b[i] ? b[i].value : 0)) * 255);
    st.tl = trig(6); st.tr = trig(7);
    if (st.tl > 200) st.buttons |= P.L;
    if (st.tr > 200) st.buttons |= P.R;
    // Dead zone, so an idle stick is not input.
    for (const k of ['sx', 'sy', 'cx', 'cy']) if (Math.abs(st[k]) < 20) st[k] = 0;
    return st;
  }
  const active = (st) => st.buttons || st.sx || st.sy || st.cx || st.cy || st.tl > 30 || st.tr > 30;

  // --- Players ------------------------------------------------------------------
  let enabled = false;
  const now = () => performance.now();
  function freeSlot() {
    for (let s = 1; s < SLOTS; s++) if (!(joined >> s & 1)) return s;  // a slot not in the game
    for (let s = 1; s < SLOTS; s++) if (!slots[s]) return s;           // in the game, nobody driving it
    return -1;
  }
  function request(slot, on) {
    if (!on && slot === 0) return true;
    const r = Module._fsa_coop_request(slot, on ? 1 : 0);
    if (r) joined = on ? joined | (1 << slot) : joined & ~(1 << slot);  // until the game's state says otherwise
    else log(`P${slot + 1} ${on ? 'join' : 'leave'} refused (${Module.UTF8ToString(Module._fsa_coop_why())})`);
    return !!r;
  }
  function join(d, slot) {
    if (net) return log('joining is off in a netplay room (each browser is one seat)'), false;
    if (d.slot >= 0) return false;
    if (slot === undefined) slot = freeSlot();
    if (slot < 0) return log('all four players are taken'), false;
    if (!(joined >> slot & 1) && !request(slot, true)) return false;
    // Player 1's own device is now P1's for good (it no longer just follows).
    if (p1Recent && p1Recent !== d.id && !slots[0]) {
      const p = devs.get(p1Recent);
      if (p && p.slot < 0) { slots[0] = p.id; p.slot = 0; }
    }
    slots[slot] = d.id;
    d.slot = slot;
    d.swallow = true;
    log(`${d.name} is P${slot + 1} (${NAMES[slot]})`);
    roster();
    return true;
  }
  function leave(slot) {
    const id = slots[slot];
    if (id) { const d = devs.get(id); if (d) { d.slot = -1; d.swallow = true; } }
    slots[slot] = null;
    if (slot > 0) request(slot, false);
    log(`P${slot + 1} left${id ? ' (' + devs.get(id).name + ')' : ''}`);
    roster();
  }
  function assign(id, slot) {
    const d = devs.get(id);
    if (!d) return false;
    if (d.slot >= 0) { slots[d.slot] = null; d.slot = -1; }
    if (slot < 0) { roster(); return true; }
    if (slots[slot]) { const o = devs.get(slots[slot]); if (o) o.slot = -1; slots[slot] = null; }
    return join(d, slot);
  }

  // Once a frame: read devices, apply joins / leaves, send the four pads.
  function poll() {
    const t = now();
    // The game's players: in the game or wanted (a join takes a few frames).
    if (Module._fsa_coop_state) {
      const st = Module._fsa_coop_state();
      const g = (st & 0xF) | ((st >> 8) & 0xF) | 1;
      if (g !== joined) {
        joined = g;
        for (let s = 1; s < SLOTS; s++)
          if (!(joined >> s & 1) && slots[s]) { const d = devs.get(slots[s]); if (d) d.slot = -1; slots[s] = null; }
      }
    }
    kbA.st = keyState(KEYS_A);
    kbB.st = keyState(KEYS_B);
    touch.st = touchSt;
    const gps = navigator.getGamepads ? navigator.getGamepads() : [];
    const seen = new Set();
    for (const gp of gps) {
      if (!gp || !gp.connected) continue;
      const id = 'pad' + gp.index;
      seen.add(id);
      const d = device(id, 'pad', (gp.id || 'Gamepad').replace(/\s*\(.*$/, '').slice(0, 24) || 'Gamepad ' + (gp.index + 1));
      d.lostAt = 0;
      d.st = padState(gp);
    }
    for (const d of devs.values()) {
      if (d.kind !== 'pad' || seen.has(d.id)) continue;
      d.st = zero();
      if (!d.lostAt) d.lostAt = t;
      if (t - d.lostAt > GRACE_MS) {  // unplugged: the player leaves
        if (d.slot > 0) leave(d.slot);
        else if (d.slot === 0) { slots[0] = null; d.slot = -1; }
        devs.delete(d.id);
        roster();
      }
    }
    for (const d of devs.values()) {
      const st = d.st;
      const start = !!(st.buttons & P.START);
      const pressed = start && !d.prevStart;
      d.prevStart = start;
      if (pressed && d.slot < 0 && !net) {
        // Join: set B's Start, or Start on a gamepad / the touch pad once some
        // other device is player 1's (the first device anyone uses is P1's).
        const p1Dev = slots[0] || p1Recent;
        if (d.kind === 'kbB' || ((d.kind === 'pad' || d.kind === 'touch') && p1Dev && p1Dev !== d.id)) join(d);
      }
      if (active(st)) {
        d.last = t;
        if (d.slot < 0 && !d.swallow && (!p1Recent || !(st.buttons & P.START))) p1Recent = d.id;
      }
      // Leave by holding Back / the set's leave key.
      const leaveHeld = d.kind === 'pad' ? (st.buttons & P.Z) && padBack(d) : d.kind in LEAVE_KEY && down.has(LEAVE_KEY[d.kind]);
      if (leaveHeld && d.slot > 0) {
        if (!d.leaveSince) d.leaveSince = t;
        else if (t - d.leaveSince > HOLD_LEAVE_MS) { d.leaveSince = 0; leave(d.slot); }
      } else d.leaveSince = 0;
      if (d.swallow) { if (!active(st)) d.swallow = false; }
    }
    // The four pads. A slot in the game but without a device is a connected,
    // idle pad (its Link stands still).
    for (let s = 0; s < SLOTS; s++) {
      const acc = zero();
      let any = false;
      for (const d of devs.values()) {
        if (d.swallow) continue;
        if (s === 0 ? d.slot === 0 || d.slot < 0 : d.slot === s) {
          const st = d.st;
          acc.buttons |= st.buttons;
          for (const k of ['sx', 'sy', 'cx', 'cy']) if (Math.abs(st[k]) > Math.abs(acc[k])) acc[k] = st[k];
          acc.tl = Math.max(acc.tl, st.tl); acc.tr = Math.max(acc.tr, st.tr);
          any = true;
        }
      }
      const connected = s === 0 || (joined >> s & 1) || any;
      Module._fsa_coop_pad(s, connected ? 1 : 0, acc.buttons, acc.sx, acc.sy, acc.cx, acc.cy, acc.tl, acc.tr);
    }
    tick();
  }
  // A gamepad's Back is mapped to Z (as Aurora does); a leave needs the Back
  // button itself, not a trigger mapped there.
  function padBack(d) {
    const gp = (navigator.getGamepads ? navigator.getGamepads() : [])[Number(d.id.slice(3))];
    return !!(gp && gp.buttons[8] && gp.buttons[8].pressed);
  }

  // --- Roster -------------------------------------------------------------------
  const css = document.createElement('style');
  css.textContent = `
#coop{position:fixed;top:44px;right:6px;z-index:60;display:flex;flex-direction:column;gap:3px;align-items:flex-end;
  font:11px/1.3 system-ui,sans-serif;color:#e6e6e6;pointer-events:none}
#coop .p{display:flex;align-items:center;gap:5px;background:rgba(0,0,0,.62);border-radius:5px;padding:2px 6px 2px 4px;
  border-left:4px solid var(--c)}
#coop .p.idle{opacity:.55}
#coop .p b{color:var(--c);font-weight:700}
#coop button{pointer-events:auto;font:inherit;line-height:1;padding:2px 6px;border-radius:4px;border:1px solid #555;
  background:#222;color:#ddd;cursor:pointer}
#coop .hint{background:rgba(0,0,0,.5);border-radius:5px;padding:2px 6px;opacity:.8}
#coop.min .hint{display:none}
#coop .flash{animation:coopflash .9s ease-out}
@keyframes coopflash{from{background:var(--c)}}`;
  document.head.append(css);
  const box = document.createElement('div');
  box.id = 'coop';
  box.setAttribute('aria-live', 'polite');
  document.body.append(box);
  let lastRoster = '';
  let hintUntil = now() + 15000;
  function roster(force) {
    const rows = [];
    for (let s = 0; s < SLOTS; s++) {
      if (!(joined >> s & 1)) continue;
      const id = slots[s];
      let dev = id ? devs.get(id).name : s === 0 ? (p1Recent ? devs.get(p1Recent).name : 'any device') : 'nobody';
      if (s === 0 && !id) dev = dev + (devs.size > 1 ? ' (all free devices)' : '');
      rows.push([s, dev, !id && s > 0]);
    }
    const key = JSON.stringify(rows) + (now() < hintUntil);
    if (key === lastRoster && !force) return;
    lastRoster = key;
    box.replaceChildren();
    for (const [s, dev, idle] of rows) {
      const r = document.createElement('div');
      r.className = 'p' + (idle ? ' idle' : '');
      r.style.setProperty('--c', COLORS[s]);
      const label = document.createElement('span');
      label.innerHTML = `<b>P${s + 1}</b> `;
      label.append(dev);
      r.append(label);
      if (s > 0 && !net) {
        const x = document.createElement('button');
        x.textContent = '×';
        x.title = `P${s + 1} leaves`;
        x.onclick = () => leave(s);
        r.append(x);
      }
      box.append(r);
    }
    if (!net && freeSlot() >= 0) {
      const j = document.createElement('button');
      j.textContent = '+ Join';
      j.title = 'A new player with the touch pad (or press Start on a gamepad, ; on the keyboard)';
      j.onclick = () => { if (!join(touch)) { const d = [...devs.values()].find((x) => x.slot < 0 && x.id !== p1Recent); if (d) join(d); } };
      box.append(j);
      if (now() < hintUntil) {
        const h = document.createElement('div');
        h.className = 'hint';
        h.textContent = 'Join: Start on another gamepad, ; (arrow keys player), or + Join';
        box.append(h);
      }
    }
  }
  function tick() {
    roster();
  }

  // --- GBA views on the shared screen ---------------------------------------------
  // In multiplayer a player in a house, cave or the underground sees that room
  // on their own GBA, not on the TV. Here every player shares the screen, so
  // the room view of each such player appears beside the 4:3 picture (in the
  // window's side space when there is room for it, e.g. a 16:9 window with the
  // 4:3 picture) or as a picture-in-picture corner over it, framed in the
  // player's colour, only while they are in a room. Drawn by the game's own
  // GameCube renderer of the GBA records (game/src/port/gba_gcview.cpp, exact
  // for rooms), or natively (platform/gba_view.cpp): ?coopview=gc|native|0.
  // With ?gba=N's own panel (fsa_gba.js) on, that panel shows the views instead.
  function views() {
    const vq = q.get('coopview');
    if (vq === '0' || !Module._fsa_gba_view_info) return;
    if (document.getElementById('gbapanel')) return;
    let vmode = vq === 'native' ? 'native' : 'gc';
    const info = Module._malloc(4 * 2048), serial = Module._malloc(4);
    const css2 = document.createElement('style');
    css2.textContent = `
.coopview{position:fixed;z-index:40;pointer-events:none;border:3px solid var(--c);border-radius:6px;background:#000;
  box-shadow:0 2px 10px rgba(0,0,0,.6);display:none}
.coopview canvas{display:block;width:100%;height:100%;image-rendering:pixelated;border-radius:3px}
.coopview span{position:absolute;left:4px;top:2px;font:700 11px/1.2 system-ui,sans-serif;color:var(--c);
  text-shadow:0 0 3px #000,0 0 3px #000}`;
    document.head.append(css2);
    const panes = [0, 1, 2, 3].map((p) => {
      const el = document.createElement('div');
      el.className = 'coopview';
      el.style.setProperty('--c', COLORS[p]);
      const c = document.createElement('canvas');
      c.width = 240; c.height = 160;
      const tag = document.createElement('span');
      tag.textContent = 'P' + (p + 1);
      el.append(c, tag);
      document.body.append(el);
      return { el, ctx: c.getContext('2d'), img: new ImageData(240, 160), shown: false };
    });
    let gcMask = -1;
    function inRoom(p) {
      if (!Module._fsa_gba_link_info(p, info)) return false;
      if (!new Uint32Array(HEAPU8.buffer, info, 1)[0]) return false;  // no GBA in that port
      Module._fsa_gba_view_info(p, info);
      const a = new Uint32Array(HEAPU8.buffer, info, 19);
      if (!a[17]) return false;  // no record yet
      const w0 = a[18];
      return ((w0 >>> 24) & 15) === 5 && ((w0 >>> 19) & 7) !== 0;
    }
    function layout(list) {
      const cv = document.getElementById('canvas');
      if (!cv) return;
      const r = cv.getBoundingClientRect();
      const left = r.left, right = innerWidth - r.right;
      const side = Math.max(left, right);
      const n = list.length;
      // Beside the picture: a column in the wider margin, as large as fits.
      let s = Math.min(2, (side - 16) / 240, (r.height - 8 * n) / n / 160);
      const beside = s >= 0.6;
      if (!beside) s = Math.max(0.45, Math.min(1.2, r.width * 0.3 / 240));
      list.forEach((p, i) => {
        const el = panes[p].el, w = 240 * s, h = 160 * s;
        let x, y;
        if (beside) {
          x = right >= left ? r.right + (right - w) / 2 : (left - w) / 2;
          y = r.top + (r.height - n * (h + 8)) / 2 + i * (h + 8);
        } else {  // picture-in-picture corners, by player number
          x = p & 1 ? r.right - w - 8 : r.left + 8;
          y = p & 2 ? r.bottom - h - 8 : r.top + 8;
        }
        Object.assign(el.style, { left: x + 'px', top: y + 'px', width: w + 'px', height: h + 'px' });
      });
    }
    function draw() {
      const st = Module._fsa_coop_state ? Module._fsa_coop_state() : 0;
      const playing = st & 0xF;
      const multi = (playing & 0xE) !== 0;  // one player: the game shows the room on the TV itself
      const list = [];
      for (let p = 0; p < 4; p++) if (multi && (playing >> p & 1) && inRoom(p)) list.push(p);
      const want = vmode === 'gc' ? list.reduce((m, p) => m | (1 << p), 0) : 0;
      if (want !== gcMask) { gcMask = want; Module._fsa_gba_gcview(want); }
      for (let p = 0; p < 4; p++) {
        const pane = panes[p], on = list.includes(p);
        if (on !== pane.shown) { pane.shown = on; pane.el.style.display = on ? 'block' : 'none'; }
        if (!on) continue;
        let ptr = vmode === 'gc' ? Module._fsa_gba_gcview_pixels(p, serial) : 0;
        if (!ptr) ptr = Module._fsa_gba_render(p);
        pane.img.data.set(HEAPU8.subarray(ptr, ptr + 240 * 160 * 4));
        pane.ctx.putImageData(pane.img, 0, 0);
      }
      if (list.length) layout(list);
    }
    setInterval(draw, 50);
    window.fsaCoopViews = { mode: (m) => { if (m) vmode = m; return vmode; } };
  }

  window.fsaCoop = {
    state: () => ({ joined, slots: slots.slice(), devices: [...devs.values()].map((d) => ({ id: d.id, kind: d.kind, name: d.name, slot: d.slot })) }),
    devices: () => [...devs.keys()],
    join: (id, slot) => join(id ? devs.get(id) || device(id, 'virtual', id) : touch, slot),
    leave,
    assign,
    request,
    poll,
    colors: COLORS,
  };

  views();
  if (harness) {
    // The script drives the pads; joins come from its steps ("set": {"coop_join": N}).
    log('off: a harness script drives the pads');
    return;
  }
  Module._fsa_coop_enable(1);
  enabled = true;
  Module.fsaCoopOn = true;
  for (let s = 1; s < startN; s++) request(s, true);
  if (net) log('netplay room: every device drives this browser\'s seat');
  const loop = () => { poll(); requestAnimationFrame(loop); };
  requestAnimationFrame(loop);
  setInterval(() => { if (document.hidden) poll(); }, 50);  // rAF stops in a hidden tab
  log('on: devices follow P1; press Start on another gamepad (or ; ) to join');
  roster(true);
};
