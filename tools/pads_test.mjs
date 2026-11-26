#!/usr/bin/env node
// Unit tests of the gamepad tables (platform/web/fsa_pads.js; docs/MULTIPLAYER.md
// "Controllers"): Gamepad API snapshots for each controller family, in the
// browser's "standard" mapping and in each raw ("") layout, through
// identify / layoutFor / read / toGC. The snapshots are synthetic, built as the
// browsers report these pads (button / axis counts and orders as in the layout
// sources cited in fsa_pads.js); no physical controller was recorded.
//
//   node tools/pads_test.mjs        (exit 1 on a failure)
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const F = require('../platform/web/fsa_pads.js');
const { P } = F;

let fails = 0, passes = 0;
function ok(cond, what) {
  if (cond) passes++;
  else { fails++; console.log('FAIL ' + what); }
}
function eq(a, b, what) { ok(JSON.stringify(a) === JSON.stringify(b), `${what}: got ${JSON.stringify(a)}, want ${JSON.stringify(b)}`); }

// A Gamepad snapshot: nb buttons, na axes (rest values `rest`), with
// overrides {b: {i: value}, a: {i: value}}.
function pad(id, mapping, nb, na, o = {}, rest = {}) {
  const buttons = [];
  for (let i = 0; i < nb; i++) {
    const v = o.b && o.b[i] !== undefined ? o.b[i] : 0;
    buttons.push({ pressed: v > 0.5 || (o.b && o.b[i] === 1), touched: v > 0, value: v });
  }
  const axes = [];
  for (let i = 0; i < na; i++) axes.push(o.a && o.a[i] !== undefined ? o.a[i] : rest[i] !== undefined ? rest[i] : 0);
  return { id, mapping, index: 0, connected: true, buttons, axes, timestamp: 0 };
}
const OPTS = F.options({});
const gc = (gp, platform, mem = {}, gmem = {}, opts = OPTS) => {
  const lay = F.layoutFor(gp, platform);
  return { lay, lg: F.read(gp, lay, mem), st: F.toGC(F.read(gp, lay, mem), opts, gmem) };
};

// --- identify -------------------------------------------------------------------------
const IDS = [
  ['Xbox 360 Controller (XInput STANDARD GAMEPAD)', 'xbox', '', ''],                         // Chrome / Edge, Windows
  ['Xbox Wireless Controller (STANDARD GAMEPAD Vendor: 045e Product: 0b13)', 'xbox', '045e', '0b13'],  // Chrome, Linux / mac
  ['Microsoft X-Box 360 pad (Vendor: 045e Product: 028e)', 'xbox', '045e', '028e'],          // Chrome raw
  ['045e-028e-Microsoft X-Box 360 pad', 'xbox', '045e', '028e'],                              // Firefox, Linux
  ['xinput', 'xbox', '', ''],                                                                 // Firefox, Windows
  ['45e-b13-Xbox Wireless Controller', 'xbox', '045e', '0b13'],                               // Safari
  ['046d-c21d-Logitech Gamepad F310', 'xbox', '046d', 'c21d'],                                // F310 in XInput mode
  ['046d-c216-Logitech Dual Action', 'generic', '046d', 'c216'],                              // F310 in DirectInput mode
  ['0e6f-02a8-PDP Xbox Wired Controller', 'xbox', '0e6f', '02a8'],
  ['Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 09cc)', 'ps', '054c', '09cc'], // DualShock 4
  ['054c-0ce6-Sony Interactive Entertainment DualSense Wireless Controller', 'ps', '054c', '0ce6'],
  ['Pro Controller (STANDARD GAMEPAD Vendor: 057e Product: 2009)', 'switch', '057e', '2009'],
  ['057e-2006-Joy-Con (L)', 'joycon-l', '057e', '2006'],
  ['057e-2007-Joy-Con (R)', 'joycon-r', '057e', '2007'],
  ['Joy-Con L+R (STANDARD GAMEPAD Vendor: 057e Product: 200e)', 'joycon-pair', '057e', '200e'],
  ['2dc8-6101-8BitDo Pro 2', '8bitdo', '2dc8', '6101'],
  ['USB Gamepad (Vendor: 0079 Product: 0011)', 'generic', '0079', '0011'],
];
for (const [id, family, vendor, product] of IDS) {
  const i = F.identify({ id });
  eq([i.family, i.vendor, i.product], [family, vendor, product], `identify "${id}"`);
}
eq(F.identify({ id: 'xinput' }).name, 'Xbox controller', 'identify name of "xinput"');
eq(F.identify({ id: 'Xbox 360 Controller (XInput STANDARD GAMEPAD)' }).name, 'Xbox 360 Controller', 'identify name, Chrome XInput');

// --- layout choice ------------------------------------------------------------------------
const LAYOUT_CASES = [
  [pad('Xbox 360 Controller (XInput STANDARD GAMEPAD)', 'standard', 17, 4), 'win', 'standard'],
  [pad('045e-028e-Microsoft X-Box 360 pad', '', 11, 8), 'Linux x86_64', 'xbox-linux'],
  [pad('045e-028e-Microsoft X-Box 360 pad', '', 15, 6), 'MacIntel', 'xbox-mac-usb'],
  [pad('45e-b13-Xbox Wireless Controller', '', 19, 6), 'MacIntel', 'xbox-mac-bt'],
  [pad('Xbox 360 Controller (Vendor: 045e Product: 028e)', '', 11, 5), 'Win32', 'xbox-win-hid'],
  [pad('054c-09cc-Wireless Controller', '', 13, 8), 'Linux', 'ps-linux'],
  [pad('054c-0ce6-DualSense Wireless Controller', '', 15, 6), 'Win32', 'ps-hid'],
  [pad('057e-2009-Pro Controller', '', 14, 6), 'Linux', 'switch-linux'],
  [pad('057e-2009-Pro Controller', '', 14, 4), 'MacIntel', 'switch-hid'],
  [pad('057e-2006-Joy-Con (L)', '', 16, 0), 'Win32', 'joycon-l'],
  [pad('2dc8-6101-8BitDo Pro 2', '', 15, 6), 'Linux', '8bitdo-linux'],
  [pad('2dc8-6101-8BitDo Pro 2', '', 15, 6), 'Win32', '8bitdo-hid'],
  [pad('0079-0011-USB Gamepad', '', 10, 5), 'Linux', 'generic'],
  [pad('Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 09cc)', 'standard', 18, 4), 'Linux', 'standard'],
];
for (const [gp, plat, want] of LAYOUT_CASES) eq(F.layoutFor(gp, plat).name, want, `layoutFor ${gp.id} on ${plat}`);

// --- every layout: each logical button alone reads as itself -------------------------------------
for (const [name, L] of Object.entries(F.LAYOUTS)) {
  const nb = Math.max(17, ...Object.values(L.spec.b)) + 1;
  const lay = Object.assign({ name }, L);
  const rest = {};  // trigger axes rest at -1
  for (const s of [...L.spec.lt, ...L.spec.rt]) { const m = /^a(\d+)$/.exec(s); if (m) rest[m[1]] = -1; }
  for (const [k, i] of Object.entries(L.spec.b)) {
    const gp = pad('test', name === 'standard' ? 'standard' : '', nb, 8, { b: { [i]: 1 } }, rest);
    const mem = {};
    const lg = F.read(gp, lay, mem);
    const on = F.BUTTONS.filter((b) => lg[b]);
    eq(on, [k], `${name}: button ${i} alone reads as ${k}`);
  }
}

// --- Xbox, standard mapping (Chrome / Edge Windows XInput, Chrome Linux / mac, Firefox xinput, Safari) ---
{
  const id = 'Xbox 360 Controller (XInput STANDARD GAMEPAD)';
  const press = (b, a) => gc(pad(id, 'standard', 17, 4, { b, a }), 'win').st;
  eq(press({ 0: 1 }).buttons, P.A, 'xbox std: A -> A');
  eq(press({ 1: 1 }).buttons, P.B, 'xbox std: B -> B');
  eq(press({ 2: 1 }).buttons, P.X, 'xbox std: X -> X');
  eq(press({ 3: 1 }).buttons, P.Y, 'xbox std: Y -> Y');
  eq(press({ 5: 1 }).buttons, P.Z, 'xbox std: RB -> Z');
  eq(press({ 4: 1 }).buttons, P.Z, 'xbox std: LB -> Z');
  eq(press({ 9: 1 }).buttons, P.START, 'xbox std: Menu -> Start');
  eq(press({ 8: 1 }).buttons, 0, 'xbox std: View is no game button (it is leave)');
  eq(press({ 16: 1 }).buttons, 0, 'xbox std: Guide is no game button (it joins)');
  eq(press({ 12: 1, 15: 1 }).buttons, P.UP | P.RIGHT, 'xbox std: d-pad up+right');
  const full = press({ 6: 1, 7: 1 });
  eq([full.tl, full.tr, full.buttons], [255, 255, P.L | P.R], 'xbox std: triggers full -> 255 + L/R click');
  const half = press({ 6: 0.5 });
  ok(half.tl > 100 && half.tl < 140 && !(half.buttons & P.L), `xbox std: LT half -> analog ~128, no click (${half.tl})`);
  eq(press({ 6: 0.03 }).tl, 0, 'xbox std: LT resting at 3% -> 0');
  let st = press({}, { 0: 1, 3: 1 });
  eq([st.sx, st.sy, st.cx, st.cy], [127, 0, 0, -127], 'xbox std: stick right, C down (y flipped)');
  st = press({}, { 1: -1, 2: -1 });
  eq([st.sx, st.sy, st.cx, st.cy], [0, 127, -127, 0], 'xbox std: stick up, C left');
  const g = F.layoutFor(pad(id, 'standard', 17, 4), 'win');
  const lg = F.read(pad(id, 'standard', 17, 4, { b: { 16: 1, 8: 1 } }), g, {});
  ok(lg.guide && lg.back, 'xbox std: Guide (16) and View (8) read');
}
// ?padface=gc: the GameCube's positions (B left of A, X right of it).
{
  const opts = F.options({ padface: 'gc' });
  const st = (b) => gc(pad('xinput', 'standard', 17, 4, { b }), 'win', {}, {}, opts).st.buttons;
  eq([st({ 2: 1 }), st({ 1: 1 })], [P.B, P.X], 'padface=gc: Xbox X (left) -> B, Xbox B (right) -> X');
}

// --- Xbox raw, Linux xpad (Firefox; Chrome raw): triggers axes 2/5 rest -1, d-pad axes 6/7 ---
{
  const id = '045e-028e-Microsoft X-Box 360 pad';
  const REST = { 2: -1, 5: -1 };
  const mk = (o) => pad(id, '', 11, 8, o, REST);
  const mem = {}, gmem = {};
  let r = gc(mk({}), 'Linux', mem, gmem);
  eq([r.st.buttons, r.st.tl, r.st.tr], [0, 0, 0], 'xbox-linux: at rest nothing');
  r = gc(mk({ b: { 0: 1, 7: 1, 5: 1 } }), 'Linux', mem, gmem);
  eq(r.st.buttons, P.A | P.START | P.Z, 'xbox-linux: A, Start, RB');
  r = gc(mk({ a: { 2: 1, 5: 0 } }), 'Linux', mem, gmem);
  ok(r.st.tl === 255 && (r.st.buttons & P.L) && r.st.tr > 110 && r.st.tr < 140 && !(r.st.buttons & P.R), `xbox-linux: LT full (axis 2 = 1), RT half (axis 5 = 0) ${JSON.stringify(r.st)}`);
  r = gc(mk({ a: { 6: -1, 7: 1 } }), 'Linux', mem, gmem);
  eq(r.st.buttons, P.LEFT | P.DOWN, 'xbox-linux: hat axes 6/7 left+down');
  r = gc(mk({ b: { 8: 1, 6: 1 } }), 'Linux', mem, gmem);
  ok(r.lg.guide && r.lg.back && r.st.buttons === 0, 'xbox-linux: Guide 8, Back 6, no game bits');
  // A trigger axis that never sent an event reads 0 (not -1): not half pressed.
  const fresh = gc(pad(id, '', 11, 8), 'Linux', {}, {});
  eq([fresh.st.tl, fresh.st.tr], [0, 0], 'xbox-linux: untouched trigger axes (0) are not half pressed');
}
// --- Xbox raw, macOS 360Controller: d-pad buttons 11-14, Start 8, Back 9, Guide 10 ---
{
  const id = '045e-028e-Xbox 360 Wired Controller';
  const r = gc(pad(id, '', 15, 6, { b: { 11: 1, 14: 1, 8: 1 }, a: { 2: 1 } }, { 5: -1 }), 'MacIntel');
  eq([r.lay.name, r.st.buttons, r.st.tl], ['xbox-mac-usb', P.UP | P.RIGHT | P.START | P.L, 255], 'xbox-mac-usb: d-pad buttons, Start, LT axis 2');
}
// --- Xbox raw, macOS Bluetooth: hat as Chrome's axis 9, or Firefox's four extra buttons ---
{
  const id = '45e-b13-Xbox Wireless Controller';
  // Chrome: a hat axis (9/7 at rest, -1 = up, 3/7*... see hatDir).
  const mem = {};
  let r = gc(pad(id, '', 16, 10, {}, { 4: -1, 5: -1, 9: 9 / 7 }), 'MacIntel', mem);
  eq([r.lay.name, r.st.buttons], ['xbox-mac-bt', 0], 'xbox-mac-bt (Chrome hat axis): rest');
  r = gc(pad(id, '', 16, 10, { a: { 9: -1 } }, { 4: -1, 5: -1 }), 'MacIntel', mem);
  eq(r.st.buttons, P.UP, 'xbox-mac-bt (Chrome hat axis): up');
  r = gc(pad(id, '', 16, 10, { a: { 9: -1 + 2 * 3 / 7 } }, { 4: -1, 5: -1 }), 'MacIntel', mem);
  eq(r.st.buttons, P.DOWN | P.RIGHT, 'xbox-mac-bt (Chrome hat axis): down-right');
  r = gc(pad(id, '', 16, 10, { b: { 0: 1, 3: 1, 4: 1, 7: 1, 11: 1 }, a: { 5: 1 } }, { 4: -1, 9: 9 / 7 }), 'MacIntel', mem);
  eq(r.st.buttons, P.A | P.X | P.Y | P.Z | P.START | P.L, 'xbox-mac-bt: A, X (3), Y (4), RB (7), Menu (11), LT axis 5');
  // Firefox: four d-pad buttons after the 15 (up, right, down, left).
  r = gc(pad(id, '', 19, 6, { b: { 15: 1, 16: 1 } }, { 4: -1, 5: -1 }), 'MacIntel', {});
  eq(r.st.buttons, P.UP | P.RIGHT, 'xbox-mac-bt (Firefox d-pad buttons): up+right');
  r = gc(pad(id, '', 19, 6, { b: { 17: 1, 18: 1 } }, { 4: -1, 5: -1 }), 'MacIntel', {});
  eq(r.st.buttons, P.DOWN | P.LEFT, 'xbox-mac-bt (Firefox d-pad buttons): down+left');
}
// --- Xbox raw, Windows DirectInput: one combined trigger axis ---
{
  const id = 'Xbox 360 Controller (Vendor: 045e Product: 028e)';
  let r = gc(pad(id, '', 11, 10, { a: { 2: 1 } }, { 9: 9 / 7 }), 'Win32');
  eq([r.lay.name, r.st.tl, r.st.tr], ['xbox-win-hid', 255, 0], 'xbox-win-hid: axis 2 = +1 -> LT');
  r = gc(pad(id, '', 11, 10, { a: { 2: -1 } }, { 9: 9 / 7 }), 'Win32');
  eq([r.st.tl, r.st.tr], [0, 255], 'xbox-win-hid: axis 2 = -1 -> RT');
  r = gc(pad(id, '', 11, 10, { a: { 9: -1 + 2 * 6 / 7 } }), 'Win32');
  eq(r.st.buttons, P.LEFT, 'xbox-win-hid: hat axis left');
}
// --- DualShock 4 / DualSense ---
{
  let r = gc(pad('Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 09cc)', 'standard', 18, 4, { b: { 0: 1, 2: 1 } }), 'Linux');
  eq(r.st.buttons, P.A | P.X, 'ps std: cross -> A, square -> X');
  r = gc(pad('054c-09cc-Wireless Controller', '', 13, 8, { b: { 0: 1, 3: 1, 2: 1, 9: 1 }, a: { 5: 1, 7: -1 } }, { 2: -1 }), 'Linux');
  eq([r.lay.name, r.st.buttons, r.st.tr], ['ps-linux', P.A | P.X | P.Y | P.START | P.R | P.UP, 255], 'ps-linux: cross A, square (3) X, triangle (2) Y, options, R2 axis 5, hat up');
  r = gc(pad('054c-0ce6-DualSense Wireless Controller', '', 15, 10, { b: { 1: 1, 0: 1, 9: 1 }, a: { 3: 1, 9: -1 + 2 * 4 / 7 } }, { 4: -1 }), 'Win32');
  eq([r.lay.name, r.st.buttons, r.st.tl], ['ps-hid', P.A | P.X | P.START | P.L | P.DOWN, 255], 'ps-hid: cross (1) A, square (0) X, options, L2 axis 3, hat down');
}
// --- Switch Pro, Joy-Cons ---
{
  let r = gc(pad('Pro Controller (STANDARD GAMEPAD Vendor: 057e Product: 2009)', 'standard', 18, 4, { b: { 0: 1, 6: 1 } }), 'Win32');
  eq(r.st.buttons, P.A | P.L, 'switch std: bottom (labelled B) -> A, ZL -> L (digital, full)');
  r = gc(pad('057e-2009-Pro Controller', '', 14, 6, { b: { 1: 1, 7: 1, 10: 1 }, a: { 4: 1 } }), 'Linux');
  eq([r.lay.name, r.st.buttons, r.st.tl], ['switch-linux', P.B | P.L | P.START | P.RIGHT, 255], 'switch-linux: right (A) -> B, ZL b7 -> L 255, + -> Start, hat right');
  r = gc(pad('057e-2009-Pro Controller', '', 14, 10, { b: { 2: 1, 9: 1 } }, { 9: 9 / 7 }), 'MacIntel');
  eq([r.lay.name, r.st.buttons], ['switch-hid', P.X | P.START], 'switch-hid: left (Y) -> X, + -> Start');
  r = gc(pad('Joy-Con L+R (STANDARD GAMEPAD Vendor: 057e Product: 200e)', 'standard', 18, 4, { b: { 3: 1 } }), 'Linux');
  eq(r.st.buttons, P.Y, 'joycon pair std: top -> Y');
  r = gc(pad('057e-200e-Nintendo Switch Combined Joy-Cons', '', 18, 4, { b: { 14: 1, 0: 1 } }), 'Linux');
  eq([r.lay.name, r.st.buttons], ['joycon-pair', P.UP | P.A], 'joycon-pair raw: d-pad buttons 14.., A');
  // One Joy-Con sideways: its hat is the stick.
  r = gc(pad('057e-2006-Joy-Con (L)', '', 16, 10, { a: { 9: -1 + 2 * 2 / 7 }, b: { 0: 1 } }), 'Win32');
  ok(r.lay.name === 'joycon-l' && r.st.sx === 127 && r.st.sy === 0 && r.st.buttons === P.A, `joycon-l: hat right -> stick right, A ${JSON.stringify(r.st)}`);
}
// --- 8BitDo (D-input) ---
{
  let r = gc(pad('2dc8-6101-8BitDo Pro 2', '', 15, 10, { b: { 0: 1, 4: 1, 8: 1, 11: 1 } }, { 9: 9 / 7 }), 'Win32');
  eq([r.lay.name, r.st.buttons, r.st.tl], ['8bitdo-hid', P.B | P.X | P.L | P.START, 255], '8bitdo-hid: b0 (east, labelled A) -> B, b4 (west, labelled Y) -> X, LT b8, Start b11');
  r = gc(pad('2dc8-6101-8BitDo Pro 2', '', 15, 8, { b: { 1: 1 }, a: { 5: 1, 6: 1 } }, { 4: -1 }), 'Linux');
  eq([r.lay.name, r.st.buttons, r.st.tl], ['8bitdo-linux', P.A | P.L | P.RIGHT, 255], '8bitdo-linux: b1 -> A, LT axis 5, hat axis right');
}
// --- generic ---
{
  const r = gc(pad('0079-0011-USB Gamepad', '', 10, 7, { b: { 9: 1, 0: 1 }, a: { 5: 1, 6: -1 } }), 'Linux');
  eq([r.lay.name, r.st.buttons], ['generic', P.A | P.START | P.RIGHT | P.UP], 'generic: A, Start, the last two free axes as d-pad');
}

// --- sticks: dead zone and scaling against PADClamp ------------------------------------------
{
  const s = (x, y, dz = 0.15) => F.stick(x, y, dz);
  eq(s(0.1, 0), [0, 0], 'stick: 0.10 in the dead zone');
  eq(s(0.1, 0.1), [0, 0], 'stick: (0.1, 0.1) radial 0.14 in the dead zone');
  const first = s(0.16, 0)[0];
  ok(first >= 16 && first <= 20, `stick: just past the dead zone is just past PADClamp's 15 (${first})`);
  eq(s(1, 0), [127, 0], 'stick: full right');
  eq(s(0.96, 0), [127, 0], 'stick: 96% saturates');
  const d = s(0.7071, 0.7071);
  ok(Math.abs(d[0] - 90) <= 2 && Math.abs(d[1] + 90) <= 2, `stick: diagonal (browser down-right) keeps its angle, y flipped (${d})`);
  const mid = s(0.55, 0)[0];
  ok(mid > 60 && mid < 80, `stick: half throw ~ half range (${mid})`);
  eq(F.stick(0.2, 0, 0.25), [0, 0], 'stick: ?deadzone=0.25');
}
// --- trigger click hysteresis ---------------------------------------------------------------------
{
  const gmem = {};
  const at = (v) => F.toGC({ lt: v, rt: 0, lx: 0, ly: 0, rx: 0, ry: 0 }, OPTS, gmem).buttons & P.L;
  ok(!at(0.85), 'trigger: 85% from rest is no click');
  ok(at(0.9), 'trigger: 90% clicks');
  ok(at(0.82), 'trigger: held above 80% stays clicked');
  ok(!at(0.78), 'trigger: below 80% releases');
}
// --- the hat axis decoder ---
eq([F.hatDir(9 / 7), F.hatDir(0), F.hatDir(-1), F.hatDir(1)], [[0, 0], [0, 0], [0, -1], [-1, -1]], 'hatDir: rest 9/7, unreported 0, up, up-left');
// --- options -----------------------------------------------------------------------------------
{
  const o = F.options(new URLSearchParams('padjoin=start&deadzone=0.2'));
  ok(o.join.has('start') && !o.join.has('a') && o.deadzone === 0.2, 'options: padjoin / deadzone');
  const d = F.options(new URLSearchParams(''));
  ok(d.join.has('start') && d.join.has('guide') && d.join.has('a') && d.face === 'label' && d.deadzone === 0.15, 'options: defaults');
}

console.log(`${passes} passed, ${fails} failed`);
process.exit(fails ? 1 : 0);
