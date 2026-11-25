// The "Picture" setting: aspect ratio, zoom and what happens at an area's
// edge (docs/VIEW.md). Self-contained: it adds its own button to the top bar,
// its own panel and styles, and tells the game through Module._fsa_view_set
// (platform/view.cpp). index.html only asks it for the picture's aspect when it
// sizes the canvas (window.fsaView.aspect()).
//
// The choice is per viewer (localStorage, best effort) and can be given in the
// URL, which wins for that visit without being saved:
//   ?aspect=4:3 | 16:10 | 16:9 | 21:9 | fit | <number>   (fit: the window's own)
//   ?zoom=0.75 .. 2                                       (1: the game's framing)
//   ?edge=fit | free     (fit: stay inside the area, crop where it is too narrow;
//                         free: widen evenly and show what lies past the area)
// The default (4:3, zoom 1) is the game as it shipped: nothing is changed.
(function () {
  'use strict';

  var STORE = 'fsaport.view';
  var NATIVE = 4 / 3;
  var ASPECTS = [
    { id: '4:3', label: '4:3', value: NATIVE },
    { id: '16:10', label: '16:10', value: 16 / 10 },
    { id: '16:9', label: '16:9', value: 16 / 9 },
    { id: '21:9', label: '21:9', value: 21 / 9 },
    { id: 'fit', label: 'Fill window', value: 0 },
  ];
  var ZOOMS = [0.75, 1, 1.25, 1.5, 2];

  var state = { aspect: '4:3', zoom: 1, edge: 'fit' };

  function parseAspect(s) {
    if (!s) return null;
    s = String(s).trim().toLowerCase();
    for (var i = 0; i < ASPECTS.length; i++) if (ASPECTS[i].id === s) return s;
    var m = /^(\d+(?:\.\d+)?)[:x/](\d+(?:\.\d+)?)$/.exec(s);
    var v = m ? Number(m[1]) / Number(m[2]) : Number(s);
    if (!isFinite(v) || v <= 0) return null;
    return String(Math.min(3, Math.max(NATIVE, v)));
  }
  function parseZoom(s) {
    var v = Number(s);
    return isFinite(v) && v > 0 ? Math.min(3, Math.max(0.5, v)) : null;
  }
  function parseEdge(s) { return s === 'free' || s === 'fit' ? s : null; }

  try {
    var saved = JSON.parse(localStorage.getItem(STORE) || 'null');
    if (saved) {
      state.aspect = parseAspect(saved.aspect) || state.aspect;
      state.zoom = parseZoom(saved.zoom) || state.zoom;
      state.edge = parseEdge(saved.edge) || state.edge;
    }
  } catch (e) { /* private mode, bad JSON */ }
  var q = new URLSearchParams(location.search);
  var fromUrl = q.has('aspect') || q.has('zoom') || q.has('edge');
  if (q.has('aspect')) state.aspect = parseAspect(q.get('aspect')) || state.aspect;
  if (q.has('zoom')) state.zoom = parseZoom(q.get('zoom')) || state.zoom;
  if (q.has('edge')) state.edge = parseEdge(q.get('edge')) || state.edge;

  function save() {
    if (fromUrl) return;
    try { localStorage.setItem(STORE, JSON.stringify(state)); } catch (e) { /* private mode */ }
  }

  // The stage's inner size (index.html's #stage, minus its padding).
  function stageBox() {
    var stage = document.getElementById('stage');
    if (!stage) return { w: innerWidth, h: innerHeight };
    var cs = getComputedStyle(stage);
    return {
      w: stage.clientWidth - parseFloat(cs.paddingLeft) - parseFloat(cs.paddingRight),
      h: stage.clientHeight - parseFloat(cs.paddingTop) - parseFloat(cs.paddingBottom),
    };
  }

  // The picture's aspect as it should be laid out now.
  function aspect() {
    if (state.aspect === 'fit') {
      var b = stageBox();
      if (b.w <= 0 || b.h <= 0) return NATIVE;
      // Quantised, so a resize by a pixel does not resize the game's canvas.
      return Math.min(3, Math.max(NATIVE, Math.round((b.w / b.h) * 50) / 50));
    }
    for (var i = 0; i < ASPECTS.length; i++) if (ASPECTS[i].id === state.aspect) return ASPECTS[i].value;
    return Math.min(3, Math.max(NATIVE, Number(state.aspect) || NATIVE));
  }

  // Tells the game; repeated until the runtime is there.
  var sent = null;
  function push() {
    var a = aspect();
    var key = a.toFixed(4) + '/' + state.zoom + '/' + state.edge;
    if (key === sent) return;
    try {
      Module._fsa_view_set(a, state.zoom, state.edge === 'free' ? 1 : 0);
    } catch (e) {  // the game code is not loaded yet
      setTimeout(push, 200);
      return;
    }
    sent = key;
    // The game resizes the canvas at its next frame (SDL); lay out again after.
    setTimeout(relayout, 120);
    setTimeout(relayout, 500);
  }
  function relayout() { window.dispatchEvent(new Event('resize')); }

  function set(changes) {
    for (var k in changes) {
      if (k === 'aspect') state.aspect = parseAspect(changes.aspect) || state.aspect;
      else if (k === 'zoom') state.zoom = parseZoom(changes.zoom) || state.zoom;
      else if (k === 'edge') state.edge = parseEdge(changes.edge) || state.edge;
    }
    fromUrl = false;
    save();
    push();
    relayout();
    render();
  }

  window.fsaView = {
    aspect: aspect,
    get: function () { return { aspect: state.aspect, zoom: state.zoom, edge: state.edge, value: aspect() }; },
    set: set,
  };

  //--------------------------------------------------------------------
  // UI: a "Picture" button in the top bar and its panel.
  //--------------------------------------------------------------------
  var css = document.createElement('style');
  css.textContent = [
    '#viewpanel { position: fixed; z-index: 10; top: calc(var(--safe-t, 0px) + 46px); right: calc(var(--safe-r, 0px) + 8px);',
    '  display: none; width: min(300px, calc(100vw - 32px)); padding: 12px 14px; border-radius: 12px;',
    '  border: 1px solid var(--hair, rgba(255,255,255,.16)); background: rgba(18, 21, 27, .95); color: var(--fg, #e6e6e6);',
    '  font: 13px/1.4 system-ui, sans-serif; box-shadow: 0 12px 30px -12px rgba(0,0,0,.9); }',
    '#viewpanel.open { display: block; }',
    '#viewpanel h3 { margin: 0 0 8px; font-size: 13px; font-weight: 600; }',
    '#viewpanel .row { margin: 10px 0 0; }',
    '#viewpanel .lbl { color: var(--muted, #8a8f99); font-size: 12px; margin-bottom: 4px; }',
    '#viewpanel .seg { display: flex; flex-wrap: wrap; gap: 4px; }',
    '#viewpanel .seg button { flex: 1 1 auto; min-width: 44px; height: 30px; padding: 0 8px; border-radius: 8px;',
    '  border: 1px solid var(--hair, rgba(255,255,255,.16)); background: rgba(255,255,255,.04); color: #c9d1dc;',
    '  font: 600 12px system-ui, sans-serif; touch-action: manipulation; }',
    '#viewpanel .seg button[aria-pressed="true"] { color: #08111d; background: var(--accent, #6fb2ff); border-color: transparent; }',
    '#viewpanel .note { margin: 10px 0 0; color: var(--muted, #8a8f99); font-size: 11px; }',
  ].join('\n');
  document.head.appendChild(css);

  var bar = document.getElementById('bar');
  var btn = document.createElement('button');
  btn.type = 'button';
  btn.id = 'viewtoggle';
  btn.title = 'Picture: aspect ratio and zoom';
  btn.setAttribute('aria-expanded', 'false');
  btn.setAttribute('aria-controls', 'viewpanel');
  btn.textContent = 'Picture';
  if (bar) bar.insertBefore(btn, bar.firstChild);

  var panel = document.createElement('div');
  panel.id = 'viewpanel';
  panel.setAttribute('role', 'dialog');
  panel.setAttribute('aria-label', 'Picture');
  document.body.appendChild(panel);

  function seg(label, items, current, onPick) {
    var row = document.createElement('div');
    row.className = 'row';
    var l = document.createElement('div');
    l.className = 'lbl';
    l.textContent = label;
    row.appendChild(l);
    var g = document.createElement('div');
    g.className = 'seg';
    g.setAttribute('role', 'group');
    g.setAttribute('aria-label', label);
    items.forEach(function (it) {
      var b = document.createElement('button');
      b.type = 'button';
      b.textContent = it.label;
      b.setAttribute('aria-pressed', String(it.id) === String(current) ? 'true' : 'false');
      b.addEventListener('click', function () { onPick(it.id); });
      g.appendChild(b);
    });
    row.appendChild(g);
    return row;
  }

  function render() {
    panel.textContent = '';
    var h = document.createElement('h3');
    h.textContent = 'Picture';
    panel.appendChild(h);
    var isPreset = ASPECTS.some(function (a) { return a.id === state.aspect; });
    panel.appendChild(seg('Aspect ratio', ASPECTS, isPreset ? state.aspect : '', function (id) { set({ aspect: id }); }));
    panel.appendChild(seg('Zoom', ZOOMS.map(function (z) { return { id: z, label: (z === 1 ? '1x' : z + 'x') }; }),
      state.zoom, function (z) { set({ zoom: z }); }));
    panel.appendChild(seg('At the edge of an area', [
      { id: 'fit', label: 'Stay inside' }, { id: 'free', label: 'Show beyond' }],
      state.edge, function (id) { set({ edge: id }); }));
    var n = document.createElement('p');
    n.className = 'note';
    n.textContent = '4:3 at 1x is the original picture. Wider or zoomed pictures change only what is drawn; ' +
      'the game plays the same. Menus and movies stay 4:3.';
    panel.appendChild(n);
  }
  render();

  function toggle(open) {
    var o = open === undefined ? !panel.classList.contains('open') : open;
    panel.classList.toggle('open', o);
    btn.setAttribute('aria-expanded', o ? 'true' : 'false');
    btn.setAttribute('aria-pressed', o ? 'true' : 'false');
  }
  btn.addEventListener('click', function (e) { e.stopPropagation(); toggle(); });
  document.addEventListener('pointerdown', function (e) {
    if (panel.classList.contains('open') && !panel.contains(e.target) && e.target !== btn) toggle(false);
  });
  document.addEventListener('keydown', function (e) { if (e.key === 'Escape') toggle(false); });

  // "Fill window" follows the window.
  if (window.layoutHooks) window.layoutHooks.push(function () { if (state.aspect === 'fit') push(); });
  push();
})();
