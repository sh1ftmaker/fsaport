#!/usr/bin/env node
// Live test of the co-op gamepad path (docs/MULTIPLAYER.md "Controllers") with
// fake Gamepad API pads injected over CDP: navigator.getGamepads is replaced
// before the page loads, gamepadconnected / gamepaddisconnected are fired as a
// browser would, and every pad has a stub vibrationActuator that records its
// calls. No physical controller is involved.
//
//   node tools/web_pads_test.mjs --cdp 9377 --url 'http://127.0.0.1:8857/?iso=local.iso&stage=1-0&nosave=1' \
//        --out logs/xinput/live [--wait 60]
//
// Steps: the "press any button" hint; an Xbox pad (XInput, "standard") shows
// itself with A and plays P1; a raw-mapped Xbox pad (Firefox / Linux layout,
// mapping "") joins with Start, a standard Xbox Series pad with Guide, a raw
// DualSense with its first press (cross); each plays its own port (buttons,
// trigger, d-pad, sticks as fsa_coop_pad gets them); the game's own rumble
// request (Module._fsa_debug_rumble) reaches P1's pad and stops; another
// port's motor reaches that port's pad; the Rumble toggle; holding View
// leaves (and stops that pad's rumble); a pad unplugged and plugged back
// within the grace keeps its player, unplugged for longer it leaves; another
// controller on the same index is a new device; the test panel. Writes
// result.json (every check) and screenshots to --out.
import fs from 'node:fs';
import path from 'node:path';

const args = {};
for (let i = 2; i < process.argv.length; i++) {
  const a = process.argv[i];
  if (a.startsWith('--')) args[a.slice(2)] = process.argv[i + 1] && !process.argv[i + 1].startsWith('--') ? process.argv[++i] : 'true';
}
const cdp = Number(args.cdp || 9377);
const url = args.url || 'http://127.0.0.1:8857/?iso=local.iso&stage=1-0&nosave=1';
const out = args.out || 'build/web-run/pads';
const waitS = Number(args.wait || 60);
fs.mkdirSync(out, { recursive: true });
const logFile = fs.createWriteStream(path.join(out, 'console.log'));
const log = (s) => logFile.write(s + '\n');

const FAKE = `(() => {
  const pads = [null, null, null, null];
  window.__rumble = [];
  const mk = (i, spec) => ({
    id: spec.id, index: i, mapping: spec.mapping, connected: true, timestamp: performance.now(),
    buttons: Array.from({ length: spec.nb }, () => ({ pressed: false, touched: false, value: 0 })),
    axes: Array.from({ length: spec.na }, (_, k) => (spec.rest && spec.rest[k] !== undefined ? spec.rest[k] : 0)),
    vibrationActuator: spec.noRumble ? null : {
      type: 'dual-rumble', effects: ['dual-rumble', 'trigger-rumble'],
      playEffect: (type, p) => { __rumble.push(Object.assign({ t: performance.now(), pad: i, type }, p)); return Promise.resolve('complete'); },
      reset: () => { __rumble.push({ t: performance.now(), pad: i, type: 'reset' }); return Promise.resolve('complete'); },
    },
  });
  navigator.getGamepads = () => pads.map((p) => p && Object.assign({}, p, { buttons: p.buttons.map((b) => Object.assign({}, b)), axes: p.axes.slice() }));
  const fire = (type, g) => { const e = new Event(type); Object.defineProperty(e, 'gamepad', { value: g }); window.dispatchEvent(e); };
  const setB = (i, k, v) => { const b = pads[i].buttons[k]; b.value = v; b.pressed = v > 0.5; b.touched = v > 0; pads[i].timestamp = performance.now(); };
  window.__pad = {
    plug(i, spec, press) { pads[i] = mk(i, spec); for (const k in press || {}) setB(i, k, press[k]); fire('gamepadconnected', pads[i]); },
    unplug(i) { const g = pads[i]; pads[i] = null; if (g) fire('gamepaddisconnected', Object.assign({}, g, { connected: false })); },
    btn(i, k, v) { setB(i, k, v); },
    axis(i, k, v) { pads[i].axes[k] = v; pads[i].timestamp = performance.now(); },
  };
})();`;

const tab = await (await fetch(`http://127.0.0.1:${cdp}/json/new?about:blank`, { method: 'PUT' })).json();
const ws = new WebSocket(tab.webSocketDebuggerUrl);
await new Promise((r) => ws.addEventListener('open', r, { once: true }));
let nextId = 1;
const pending = new Map();
ws.addEventListener('message', (ev) => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); return; }
  if (m.method === 'Runtime.consoleAPICalled') log(`[${m.params.type}] ` + m.params.args.map((a) => a.value ?? a.description ?? '').join(' '));
  else if (m.method === 'Runtime.exceptionThrown') log('[exception] ' + (m.params.exceptionDetails.exception?.description || m.params.exceptionDetails.text));
});
const send = (method, params = {}) => new Promise((resolve) => {
  const id = nextId++;
  pending.set(id, resolve);
  ws.send(JSON.stringify({ id, method, params }));
});
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const ev = async (expr) => {
  const r = await send('Runtime.evaluate', { expression: expr, returnByValue: true, awaitPromise: true });
  if (r.result?.exceptionDetails) throw new Error(expr + ': ' + (r.result.exceptionDetails.exception?.description || r.result.exceptionDetails.text));
  return r.result?.result?.value;
};
let shotN = 0;
const shot = async (name) => {
  const r = await send('Page.captureScreenshot', { format: 'png' });
  if (r.result?.data) fs.writeFileSync(path.join(out, `${String(++shotN).padStart(2, '0')}-${name}.png`), Buffer.from(r.result.data, 'base64'));
};
const checks = [];
const check = (name, ok, detail) => { checks.push({ name, ok: !!ok, detail }); console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${detail !== undefined ? ' ' + JSON.stringify(detail) : ''}`); };

await send('Runtime.enable');
await send('Page.enable');
await send('Page.addScriptToEvaluateOnNewDocument', { source: FAKE });
await send('Page.navigate', { url });
// The game boots straight into the stage (?stage=); the co-op core starts with it.
for (let i = 0; i < 120 && !(await ev('!!window.fsaCoop')); i++) await sleep(1000);
await sleep(waitS * 1000);
// What each port's pad is, as fsa_coop_pad gets it: the latest, and the bits
// seen since the last clear.
await ev(`(() => { const o = Module._fsa_coop_pad; window.__ports = []; window.__seen = [0, 0, 0, 0];
  Module._fsa_coop_pad = (...a) => { __ports[a[0]] = a.slice(1); if (a[1]) __seen[a[0]] |= a[2]; return o(...a); }; })()`);
const port = (p) => ev(`__ports[${p}] && { c: __ports[${p}][0], b: __ports[${p}][1], sx: __ports[${p}][2], sy: __ports[${p}][3], cx: __ports[${p}][4], cy: __ports[${p}][5], tl: __ports[${p}][6], tr: __ports[${p}][7] }`);
const seen = async (p) => { const v = await ev(`__seen[${p}]`); await ev(`__seen[${p}] = 0`); return v; };
const state = () => ev('fsaCoop.state()');
const game = () => ev('Module._fsa_coop_state() & 0xF');
const rumbles = () => ev('__rumble.slice()');
const P = { A: 0x100, B: 0x200, X: 0x400, Y: 0x800, START: 0x1000, Z: 0x10, L: 0x40, R: 0x20, LEFT: 1, RIGHT: 2, DOWN: 4, UP: 8 };

const XBOX_STD = { id: 'Xbox 360 Controller (XInput STANDARD GAMEPAD)', mapping: 'standard', nb: 17, na: 4 };
const XBOX_RAW = { id: '045e-028e-Microsoft X-Box 360 pad', mapping: '', nb: 11, na: 8, rest: { 2: -1, 5: -1 } };
const SERIES_STD = { id: 'Xbox Wireless Controller (STANDARD GAMEPAD Vendor: 045e Product: 0b13)', mapping: 'standard', nb: 17, na: 4 };
const DUALSENSE_RAW = { id: '054c-0ce6-Sony Interactive Entertainment DualSense Wireless Controller', mapping: '', nb: 13, na: 8, rest: { 2: -1, 5: -1 } };
const SWITCH_STD = { id: 'Pro Controller (STANDARD GAMEPAD Vendor: 057e Product: 2009)', mapping: 'standard', nb: 18, na: 4 };

// 1. No pad yet: the hint.
check('hint "press any button" while no pad is seen', (await ev(`document.getElementById('coop').textContent`)).includes('Press any button'));
await shot('no-pad-hint');

// 2. An XInput pad shows itself with A: nobody else has played, so it is P1's
//    (the press reaches the game), and it plays port 0.
await ev(`__pad.plug(0, ${JSON.stringify(XBOX_STD)}, { 0: 1 })`);
await sleep(300);
check('Xbox (standard) A reveal -> P1 follows, A reaches port 0', (await seen(0)) & P.A);
await ev('__pad.btn(0, 0, 0); __pad.axis(0, 0, -1)');
await sleep(1500);
let p0 = await port(0);
check('Xbox (standard) stick left -> port 0 stick x -127', p0.sx === -127, p0);
await ev('__pad.axis(0, 0, 0); __pad.btn(0, 7, 1); __pad.btn(0, 5, 1)');
await sleep(300);
p0 = await port(0);
check('Xbox (standard) RT full + RB -> port 0 R analog 255 + R + Z', p0.tr === 255 && (p0.b & P.R) && (p0.b & P.Z), p0);
await ev('__pad.btn(0, 7, 0); __pad.btn(0, 5, 0)');
check('hint gone once a pad is seen', !(await ev(`document.getElementById('coop').textContent`)).includes('Press any button'));

// 3. A raw-mapped Xbox pad (Firefox on Linux: mapping "") joins with Start.
await seen(1); await seen(0);
await ev(`__pad.plug(1, ${JSON.stringify(XBOX_RAW)}, { 7: 1 })`);
await sleep(400);
let st = await state();
check('Xbox (raw, Firefox Linux layout) Start -> P2', st.slots[1] === 'pad1', st.devices.find((d) => d.id === 'pad1'));
check('the joining Start reached no port', !(((await seen(1)) | (await seen(0))) & P.START));
await ev('__pad.btn(1, 7, 0)');
await sleep(3000);
check('P2 in the game', ((await game()) & 2) !== 0, await game());
await ev('__pad.btn(1, 0, 1); __pad.axis(1, 2, 1); __pad.axis(1, 6, -1); __pad.axis(1, 4, -1)');
await sleep(300);
let p1 = await port(1);
check('P2: A, LT axis 2 full (L 255 + click), d-pad axis 6 left, right stick up (axis 4) -> port 1', p1.b === (P.A | P.L | P.LEFT) && p1.tl === 255 && p1.cy === 127, p1);
p0 = await port(0);
check('...and nothing of it on port 0', !(p0.b & P.A) && p0.tl === 0, p0);
await ev('__pad.btn(1, 0, 0); __pad.axis(1, 2, -1); __pad.axis(1, 6, 0); __pad.axis(1, 4, 0)');

// 4. A standard Xbox Series pad joins with Guide (button 16).
await ev(`__pad.plug(2, ${JSON.stringify(SERIES_STD)}, { 16: 1 })`);
await sleep(400);
st = await state();
check('Xbox Series (standard) Guide -> P3', st.slots[2] === 'pad2');
await ev('__pad.btn(2, 16, 0)');
await sleep(2500);
await ev('__pad.btn(2, 2, 1); __pad.axis(2, 1, 0.5)');
await sleep(300);
const p2 = await port(2);
check('P3: X and half stick down -> port 2', p2.b === P.X && p2.sy < -50 && p2.sy > -90, p2);
await ev('__pad.btn(2, 2, 0); __pad.axis(2, 1, 0)');

// 5. A raw DualSense (Linux layout) joins with its first press, cross (= A).
await seen(3); await seen(0);
await ev(`__pad.plug(3, ${JSON.stringify(DUALSENSE_RAW)}, { 0: 1 })`);
await sleep(400);
st = await state();
check('DualSense (raw) first press (cross) -> P4', st.slots[3] === 'pad3', st.devices.find((d) => d.id === 'pad3'));
check('the joining cross reached no port', !(((await seen(3)) | (await seen(0))) & P.A));
await ev('__pad.btn(3, 0, 0)');
await sleep(2500);
check('four players in the game', (await game()) === 15, await game());
await ev('__pad.btn(3, 3, 1)');  // square (west) -> X
await sleep(300);
check('P4: square -> X on port 3', (await port(3)).b === P.X, await port(3));
await ev('__pad.btn(3, 3, 0)');
await sleep(500);
await shot('four-players');

// 6. Rumble: the game's own request on port 0 goes to P1's pad and stops.
await ev('__rumble.length = 0');
const opt = await ev('Module._fsa_debug_rumble(1)');
check('the game\'s rumble option is on', opt === 1, opt);
await sleep(1500);
let r = await rumbles();
const r0 = r.filter((e) => e.pad === 0);
check('game rumble (kind 1, 30 frames) -> pad 0 dual-rumble effects, then a reset', r0.some((e) => e.type === 'dual-rumble' && e.strongMagnitude > 0) && r0[r0.length - 1].type === 'reset',
      { effects: r0.filter((e) => e.type === 'dual-rumble').length, last: r0[r0.length - 1], span: r0.length ? Math.round(r0[r0.length - 1].t - r0[0].t) : 0 });
check('...and no other pad rumbled', r.every((e) => e.pad === 0), r.filter((e) => e.pad !== 0));
check('motor commands came through PADControlMotor', (await ev('fsaCoop.motors()')).motor[0] === 0);
// Another port: its own pad (the game itself rumbles port 0 only; this is the
// port-side routing).
await ev('__rumble.length = 0; Module.fsaPadMotor(2, 1)');
await sleep(500);
r = await rumbles();
check('port 2 motor on -> pad 2 rumbles (renewed)', r.filter((e) => e.pad === 2 && e.type === 'dual-rumble').length >= 2 && r.every((e) => e.pad === 2), r.length);
// Leaving stops it: hold Back (standard 8) on pad 2.
await ev('__rumble.length = 0; __pad.btn(2, 8, 1)');
await sleep(2000);
await ev('__pad.btn(2, 8, 0); Module.fsaPadMotor(2, 0)');
st = await state();
r = await rumbles();
check('pad 2 holds View -> P3 leaves', st.slots[2] === null, st.slots);
check('...and its rumble stops', r.some((e) => e.pad === 2 && e.type === 'reset'), r.filter((e) => e.pad === 2).slice(-2));
// The toggle.
await ev('__rumble.length = 0; fsaCoop.rumble(false)');
await ev('Module._fsa_debug_rumble(1)');
await sleep(1000);
check('Rumble off -> no effect', (await rumbles()).filter((e) => e.type === 'dual-rumble').length === 0);
check('roster shows "Rumble off"', (await ev(`document.getElementById('coop').textContent`)).includes('Rumble off'));
await ev('fsaCoop.rumble(true)');

// 7. Leave by holding View on the raw Xbox pad (its Back is button 6).
await ev('__pad.btn(1, 6, 1)');
await sleep(800);
check('View held 0.8 s: still P2', (await state()).slots[1] === 'pad1');
await sleep(1200);
await ev('__pad.btn(1, 6, 0)');
st = await state();
check('View held 2 s -> P2 leaves', st.slots[1] === null, st.slots);
await sleep(2000);
check('P2 out of the game', ((await game()) & 2) === 0, await game());
// Start again: back in.
await ev('__pad.btn(1, 7, 1)');
await sleep(400);
await ev('__pad.btn(1, 7, 0)');
check('Start again -> a player again', (await state()).slots.includes('pad1'), (await state()).slots);

// 8. Unplug and plug back within the grace: same player.
await ev('__pad.unplug(3)');
await sleep(1000);
st = await state();
check('pad 3 unplugged 1 s: P4 kept (grace)', st.slots[3] === 'pad3' && st.devices.find((d) => d.id === 'pad3').lost, st.devices.find((d) => d.id === 'pad3'));
await ev(`__pad.plug(3, ${JSON.stringify(DUALSENSE_RAW)}, { 0: 1 })`);
await sleep(300);
st = await state();
check('plugged back (with a press): still P4, the press not a new join', st.slots[3] === 'pad3' && !st.devices.find((d) => d.id === 'pad3').lost, st.slots);
await ev('__pad.btn(3, 0, 0)');
await ev('__pad.unplug(3)');
await sleep(4000);
st = await state();
check('unplugged 4 s -> P4 leaves, the device is gone', st.slots[3] === null && !st.devices.some((d) => d.id === 'pad3'), st.slots);
// Another controller on the reused index.
await ev(`__pad.plug(3, ${JSON.stringify(SWITCH_STD)}, { 9: 1 })`);
await sleep(400);
st = await state();
const sw = st.devices.find((d) => d.id === 'pad3');
check('a Switch Pro on index 3 is a new device and joins with +', sw && sw.family === 'switch' && st.slots.includes('pad3'), sw);
await ev('__pad.btn(3, 9, 0)');
await sleep(500);

// 9. The test panel.
await ev('fsaCoop.padtest(true)');
await ev('__pad.btn(0, 0, 1); __pad.axis(0, 0, 0.6); __pad.btn(0, 6, 0.5)');
await sleep(400);
const pads = await ev(`document.querySelectorAll('#padtest .pad').length`);
check('test panel shows every pad', pads === 4, pads);
const gcText = await ev(`document.querySelector('#padtest .pad .gc').textContent`);
check('test panel: pad 0 GameCube state', /A/.test(gcText) && /stick \d+,0/.test(gcText), gcText);
await shot('padtest');
await ev('__pad.btn(0, 0, 0); __pad.axis(0, 0, 0); __pad.btn(0, 6, 0); fsaCoop.padtest(false)');

const fails = checks.filter((c) => !c.ok).length;
fs.writeFileSync(path.join(out, 'result.json'), JSON.stringify({ url, checks, fails }, null, 1));
console.log(`${checks.length - fails}/${checks.length} checks passed`);
await send('Page.close').catch(() => {});
await fetch(`http://127.0.0.1:${cdp}/json/close/${tab.id}`).catch(() => {});
process.exit(fails ? 1 : 0);
