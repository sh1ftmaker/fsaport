// --pre-js (deluxe): gamepads -> GameCube pads, for local co-op (fsa_coop.js;
// docs/MULTIPLAYER.md "Controllers").
//
// Pure functions, no page state: fsa_coop.js calls them once a frame per pad,
// tools/pads_test.mjs runs them in node on recorded / synthetic snapshots.
//
//   FsaPads.identify(gp)                 {vendor, product, name, family, xinput}
//   FsaPads.layoutFor(gp, platform)      {name, src, spec}: how to read this pad
//   FsaPads.read(gp, layout, mem)        logical controls (positional face
//                                        buttons, sticks -1..1, triggers 0..1)
//   FsaPads.toGC(logical, opts, mem)     {buttons, sx, sy, cx, cy, tl, tr} as
//                                        fsa_coop_pad takes them
//   FsaPads.options(query)               the ?pad... options
//
// How browsers report a controller (Gamepad API, https://w3c.github.io/gamepad/):
//   - mapping "standard": the W3C layout, buttons 0-3 bottom / right / left /
//     top face, 4/5 shoulders, 6/7 triggers (analog .value), 8 back/select,
//     9 start, 10/11 stick clicks, 12-15 d-pad up/down/left/right, 16 home
//     (guide) where the browser exposes it; axes 0/1 left stick, 2/3 right
//     stick, down / right positive. Chrome and Edge give it for XInput pads
//     on Windows (id "... (XInput STANDARD GAMEPAD)", no vendor id: XInput
//     hides it, and allows four pads), for the devices in Chromium's
//     device/gamepad/gamepad_standard_mappings_{linux,mac,win}.cc and its own
//     Switch / Joy-Con driver (nintendo_controller.cc); Firefox for XInput on
//     Windows (id "xinput") and for the devices it remaps
//     (dom/gamepad/GamepadRemapping.cpp: DualShock 4, DualSense, Xbox One S
//     Bluetooth, Switch Pro, 8BitDo, ...); Safari for what GameController.framework
//     knows (Xbox, DualShock 4, DualSense, Switch Pro, MFi).
//   - mapping "": raw device order, which depends on the OS driver. Linux
//     (Chrome and Firefox both read evdev): buttons in input-event code order,
//     axes in ABS code order with the hat's X / Y as two more axes. macOS /
//     Windows raw HID: HID usage order; Chrome turns a hat switch into one
//     axis (8 directions as -1 .. 1 in steps of 2/7, centre 9/7 ~ 1.2857,
//     Chromium gamepad_device_mac.mm / raw_input_gamepad_device_win.cc),
//     Firefox into four extra buttons after the others, up / right / down /
//     left (UnpackDpad in dom/gamepad/{cocoa,windows}/*Gamepad.cpp).
//   - every browser hides all pads until a button is pressed after the page
//     loaded (fingerprinting protection; "gamepadconnected" then fires).
//
// The raw layouts below are SDL's community database
// (https://github.com/mdqinc/SDL_GameControllerDB, gamecontrollerdb.txt,
// checked at 01af257a, 2026-11-26): SDL enumerates Linux evdev and HID
// devices in the same order the browsers do, so its b<n> / a<n> indices are
// the Gamepad API's buttons[n] / axes[n]. Each entry names the GUID it was
// taken from. SDL's names are positional: "a" = bottom face button
// (south), "b" = right (east), "x" = left (west), "y" = top (north),
// whatever is printed on them; this file uses the same names.
var FsaPads = (function () {
  'use strict';
  // PAD_* bits (dolphin/pad.h).
  const P = { LEFT: 1, RIGHT: 2, DOWN: 4, UP: 8, Z: 0x10, R: 0x20, L: 0x40, A: 0x100, B: 0x200, X: 0x400, Y: 0x800, START: 0x1000 };

  // --- Recognising the device -----------------------------------------------------
  // gamepad.id formats:
  //   Chrome Windows XInput  "Xbox 360 Controller (XInput STANDARD GAMEPAD)"
  //   Chrome elsewhere       "Xbox Wireless Controller (STANDARD GAMEPAD Vendor: 045e Product: 0b13)",
  //                          "USB Gamepad (Vendor: 0079 Product: 0011)"
  //   Firefox                "045e-028e-Microsoft X-Box 360 pad", Windows XInput: "xinput"
  //   Safari                 "45e-b13-Xbox Wireless Controller" (hex without leading zeros)
  const VENDOR = {
    '045e': 'xbox',      // Microsoft
    '054c': 'ps',        // Sony
    '057e': 'switch',    // Nintendo
    '2dc8': '8bitdo',    // 8BitDo (D-input / macOS modes; in XInput mode it reports 045e:028e)
    // XInput-protocol third-party pads (same layout as Microsoft's; the xpad
    // driver's table, linux drivers/input/joystick/xpad.c, lists these vendors)
    '0e6f': 'xbox',      // PDP / Afterglow / Rock Candy
    '0f0d': 'xbox',      // Hori (its XInput pads; Hori's PS pads have Sony-style ids)
    '24c6': 'xbox',      // PowerA / ThrustMaster
    '20d6': 'xbox',      // PowerA (Xbox One / Series)
    '0738': 'xbox',      // Mad Catz
    '1532': 'xbox',      // Razer
    '146b': 'xbox',      // Nacon / BigBen
    '2e24': 'xbox',      // Hyperkin
  };
  // Logitech: F310 / F510 / F710 in XInput mode (switch on the back at "X").
  const LOGITECH_XINPUT = new Set(['c21d', 'c21e', 'c21f', 'c242']);
  const JOYCON = { '2006': 'joycon-l', '2007': 'joycon-r', '200e': 'joycon-pair' };

  function hex4(s) { return s ? s.toLowerCase().padStart(4, '0') : ''; }
  function identify(gp) {
    const id = String((gp && gp.id) || '');
    let vendor = '', product = '', name = id;
    let m = /Vendor:\s*([0-9a-f]{1,4})\s+Product:\s*([0-9a-f]{1,4})/i.exec(id);
    if (m) { vendor = hex4(m[1]); product = hex4(m[2]); name = id.replace(/\s*\(.*$/, ''); }
    else if ((m = /^([0-9a-f]{1,4})-([0-9a-f]{1,4})-(.*)$/i.exec(id))) { vendor = hex4(m[1]); product = hex4(m[2]); name = m[3]; }
    const xinput = /xinput/i.test(id);
    let family = VENDOR[vendor] || '';
    if (vendor === '046d' && LOGITECH_XINPUT.has(product)) family = 'xbox';
    if (family === 'switch' && JOYCON[product]) family = JOYCON[product];
    if (!family) {
      if (xinput || /x-?box|xinput/i.test(id)) family = 'xbox';
      else if (/dualshock|dualsense|playstation|wireless controller.*054c/i.test(id)) family = 'ps';
      else if (/joy-?con.*\(l\)|joy-?con l\b/i.test(id) && !/l\+r/i.test(id)) family = 'joycon-l';
      else if (/joy-?con.*\(r\)|joy-?con r\b/i.test(id) && !/l\+r/i.test(id)) family = 'joycon-r';
      else if (/joy-?con/i.test(id)) family = 'joycon-pair';
      else if (/pro controller|nintendo|switch/i.test(id)) family = 'switch';
      else if (/8bitdo/i.test(id)) family = '8bitdo';
      else family = 'generic';
    }
    if (xinput && !name.replace(/xinput/i, '').trim()) name = 'Xbox controller';
    name = name.replace(/\s*\((XInput )?STANDARD GAMEPAD\)\s*$/i, '').trim() || 'Gamepad';
    return { vendor, product, name, family, xinput };
  }

  // --- Layouts --------------------------------------------------------------------
  // spec.b: logical button -> buttons[] index; spec.ax: lx ly rx ry -> axes[]
  // index (browser sign: right / down positive); spec.lt / rt: a list of
  // sources, 'aN' an axis (-1 rest .. 1 full), 'bN' a button's value,
  // '+aN' / '-aN' one half of a combined axis (0 rest); spec.dpad:
  // 'buttons' (in spec.b), {axes: [x, y]} (-1 / 0 / 1), or 'hat' (find
  // Chrome's hat axis or Firefox's four extra buttons, see readHat);
  // spec.stickHat: the hat is the stick (a single sideways Joy-Con).
  const STD = {
    b: { a: 0, b: 1, x: 2, y: 3, lb: 4, rb: 5, back: 8, start: 9, ls: 10, rs: 11,
         up: 12, down: 13, left: 14, right: 15, guide: 16 },
    ax: { lx: 0, ly: 1, rx: 2, ry: 3 }, lt: ['b6'], rt: ['b7'], dpad: 'buttons',
  };
  const LAYOUTS = {
    standard: { src: 'W3C Gamepad "standard" mapping (https://w3c.github.io/gamepad/#remapping)', spec: STD },
    // Xbox 360 / One / Series and XInput pads, Linux xpad (Chrome raw, Firefox).
    'xbox-linux': { src: 'gamecontrollerdb 030000005e0400008e02000004010000 "Microsoft Xbox 360 Controller", Linux (xpad, evdev order)',
      spec: { b: { a: 0, b: 1, x: 2, y: 3, lb: 4, rb: 5, back: 6, start: 7, guide: 8, ls: 9, rs: 10 },
              ax: { lx: 0, ly: 1, rx: 3, ry: 4 }, lt: ['a2'], rt: ['a5'], dpad: { axes: [6, 7] } } },
    // Xbox 360 / One over USB with the 360Controller driver (Tattiebogle) on macOS.
    'xbox-mac-usb': { src: 'gamecontrollerdb 030000005e0400008e02000000000000 "Xbox 360 Controller", Mac OS X (360Controller driver)',
      spec: { b: { a: 0, b: 1, x: 2, y: 3, lb: 4, rb: 5, ls: 6, rs: 7, start: 8, back: 9, guide: 10,
                   up: 11, down: 12, left: 13, right: 14 },
              ax: { lx: 0, ly: 1, rx: 3, ry: 4 }, lt: ['a2'], rt: ['a5'], dpad: 'buttons' } },
    // Xbox One S / Series over Bluetooth on macOS (HID, firmware 5.x).
    'xbox-mac-bt': { src: 'gamecontrollerdb 030000005e040000130b000011050000 "Xbox One Controller" (Series, BT), Mac OS X',
      spec: { b: { a: 0, b: 1, x: 3, y: 4, lb: 6, rb: 7, back: 10, start: 11, guide: 12, ls: 13, rs: 14 },
              ax: { lx: 0, ly: 1, rx: 2, ry: 3 }, lt: ['a5'], rt: ['a4'], dpad: 'hat' } },
    // An XInput pad seen through DirectInput / raw HID on Windows: both
    // triggers on one axis (LT +, RT -), no guide.
    'xbox-win-hid': { src: 'gamecontrollerdb 030000005e0400008e02000000000000 "Xbox 360 Controller", Windows (DirectInput)',
      spec: { b: { a: 0, b: 1, x: 2, y: 3, lb: 4, rb: 5, back: 6, start: 7, ls: 8, rs: 9 },
              ax: { lx: 0, ly: 1, rx: 3, ry: 4 }, lt: ['+a2'], rt: ['-a2'], dpad: 'hat' } },
    // DualShock 4 / DualSense, Linux hid-sony / hid-playstation (evdev order).
    'ps-linux': { src: 'gamecontrollerdb 030000004c050000c405000000810000 "PS4 Controller" and 030000004c050000e60c000011810000 "PS5 Controller", Linux',
      spec: { b: { a: 0, b: 1, y: 2, x: 3, lb: 4, rb: 5, back: 8, start: 9, guide: 10, ls: 11, rs: 12 },
              ax: { lx: 0, ly: 1, rx: 3, ry: 4 }, lt: ['a2', 'b6'], rt: ['a5', 'b7'], dpad: { axes: [6, 7] } } },
    // DualShock 4 / DualSense, raw HID on Windows / macOS.
    'ps-hid': { src: 'gamecontrollerdb 030000004c050000c405000000000000 "PS4 Controller" and 030000004c050000e60c000000000000 "PS5 Controller", Windows / Mac OS X',
      spec: { b: { x: 0, a: 1, b: 2, y: 3, lb: 4, rb: 5, back: 8, start: 9, ls: 10, rs: 11, guide: 12 },
              ax: { lx: 0, ly: 1, rx: 2, ry: 5 }, lt: ['a3', 'b6'], rt: ['a4', 'b7'], dpad: 'hat' } },
    // Switch Pro Controller, Linux hid-nintendo (evdev order; triggers are digital).
    'switch-linux': { src: 'gamecontrollerdb 030000007e0500000920000011810000 "Nintendo Switch Pro Controller", Linux (hid-nintendo)',
      spec: { b: { a: 0, b: 1, y: 2, x: 3, lb: 5, rb: 6, back: 9, start: 10, guide: 11, ls: 12, rs: 13 },
              ax: { lx: 0, ly: 1, rx: 2, ry: 3 }, lt: ['b7'], rt: ['b8'], dpad: { axes: [4, 5] } } },
    // Switch Pro Controller, raw HID on Windows / macOS.
    'switch-hid': { src: 'gamecontrollerdb 030000007e0500000920000000000000 "Nintendo Switch Pro Controller", Windows / Mac OS X',
      spec: { b: { a: 0, b: 1, x: 2, y: 3, lb: 4, rb: 5, back: 8, start: 9, ls: 10, rs: 11, guide: 12 },
              ax: { lx: 0, ly: 1, rx: 2, ry: 3 }, lt: ['b6'], rt: ['b7'], dpad: 'hat' } },
    // Joy-Con pair as one pad (Linux joycond "Combined Joy-Cons").
    'joycon-pair': { src: 'gamecontrollerdb 060000007e0500000620000000000000 "Nintendo Switch Combined Joy-Cons", Linux',
      spec: { b: { a: 0, b: 1, y: 2, x: 3, lb: 5, rb: 6, back: 9, start: 10, guide: 11, ls: 12, rs: 13,
                   up: 14, down: 15, left: 16, right: 17 },
              ax: { lx: 0, ly: 1, rx: 2, ry: 3 }, lt: ['b7'], rt: ['b8'], dpad: 'buttons' } },
    // One Joy-Con held sideways: its four face buttons are the GC face
    // buttons, SL / SR the shoulders, the stick a hat.
    'joycon-l': { src: 'gamecontrollerdb 030000007e0500000620000001000000 "Nintendo Switch Joy-Con (L)", Linux / Windows / Mac OS X',
      spec: { b: { a: 0, b: 1, x: 2, y: 3, lb: 4, rb: 5, start: 8, ls: 10, back: 13 }, ax: {}, lt: [], rt: [], dpad: 'hat', stickHat: true } },
    'joycon-r': { src: 'gamecontrollerdb 030000007e0500000720000000000000 "Nintendo Switch Joy-Con (R)", Windows / Linux',
      spec: { b: { a: 0, b: 1, x: 2, y: 3, lb: 4, rb: 5, start: 9, ls: 11, back: 12 }, ax: {}, lt: [], rt: [], dpad: 'hat', stickHat: true } },
    // 8BitDo Pro 2 / SN30 Pro / N30 Pro 2 in D-input mode (Nintendo labels:
    // B bottom, A right). Triggers digital on some firmware, analog on others.
    '8bitdo-linux': { src: 'gamecontrollerdb 03000000c82d00000660000011010000 "8BitDo Pro 2" and 03000000c82d00001590000011010000 "8BitDo N30 Pro 2", Linux',
      spec: { b: { b: 0, a: 1, y: 3, x: 4, lb: 6, rb: 7, back: 10, start: 11, guide: 12, ls: 13, rs: 14 },
              ax: { lx: 0, ly: 1, rx: 2, ry: 3 }, lt: ['a5', 'b8'], rt: ['a4', 'b9'], dpad: 'hat' } },
    '8bitdo-hid': { src: 'gamecontrollerdb 03000000c82d00000660000000000000 "8BitDo Pro 2", Windows',
      spec: { b: { b: 0, a: 1, y: 3, x: 4, lb: 6, rb: 7, back: 10, start: 11, guide: 12, ls: 13, rs: 14 },
              ax: { lx: 0, ly: 1, rx: 3, ry: 4 }, lt: ['b8'], rt: ['b9'], dpad: 'hat' } },
    // Anything else: the commonest raw order (what most cheap USB pads and
    // the DirectInput "Generic USB Joystick" entries look like), axes 0-3
    // the sticks, a hat or four d-pad buttons wherever they are.
    generic: { src: 'a guess: the W3C standard order for buttons 0-11, axes 0/1 and 2/3 the sticks, the hat found at run time',
      spec: { b: { a: 0, b: 1, x: 2, y: 3, lb: 4, rb: 5, back: 8, start: 9, ls: 10, rs: 11 },
              ax: { lx: 0, ly: 1, rx: 2, ry: 3 }, lt: ['b6'], rt: ['b7'], dpad: 'hat' } },
  };

  function platformOf(s) {
    s = String(s || '').toLowerCase();
    if (/linux|android|cros|x11/.test(s)) return 'linux';
    if (/mac|iphone|ipad|darwin/.test(s)) return 'mac';
    if (/win/.test(s)) return 'win';
    return 'linux';
  }
  function hostPlatform() {
    if (typeof navigator === 'undefined') return 'linux';
    return platformOf((navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || navigator.userAgent);
  }
  function layout(name) { return Object.assign({ name }, LAYOUTS[name]); }
  function layoutFor(gp, platform) {
    const info = identify(gp);
    if (gp && gp.mapping === 'standard') return Object.assign(layout('standard'), { info, os: platform ? platformOf(platform) : hostPlatform() });
    const os = platform ? platformOf(platform) : hostPlatform();
    const nb = (gp && gp.buttons ? gp.buttons.length : 0), na = (gp && gp.axes ? gp.axes.length : 0);
    let name = 'generic';
    switch (info.family) {
      case 'xbox':
        if (os === 'linux') name = 'xbox-linux';
        else if (os === 'mac') {
          // The 360Controller driver puts the d-pad on buttons 11-14 and
          // reports 6 axes; Bluetooth HID has a hat and 15+ buttons with gaps.
          const bt = ['02e0', '02fd', '0b13', '0b20', '0b22', '0b05'].includes(info.product);
          name = bt || (nb >= 15 && na !== 6) ? 'xbox-mac-bt' : 'xbox-mac-usb';
        } else name = 'xbox-win-hid';
        break;
      case 'ps': name = os === 'linux' ? 'ps-linux' : 'ps-hid'; break;
      case 'switch': name = os === 'linux' ? 'switch-linux' : 'switch-hid'; break;
      case 'joycon-pair': name = 'joycon-pair'; break;
      case 'joycon-l': case 'joycon-r': name = info.family; break;
      case '8bitdo': name = os === 'linux' ? '8bitdo-linux' : '8bitdo-hid'; break;
    }
    return Object.assign(layout(name), { info, os });
  }

  // --- Reading ----------------------------------------------------------------------
  const val = (b) => (b ? (typeof b === 'object' ? (b.value || (b.pressed ? 1 : 0)) : Number(b) || 0) : 0);
  const prs = (b) => !!(b && (typeof b === 'object' ? b.pressed || b.value > 0.5 : b > 0.5));
  // Chrome's hat axis: 0..7 clockwise from up as -1 + 2k/7; anything else
  // (9/7 at rest) is the centre. -> [x, y] with y down positive.
  const HAT = [[0, -1], [1, -1], [1, 0], [1, 1], [0, 1], [-1, 1], [-1, 0], [-1, -1]];
  function hatDir(v) {
    if (!(v >= -1.01 && v <= 1.01)) return [0, 0];
    const k = Math.round((v + 1) * 7 / 2);
    if (Math.abs(v - (-1 + 2 * k / 7)) > 0.07) return [0, 0];  // between two steps (0 before any report): centre
    return k >= 0 && k <= 7 ? HAT[k] : [0, 0];
  }
  // A hat, wherever the browser put it: an axis that has once been outside
  // -1..1 (Chrome's resting 9/7) is the hat axis; on macOS / Windows with ten
  // axes, axis 9 is (Chrome numbers HID axes by usage, X 0x30 = axis 0 ..
  // hat switch 0x39 = axis 9, so a d-pad press that reveals the pad reads
  // right too); otherwise, when there are four buttons beyond the layout's
  // last one, they are Firefox's up / right / down / left; otherwise the last
  // two axes no stick or trigger uses, as -1 / 0 / 1 (Linux evdev's HAT0X/Y).
  function readHat(gp, spec, mem, os) {
    const ax = gp.axes || [], b = gp.buttons || [];
    if (mem.hatAxis === undefined) {
      for (let i = 0; i < ax.length; i++) if (Math.abs(ax[i]) > 1.05) { mem.hatAxis = i; break; }
    }
    if (mem.hatAxis === undefined && os !== 'linux' && ax.length === 10) mem.hatAxis = 9;
    if (mem.hatAxis !== undefined) return hatDir(ax[mem.hatAxis]);
    const top = Math.max(-1, ...Object.values(spec.b));
    if (b.length >= top + 5) {
      const base = b.length - 4;
      return [(prs(b[base + 1]) ? 1 : 0) - (prs(b[base + 3]) ? 1 : 0), (prs(b[base + 2]) ? 1 : 0) - (prs(b[base]) ? 1 : 0)];
    }
    const used = new Set(Object.values(spec.ax));
    const free = [];
    for (let i = 0; i < ax.length; i++) if (!used.has(i) && !(spec.lt || []).concat(spec.rt || []).some((s) => s.replace(/^[+-]/, '') === 'a' + i)) free.push(i);
    if (free.length >= 2) {
      const [i, j] = free.slice(-2);
      const t = (v) => (v > 0.5 ? 1 : v < -0.5 ? -1 : 0);
      return [t(ax[i]), t(ax[j])];
    }
    return [0, 0];
  }
  // A trigger from its sources. An axis trigger rests at -1; until it has
  // been seen anywhere but exactly 0 its value is not trusted (Linux evdev
  // and Chrome report 0 for an axis that has sent no event yet, which would
  // read as a half-pressed trigger).
  function readTrigger(gp, srcs, mem) {
    const ax = gp.axes || [], b = gp.buttons || [];
    let v = 0;
    for (const s of srcs || []) {
      const m = /^([+-]?)([ab])(\d+)$/.exec(s);
      if (!m) continue;
      const i = Number(m[3]);
      let x = 0;
      if (m[2] === 'b') x = val(b[i]);
      else if (m[1]) { const a = ax[i] || 0; x = m[1] === '+' ? Math.max(0, a) : Math.max(0, -a); }
      else {
        const a = ax[i];
        if (a === undefined) continue;
        const key = 'seen' + i;
        if (a !== 0) mem[key] = true;
        x = mem[key] ? (a + 1) / 2 : 0;
      }
      v = Math.max(v, x);
    }
    return Math.max(0, Math.min(1, v));
  }
  const BUTTONS = ['a', 'b', 'x', 'y', 'lb', 'rb', 'back', 'start', 'guide', 'ls', 'rs', 'up', 'down', 'left', 'right'];
  // -> {a, b, x, y, lb, rb, back, start, guide, ls, rs, up, down, left, right: bool,
  //     lx, ly, rx, ry: -1..1 (down / right positive), lt, rt: 0..1}
  function read(gp, lay, mem) {
    mem = mem || {};
    const spec = lay.spec, b = gp.buttons || [], ax = gp.axes || [];
    const o = {};
    for (const k of BUTTONS) o[k] = spec.b[k] !== undefined && prs(b[spec.b[k]]);
    const a = (k) => (spec.ax[k] !== undefined ? Math.max(-1, Math.min(1, Number(ax[spec.ax[k]]) || 0)) : 0);
    o.lx = a('lx'); o.ly = a('ly'); o.rx = a('rx'); o.ry = a('ry');
    o.lt = readTrigger(gp, spec.lt, mem);
    o.rt = readTrigger(gp, spec.rt, mem);
    if (spec.dpad === 'hat' || (spec.dpad && spec.dpad.axes)) {
      let x, y;
      if (spec.dpad.axes) {
        const t = (v) => (v > 0.5 ? 1 : v < -0.5 ? -1 : 0);
        x = t(ax[spec.dpad.axes[0]]); y = t(ax[spec.dpad.axes[1]]);
      } else [x, y] = readHat(gp, spec, mem, lay.os);
      if (spec.stickHat) { o.lx = x; o.ly = y; }
      else { o.up = o.up || y < 0; o.down = o.down || y > 0; o.left = o.left || x < 0; o.right = o.right || x > 0; }
    }
    return o;
  }

  // --- Options ------------------------------------------------------------------------
  //   ?padface=label|gc   label (default): bottom -> A, right -> B, left -> X,
  //                       top -> Y, as Aurora's SDL mapping does (on an Xbox
  //                       pad the letters match the GameCube's). gc: by the
  //                       GameCube's positions instead, B is left of A and X
  //                       right of it: left (Xbox X) -> B, right (Xbox B) -> X.
  //   ?deadzone=0.15      the sticks' radial dead zone (0..0.5)
  //   ?padjoin=start,guide,a  what joins a pad nobody plays with: Start and
  //                       Guide always, A only as the pad's first press (the
  //                       one that makes the browser show it)
  function options(q) {
    const get = (k) => (q && q.get ? q.get(k) : q && q[k]);
    const dzs = get('deadzone'), dz = dzs === null || dzs === undefined || dzs === '' ? NaN : Number(dzs);
    const join = String(get('padjoin') || 'start,guide,a').toLowerCase().split(/[,\s]+/).filter(Boolean);
    return {
      face: get('padface') === 'gc' ? 'gc' : 'label',
      deadzone: dz >= 0 && dz <= 0.5 ? dz : 0.15,
      join: new Set(join),
    };
  }

  // --- To the GameCube pad --------------------------------------------------------------
  // Sticks: radial dead zone, then scaled so the first step past it is just
  // past PADClamp's own dead zone (15 of 127, Aurora pad.cpp ClampRegion, which
  // JUTGamePad::read applies after this), full at 95% of the throw (worn
  // sticks and the Xbox's circular gate rarely reach 1.0). JUTGamePad's
  // stick-as-d-pad threshold (sPressPoint 0.5 of the clamped 72) is then about
  // half way, as on a GameCube stick. Aurora's own SDL path cuts at 8000/32768
  // without rescaling; the touch pad (touch.js) uses a 16% dead zone, rescaled.
  const GC_MIN = 16, OUTER = 0.95;
  function stick(x, y, dz) {
    const r = Math.hypot(x, y);
    if (r <= dz || r === 0) return [0, 0];
    const t = Math.min(1, (r - dz) / (OUTER - dz));
    const m = GC_MIN + t * (127 - GC_MIN);
    return [Math.round(x / r * m), Math.round(-y / r * m)];  // the GameCube's stick is up-positive
  }
  // Triggers: analog 0..255 with a small rest band (worn triggers idle a few
  // percent in), the digital L / R click at 90% (Aurora: 31150/32767 = 95%),
  // released below 80% so it does not chatter.
  const TRIG_REST = 0.04, CLICK_ON = 0.9, CLICK_OFF = 0.8;
  function toGC(o, opts, mem) {
    opts = opts || { face: 'label', deadzone: 0.15 };
    mem = mem || {};
    let bt = 0;
    if (o.a) bt |= P.A;
    if (opts.face === 'gc') { if (o.x) bt |= P.B; if (o.b) bt |= P.X; }
    else { if (o.b) bt |= P.B; if (o.x) bt |= P.X; }
    if (o.y) bt |= P.Y;
    if (o.start) bt |= P.START;
    if (o.rb || o.lb) bt |= P.Z;  // RB is Z; LB too (the GameCube has one shoulder button)
    if (o.up) bt |= P.UP;
    if (o.down) bt |= P.DOWN;
    if (o.left) bt |= P.LEFT;
    if (o.right) bt |= P.RIGHT;
    const [sx, sy] = stick(o.lx, o.ly, opts.deadzone);
    const [cx, cy] = stick(o.rx, o.ry, opts.deadzone);
    const trig = (v) => (v <= TRIG_REST ? 0 : Math.round(Math.min(1, (v - TRIG_REST) / (1 - TRIG_REST)) * 255));
    const tl = trig(o.lt), tr = trig(o.rt);
    mem.clickL = o.lt >= CLICK_ON || (mem.clickL && o.lt > CLICK_OFF);
    mem.clickR = o.rt >= CLICK_ON || (mem.clickR && o.rt > CLICK_OFF);
    if (mem.clickL) bt |= P.L;
    if (mem.clickR) bt |= P.R;
    return { buttons: bt, sx, sy, cx, cy, tl, tr };
  }

  const NAMES = [[P.A, 'A'], [P.B, 'B'], [P.X, 'X'], [P.Y, 'Y'], [P.START, 'Start'], [P.Z, 'Z'], [P.L, 'L'], [P.R, 'R'],
                 [P.UP, 'Up'], [P.DOWN, 'Down'], [P.LEFT, 'Left'], [P.RIGHT, 'Right']];
  const buttonNames = (bits) => NAMES.filter(([m]) => bits & m).map(([, n]) => n);

  return { P, LAYOUTS, identify, layoutFor, read, toGC, options, stick, hatDir, platformOf, buttonNames, BUTTONS };
})();
if (typeof module === 'object' && module && module.exports) module.exports = FsaPads;
