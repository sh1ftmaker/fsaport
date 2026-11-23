// On-screen GameCube controller for phones and tablets, in the style of
// katamport's touch pads. The page's markup and CSS (index.html) draw it; this
// file hit-tests fingers against it and sends the result to the game through
// Module._fsa_touch_set (platform/input.cpp), which merges it into controller
// port 0 after every PADRead. Keyboard and gamepads keep working alongside.
//
// Design notes carried over from katamport:
//  - Every control is hit-tested by coordinate from cached geometry, with a
//    slop margin, nearest centre winning where the enlarged targets touch.
//    Measuring per event would force a layout per finger move, and a pressed
//    pad shrinks (scale .94), which would make it slip out from under the
//    thumb holding it.
//  - Each finger is tracked on its own (pointerId -> what it holds and where it
//    is), and the union is what the game sees, so two fingers can share a
//    button and releasing one does not release the other.
//  - Anything that can take the page away mid-press releases everything.
(function () {
  'use strict';

  // PAD_* bits (dolphin/pad.h).
  var BITS = { A: 0x0100, B: 0x0200, X: 0x0400, Y: 0x0800, START: 0x1000, Z: 0x0010, R: 0x0020, L: 0x0040 };
  var ROUND = { A: 1, B: 1, X: 1, Y: 1 };

  var body = document.body;
  var layer = document.getElementById('touch');
  var stickEl = document.getElementById('stick');
  var toggleBtn = document.getElementById('touchtoggle');

  var pads = [];
  var nodes = layer.querySelectorAll('[data-btn]');
  for (var i = 0; i < nodes.length; i++) {
    var name = nodes[i].getAttribute('data-btn');
    pads.push({ el: nodes[i], bit: BITS[name], round: !!ROUND[name], cx: 0, cy: 0, rx: 1, ry: 1 });
  }

  //--------------------------------------------------------------------
  // On / off. On by default where the primary pointer is a finger; the
  // choice is remembered (per browser, best effort).
  //--------------------------------------------------------------------
  var STORE = 'fsaport.touch';
  var touchOn = false;
  try {
    var saved = localStorage.getItem(STORE);
    touchOn = saved === null ? !!(window.matchMedia && matchMedia('(pointer: coarse)').matches) : saved === '1';
  } catch (e) {
    touchOn = !!(window.matchMedia && matchMedia('(pointer: coarse)').matches);
  }

  function setTouch(on) {
    touchOn = on;
    body.classList.toggle('touch-on', on);
    toggleBtn.setAttribute('aria-pressed', on ? 'true' : 'false');
    try { localStorage.setItem(STORE, on ? '1' : '0'); } catch (e) { /* private mode */ }
    releaseAll();
    requestAnimationFrame(function () { window.dispatchEvent(new Event('resize')); });
  }
  toggleBtn.addEventListener('click', function () { setTouch(!touchOn); });

  //--------------------------------------------------------------------
  // Geometry
  //--------------------------------------------------------------------
  var PAD_SLOP = 0.30;     // of each control's half-size, added all round
  var STICK_SLOP = 0.20;   // the stick keeps tracking a little past its rim
  var STICK_DEAD = 0.16;   // fraction of the radius that reports centre
  var STICK_FULL = 0.86;   // full deflection a little inside the rim

  var stickGeom = null;    // {cx, cy, r}
  var geomOk = false;

  function measure() {
    if (!layer.offsetWidth) { geomOk = false; return; }
    var s = stickEl.getBoundingClientRect();
    if (!s.width) { geomOk = false; return; }
    stickGeom = { cx: s.left + s.width / 2, cy: s.top + s.height / 2, r: s.width / 2 };
    for (var i = 0; i < pads.length; i++) {
      var p = pads[i];
      var b = p.el.getBoundingClientRect();
      p.cx = b.left + b.width / 2;
      p.cy = b.top + b.height / 2;
      p.rx = Math.max(1, b.width / 2 * (1 + PAD_SLOP));
      p.ry = Math.max(1, b.height / 2 * (1 + PAD_SLOP));
    }
    geomOk = true;
  }

  function inStick(x, y, slop) {
    if (!geomOk) return false;
    var dx = x - stickGeom.cx, dy = y - stickGeom.cy, r = stickGeom.r * (1 + slop);
    return dx * dx + dy * dy <= r * r;
  }

  // The nearest button whose enlarged target contains the point, distances
  // normalised by each button's own size so a big A cannot out-reach a small
  // button the finger is actually on.
  function padAt(x, y) {
    if (!geomOk) return 0;
    var bestBit = 0, best = Infinity;
    for (var i = 0; i < pads.length; i++) {
      var p = pads[i];
      var nx = (x - p.cx) / p.rx, ny = (y - p.cy) / p.ry;
      var d = p.round ? Math.sqrt(nx * nx + ny * ny) : Math.max(Math.abs(nx), Math.abs(ny));
      if (d <= 1 && d < best) { best = d; bestBit = p.bit; }
    }
    return bestBit;
  }

  //--------------------------------------------------------------------
  // Fingers
  //
  // pointerId -> { kind: 'stick' | 'pad', bits, x, y }. A finger that lands
  // on the stick owns the stick until it lifts, wherever it slides; a finger
  // on the buttons can roll from one to the next.
  //--------------------------------------------------------------------
  var fingers = Object.create(null);
  var stickX = 0, stickY = 0;
  var lastSent = '';

  function stickFrom(x, y) {
    var dx = x - stickGeom.cx, dy = y - stickGeom.cy;
    var full = stickGeom.r * STICK_FULL;
    var len = Math.sqrt(dx * dx + dy * dy);
    var mag = len / full;
    if (mag < STICK_DEAD) return { x: 0, y: 0, kx: dx, ky: dy, mag: 0, deg: 0 };
    var clamped = Math.min(1, (mag - STICK_DEAD) / (1 - STICK_DEAD));
    var ux = dx / len, uy = dy / len;
    var knob = Math.min(len, full);
    return {
      x: Math.round(ux * clamped * 127),
      y: Math.round(-uy * clamped * 127),        // the game's stick is up-positive
      kx: ux * knob, ky: uy * knob, mag: clamped,
      deg: (Math.atan2(dx, -dy) * 180 / Math.PI + 360) % 360  // clockwise from 12
    };
  }

  function sync() {
    var bits = 0, n = 0, stick = null;
    for (var id in fingers) {
      var f = fingers[id];
      n++;
      if (f.kind === 'stick') stick = f;
      else bits |= f.bits;
    }
    for (var i = 0; i < pads.length; i++) pads[i].el.classList.toggle('down', (bits & pads[i].bit) !== 0);

    if (stick && geomOk) {
      var s = stickFrom(stick.x, stick.y);
      stickX = s.x; stickY = s.y;
      stickEl.style.setProperty('--kx', s.kx.toFixed(1) + 'px');
      stickEl.style.setProperty('--ky', s.ky.toFixed(1) + 'px');
      stickEl.style.setProperty('--dir', s.deg.toFixed(1) + 'deg');
      stickEl.style.setProperty('--mag', s.mag.toFixed(2));
      stickEl.classList.toggle('on', s.mag > 0);
    } else {
      stickX = 0; stickY = 0;
      stickEl.style.setProperty('--kx', '0px');
      stickEl.style.setProperty('--ky', '0px');
      stickEl.classList.remove('on');
    }
    body.classList.toggle('touching', n > 0);
    send(bits);
  }

  function send(bits) {
    // L and R are analog triggers on a GameCube pad: a full press also sends
    // the analog value, which is what the game reads for some actions.
    var tl = (bits & BITS.L) ? 255 : 0, tr = (bits & BITS.R) ? 255 : 0;
    var key = bits + ',' + stickX + ',' + stickY;
    if (key === lastSent) return;
    if (Module.calledRun && Module._fsa_touch_set) {
      Module._fsa_touch_set(bits, stickX, stickY, tl, tr);
      lastSent = key;
    }
  }

  function buzz(ms) {
    if (navigator.vibrate) { try { navigator.vibrate(ms); } catch (e) { /* ignore */ } }
  }

  function live() { return touchOn && body.classList.contains('running') && !body.classList.contains('log-open'); }

  function onDown(e) {
    if (!live()) return;
    if (!geomOk) measure();
    var x = e.clientX, y = e.clientY, f;
    if (inStick(x, y, STICK_SLOP)) {
      f = { kind: 'stick', bits: 0, x: x, y: y };
    } else {
      var bits = padAt(x, y);
      if (!bits) return;          // not ours: the bar buttons and the page still work
      f = { kind: 'pad', bits: bits, x: x, y: y };
      buzz(8);
    }
    e.preventDefault();           // no scroll, zoom or synthetic mouse events
    // Capture to the overlay, which outlives restyles; lostpointercapture
    // turns the overlay being hidden into a clean release.
    try { layer.setPointerCapture(e.pointerId); } catch (err) { /* not fatal */ }
    fingers[e.pointerId] = f;
    sync();
  }

  function onMove(e) {
    var f = fingers[e.pointerId];
    if (!f) return;
    e.preventDefault();
    f.x = e.clientX; f.y = e.clientY;
    if (f.kind === 'pad') {
      var bits = padAt(f.x, f.y);
      if (bits === f.bits) return;
      if (bits && !f.bits) buzz(8);
      f.bits = bits;
    }
    sync();
  }

  function release(id) {
    if (!(id in fingers)) return;
    delete fingers[id];
    sync();
  }
  function onUp(e) {
    if (!(e.pointerId in fingers)) return;
    e.preventDefault();
    release(e.pointerId);
  }

  function releaseAll() {
    var any = false;
    for (var id in fingers) {
      any = true;
      try { layer.releasePointerCapture(parseInt(id, 10)); } catch (e) { /* already gone */ }
    }
    fingers = Object.create(null);
    if (any) sync(); else { stickX = 0; stickY = 0; send(0); }
  }

  window.addEventListener('pointerdown', onDown, { passive: false });
  window.addEventListener('pointermove', onMove, { passive: false });
  window.addEventListener('pointerup', onUp, { passive: false });
  window.addEventListener('pointercancel', onUp, { passive: false });
  layer.addEventListener('lostpointercapture', function (e) { release(e.pointerId); });

  // Re-decide what each finger holds when the controls move under it
  // (rotation, fullscreen, the band appearing): no pointer event says so.
  function relayout() {
    measure();
    for (var id in fingers) {
      var f = fingers[id];
      if (f.kind === 'pad') f.bits = padAt(f.x, f.y);
    }
    sync();
  }
  if (window.layoutHooks) window.layoutHooks.push(relayout);
  else window.addEventListener('resize', relayout);

  // iOS: preventDefault on touchstart is what stops the loupe, the callout and
  // double-tap zoom, and once one of those takes a touch Safari may never send
  // its pointerup. The touches list is the backstop: when no finger is on the
  // screen, nothing can be held.
  function overGame(t) {
    return t && t.closest && (t.closest('#touch') || t.closest('#stage'));
  }
  document.addEventListener('touchstart', function (e) { if (live() && overGame(e.target)) e.preventDefault(); }, { passive: false });
  document.addEventListener('touchmove', function (e) { if (live() && overGame(e.target)) e.preventDefault(); }, { passive: false });
  document.addEventListener('touchend', function (e) { if (e.touches.length === 0) releaseAll(); });
  document.addEventListener('touchcancel', function (e) { if (e.touches.length === 0) releaseAll(); });

  // The other ways a finger vanishes without a pointerup.
  window.addEventListener('blur', releaseAll);
  window.addEventListener('pagehide', releaseAll);
  document.addEventListener('visibilitychange', function () { if (document.hidden) releaseAll(); });
  document.addEventListener('contextmenu', function (e) { if (live() && overGame(e.target)) e.preventDefault(); });

  setTouch(touchOn);
})();
