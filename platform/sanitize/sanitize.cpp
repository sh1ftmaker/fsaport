// Sanitizer runtime for the diagnostics build (cmake -DFSA_SANITIZE=undefined
// or address; docs/DIAGNOSTICS.md "Sanitizer run"). Linked only into those
// builds, and built without sanitizers itself.
//
// Every distinct site becomes one record in the diag stream "sanitizer"
// (platform/diag.cpp): kind, file:line:col, the enclosing function (from the
// JS stack, demangled and tied to its source unit through fsa_symtab.json,
// which tools/san_symtab.py writes from the link map), a detail string, the
// hit count and the scene of the first hit. A record is posted when a site is
// first hit and again (same "key", higher "count") every 10 s while its count
// grows; the last line per key is the final one (tools/san_summary.py).
//
// UBSan: this file *is* the runtime. The game units are compiled with
// -fsanitize=... -fsanitize-recover=all and call the __ubsan_handle_* entry
// points below, which count and return, so the run continues.
// ASan: emscripten's runtime does the checking (halt_on_error=0); its report
// text comes here through __asan_set_error_report_callback and is reduced to
// the same record. ASan reports a given pc once (suppress_equal_pcs), so ASan
// records have count 1.
#include <emscripten.h>
#include <emscripten/em_js.h>
#include <emscripten/eventloop.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" void fsa_diag(const char* name, const char* json);
// {"mode","stage","level","in_stage"} (game_root_sub_04_80139BE8.cpp; shared
// with the other diagnostics streams).
extern "C" int fsa_diag_scene(char* buf, int n);

// --- JS side: who called, and its unit ---------------------------------------------
// The wasm frame calling the outermost sanitizer entry point on the JS stack
// (__ubsan_handle_*, __asan_report_*, an interceptor, or this file's
// fsa_sanitizer_rt:: helpers when wasm-opt inlined the handler into the game
// function) is the function the check sits in (for code inlined into it, file:line still names the inlined
// source). Chrome prints wasm frames as "at fsa.wasm.<demangled name> (url)";
// fsa_symtab.json (tools/san_symtab.py) maps that name to [mangled, unit,
// decomp symbol]; it is fetched
// synchronously once (sanitizer builds only). No backslashes in EM_JS bodies:
// emscripten drops them.
EM_JS_DEPS(fsa_sanitize, "$stringToUTF8,$lengthBytesUTF8");
EM_JS(void, fsa_san_js_caller, (char* out, int cap), {
  if (!Module.fsaSanSym) {
    Module.fsaSanSym = {};
    try {
      const x = new XMLHttpRequest();
      x.open('GET', 'fsa_symtab.json', false);
      x.send();
      if (x.status === 200) Module.fsaSanSym = JSON.parse(x.responseText);
    } catch (e) {}
  }
  const limit = Error.stackTraceLimit;
  Error.stackTraceLimit = 100;
  const stack = new Error().stack.split(String.fromCharCode(10));
  Error.stackTraceLimit = limit;
  const rt = new RegExp('^(__ubsan_handle|__asan|__sanitizer|__interceptor|__wrap_|__lsan|fsa_sanitizer_rt::|fsa_san_)');
  const frame = new RegExp('^ *at (?:[^ ]*[.]wasm[.])?(.*) [(][^ ]*wasm-function.[0-9]+.:0x[0-9a-f]+[)]$');
  const frames = [];
  for (const line of stack) {
    const m = frame.exec(line);
    if (m) frames.push(m[1]);
  }
  let last = -1;
  for (let i = 0; i < frames.length; i++) if (rt.test(frames[i])) last = i;
  const tab = String.fromCharCode(9);
  let r = '';
  for (let i = last + 1; i < frames.length; i++) {
    if (rt.test(frames[i])) continue;
    const s = Module.fsaSanSym[frames[i]];
    r = (s ? s[0] : '') + tab + frames[i] + tab + (s ? s[1] : '') + tab + (s ? s[2] || '' : '');
    break;
  }
  stringToUTF8(r, out, cap);
});

// --- Sites ---------------------------------------------------------------------------
namespace fsa_sanitizer_rt {

struct Site {
  const void* id;  // check data (UBSan) or pc (ASan)
  int sub;         // distinguishes kinds sharing one id
  uint32_t count;
  uint32_t posted;  // count in the last posted line
  char* json;       // the record up to (not including) ",\"count\""
};

constexpr int kSlots = 1 << 15;
Site gSites[kSlots];
int gUsed = 0;
int gBusy = 0;  // reentrancy: a check hit while reporting (e.g. in fsa_diag_scene)
bool gTimer = false;

Site* lookup(const void* id, int sub) {
  uint32_t h = ((uint32_t)(uintptr_t)id * 2654435761u) ^ (uint32_t)sub * 40503u;
  for (int i = 0; i < kSlots; i++) {
    Site* s = &gSites[(h + i) & (kSlots - 1)];
    if (s->json == nullptr && s->count == 0) {
      if (gUsed >= kSlots - 64) return nullptr;
      s->id = id;
      s->sub = sub;
      gUsed++;
      return s;
    }
    if (s->id == id && s->sub == sub) return s;
  }
  return nullptr;
}

// Minimal JSON string writer.
struct Buf {
  char d[4096];
  int n = 0;
  void raw(const char* s) {
    while (*s && n < (int)sizeof(d) - 1) d[n++] = *s++;
    d[n] = 0;
  }
  void str(const char* s) {
    raw("\"");
    for (; s && *s && n < (int)sizeof(d) - 8; s++) {
      unsigned char c = (unsigned char)*s;
      if (c == '"' || c == '\\') {
        d[n++] = '\\';
        d[n++] = c;
      } else if (c < 0x20) {
        n += snprintf(d + n, sizeof(d) - n, "\\u%04x", c);
      } else {
        d[n++] = c;
      }
    }
    d[n] = 0;
    raw("\"");
  }
  void key(const char* k) {
    raw(n > 1 ? ",\"" : "\"");
    raw(k);
    raw("\":");
  }
};

void post(Site* s) {
  char line[4200];
  snprintf(line, sizeof(line), "%s,\"count\":%u}", s->json, s->count);
  fsa_diag("sanitizer", line);
  s->posted = s->count;
}

void flush_counts(void*) {
  for (int i = 0; i < kSlots; i++)
    if (gSites[i].json && gSites[i].count != gSites[i].posted) post(&gSites[i]);
}

// Repo-relative path ("game/src/..."), or the path as compiled.
const char* relpath(const char* f) {
  if (!f) return "";
  const char* p = strstr(f, "/game/");
  if (p) return p + 1;
  p = strstr(f, "/extern/");
  if (p) return p + 1;
  p = strstr(f, "/platform/");
  return p ? p + 1 : f;
}

}  // namespace fsa_sanitizer_rt
using namespace fsa_sanitizer_rt;

// The first hit of a site: builds and posts its record. `function` may be
// given (ASan report text); otherwise it comes from the JS stack.
static void fsa_san_new(Site* s, const char* kind, const char* file, unsigned line, unsigned col,
                            const char* detail, const char* extraJson) {
  char caller[1024];
  fsa_san_js_caller(caller, sizeof(caller));
  char* mangled = caller;
  char* demangled = strchr(caller, '\t');
  char* unit = nullptr;
  char* dsym = nullptr;
  if (demangled) {
    *demangled++ = 0;
    unit = strchr(demangled, '\t');
    if (unit) {
      *unit++ = 0;
      dsym = strchr(unit, '\t');
      if (dsym) *dsym++ = 0;
    }
  }
  char scene[128];
  if (fsa_diag_scene(scene, sizeof(scene)) <= 0) snprintf(scene, sizeof(scene), "null");
  Buf b;
  b.raw("{");
  char keybuf[600];
  snprintf(keybuf, sizeof(keybuf), "%s|%s:%u:%u", kind, relpath(file), line, col);
  b.key("key"), b.str(keybuf);
  b.key("kind"), b.str(kind);
  b.key("file"), b.str(relpath(file));
  char num[32];
  snprintf(num, sizeof(num), "%u", line);
  b.key("line"), b.raw(num);
  snprintf(num, sizeof(num), "%u", col);
  b.key("col"), b.raw(num);
  b.key("function"), b.str(demangled ? demangled : "");
  b.key("symbol"), b.str(mangled);
  b.key("sym"), b.str(dsym ? dsym : "");
  b.key("unit"), b.str(unit ? unit : "");
  b.key("detail"), b.str(detail);
  b.key("first_scene"), b.raw(scene);
  if (extraJson) b.raw(extraJson);
  s->json = (char*)malloc(b.n + 1);
  memcpy(s->json, b.d, b.n + 1);
  s->count = 1;
  post(s);
  printf("[sanitizer] %s %s:%u:%u in %s: %s\n", kind, relpath(file), line, col,
         demangled ? demangled : "?", detail);
  if (!gTimer) {
    gTimer = true;
    emscripten_set_interval(flush_counts, 10000, nullptr);
  }
}

// Exported: post the pending counts now (e.g. before closing the page).
extern "C" EMSCRIPTEN_KEEPALIVE void fsa_san_flush() { flush_counts(nullptr); }

// =====================================================================================
#ifdef FSA_SAN_UNDEFINED
// UBSan handler ABI (compiler-rt ubsan_handlers.h; clang emits these calls).
namespace fsa_sanitizer_rt {
struct SourceLocation {
  const char* file;
  uint32_t line;
  uint32_t col;
};
struct TypeDescriptor {
  uint16_t kind;  // 0 integer, 1 float, 0xffff unknown
  uint16_t info;  // integer: log2(bits) << 1 | signed; float: bits
  char name[1];
};
typedef uintptr_t ValueHandle;

struct TypeMismatchData {
  SourceLocation loc;
  const TypeDescriptor* type;
  unsigned char logAlignment;
  unsigned char checkKind;
};
struct OverflowData {
  SourceLocation loc;
  const TypeDescriptor* type;
};
struct ShiftOutOfBoundsData {
  SourceLocation loc;
  const TypeDescriptor* lhsType;
  const TypeDescriptor* rhsType;
};
struct OutOfBoundsData {
  SourceLocation loc;
  const TypeDescriptor* arrayType;
  const TypeDescriptor* indexType;
};
struct UnreachableData {
  SourceLocation loc;
};
struct VLABoundData {
  SourceLocation loc;
  const TypeDescriptor* type;
};
struct FloatCastOverflowData {
  SourceLocation loc;
  const TypeDescriptor* fromType;
  const TypeDescriptor* toType;
};
struct InvalidValueData {
  SourceLocation loc;
  const TypeDescriptor* type;
};
struct InvalidBuiltinData {
  SourceLocation loc;
  unsigned char kind;
};
struct NonNullReturnData {
  SourceLocation attrLoc;
};
struct NonNullArgData {
  SourceLocation loc;
  SourceLocation attrLoc;
  int argIndex;
};
struct PointerOverflowData {
  SourceLocation loc;
};
struct AlignmentAssumptionData {
  SourceLocation loc;
  SourceLocation assumptionLoc;
  const TypeDescriptor* type;
};

int intBits(const TypeDescriptor* t) { return 1 << (t->info >> 1); }
bool isSigned(const TypeDescriptor* t) { return t->info & 1; }

// A value as text (integers inline up to 32 bits, wider ones by pointer).
void value(char* out, size_t cap, const TypeDescriptor* t, ValueHandle v) {
  if (t->kind == 0) {
    int bits = intBits(t);
    if (bits <= 32) {
      if (isSigned(t)) {
        int64_t x = (int32_t)(uint32_t)v;
        if (bits < 32) x = (int64_t)((int32_t)((uint32_t)v << (32 - bits)) >> (32 - bits));
        snprintf(out, cap, "%lld", (long long)x);
      } else {
        snprintf(out, cap, "%u", (unsigned)v);
      }
    } else if (bits == 64) {
      uint64_t x = *(const uint64_t*)v;
      if (isSigned(t))
        snprintf(out, cap, "%lld", (long long)(int64_t)x);
      else
        snprintf(out, cap, "%llu", (unsigned long long)x);
    } else {
      snprintf(out, cap, "<%d-bit>", bits);
    }
  } else if (t->kind == 1) {
    if (t->info == 32) {
      float f;
      uint32_t u = (uint32_t)v;
      memcpy(&f, &u, 4);
      snprintf(out, cap, "%g", (double)f);
    } else if (t->info == 64) {
      snprintf(out, cap, "%g", *(const double*)v);
    } else {
      snprintf(out, cap, "<%d-bit float>", t->info);
    }
  } else {
    snprintf(out, cap, "<unknown>");
  }
}

// Counts a hit; true when it is the site's first (report it then).
Site* hit(const void* data, int sub) {
  if (gBusy) return nullptr;
  Site* s = lookup(data, sub);
  if (!s) return nullptr;
  if (s->json) {
    s->count++;
    return nullptr;
  }
  return s;
}

void report(Site* s, const char* kind, const SourceLocation& loc, const char* detail) {
  gBusy++;
  fsa_san_new(s, kind, loc.file, loc.line, loc.col, detail, nullptr);
  gBusy--;
}

const char* const kCheckKinds[] = {"load of", "store to", "reference binding to", "member access within",
                                   "member call on", "constructor call on", "downcast of", "downcast of",
                                   "upcast of", "cast to virtual base of", "_Nonnull binding to",
                                   "dynamic operation on"};

void overflow(OverflowData* d, ValueHandle l, ValueHandle r, const char* op) {
  Site* s = hit(d, 0);
  if (!s) return;
  char a[48], b[48], detail[256];
  value(a, sizeof(a), d->type, l);
  value(b, sizeof(b), d->type, r);
  snprintf(detail, sizeof(detail), "%s %s %s cannot be represented in type %s", a, op, b, d->type->name);
  report(s, isSigned(d->type) ? "signed-integer-overflow" : "unsigned-integer-overflow", d->loc, detail);
}
}  // namespace fsa_sanitizer_rt
using namespace fsa_sanitizer_rt;

extern "C" {
void __ubsan_handle_type_mismatch_v1(TypeMismatchData* d, ValueHandle ptr) {
  uintptr_t align = (uintptr_t)1 << d->logAlignment;
  // alignment-wide: 4-aligned but the type wants 8 (a u64/double member).
  // The GameCube heaps hand out 4-aligned blocks; on wasm that is harmless.
  const char* kind = !ptr                      ? "null"
                     : !(ptr & (align - 1))    ? "object-size"
                     : align > 4 && !(ptr & 3) ? "alignment-wide"
                                               : "alignment";
  Site* s = hit(d, (int)strlen(kind));  // the kinds differ in length
  if (!s) return;
  const char* what = d->checkKind < 12 ? kCheckKinds[d->checkKind] : "access of";
  char detail[300];
  if (!ptr)
    snprintf(detail, sizeof(detail), "%s null pointer of type %s", what, d->type->name);
  else if (ptr & (align - 1))
    snprintf(detail, sizeof(detail), "%s misaligned address 0x%08lx for type %s, which requires %lu byte alignment",
             what, (unsigned long)ptr, d->type->name, (unsigned long)align);
  else
    snprintf(detail, sizeof(detail), "%s address 0x%08lx with insufficient space for an object of type %s", what,
             (unsigned long)ptr, d->type->name);
  report(s, kind, d->loc, detail);
}
void __ubsan_handle_alignment_assumption(AlignmentAssumptionData* d, ValueHandle ptr, ValueHandle align,
                                         ValueHandle offset) {
  Site* s = hit(d, 0);
  if (!s) return;
  char detail[200];
  snprintf(detail, sizeof(detail), "assumption of %lu byte alignment for pointer 0x%08lx failed", (unsigned long)align,
           (unsigned long)ptr);
  report(s, "alignment-assumption", d->loc, detail);
}
void __ubsan_handle_add_overflow(OverflowData* d, ValueHandle l, ValueHandle r) { overflow(d, l, r, "+"); }
void __ubsan_handle_sub_overflow(OverflowData* d, ValueHandle l, ValueHandle r) { overflow(d, l, r, "-"); }
void __ubsan_handle_mul_overflow(OverflowData* d, ValueHandle l, ValueHandle r) { overflow(d, l, r, "*"); }
void __ubsan_handle_negate_overflow(OverflowData* d, ValueHandle v) {
  Site* s = hit(d, 0);
  if (!s) return;
  char a[48], detail[200];
  value(a, sizeof(a), d->type, v);
  snprintf(detail, sizeof(detail), "negation of %s cannot be represented in type %s", a, d->type->name);
  report(s, "signed-integer-overflow", d->loc, detail);
}
void __ubsan_handle_divrem_overflow(OverflowData* d, ValueHandle l, ValueHandle r) {
  char a[48], b[48], detail[200];
  value(a, sizeof(a), d->type, l);
  value(b, sizeof(b), d->type, r);
  bool zero = strcmp(b, "0") == 0;
  Site* s = hit(d, zero);
  if (!s) return;
  if (zero)
    snprintf(detail, sizeof(detail), "division of %s by zero (traps in wasm)", a);
  else
    snprintf(detail, sizeof(detail), "division of %s by %s cannot be represented in type %s", a, b, d->type->name);
  report(s, zero ? "integer-divide-by-zero" : "signed-integer-overflow", d->loc, detail);
}
void __ubsan_handle_shift_out_of_bounds(ShiftOutOfBoundsData* d, ValueHandle l, ValueHandle r) {
  char a[48], b[48], detail[300];
  value(a, sizeof(a), d->lhsType, l);
  value(b, sizeof(b), d->rhsType, r);
  int bits = intBits(d->lhsType);
  long long rv = atoll(b);
  bool exponent = rv < 0 || rv >= bits;
  Site* s = hit(d, exponent);
  if (!s) return;
  if (exponent)
    snprintf(detail, sizeof(detail), "shift exponent %s is %s for %d-bit type %s", b,
             rv < 0 ? "negative" : "too large", bits, d->lhsType->name);
  else if (a[0] == '-')
    snprintf(detail, sizeof(detail), "left shift of negative value %s", a);
  else
    snprintf(detail, sizeof(detail), "left shift of %s by %s places cannot be represented in type %s", a, b,
             d->lhsType->name);
  report(s, exponent ? "shift-exponent" : "shift-base", d->loc, detail);
}
void __ubsan_handle_out_of_bounds(OutOfBoundsData* d, ValueHandle index) {
  Site* s = hit(d, 0);
  if (!s) return;
  char a[48], detail[300];
  value(a, sizeof(a), d->indexType, index);
  snprintf(detail, sizeof(detail), "index %s out of bounds for type %s", a, d->arrayType->name);
  report(s, "array-bounds", d->loc, detail);
}
void __ubsan_handle_builtin_unreachable(UnreachableData* d) {
  Site* s = hit(d, 0);
  if (s) report(s, "unreachable", d->loc, "execution reached an unreachable program point (traps next)");
}
void __ubsan_handle_missing_return(UnreachableData* d) {
  Site* s = hit(d, 0);
  if (s) report(s, "return", d->loc, "execution reached the end of a value-returning function without returning a value");
}
void __ubsan_handle_vla_bound_not_positive(VLABoundData* d, ValueHandle bound) {
  Site* s = hit(d, 0);
  if (!s) return;
  char a[48], detail[200];
  value(a, sizeof(a), d->type, bound);
  snprintf(detail, sizeof(detail), "variable length array bound evaluates to non-positive value %s", a);
  report(s, "vla-bound", d->loc, detail);
}
void __ubsan_handle_float_cast_overflow(FloatCastOverflowData* d, ValueHandle from) {
  Site* s = hit(d, 0);
  if (!s) return;
  char a[48], detail[300];
  value(a, sizeof(a), d->fromType, from);
  snprintf(detail, sizeof(detail), "%s is outside the range of representable values of type %s (from %s)", a,
           d->toType->name, d->fromType->name);
  report(s, "float-cast-overflow", d->loc, detail);
}
void __ubsan_handle_load_invalid_value(InvalidValueData* d, ValueHandle v) {
  Site* s = hit(d, 0);
  if (!s) return;
  char a[48], detail[300];
  value(a, sizeof(a), d->type, v);
  snprintf(detail, sizeof(detail), "load of value %s, which is not a valid value for type %s", a, d->type->name);
  report(s, strstr(d->type->name, "bool") ? "bool" : "enum", d->loc,
         detail);
}
void __ubsan_handle_invalid_builtin(InvalidBuiltinData* d) {
  Site* s = hit(d, 0);
  if (!s) return;
  report(s, "builtin", d->loc, d->kind == 0 ? "passing zero to ctz(), which is not a valid argument"
                                            : "passing zero to clz(), which is not a valid argument");
}
void __ubsan_handle_nonnull_arg(NonNullArgData* d) {
  Site* s = hit(d, 0);
  if (!s) return;
  char detail[200];
  snprintf(detail, sizeof(detail), "null pointer passed as argument %d, which is declared to never be null",
           d->argIndex);
  report(s, "nonnull-attribute", d->loc, detail);
}
void __ubsan_handle_nonnull_return_v1(NonNullReturnData* d, SourceLocation* loc) {
  Site* s = hit(d, 0);
  if (s) report(s, "returns-nonnull-attribute", *loc, "null pointer returned from function declared to never return null");
}
void __ubsan_handle_nullability_arg(NonNullArgData* d) { __ubsan_handle_nonnull_arg(d); }
void __ubsan_handle_nullability_return_v1(NonNullReturnData* d, SourceLocation* loc) {
  __ubsan_handle_nonnull_return_v1(d, loc);
}
void __ubsan_handle_pointer_overflow(PointerOverflowData* d, ValueHandle base, ValueHandle result) {
  int sub = base == 0 ? 0 : result == 0 ? 1 : 2;
  Site* s = hit(d, sub);
  if (!s) return;
  char detail[200];
  if (sub == 0)
    snprintf(detail, sizeof(detail), "applying non-zero offset 0x%08lx to null pointer", (unsigned long)result);
  else if (sub == 1)
    snprintf(detail, sizeof(detail), "applying non-zero offset to non-null pointer 0x%08lx produced null pointer",
             (unsigned long)base);
  else
    snprintf(detail, sizeof(detail), "pointer index expression with base 0x%08lx overflowed to 0x%08lx",
             (unsigned long)base, (unsigned long)result);
  report(s, "pointer-overflow", d->loc, detail);
}
}  // extern "C"
#endif  // FSA_SAN_UNDEFINED

// =====================================================================================
#ifdef FSA_SAN_ADDRESS
extern "C" void __asan_set_error_report_callback(void (*cb)(const char*));
extern "C" int fsa_jkr_describe(const void* addr, char* out);  // JKRExpHeap.cpp

extern "C" const char* __asan_default_options() {
  // Keep running after a report; one report per pc; no leak check (the game
  // never frees its heaps); stack-use-after-return would need the fake stack,
  // which does not survive the JSPI coroutine switches. ASan fills the first
  // 4 KB of each malloc with 0xbe by default; the port relies on fresh memory
  // being zero like emscripten's own malloc gives it (Aurora read a 0xbebebebe
  // pointer and trapped), so no fill: uninitialised reads are another stream.
  return "halt_on_error=0:suppress_equal_pcs=1:detect_leaks=0:print_summary=0:"
         "detect_stack_use_after_return=0:allocator_may_return_null=1:print_legend=0:"
         "max_malloc_fill_size=0";
}

namespace fsa_sanitizer_rt {
// The report text -> one record. The interesting lines:
//   ERROR: AddressSanitizer: <bug> on address 0x... at pc 0x... bp ... sp ...
//   READ of size 4 at 0x... thread T0
//       #0 0x... in Func file:line:col
//   0x... is located 4 bytes after 12-byte region [...)   (or "Address 0x... is ...")
const char* after(const char* s, const char* pat) {
  const char* p = strstr(s, pat);
  return p ? p + strlen(pat) : nullptr;
}

void asan_report(const char* text) {
  if (gBusy) return;
  char bug[64] = "unknown";
  unsigned long addr = 0, pc = 0;
  if (const char* p = after(text, "AddressSanitizer: ")) {
    sscanf(p, "%63s", bug);
    if (const char* q = after(p, " on address ")) addr = strtoul(q, nullptr, 16);
    if (const char* q = after(p, " at pc ")) pc = strtoul(q, nullptr, 16);
  }
  (void)pc;
  gBusy++;
  // The access line, and the first frames.
  char access[128] = "";
  if (const char* p = strstr(text, "READ of size")) sscanf(p, "%127[^\n]", access);
  else if (const char* p = strstr(text, "WRITE of size")) sscanf(p, "%127[^\n]", access);
  char file[512] = "", func[512] = "";
  unsigned line = 0, col = 0;
  char stack[1400] = "";
  int sn = 0;
  const char* f = strstr(text, "    #0 ");
  for (int i = 0; f && i < 6; i++) {
    char fr[700];
    sscanf(f, " %699[^\n]", fr);
    // "#0 0xADDR in FUNC FILE:LINE:COL" (FUNC may contain spaces).
    const char* in = strstr(fr, " in ");
    if (in) {
      const char* loc = strrchr(fr, ' ');
      if (i == 0) {
        size_t fl = loc > in + 4 ? (size_t)(loc - (in + 4)) : 0;
        if (fl >= sizeof(func)) fl = sizeof(func) - 1;
        memcpy(func, in + 4, fl);
        func[fl] = 0;
        char l[512];
        snprintf(l, sizeof(l), "%s", loc + 1);
        char* c1 = strchr(l, ':');
        if (c1) {
          *c1 = 0;
          line = (unsigned)atoi(c1 + 1);
          char* c2 = strchr(c1 + 1, ':');
          if (c2) col = (unsigned)atoi(c2 + 1);
          snprintf(file, sizeof(file), "%s", l);
        }
      }
      sn += snprintf(stack + sn, sizeof(stack) - sn, "%s%s", i ? " <- " : "", in + 4);
      if (sn >= (int)sizeof(stack)) sn = sizeof(stack) - 1;
    }
    char tag[16];
    snprintf(tag, sizeof(tag), "    #%d ", i + 1);
    f = strstr(f + 1, tag);
  }
  char located[256] = "";
  if (const char* p = strstr(text, " is located ")) {
    const char* b = p;
    while (b > text && b[-1] != '\n') b--;
    sscanf(b, "%255[^\n]", located);
  }
  char heap[256] = "";
  if (strcmp(bug, "use-after-poison") == 0 || strcmp(bug, "unknown-crash") == 0) fsa_jkr_describe((void*)addr, heap);
  char detail[1024];
  snprintf(detail, sizeof(detail), "%s%s%s%s%s", access, located[0] ? "; " : "", located, heap[0] ? "; " : "", heap);
  char extra[1600];
  Buf eb;
  eb.raw(",\"stack\":");
  eb.str(stack);
  snprintf(extra, sizeof(extra), "%s", eb.d);
  char kind[80];
  snprintf(kind, sizeof(kind), "asan-%s", bug);
  // One record per kind and file:line:col (ASan itself reports each pc once;
  // an unrolled loop has several pcs on one line): count = reports.
  uint32_t h = 2166136261u;
  const char* parts[2] = {kind, file};
  for (const char* k : parts) {
    for (; *k; k++) h = (h ^ (unsigned char)*k) * 16777619u;
  }
  h = (h ^ line) * 16777619u;
  h = (h ^ col) * 16777619u;
  Site* s = lookup((const void*)(uintptr_t)h, 0x41);
  if (s && s->json) {
    s->count++;
  } else if (s) {
    fsa_san_new(s, kind, file, line, col, detail, extra);
  }
  gBusy--;
}

__attribute__((constructor)) void asan_hook() { __asan_set_error_report_callback(asan_report); }
}  // namespace fsa_sanitizer_rt
using namespace fsa_sanitizer_rt;
#endif  // FSA_SAN_ADDRESS
