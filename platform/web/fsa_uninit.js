// Uninitialised-read diagnostics (FSA_DIAG_UNINIT builds only; see
// platform/diag_uninit.cpp and docs/DIAGNOSTICS.md). Names the functions in
// the runtime's findings from fsa.uninit.json (written by
// tools/uninit_instrument.py) and writes them to the "uninit" stream.
(function () {
  var names = null, queued = [];
  var KIND = { 1: 'read-before-write', 2: 'never-written-after-ctor', 3: 'poison-value-outside-heap' };
  var LOAD = { 2: 'u16', 4: 'u32/f32', 8: 'u64' };
  function hex(v) { return '0x' + (v >>> 0).toString(16); }
  function fn(idx) {
    var f = names && names[idx];
    if (!f) return idx ? { wasm_index: idx } : null;
    var o = { name: f[1], itanium: f[0] };
    if (f[2]) o.mwcc = f[2];
    if (f[3]) o.orig_addr = f[3];
    return o;
  }
  function className(idx) {
    var f = names && names[idx];
    return f && f[4] ? f[4] : null;
  }
  function out(a) {
    var kind = a[0], where = a[1], cls = a[2], site = a[3], off = a[4], size = a[5], load = a[6];
    var rec = { evidence: KIND[kind] || String(kind) };
    if (kind === 3) {
      rec.address = (off >>> 0) === 0xFFFFFFFF ? 'many' : hex(off);
    } else {
      rec['class'] = cls ? className(cls) : null;
      var c = fn(cls);
      if (c) { rec.ctor = c.name; if (c.mwcc) rec.ctor_mwcc = c.mwcc; if (c.orig_addr) rec.ctor_addr = c.orig_addr; }
      if ((off >>> 0) === 0xFFFFFFFF) {
        // More than 8 offsets of one class read by one function (a loop over
        // an array): the rest are folded into this record.
        rec.field = 'many';
        rec.fields_from = hex(a[11]);
        rec.fields_to = hex(a[12]);
      } else {
        rec.field = hex(off);
      }
    }
    rec.size = size;
    if (kind !== 2) {
      var w = fn(where);
      if (w) { rec.where = w.name; if (w.mwcc) rec.where_mwcc = w.mwcc; if (w.orig_addr) rec.where_addr = w.orig_addr; }
      rec.load = LOAD[load] || load;
    }
    if (site) { var s = fn(site); if (s) rec.alloc_site = s.name; }
    if (a[9]) rec.alloc_size = a[9];
    rec.count = a[7];
    if (kind === 2) rec.objects = a[8];
    if (a[10]) { try { rec.scene = JSON.parse(a[10]); } catch (e) { rec.scene = a[10]; } }
    rec.key = [kind, where, cls, site, off, size, load].join(':');
    var diag = Module.fsaDiag || window.fsaDiag;
    if (diag) diag('uninit', JSON.stringify(rec));
  }
  Module.fsaUninitEmit = function () {
    var a = Array.prototype.slice.call(arguments);
    if (names) out(a); else queued.push(a);
  };
  fetch('fsa.uninit.json').then(function (r) { return r.json(); }).then(function (j) {
    names = j.funcs;
    queued.forEach(out); queued = [];
  }).catch(function (e) {
    console.log('[uninit] no fsa.uninit.json: ' + e);
    names = {};
    queued.forEach(out); queued = [];
  });
  setInterval(function () {
    if (Module._fsa_uninit_tick && names) Module._fsa_uninit_tick();
  }, 5000);
  console.log('[uninit] diagnostics build: heap poisoned with 0xA5, findings -> "uninit" stream');
})();
