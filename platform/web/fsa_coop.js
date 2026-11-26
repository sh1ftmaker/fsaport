// --pre-js (deluxe): local co-op on one screen, the page's half
// (docs/MULTIPLAYER.md "Local co-op"; the game's half is platform/coop.cpp).
//
// An input-device core in the manner of katam-port's web/input_devices.js:
// every device is a potential player and people join and leave at any time.
//
//   devices   keyboard set A (WASD, Space, Left Shift, E, Q, Enter, Z, C, F),
//             keyboard set B (the arrows and . , / ; ' M and Right Shift, or the
//             numpad), the on-screen touch pad, and each Gamepad API pad:
//             Xbox / XInput pads, DualShock 4 / DualSense, Switch Pro, Joy-Cons,
//             8BitDo and others, recognised from gamepad.id and read with the
//             browser's "standard" mapping or a per-family raw layout
//             (fsa_pads.js; docs/MULTIPLAYER.md "Controllers"): face buttons,
//             Start, RB / LB = Z, analog triggers = L / R (click at full
//             press), d-pad, left stick, right stick = C stick.
//   follow    a device nobody has joined drives player 1. One person with any
//             device plays exactly as before.
//   join      Start or Guide on a gamepad that is not the one player 1 is
//             using (or A, as the press that makes the browser show the pad),
//             set B's Start, a parked device's Start, or Join in the roster:
//             the device becomes the next free player (P2..P4). That press
//             does not reach the game.
//   leave     hold Back / View (gamepad) / Backspace (set B) / ` (set A) for a
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
//   rumble    the game's PADControlMotor per port (platform/coop.cpp) goes to
//             the pad playing that port: vibrationActuator 'dual-rumble'
//             (Chrome, Edge, Safari) or hapticActuators (Firefox); stopped
//             when the player leaves, the pad goes, or the tab is hidden.
//
//   ?coop=0        off: Aurora's own keyboard / gamepad routing, as before
//   ?coop=N        start with players 1..N joined (devices still to claim)
//   ?rumble=0      no rumble (also the roster's Rumble toggle, remembered)
//   ?padtest=1     the controller test panel open at start (roster: Pads)
//   ?padface= ?deadzone= ?padjoin=   gamepad options (fsa_pads.js)
//   window.fsaCoop  state(), join(dev|null), leave(slot), assign(dev, slot), devices(),
//                   rumble(on?), motors(), padtest(on?)
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
      d = { id, kind, name, slot: -1, st: zero(), prevStart: false, swallow: false, leaveSince: 0, lostAt: 0, last: 0,
            fresh: true, gp: null, gpid: '', lg: null, rumbleAt: 0, rumbling: false };
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

  // Gamepads: fsa_pads.js recognises the pad and reads it with the browser's
  // standard mapping or its family's raw layout.
  const popts = FsaPads.options(q);
  function padRead(d, gp) {
    if (!d.lay || d.gpid !== gp.id || d.mapping !== gp.mapping) {
      d.lay = FsaPads.layoutFor(gp);
      d.gpid = gp.id;
      d.mapping = gp.mapping;
      d.mem = {};
      d.gmem = {};
      log(`${d.name}: ${d.lay.info.family}, mapping "${gp.mapping}", layout ${d.lay.name}`);
    }
    d.gp = gp;
    d.lg = FsaPads.read(gp, d.lay, d.mem);
    return FsaPads.toGC(d.lg, popts, d.gmem);
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
      padSeen = true;
      const id = 'pad' + gp.index;
      let d = devs.get(id);
      // Another controller on a reused index: the old one is gone.
      if (d && d.gpid && d.gpid !== gp.id) { padGone(d); d = null; }
      // A pad that went away and came back on another index keeps its player.
      if (!d) {
        const back = [...devs.values()].find((x) => x.kind === 'pad' && x.lostAt && x.gpid === gp.id);
        if (back) {
          devs.delete(back.id);
          if (p1Recent === back.id) p1Recent = id;
          back.id = id;
          devs.set(id, back);
          if (back.slot >= 0) slots[back.slot] = id;
          d = back;
        }
      }
      seen.add(id);
      if (!d) d = device(id, 'pad', FsaPads.identify(gp).name.slice(0, 24) || 'Gamepad ' + (gp.index + 1));
      if (d.lostAt) { log(`${d.name} is back`); d.lostAt = 0; }
      d.st = padRead(d, gp);
    }
    for (const d of [...devs.values()]) {
      if (d.kind !== 'pad' || seen.has(d.id)) continue;
      d.st = zero();
      d.lg = null;
      d.gp = null;
      if (!d.lostAt) d.lostAt = t;
      if (t - d.lostAt > GRACE_MS) padGone(d);  // unplugged: the player leaves
    }
    for (const d of devs.values()) {
      const st = d.st;
      // The join press: Start; on a gamepad also Guide, and A as the press
      // that made the browser show it (?padjoin=). Held, it stays "pressed"
      // until every one of them is up.
      let start = !!(st.buttons & P.START);
      if (d.kind === 'pad' && d.lg) {
        const lg = d.lg;
        start = (lg.start && popts.join.has('start')) || (lg.guide && popts.join.has('guide')) ||
                (d.fresh && lg.a && popts.join.has('a')) || (d.prevStart && (lg.start || lg.guide || lg.a));
      }
      const pressed = start && !d.prevStart;
      d.prevStart = start;
      if (pressed && d.slot < 0 && !net) {
        // Join: set B's Start, or Start on a gamepad / the touch pad once some
        // other device is player 1's (the first device anyone uses is P1's).
        const p1Dev = slots[0] || p1Recent;
        if (d.kind === 'kbB' || ((d.kind === 'pad' || d.kind === 'touch') && p1Dev && p1Dev !== d.id)) join(d);
      }
      if (active(st) || (d.lg && (d.lg.guide || d.lg.back))) {
        d.last = t;
        d.fresh = false;
        if (d.slot < 0 && !d.swallow && (!p1Recent || !(st.buttons & P.START))) p1Recent = d.id;
      }
      // Leave by holding Back / the set's leave key.
      const leaveHeld = d.kind === 'pad' ? !!(d.lg && d.lg.back) : d.kind in LEAVE_KEY && down.has(LEAVE_KEY[d.kind]);
      if (leaveHeld && d.slot > 0) {
        if (!d.leaveSince) d.leaveSince = t;
        else if (t - d.leaveSince > HOLD_LEAVE_MS) { d.leaveSince = 0; leave(d.slot); }
      } else d.leaveSince = 0;
      if (d.swallow) { if (!active(st) && !(d.lg && d.lg.guide)) d.swallow = false; }
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
    rumbleTick(t);
    tick();
  }
  function padGone(d) {
    if (d.slot > 0) leave(d.slot);
    else if (d.slot === 0) { slots[0] = null; d.slot = -1; }
    if (p1Recent === d.id) p1Recent = null;
    rumbleStop(d);
    devs.delete(d.id);
    log(`${d.name} disconnected`);
    roster();
  }
  let padSeen = false;
  addEventListener('gamepadconnected', (e) => {
    padSeen = true;
    log(`gamepad ${e.gamepad.index} connected: ${e.gamepad.id} (mapping "${e.gamepad.mapping}")`);
    if (enabled) roster();
  });
  addEventListener('gamepaddisconnected', (e) => {
    const d = devs.get('pad' + e.gamepad.index);
    log(`gamepad ${e.gamepad.index} disconnected: ${e.gamepad.id}`);
    if (d && !d.lostAt) { d.lostAt = now(); rumbleStop(d); d.gp = null; }
  });

  // --- Rumble ---------------------------------------------------------------------
  // The game's motor commands per port (Module.fsaPadMotor, from
  // platform/coop.cpp's hook in Aurora's PADControlMotor; PAD_MOTOR_STOP 0,
  // RUMBLE 1, STOP_HARD 2) go to the pad playing that port; port 0 without an
  // owner: the pad player 1 used last. (The game itself rumbles port 0 only:
  // GameRoot startRumble_80134DE4 and its frame update use getGamePad(0); a
  // multiplayer player is a GBA, which has no motor.) A GameCube pad has one
  // on/off motor; here it is both motors of a dual-rumble pad, strong 0.6 and
  // weak 0.4 (Aurora's default intensity is 0.5 / 0.5, pad.cpp
  // m_rumbleIntensityLow/High). While on, the effect is renewed every 150 ms
  // for 250 ms (Chrome caps an effect at 5 s, and a stop that never comes must
  // not leave a pad buzzing).
  const motor = [0, 0, 0, 0];
  let rumbleOn = q.get('rumble') !== '0';
  try { if (q.get('rumble') === null && localStorage.getItem('fsaRumble') === '0') rumbleOn = false; } catch (e) {}
  let rumbleCalls = 0;
  Module.fsaPadMotor = (chan, cmd) => {
    if (chan >= 0 && chan < SLOTS) motor[chan] = cmd === 1 ? 1 : 0;
  };
  function actuate(d, on) {
    const gp = d.gp;
    if (!gp) return;
    try {
      const va = gp.vibrationActuator;
      if (va && va.playEffect && (!va.effects || va.effects.includes('dual-rumble'))) {
        rumbleCalls++;
        if (on) va.playEffect('dual-rumble', { startDelay: 0, duration: 250, strongMagnitude: 0.6, weakMagnitude: 0.4 }).catch(() => {});
        else if (va.reset) va.reset().catch(() => {});
        else va.playEffect('dual-rumble', { startDelay: 0, duration: 0, strongMagnitude: 0, weakMagnitude: 0 }).catch(() => {});
        return;
      }
      const ha = gp.hapticActuators && gp.hapticActuators[0];
      if (ha && ha.pulse) { rumbleCalls++; ha.pulse(on ? 0.6 : 0, on ? 250 : 0); }
    } catch (e) {}
  }
  function rumbleStop(d) { if (d.rumbling) { actuate(d, false); d.rumbling = false; } }
  function rumbleTick(t) {
    const hidden = document.hidden;
    for (const d of devs.values()) {
      if (d.kind !== 'pad') continue;
      const port = d.slot >= 0 ? d.slot : d.id === p1Recent && !slots[0] ? 0 : -1;
      const want = rumbleOn && !hidden && !d.lostAt && port >= 0 && motor[port] === 1;
      if (want && (!d.rumbling || t - d.rumbleAt > 150)) { actuate(d, true); d.rumbling = true; d.rumbleAt = t; }
      else if (!want && d.rumbling) rumbleStop(d);
    }
  }
  document.addEventListener('visibilitychange', () => { if (document.hidden) for (const d of devs.values()) rumbleStop(d); });
  function setRumble(on) {
    rumbleOn = !!on;
    try { localStorage.setItem('fsaRumble', rumbleOn ? '1' : '0'); } catch (e) {}
    if (!rumbleOn) for (const d of devs.values()) rumbleStop(d);
    log('rumble ' + (rumbleOn ? 'on' : 'off'));
    roster(true);
    return rumbleOn;
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
    const key = JSON.stringify(rows) + (now() < hintUntil) + padSeen + rumbleOn + !!testPanel;
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
    const bar = document.createElement('div');
    bar.className = 'bar';
    const btn = (text, title, fn, on) => {
      const b = document.createElement('button');
      b.textContent = text;
      b.title = title;
      if (on !== undefined) b.setAttribute('aria-pressed', on ? 'true' : 'false');
      b.onclick = fn;
      bar.append(b);
    };
    if (!net && freeSlot() >= 0)
      btn('+ Join', 'A new player with the touch pad (or press Start / Guide on a gamepad, ; on the keyboard)',
          () => { if (!join(touch)) { const d = [...devs.values()].find((x) => x.slot < 0 && x.id !== p1Recent); if (d) join(d); } });
    if (padSeen) btn(rumbleOn ? 'Rumble on' : 'Rumble off', 'Controller rumble (?rumble=0 turns it off)', () => setRumble(!rumbleOn), rumbleOn);
    btn('Pads', 'Controller test: each pad\'s raw buttons and axes and what the game gets', () => padtest(!testPanel), !!testPanel);
    box.append(bar);
    // Browsers show no gamepad to a page until one of its buttons is pressed.
    if (!padSeen && navigator.getGamepads) {
      const h = document.createElement('div');
      h.className = 'hint';
      h.textContent = 'Controller? Press any button on it to connect it';
      box.append(h);
    }
    if (!net && freeSlot() >= 0 && now() < hintUntil) {
      const h = document.createElement('div');
      h.className = 'hint';
      h.textContent = 'Join: Start or Guide on another gamepad, ; (arrow keys player), or + Join';
      box.append(h);
    }
  }
  function tick() {
    roster();
  }

  // --- Controller test panel (?padtest=1, roster: Pads) ----------------------------
  // Each pad the browser shows: its id, mapping and the layout read with, the
  // raw buttons (lit when pressed, analog value under it) and axes, the GameCube
  // pad it makes and the player it drives, and a rumble test.
  let testPanel = null;
  function padtest(on) {
    if (on === undefined) return !!testPanel;
    if (!on) { if (testPanel) { clearInterval(testPanel.timer); testPanel.el.remove(); testPanel = null; } roster(true); return false; }
    if (testPanel) return true;
    const el = document.createElement('div');
    el.id = 'padtest';
    el.innerHTML = '<div class="hd"><b>Controllers</b> <span class="sub">press a button on a pad to show it</span>' +
                   '<button type="button" class="x" title="Close">×</button></div><div class="pads"></div>';
    el.querySelector('.x').onclick = () => padtest(false);
    document.body.append(el);
    const list = el.querySelector('.pads');
    const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);
    const draw = () => {
      const gps = [...(navigator.getGamepads ? navigator.getGamepads() : [])].filter((g) => g && g.connected);
      if (!gps.length) { list.innerHTML = '<div class="none">No controller yet. Browsers show a pad only after one of its buttons is pressed on this page.</div>'; return; }
      list.innerHTML = gps.map((gp) => {
        const d = devs.get('pad' + gp.index);
        const lay = d && d.lay ? d.lay : FsaPads.layoutFor(gp);
        const st = d ? d.st : FsaPads.toGC(FsaPads.read(gp, lay, {}), popts, {});
        const who = !d ? '' : d.slot >= 0 ? `P${d.slot + 1}` : d.id === p1Recent || !p1Recent ? 'P1 (follows)' : 'free (Start / Guide joins)';
        const col = d && d.slot >= 0 ? COLORS[d.slot] : '#888';
        const bs = [...gp.buttons].map((b, i) => `<span class="b${b.pressed ? ' on' : ''}" title="button ${i}">${i}` +
                     (b.value > 0 && b.value < 1 ? `<i>${b.value.toFixed(2)}</i>` : '') + '</span>').join('');
        const as = [...gp.axes].map((v, i) => `<span class="a" title="axis ${i}"><em style="left:${(Math.max(-1, Math.min(1, v)) + 1) * 50}%"></em>` +
                     `<i>${i}: ${v.toFixed(2)}</i></span>`).join('');
        const lg = d && d.lg ? Object.keys(d.lg).filter((k) => d.lg[k] === true).join(' ') : '';
        return `<div class="pad" style="--c:${col}"><div class="t"><b>#${gp.index}</b> ${esc(gp.id)}</div>` +
               `<div class="m">mapping "${esc(gp.mapping)}" · ${esc(lay.info.family)} · layout <b>${esc(lay.name)}</b>` +
               ` · <span style="color:${col}">${esc(who)}</span>` +
               `${d && d.rumbling ? ' · <b>rumbling</b>' : ''}${gp.vibrationActuator || gp.hapticActuators ? '' : ' · no rumble API'}</div>` +
               `<div class="row">${bs}</div><div class="row">${as}</div>` +
               `<div class="gc">GameCube: <b>${FsaPads.buttonNames(st.buttons).join(' ') || '-'}</b>` +
               ` stick ${st.sx},${st.sy} C ${st.cx},${st.cy} L ${st.tl} R ${st.tr}` +
               (lg ? ` <span class="sub">(${esc(lg)})</span>` : '') + '</div>' +
               `<button type="button" data-rumble="${gp.index}">Test rumble</button></div>`;
      }).join('');
    };
    list.addEventListener('click', (e) => {
      const i = e.target.dataset && e.target.dataset.rumble;
      if (i === undefined) return;
      const d = devs.get('pad' + i);
      if (d) { actuate(d, true); setTimeout(() => actuate(d, false), 400); }
    });
    draw();
    testPanel = { el, timer: setInterval(draw, 100) };
    roster(true);
    return true;
  }
  const css3 = document.createElement('style');
  css3.textContent = `
#coop .bar{display:flex;gap:3px;justify-content:flex-end;flex-wrap:wrap}
#coop button[aria-pressed=false]{opacity:.7}
#padtest{position:fixed;left:50%;top:44px;transform:translateX(-50%);z-index:70;width:min(720px,calc(100vw - 32px));
  max-height:calc(100vh - 60px);overflow:auto;background:rgba(14,16,20,.94);color:#e6e6e6;border:1px solid #444;
  border-radius:8px;padding:8px 10px;font:12px/1.35 system-ui,sans-serif;box-shadow:0 4px 18px rgba(0,0,0,.6)}
#padtest .hd{display:flex;align-items:center;gap:8px;margin-bottom:6px}
#padtest .hd .x{margin-left:auto}
#padtest .sub{opacity:.65}
#padtest button{font:inherit;padding:2px 8px;border-radius:4px;border:1px solid #555;background:#222;color:#ddd;cursor:pointer}
#padtest .pad{border-left:4px solid var(--c);padding:4px 8px;margin:6px 0;background:rgba(255,255,255,.04);border-radius:4px}
#padtest .t{font-weight:600;word-break:break-all}
#padtest .m,#padtest .gc{margin:2px 0}
#padtest .row{display:flex;flex-wrap:wrap;gap:3px;margin:3px 0}
#padtest .b{min-width:22px;text-align:center;border:1px solid #555;border-radius:4px;padding:1px 2px;font-variant-numeric:tabular-nums}
#padtest .b.on{background:var(--c);color:#000;border-color:var(--c)}
#padtest .b i,#padtest .a i{display:block;font-style:normal;font-size:10px;opacity:.8}
#padtest .a{position:relative;width:84px;height:28px;border:1px solid #555;border-radius:4px;overflow:hidden}
#padtest .a em{position:absolute;top:0;bottom:0;width:3px;margin-left:-1px;background:var(--c)}
#padtest .a i{position:absolute;left:4px;bottom:1px}
#padtest .none{opacity:.75;padding:6px 0}`;
  document.head.append(css3);

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
    state: () => ({ joined, slots: slots.slice(), devices: [...devs.values()].map((d) => ({ id: d.id, kind: d.kind, name: d.name, slot: d.slot,
      layout: d.lay ? d.lay.name : undefined, family: d.lay ? d.lay.info.family : undefined,
      rumbling: d.rumbling || undefined, lost: !!d.lostAt || undefined, pad: d.kind === 'pad' ? d.st : undefined })) }),
    devices: () => [...devs.keys()],
    join: (id, slot) => join(id ? devs.get(id) || device(id, 'virtual', id) : touch, slot),
    leave,
    assign,
    request,
    poll,
    rumble: (on) => (on === undefined ? rumbleOn : setRumble(on)),
    motors: () => ({ motor: motor.slice(), calls: rumbleCalls }),
    padtest,
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
  log('on: devices follow P1; press Start or Guide on another gamepad (or ; ) to join');
  roster(true);
  if (q.get('padtest') === '1') padtest(true);
};
