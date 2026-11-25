// Diagnostics build only (cmake -DFSA_DIAG=ON; docs/DIAGNOSTICS.md, "Function
// traces (fntrace)"): per-call traces of functions selected at run time.
//
// tools/fntrace_instrument.py rewrites every instrumented function after the
// link so that its entry tests g_sel[id] (one byte per table index) and, when
// it is set, stores its parameters to g_args and calls fsa_ft_enter(id, sig);
// a non-zero token makes the exit store the result to g_ret and call
// fsa_ft_exit(token). The page (platform/web/fsa_fntrace.js) reads ?trace=URL
// before main() and selects functions through fsa_ft_select & co.
//
// Per selected function:
//  - every call is counted and aggregated (argument / result value ranges and
//    distinct values, callers, scenes, and for a `this` snapshot the entry
//    values and the changes of every word): stream "fntrace_agg", written at
//    each diagnostics flush, cumulative (the last record per id wins);
//  - the first `cap` calls and then every `every`-th are written one JSON line
//    each at their exit: stream "fntrace".
// A "nested" function is only traced while a primary function of the same
// request is running (in any context); its records carry the innermost such
// call's `seq` as `parent`.
//
// Memory is the port's own (little-endian) layout; snapshots are hex bytes in
// memory order. Reads are clipped to the wasm heap.
//
// Nothing here may be instrumented (NOINST, no library code that is).
#ifdef FSA_DIAG
#include "platform.h"
#include "../game/port/fsa_diag.h"

#include <emscripten.h>

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define NOINST __attribute__((no_instrument_function))

extern "C" unsigned fsa_harness_reads(void);  // harness.cpp

namespace {

constexpr unsigned kSel = 1u << 17;    // table indices the selection covers
constexpr unsigned kFns = 64;          // selected functions
constexpr unsigned kCalls = 64;        // calls in flight
constexpr unsigned kSnap = 4096;       // `this` bytes per function (all ranges)
constexpr unsigned kRanges = 8;
constexpr unsigned kGlobals = 16;
constexpr unsigned kGlobSnap = 2048;   // global bytes per call (all globals)
constexpr unsigned kDistinct = 16;
constexpr unsigned kCallers = 32;
constexpr unsigned kScenes = 32;
constexpr unsigned kMaxArgs = 27;
constexpr unsigned kPtrArgs = 4;       // pointer arguments snapshotted per function
constexpr unsigned kPtrSnap = 1024;    // their bytes per call (all of them)
constexpr unsigned kStrMax = 64;       // C-string arguments: bytes read (NUL included)
constexpr unsigned kStrDistinct = 32;
constexpr unsigned kRetArg = 31;       // a "pointer argument" that is the result

uint8_t g_sel[kSel];   // slot + 1 of the selected function, 0 = not selected
uint64_t g_args[32];
uint64_t g_ret;

struct Distinct {
  uint64_t v[kDistinct];
  uint32_t n[kDistinct];
  uint8_t count, more;
};

NOINST void dist_add(Distinct& d, uint64_t v) {
  for (unsigned i = 0; i < d.count; i++)
    if (d.v[i] == v) {
      d.n[i]++;
      return;
    }
  if (d.count < kDistinct) {
    d.v[d.count] = v;
    d.n[d.count++] = 1;
  } else {
    d.more = 1;
  }
}

// Value statistics of one argument / the result.
struct ValStat {
  int64_t imin, imax;    // signed (i32 sign-extended; i64)
  uint64_t umin, umax;   // unsigned (i32 zero-extended)
  double fmin, fmax;     // f32/f64
  uint64_t n;
  Distinct d;
};

NOINST void val_add(ValStat& s, int kind, uint64_t raw) {
  // kind: 0 i32, 1 i64, 2 f32, 3 f64
  if (kind <= 1) {
    int64_t sv = kind == 0 ? (int64_t)(int32_t)(uint32_t)raw : (int64_t)raw;
    uint64_t uv = kind == 0 ? (uint64_t)(uint32_t)raw : raw;
    if (!s.n || sv < s.imin) s.imin = sv;
    if (!s.n || sv > s.imax) s.imax = sv;
    if (!s.n || uv < s.umin) s.umin = uv;
    if (!s.n || uv > s.umax) s.umax = uv;
    dist_add(s.d, uv);
  } else {
    double f;
    if (kind == 2) {
      float x;
      uint32_t w = (uint32_t)raw;
      memcpy(&x, &w, 4);
      f = x;
      dist_add(s.d, w);
    } else {
      memcpy(&f, &raw, 8);
      dist_add(s.d, raw);
    }
    if (f == f) {
      if (!s.n || f < s.fmin) s.fmin = f;
      if (!s.n || f > s.fmax) s.fmax = f;
    }
  }
  s.n++;
}

// One 4-byte word of the `this` snapshot.
struct WordStat {
  int32_t emin, emax;   // value at entry (s32)
  float efmin, efmax;   // at entry as f32, finite values only
  uint8_t efset;
  Distinct entry;
  uint32_t changed;     // calls that changed the word
  uint32_t bytes[4];    // calls that changed each byte
  int32_t nmin, nmax;   // new values (s32)
  Distinct next;
};

struct Range {
  unsigned start, len;
};

// Distinct C-string argument values.
struct StrStat {
  char s[kStrDistinct][kStrMax];
  uint32_t n[kStrDistinct];
  uint8_t count, more;
};

// A pointer argument whose pointee is snapshotted at entry and exit
// (`bytes` bytes, a multiple of 4) or read as a C string (at most `bytes`).
struct PtrArg {
  unsigned arg, bytes, off;  // wasm parameter index, size, offset in Call::pbuf
  bool str;
  uint64_t nulls;            // calls with a null / unreadable pointer
  WordStat* words;           // bytes / 4 (not for strings)
  StrStat* strs;
};

struct Fn {
  unsigned id, flags;   // 1 primary, 2 nested
  int req;
  int thisOverride;     // -1 by signature, 0 no, 1 yes
  unsigned cap, every;
  Range ranges[kRanges];
  unsigned nranges, snapLen;
  uint32_t globals;     // bit mask of g_globals
  PtrArg ptrs[kPtrArgs];
  unsigned nptrs, ptrLen;
  uint64_t calls, logged, untracked, nullThis;
  uint64_t sig;
  int sigSet;
  ValStat args[kMaxArgs];
  ValStat ret;
  WordStat* words;      // snapLen / 4
  unsigned callerId[kCallers];
  uint64_t callerN[kCallers];
  uint64_t callerMore;
  uint8_t sceneId[kScenes];
  uint64_t sceneN[kScenes];
  unsigned nscenes;
  uint64_t flushedCalls;
  // nested: instrumented entries (inlined copies included) while a primary
  // call of the request ran, against `calls` (the out-of-line ones traced)
  uint64_t entriesInParent, entriesStart, flushedEntries;
  bool flushedOnce;
};
Fn g_fns[kFns];
unsigned g_nfns;

struct Global {
  unsigned addr, size;
  bool deref;  // a pointer variable: snapshot `size` bytes of what it points to
  char name[96];
};
Global g_globals[kGlobals];
unsigned g_nglobals;

struct Call {
  uint8_t used, log;
  unsigned fn;
  uint64_t seq, parent;
  uint64_t args[kMaxArgs];
  uint64_t sig;
  unsigned caller, ctx, frame, reads, scene, self;
  uint64_t callNo;
  uint8_t snap[kSnap];
  uint8_t snapOk[kSnap / 8];  // bit per byte: inside the heap at entry
  uint8_t glob[kGlobSnap];
  unsigned pptr[kPtrArgs];    // pointer argument values (0 = null / not readable)
  unsigned pstrLen[kPtrArgs]; // C strings: bytes read (without the NUL)
  uint8_t pbuf[kPtrSnap];     // the pointees at entry
  Call* outer;               // enclosing traced primary call
};
Call g_calls[kCalls];
Call* g_primary;              // innermost primary call in flight
constexpr unsigned kReqs = 32;
unsigned g_reqDepth[kReqs];   // primary calls of each request in flight

// Primary call of request `req` starts (+1) or ends (-1): bracket the nested
// functions' instrumented entry counts.
NOINST void req_depth(int req, int d) {
  if (req < 0 || (unsigned)req >= kReqs)
    return;
  bool edge = d > 0 ? g_reqDepth[req]++ == 0 : --g_reqDepth[req] == 0;
  if (!edge)
    return;
  for (unsigned i = 0; i < g_nfns; i++) {
    Fn& f = g_fns[i];
    if (f.req != req || (f.flags & 1))
      continue;
    unsigned n = fsa_diag_fn_calls(f.id);
    if (d > 0)
      f.entriesStart = n;
    else
      f.entriesInParent += n - f.entriesStart;
  }
}
uint64_t g_seq, g_dropped;
int g_busy;
unsigned g_defCap = 200, g_defEvery = 50;

NOINST unsigned mem_size() { return (unsigned)__builtin_wasm_memory_size(0) << 16; }
NOINST bool readable(unsigned a, unsigned n) { return a >= 1024 && a <= mem_size() && n <= mem_size() - a; }

NOINST int arg_kind(uint64_t sig, unsigned k) { return (int)((sig >> (10 + 2 * k)) & 3); }
NOINST unsigned sig_nargs(uint64_t sig) { return (unsigned)((sig >> 5) & 31); }
NOINST int sig_ret(uint64_t sig) { return (int)(sig & 7); }
NOINST int sig_this(uint64_t sig) { return (int)((sig >> 3) & 3) - 1; }

NOINST int this_index(const Fn& f, uint64_t sig) {
  if (f.thisOverride == 0)
    return -1;
  int t = sig_this(sig);
  if (t < 0 && f.thisOverride == 1 && sig_nargs(sig) && arg_kind(sig, 0) == 0)
    t = 0;
  return t;
}

NOINST void snapshot(const Fn& f, unsigned self, uint8_t* out, uint8_t* ok) {
  unsigned o = 0;
  memset(ok, 0, kSnap / 8);
  for (unsigned r = 0; r < f.nranges; r++) {
    for (unsigned k = 0; k < f.ranges[r].len; k++, o++) {
      unsigned a = self + f.ranges[r].start + k;
      if (self && readable(a, 1)) {
        out[o] = *(const uint8_t*)(uintptr_t)a;
        ok[o >> 3] |= 1 << (o & 7);
      } else {
        out[o] = 0;
      }
    }
  }
}

NOINST unsigned glob_base(const Global& gl) {
  if (!gl.deref)
    return gl.addr;
  return readable(gl.addr, 4) ? *(const uint32_t*)(uintptr_t)gl.addr : 0;
}

NOINST void glob_snapshot(const Fn& f, uint8_t* out) {
  unsigned o = 0;
  for (unsigned g = 0; g < g_nglobals; g++) {
    if (!(f.globals & (1u << g)))
      continue;
    const Global& gl = g_globals[g];
    unsigned base = glob_base(gl);
    for (unsigned k = 0; k < gl.size && o < kGlobSnap; k++, o++)
      out[o] = base && readable(base + k, 1) ? *(const uint8_t*)(uintptr_t)(base + k) : 0;
  }
}

// --- JSON output -------------------------------------------------------------------
char g_line[1 << 18];
int g_len;
bool g_over;

NOINST void ap(const char* fmt, ...) {
  if (g_over)
    return;
  va_list va;
  va_start(va, fmt);
  int n = vsnprintf(g_line + g_len, sizeof(g_line) - g_len, fmt, va);
  va_end(va);
  if (n < 0 || n >= (int)sizeof(g_line) - g_len - 64)
    g_over = true;
  else
    g_len += n;
}
NOINST void line_start() {
  g_len = 0;
  g_over = false;
  g_line[0] = 0;
}
NOINST void hex(const uint8_t* p, unsigned n) {
  static const char* d = "0123456789abcdef";
  if (g_over || g_len + 2 * n + 64 >= sizeof(g_line)) {
    g_over = true;
    return;
  }
  for (unsigned i = 0; i < n; i++) {
    g_line[g_len++] = d[p[i] >> 4];
    g_line[g_len++] = d[p[i] & 15];
  }
  g_line[g_len] = 0;
}
NOINST void fmt_double(double f) {
  if (f != f || std::isinf(f))
    ap("null");
  else
    ap("%.9g", f);
}
NOINST void value(int kind, uint64_t raw) {
  switch (kind) {
  case 0:
    ap("{\"t\":\"i32\",\"v\":%d,\"x\":\"0x%08x\"}", (int32_t)(uint32_t)raw, (uint32_t)raw);
    break;
  case 1:
    ap("{\"t\":\"i64\",\"v\":%lld,\"x\":\"0x%016llx\"}", (long long)raw, (unsigned long long)raw);
    break;
  case 2: {
    float x;
    uint32_t w = (uint32_t)raw;
    memcpy(&x, &w, 4);
    ap("{\"t\":\"f32\",\"v\":");
    fmt_double(x);
    ap(",\"x\":\"0x%08x\"}", w);
    break;
  }
  default: {
    double x;
    memcpy(&x, &raw, 8);
    ap("{\"t\":\"f64\",\"v\":");
    fmt_double(x);
    ap(",\"x\":\"0x%016llx\"}", (unsigned long long)raw);
  }
  }
}
NOINST void fn_ref(const char* key, unsigned id) {
  const char* sym = fsa_diag_fn_sym(id);
  const char* name = fsa_diag_fn_name(id);
  ap("\"%s\":%u,\"%s_sym\":\"%s\",\"%s_name\":\"%s\"", key, id, key, sym, key, strlen(name) > 600 ? "" : name);
}
NOINST bool plausible_f32(uint32_t w) {
  float x;
  memcpy(&x, &w, 4);
  float a = fabsf(x);
  return w == 0 || (a >= 1e-6f && a <= 1e7f);
}

// Offset in `this` of snapshot byte o.
NOINST unsigned snap_off(const Fn& f, unsigned o) {
  for (unsigned r = 0; r < f.nranges; r++) {
    if (o < f.ranges[r].len)
      return f.ranges[r].start + o;
    o -= f.ranges[r].len;
  }
  return 0;
}

NOINST uint32_t word_at(const uint8_t* p) {
  uint32_t w;
  memcpy(&w, p, 4);
  return w;
}

// Snapshot words: ranges are rounded to 4 bytes at selection, so word w is
// snapshot bytes 4w..4w+3 and lies in one range.
// ok == nullptr: every byte is valid.
NOINST void aggregate_words(WordStat* words, unsigned nwords, const uint8_t* in, const uint8_t* out,
                            const uint8_t* ok) {
  if (!words)
    return;
  for (unsigned w = 0; w < nwords; w++) {
    const unsigned o = 4 * w;
    if (ok && ((ok[o >> 3] >> (o & 7)) & 15) != 15)
      continue;
    WordStat& s = words[w];
    uint32_t a = word_at(in + o), b = word_at(out + o);
    bool first = !s.entry.count;
    if (first || (int32_t)a < s.emin) s.emin = (int32_t)a;
    if (first || (int32_t)a > s.emax) s.emax = (int32_t)a;
    float fa;
    memcpy(&fa, &a, 4);
    if (std::isfinite(fa)) {
      if (!s.efset || fa < s.efmin) s.efmin = fa;
      if (!s.efset || fa > s.efmax) s.efmax = fa;
      s.efset = 1;
    }
    dist_add(s.entry, a);
    if (a != b) {
      if (!s.changed || (int32_t)b < s.nmin) s.nmin = (int32_t)b;
      if (!s.changed || (int32_t)b > s.nmax) s.nmax = (int32_t)b;
      s.changed++;
      for (int k = 0; k < 4; k++)
        if (in[o + k] != out[o + k])
          s.bytes[k]++;
      dist_add(s.next, b);
    }
  }
}

NOINST void aggregate_this(Fn& f, const uint8_t* in, const uint8_t* out, const uint8_t* ok) {
  aggregate_words(f.words, f.snapLen / 4, in, out, ok);
}

// Reads pointer argument p's pointee at a into d (a C string: at most p.bytes - 1
// characters and the NUL; *len = its length). Returns a, or 0 when unreadable.
NOINST unsigned ptr_read(const PtrArg& p, unsigned a, uint8_t* d, unsigned* len) {
  *len = 0;
  if (p.str) {
    unsigned n = 0;
    if (a && readable(a, 1)) {
      while (n + 1 < p.bytes && readable(a + n, 1) && *(const uint8_t*)(uintptr_t)(a + n))
        n++;
      memcpy(d, (const void*)(uintptr_t)a, n);
    } else {
      a = 0;
    }
    d[n] = 0;
    *len = n;
  } else if (a && readable(a, p.bytes)) {
    memcpy(d, (const void*)(uintptr_t)a, p.bytes);
  } else {
    a = 0;
    memset(d, 0, p.bytes);
  }
  return a;
}

// The pointer arguments at entry (pointees to c.pbuf) or exit (to `dst`, which
// is laid out as c.pbuf). C strings are read at entry only. The result
// (arg kRetArg) is read at exit only, into both buffers.
NOINST void ptr_snapshot(const Fn& f, Call& c, uint8_t* dst, bool entry) {
  for (unsigned k = 0; k < f.nptrs; k++) {
    const PtrArg& p = f.ptrs[k];
    uint8_t* d = dst + p.off;
    unsigned len;
    if (p.arg == kRetArg) {
      if (entry) {
        c.pptr[k] = 0;
        c.pstrLen[k] = 0;
        continue;
      }
      c.pptr[k] = ptr_read(p, sig_ret(c.sig) == 1 ? (unsigned)g_ret : 0, d, &len);
      c.pstrLen[k] = len;
      memcpy(c.pbuf + p.off, d, p.bytes);
      continue;
    }
    if (entry) {
      c.pptr[k] = ptr_read(p, p.arg < kMaxArgs ? (unsigned)c.args[p.arg] : 0, d, &len);
      c.pstrLen[k] = len;
    } else if (!p.str) {
      ptr_read(p, c.pptr[k], d, &len);
    }
  }
}

NOINST void str_add(StrStat& s, const char* v) {
  for (unsigned i = 0; i < s.count; i++)
    if (!strcmp(s.s[i], v)) {
      s.n[i]++;
      return;
    }
  if (s.count < kStrDistinct) {
    snprintf(s.s[s.count], kStrMax, "%s", v);
    s.n[s.count++] = 1;
  } else {
    s.more = 1;
  }
}

// The changed words of in -> out (len bytes, a multiple of 4): offsets in `this`
// through f's ranges, or byte offsets in the pointee when f is null.
NOINST void emit_diff(const uint8_t* in, const uint8_t* out, unsigned len, const Fn* f) {
  ap("[");
  bool any = false;
  for (unsigned o2 = 0; o2 + 4 <= len; o2 += 4) {
    uint32_t a = word_at(in + o2), b = word_at(out + o2);
    if (a == b)
      continue;
    unsigned mask = 0;
    for (int k = 0; k < 4; k++)
      mask |= (in[o2 + k] != out[o2 + k]) << k;
    ap("%s{\"off\":%u,\"mask\":%u,\"old\":\"0x%08x\",\"new\":\"0x%08x\",\"s32\":[%d,%d]", any ? "," : "",
       f ? snap_off(*f, o2) : o2, mask, a, b, (int32_t)a, (int32_t)b);
    if (plausible_f32(a) && plausible_f32(b)) {
      float fa, fb;
      memcpy(&fa, &a, 4);
      memcpy(&fb, &b, 4);
      ap(",\"f32\":[");
      fmt_double(fa);
      ap(",");
      fmt_double(fb);
      ap("]");
    }
    if (!(mask & 12))
      ap(",\"lo16\":[%d,%d]", (int16_t)a, (int16_t)b);
    if (!(mask & 3))
      ap(",\"hi16\":[%d,%d]", (int16_t)(a >> 16), (int16_t)(b >> 16));
    ap("}");
    any = true;
  }
  ap("]");
}

NOINST void jstr(const uint8_t* p, unsigned n) {
  ap("\"");
  for (unsigned i = 0; i < n && !g_over; i++) {
    uint8_t ch = p[i];
    if (ch == '"' || ch == '\\')
      ap("\\%c", ch);
    else if (ch < 0x20 || ch >= 0x7f)
      ap("\\u%04x", ch);
    else
      ap("%c", ch);
  }
  ap("\"");
}

NOINST void emit_call(Fn& f, Call& c, const uint8_t* out, const uint8_t* globOut, const uint8_t* pout) {
  char sc[128];
  line_start();
  ap("{\"seq\":%llu,", (unsigned long long)c.seq);
  fn_ref("fn", f.id);
  ap(",\"req\":%d,\"call\":%llu,\"n\":%u,\"frame\":%u,\"ctx\":%u,", f.req, (unsigned long long)c.callNo, c.reads,
     c.frame, c.ctx);
  fsa_diag_scene_json(c.scene, sc, sizeof(sc));
  ap("\"scene\":%s,", sc);
  fn_ref("caller", c.caller);
  if (c.parent)
    ap(",\"parent\":%llu", (unsigned long long)c.parent);
  ap(",\"args\":[");
  unsigned na = sig_nargs(c.sig);
  for (unsigned k = 0; k < na; k++) {
    if (k)
      ap(",");
    value(arg_kind(c.sig, k), c.args[k]);
  }
  ap("]");
  int rk = sig_ret(c.sig);
  if (rk) {
    ap(",\"ret\":");
    value(rk - 1, g_ret);
  }
  int ti = this_index(f, c.sig);
  if (ti >= 0 && f.snapLen) {
    ap(",\"this\":\"0x%08x\"", c.self);
    if (!c.self || !readable(c.self, 1)) {
      ap(",\"this_bad\":1");
    } else {
      ap(",\"snap\":[");
      unsigned o = 0;
      for (unsigned r = 0; r < f.nranges; r++) {
        ap("%s{\"off\":%u,\"in\":\"", r ? "," : "", f.ranges[r].start);
        hex(c.snap + o, f.ranges[r].len);
        ap("\",\"out\":\"");
        hex(out + o, f.ranges[r].len);
        ap("\"}");
        o += f.ranges[r].len;
      }
      ap("],\"diff\":");
      emit_diff(c.snap, out, f.snapLen, &f);
    }
  }
  if (f.nptrs) {
    ap(",\"ptrs\":[");
    for (unsigned k = 0; k < f.nptrs; k++) {
      const PtrArg& p = f.ptrs[k];
      ap("%s{\"arg\":%u,\"ptr\":\"0x%08x\"", k ? "," : "", p.arg, c.pptr[k]);
      if (!c.pptr[k]) {
        ap(",\"null\":1");
      } else if (p.str) {
        ap(",\"str\":");
        jstr(c.pbuf + p.off, c.pstrLen[k]);
        if (c.pstrLen[k] + 1 >= p.bytes)
          ap(",\"cut\":1");
      } else {
        ap(",\"in\":\"");
        hex(c.pbuf + p.off, p.bytes);
        ap("\",\"out\":\"");
        hex(pout + p.off, p.bytes);
        ap("\",\"diff\":");
        emit_diff(c.pbuf + p.off, pout + p.off, p.bytes, nullptr);
      }
      ap("}");
    }
    ap("]");
  }
  if (f.globals) {
    ap(",\"globals\":[");
    unsigned o = 0;
    bool first = true;
    for (unsigned g = 0; g < g_nglobals; g++) {
      if (!(f.globals & (1u << g)))
        continue;
      const Global& gl = g_globals[g];
      unsigned n = gl.size;
      if (o + n > kGlobSnap)
        n = o < kGlobSnap ? kGlobSnap - o : 0;
      ap("%s{\"name\":\"%s\",\"addr\":\"0x%08x\",", first ? "" : ",", gl.name, gl.addr);
      if (gl.deref)
        ap("\"deref\":1,\"base\":\"0x%08x\",", glob_base(gl));
      ap("\"in\":\"");
      hex(c.glob + o, n);
      ap("\",\"out\":\"");
      hex(globOut + o, n);
      ap("\"}");
      o += n;
      first = false;
    }
    ap("]");
  }
  ap("}");
  if (!g_over)
    fsa_diag("fntrace", g_line);
  else
    g_dropped++;
}

NOINST void emit_val(const ValStat& s, int kind) {
  ap("{\"t\":\"%s\",\"n\":%llu", kind == 0 ? "i32" : kind == 1 ? "i64" : kind == 2 ? "f32" : "f64",
     (unsigned long long)s.n);
  if (s.n) {
    if (kind <= 1)
      ap(",\"min\":%lld,\"max\":%lld,\"umin\":%llu,\"umax\":%llu", (long long)s.imin, (long long)s.imax,
         (unsigned long long)s.umin, (unsigned long long)s.umax);
    else {
      ap(",\"min\":");
      fmt_double(s.fmin);
      ap(",\"max\":");
      fmt_double(s.fmax);
    }
    ap(",\"distinct\":[");
    for (unsigned i = 0; i < s.d.count; i++)
      ap("%s[\"0x%llx\",%u]", i ? "," : "", (unsigned long long)s.d.v[i], s.d.n[i]);
    ap("],\"more\":%d", s.d.more);
  }
  ap("}");
}

NOINST void emit_words(const WordStat* words, unsigned nwords, const Fn* f);

NOINST void flush() {
  char sc[128];
  for (unsigned i = 0; i < g_nfns; i++) {
    Fn& f = g_fns[i];
    if (f.flushedOnce && f.calls == f.flushedCalls && f.entriesInParent == f.flushedEntries)
      continue;
    f.flushedOnce = true;
    f.flushedCalls = f.calls;
    f.flushedEntries = f.entriesInParent;
    line_start();
    ap("{");
    fn_ref("fn", f.id);
    unsigned long long entries = fsa_diag_fn_calls(f.id);
    ap(",\"req\":%d,\"kind\":\"%s\",\"calls\":%llu,\"logged\":%llu,\"entries\":%llu,\"untracked\":%llu,"
       "\"null_this\":%llu,\"frame\":%u",
       f.req, f.flags & 1 ? "primary" : "nested", (unsigned long long)f.calls, (unsigned long long)f.logged, entries,
       (unsigned long long)f.untracked, (unsigned long long)f.nullThis, fsa_diag_frame());
    if (!(f.flags & 1))
      ap(",\"entries_in_parent\":%llu", (unsigned long long)f.entriesInParent);
    if (f.sigSet) {
      ap(",\"sig\":\"0x%llx\",\"this_arg\":%d,\"args\":[", (unsigned long long)f.sig, this_index(f, f.sig));
      for (unsigned k = 0; k < sig_nargs(f.sig); k++) {
        if (k)
          ap(",");
        emit_val(f.args[k], arg_kind(f.sig, k));
      }
      ap("]");
      if (sig_ret(f.sig)) {
        ap(",\"ret\":");
        emit_val(f.ret, sig_ret(f.sig) - 1);
      }
    }
    ap(",\"callers\":[");
    for (unsigned k = 0; k < kCallers && f.callerN[k]; k++) {
      ap("%s{", k ? "," : "");
      fn_ref("id", f.callerId[k]);
      ap(",\"n\":%llu}", (unsigned long long)f.callerN[k]);
    }
    ap("],\"callers_more\":%llu,\"scenes\":[", (unsigned long long)f.callerMore);
    for (unsigned k = 0; k < f.nscenes; k++) {
      fsa_diag_scene_json(f.sceneId[k], sc, sizeof(sc));
      ap("%s{\"scene\":%s,\"n\":%llu}", k ? "," : "", sc, (unsigned long long)f.sceneN[k]);
    }
    ap("]");
    if (f.words) {
      ap(",\"ranges\":[");
      for (unsigned r = 0; r < f.nranges; r++)
        ap("%s[%u,%u]", r ? "," : "", f.ranges[r].start, f.ranges[r].start + f.ranges[r].len);
      ap("],\"words\":");
      emit_words(f.words, f.snapLen / 4, &f);
    }
    if (f.nptrs) {
      ap(",\"ptrs\":[");
      for (unsigned k = 0; k < f.nptrs; k++) {
        const PtrArg& p = f.ptrs[k];
        ap("%s{\"arg\":%u,\"bytes\":%u,\"nulls\":%llu", k ? "," : "", p.arg, p.bytes, (unsigned long long)p.nulls);
        if (p.str && p.strs) {
          ap(",\"str\":1,\"strings\":[");
          for (unsigned i = 0; i < p.strs->count; i++) {
            ap(i ? ",[" : "[");
            jstr((const uint8_t*)p.strs->s[i], strlen(p.strs->s[i]));
            ap(",%u]", p.strs->n[i]);
          }
          ap("],\"more\":%d", p.strs->more);
        } else if (p.words) {
          ap(",\"words\":");
          emit_words(p.words, p.bytes / 4, nullptr);
        }
        ap("}");
      }
      ap("]");
    }
    ap("}");
    if (!g_over)
      fsa_diag("fntrace_agg", g_line);
  }
  line_start();
  ap("{\"frame\":%u,\"functions\":%u,\"seq\":%llu,\"dropped\":%llu}", fsa_diag_frame(), g_nfns,
     (unsigned long long)g_seq, (unsigned long long)g_dropped);
  fsa_diag("fntrace_meta", g_line);
}

// Word statistics as a JSON list: offsets in `this` through f's ranges, or
// byte offsets in a pointee when f is null.
NOINST void emit_words(const WordStat* words, unsigned nwords, const Fn* f) {
  ap("[");
  bool first = true;
  for (unsigned w = 0; w < nwords; w++) {
    const WordStat& s = words[w];
    if (!s.entry.count)
      continue;
    ap("%s{\"off\":%u,\"emin\":%d,\"emax\":%d", first ? "" : ",", f ? snap_off(*f, 4 * w) : 4 * w, s.emin,
       s.emax);
    first = false;
    if (s.efset) {
      ap(",\"efmin\":");
      fmt_double(s.efmin);
      ap(",\"efmax\":");
      fmt_double(s.efmax);
    }
    ap(",\"entry\":[");
    for (unsigned k = 0; k < s.entry.count; k++)
      ap("%s[\"0x%08x\",%u]", k ? "," : "", (unsigned)s.entry.v[k], s.entry.n[k]);
    ap("],\"entry_more\":%d", s.entry.more);
    if (s.changed) {
      ap(",\"changed\":%u,\"bytes\":[%u,%u,%u,%u],\"nmin\":%d,\"nmax\":%d,\"new\":[", s.changed, s.bytes[0],
         s.bytes[1], s.bytes[2], s.bytes[3], s.nmin, s.nmax);
      for (unsigned k = 0; k < s.next.count; k++)
        ap("%s[\"0x%08x\",%u]", k ? "," : "", (unsigned)s.next.v[k], s.next.n[k]);
      ap("],\"new_more\":%d", s.next.more);
    }
    ap("}");
  }
  ap("]");
}

bool g_hooked;

}  // namespace

// --- Called by the rewritten code -------------------------------------------------
extern "C" NOINST EMSCRIPTEN_KEEPALIVE unsigned fsa_ft_enter(unsigned id, unsigned long long sig) {
  if (g_busy || id >= kSel || !g_sel[id])
    return 0;
  Fn& f = g_fns[g_sel[id] - 1];
  // A nested function only inside a primary call of its own request.
  Call* parent = g_primary;
  while (parent && g_fns[parent->fn].req != f.req)
    parent = parent->outer;
  if ((f.flags & 1) == 0 && !parent)
    return 0;
  g_busy = 1;
  Call* c = nullptr;
  for (unsigned i = 0; i < kCalls; i++)
    if (!g_calls[i].used) {
      c = &g_calls[i];
      break;
    }
  if (!c) {
    f.untracked++;
    g_busy = 0;
    return 0;
  }
  if (!f.sigSet) {
    f.sig = sig;
    f.sigSet = 1;
  }
  c->used = 1;
  c->fn = (unsigned)(&f - g_fns);
  c->seq = ++g_seq;
  c->parent = parent ? parent->seq : 0;
  c->sig = sig;
  unsigned na = sig_nargs(sig);
  for (unsigned k = 0; k < na; k++)
    c->args[k] = g_args[k];
  c->callNo = f.calls++;
  c->log = c->callNo < f.cap || (f.every && (c->callNo - f.cap) % f.every == 0);
  // flag 4 (nested "follow"): logged exactly when the enclosing primary call is
  if ((f.flags & 4) && parent)
    c->log = parent->log;
  // The callee has not pushed itself yet: the top of the shadow stack is the caller.
  c->caller = fsa_diag_caller(0);
  c->ctx = fsa_diag_context();
  c->frame = fsa_diag_frame();
  c->reads = fsa_harness_reads();
  c->scene = fsa_diag_scene();
  int ti = this_index(f, sig);
  c->self = ti >= 0 ? (unsigned)c->args[ti] : 0;
  if (ti >= 0 && f.snapLen) {
    if (!c->self)
      f.nullThis++;
    snapshot(f, c->self, c->snap, c->snapOk);
  }
  if (f.globals)
    glob_snapshot(f, c->glob);
  if (f.nptrs) {
    ptr_snapshot(f, *c, c->pbuf, true);
    for (unsigned k = 0; k < f.nptrs; k++) {
      PtrArg& p = f.ptrs[k];
      if (p.arg == kRetArg)
        continue;
      if (!c->pptr[k])
        p.nulls++;
      else if (p.str && p.strs)
        str_add(*p.strs, (const char*)c->pbuf + p.off);
    }
  }
  for (unsigned k = 0; k < na; k++)
    val_add(f.args[k], arg_kind(sig, k), c->args[k]);
  // callers
  unsigned k;
  for (k = 0; k < kCallers && f.callerN[k]; k++)
    if (f.callerId[k] == c->caller)
      break;
  if (k < kCallers) {
    f.callerId[k] = c->caller;
    f.callerN[k]++;
  } else {
    f.callerMore++;
  }
  for (k = 0; k < f.nscenes; k++)
    if (f.sceneId[k] == (uint8_t)c->scene)
      break;
  if (k == f.nscenes && k < kScenes) {
    f.sceneId[k] = (uint8_t)c->scene;
    f.nscenes++;
  }
  if (k < kScenes)
    f.sceneN[k]++;
  if (f.flags & 1) {
    c->outer = g_primary;
    g_primary = c;
    req_depth(f.req, 1);
  } else {
    c->outer = nullptr;
  }
  g_busy = 0;
  return (unsigned)(c - g_calls) + 1;
}

extern "C" NOINST EMSCRIPTEN_KEEPALIVE void fsa_ft_exit(unsigned tok) {
  if (!tok || tok > kCalls)
    return;
  Call& c = g_calls[tok - 1];
  if (!c.used)
    return;
  int wasBusy = g_busy;
  g_busy = 1;
  Fn& f = g_fns[c.fn];
  int rk = sig_ret(c.sig);
  if (rk)
    val_add(f.ret, rk - 1, g_ret);
  static uint8_t out[kSnap], ok[kSnap / 8], gout[kGlobSnap];
  int ti = this_index(f, c.sig);
  bool snap = ti >= 0 && f.snapLen && c.self;
  if (snap) {
    snapshot(f, c.self, out, ok);
    for (unsigned i = 0; i < kSnap / 8; i++)
      ok[i] &= c.snapOk[i];
    aggregate_this(f, c.snap, out, ok);
  }
  if (f.globals)
    glob_snapshot(f, gout);
  static uint8_t pout[kPtrSnap];
  if (f.nptrs) {
    ptr_snapshot(f, c, pout, false);
    for (unsigned k = 0; k < f.nptrs; k++) {
      PtrArg& p = f.ptrs[k];
      if (p.arg == kRetArg && !c.pptr[k])
        p.nulls++;
      else if (p.arg == kRetArg && p.str && p.strs)
        str_add(*p.strs, (const char*)c.pbuf + p.off);
      if (c.pptr[k] && !p.str)
        aggregate_words(p.words, p.bytes / 4, c.pbuf + p.off, pout + p.off, nullptr);
    }
  }
  if (c.log && !wasBusy) {
    f.logged++;
    emit_call(f, c, out, gout, pout);
  }
  if (f.flags & 1) {
    req_depth(f.req, -1);
    // Normally the innermost; a call left behind by a coroutine switch is unlinked.
    if (g_primary == &c) {
      g_primary = c.outer;
    } else {
      for (Call* p = g_primary; p; p = p->outer)
        if (p->outer == &c) {
          p->outer = c.outer;
          break;
        }
    }
  }
  c.used = 0;
  g_busy = wasBusy;
}

// Addresses and sizes the instrumenter reads out of these bodies.
extern "C" NOINST EMSCRIPTEN_KEEPALIVE uint8_t* fsa_ft_sel_ptr(void) { return g_sel; }
extern "C" NOINST EMSCRIPTEN_KEEPALIVE uint64_t* fsa_ft_args_ptr(void) { return g_args; }
extern "C" NOINST EMSCRIPTEN_KEEPALIVE uint64_t* fsa_ft_ret_ptr(void) { return &g_ret; }
extern "C" NOINST EMSCRIPTEN_KEEPALIVE unsigned fsa_ft_sel_size(void) { return kSel; }

// --- Configuration (platform/web/fsa_fntrace.js) ------------------------------------
extern "C" NOINST EMSCRIPTEN_KEEPALIVE void fsa_ft_defaults(unsigned cap, unsigned every) {
  g_defCap = cap;
  g_defEvery = every;
}

// Select function `id`: flags 1 primary, 2 nested (| 4: log a nested call exactly when its primary call is logged). this_bytes snapshots
// [0, this_bytes) of `this`; this_mode -1 by signature, 0 never, 1 parameter 0.
// cap/every 0xFFFFFFFF = default. Returns the slot, -1 when full.
extern "C" NOINST EMSCRIPTEN_KEEPALIVE int fsa_ft_select(unsigned id, unsigned flags, unsigned thisBytes, int thisMode,
                                                         unsigned cap, unsigned every, int req) {
  if (id >= kSel)
    return -1;
  if (g_sel[id])
    return g_sel[id] - 1;
  if (g_nfns >= kFns)
    return -1;
  Fn& f = g_fns[g_nfns];
  memset(&f, 0, sizeof(f));
  f.id = id;
  f.flags = flags;
  f.req = req;
  f.thisOverride = thisMode;
  f.cap = cap == 0xFFFFFFFFu ? g_defCap : cap;
  f.every = every == 0xFFFFFFFFu ? g_defEvery : every;
  if (thisBytes) {
    thisBytes = (thisBytes + 3) & ~3u;
    if (thisBytes > kSnap)
      thisBytes = kSnap;
    f.ranges[0] = Range{0, thisBytes};
    f.nranges = 1;
    f.snapLen = thisBytes;
  }
  g_sel[id] = (uint8_t)(++g_nfns);
  return (int)(g_nfns - 1);
}

extern "C" NOINST EMSCRIPTEN_KEEPALIVE int fsa_ft_add_range(int slot, unsigned start, unsigned end) {
  if (slot < 0 || (unsigned)slot >= g_nfns)
    return -1;
  Fn& f = g_fns[slot];
  start &= ~3u;
  end = (end + 3) & ~3u;
  if (end <= start || f.nranges >= kRanges || f.snapLen + (end - start) > kSnap)
    return -1;
  f.ranges[f.nranges++] = Range{start, end - start};
  f.snapLen += end - start;
  return 0;
}

// Snapshot the pointee of wasm parameter `arg` (`this` included, as in the
// records' `args`): `bytes` bytes at entry and exit, or with `str` a C string of
// at most `bytes` - 1 characters at entry. arg 31 (kRetArg) is the i32 result,
// read at exit. Returns 0, -1 when full.
extern "C" NOINST EMSCRIPTEN_KEEPALIVE int fsa_ft_add_ptr_arg(int slot, unsigned arg, unsigned bytes, int str) {
  if (slot < 0 || (unsigned)slot >= g_nfns || (arg >= kMaxArgs && arg != kRetArg))
    return -1;
  Fn& f = g_fns[slot];
  if (str)
    bytes = bytes < 2 ? kStrMax : bytes > kStrMax ? kStrMax : bytes;
  else
    bytes = (bytes + 3) & ~3u;
  if (!bytes || f.nptrs >= kPtrArgs || f.ptrLen + bytes > kPtrSnap)
    return -1;
  PtrArg& p = f.ptrs[f.nptrs++];
  p = PtrArg{};
  p.arg = arg;
  p.bytes = bytes;
  p.off = f.ptrLen;
  p.str = str != 0;
  f.ptrLen += (bytes + 3) & ~3u;
  return 0;
}

// deref: `addr` holds a pointer; the snapshot is `size` bytes at that pointer.
extern "C" NOINST EMSCRIPTEN_KEEPALIVE int fsa_ft_add_global(unsigned addr, unsigned size, const char* name,
                                                             int deref) {
  if (g_nglobals >= kGlobals)
    return -1;
  Global& g = g_globals[g_nglobals];
  g.addr = addr;
  g.deref = deref != 0;
  g.size = size > 512 ? 512 : size;
  snprintf(g.name, sizeof(g.name), "%s", name);
  return (int)g_nglobals++;
}

extern "C" NOINST EMSCRIPTEN_KEEPALIVE void fsa_ft_use_global(int slot, int global) {
  if (slot >= 0 && (unsigned)slot < g_nfns && global >= 0 && (unsigned)global < g_nglobals)
    g_fns[slot].globals |= 1u << global;
}

// After the selection: allocate the per-word statistics, install the flush hook.
extern "C" NOINST EMSCRIPTEN_KEEPALIVE void fsa_ft_start(void) {
  HostAllocScope host;
  for (unsigned i = 0; i < g_nfns; i++) {
    Fn& f = g_fns[i];
    if (f.snapLen && !f.words)
      f.words = (WordStat*)calloc(f.snapLen / 4, sizeof(WordStat));
    for (unsigned k = 0; k < f.nptrs; k++) {
      PtrArg& p = f.ptrs[k];
      if (p.str && !p.strs)
        p.strs = (StrStat*)calloc(1, sizeof(StrStat));
      else if (!p.str && !p.words)
        p.words = (WordStat*)calloc(p.bytes / 4, sizeof(WordStat));
    }
  }
  if (!g_hooked) {
    g_hooked = true;
    fsa_diag_add_flush_hook(flush);
  }
  platform_log("[fntrace] %u functions selected, %u globals\n", g_nfns, g_nglobals);
}
// diag_cov.cpp calls this at the first retrace (the reference links this unit).
extern "C" NOINST void fsa_diag_fntrace_init(void) {}
#endif  // FSA_DIAG
