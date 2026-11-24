// Endian trap: flags multi-byte loads from file bytes that bypass the
// big-endian helpers (cmake -DFSA_ENDIAN_DIAG=ON; docs/DIAGNOSTICS.md, "Endian trap").
//
// The units matching FSA_ENDIAN_DIAG_UNITS are compiled with
// -fsanitize-coverage=trace-pc-guard,trace-loads: every basic block calls
// __sanitizer_cov_trace_pc_guard (we keep its id as the current site) and every
// load calls __sanitizer_cov_loadN(addr) first. The loaders register the
// ranges that hold file bytes (port/endian_diag.h). A 2/4-byte load from such
// a range is a raw read of file data: we count it per site and classify the
// value as "swapped-looking" (the byte-swapped value is much smaller, or a
// plausible float where the raw one is not), "plain-looking" (the reverse: the
// data is little-endian GBA data or was swapped in place at load) or neutral.
// The first hit of a site captures the JS stack to name the function and the
// wasm offset; tools/endian_report.py turns offsets into file:line and units.
//
// BE<T>, BEPtr and BEOffset read byte by byte in this build (port/be.h), so the
// helpers' own reads are 1-byte loads and are not reported.
#ifdef FSA_ENDIAN_DIAG
#include <emscripten.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern "C" void fsa_diag(const char* name, const char* json);

namespace {

constexpr uint32_t kPageShift = 12;
constexpr uint32_t kPages = 0x80000000u >> kPageShift;  // MAXIMUM_MEMORY 2 GB
uint16_t g_pageRefs[kPages];

struct Range {
  uint32_t start, end;
  uint32_t fourcc;
  bool leaf;  // no other range lies inside it (a cached hit can be reused)
  char name[48];
};
constexpr int kMaxRanges = 8192;
Range g_ranges[kMaxRanges];
int g_rangeCount = 0;
int g_cached = -1;

struct Site {
  uint32_t guard;   // 0: empty slot
  uint8_t size;
  uint32_t count, swapped, plain, reported;
  uint32_t firstValue, swappedValue, offset;
  uint32_t fourcc;
  char rangeName[48];
  char func[160];
  uint32_t wasmOffset;
};
constexpr uint32_t kSites = 1u << 14;
Site g_sites[kSites];
uint32_t g_siteCount = 0;
uint32_t g_guard = 0;
uint32_t g_guardCount = 0;
bool g_inHook = false;

void pages(uint32_t start, uint32_t end, int delta) {
  if (end <= start)
    return;
  for (uint32_t p = start >> kPageShift; p <= (end - 1) >> kPageShift && p < kPages; p++) {
    int v = g_pageRefs[p] + delta;
    g_pageRefs[p] = v < 0 ? 0 : v > 0xFFFF ? 0xFFFF : v;
  }
}

void removeAt(int i) {
  pages(g_ranges[i].start, g_ranges[i].end, -1);
  g_ranges[i] = g_ranges[--g_rangeCount];
  g_cached = -1;
}

int findRange(uint32_t a) {
  if (g_cached >= 0 && g_ranges[g_cached].leaf && a >= g_ranges[g_cached].start && a < g_ranges[g_cached].end)
    return g_cached;
  int best = -1;
  for (int i = 0; i < g_rangeCount; i++) {
    const Range& r = g_ranges[i];
    if (a >= r.start && a < r.end && (best < 0 || r.end - r.start < g_ranges[best].end - g_ranges[best].start))
      best = i;
  }
  g_cached = best;
  return best;
}

int bitlen(uint32_t v) { return v ? 32 - __builtin_clz(v) : 0; }
int mag16(uint16_t v) { int16_t s = (int16_t)v; return bitlen(s < 0 ? (uint32_t)(-(int32_t)s) : (uint32_t)s); }
int mag32(uint32_t v) { int32_t s = (int32_t)v; return bitlen(s < 0 ? (uint32_t)(-(int64_t)s) : (uint32_t)s); }
bool plausibleFloat(uint32_t v) {
  if ((v & 0x7FFFFFFF) == 0)
    return true;
  int e = (int)((v >> 23) & 0xFF) - 127;
  return e >= -20 && e <= 20;
}

// +1 swapped-looking, -1 plain-looking, 0 neutral.
int classify(uint32_t v, int size) {
  if (size == 2) {
    uint16_t x = v, s = __builtin_bswap16(x);
    if (x == s)
      return 0;
    if (mag16(s) + 4 <= mag16(x))
      return 1;
    if (mag16(x) + 4 <= mag16(s))
      return -1;
    return 0;
  }
  uint32_t s = __builtin_bswap32(v);
  if (v == s)
    return 0;
  // Four printable bytes (a tag such as 'TIMG'): either order is plausible.
  bool ascii = true;
  for (int i = 0; i < 4; i++) {
    uint8_t c = v >> (8 * i);
    ascii = ascii && c >= 0x20 && c < 0x7F;
  }
  if (ascii)
    return 0;
  // A pointer into the heap (relocated offsets, resource pointers after load).
  uint32_t heapEnd = (uint32_t)__builtin_wasm_memory_size(0) << 16;
  if (v >= 0x100000 && v < heapEnd && (v & 3) == 0)
    return -1;
  int mv = mag32(v), ms = mag32(s);
  bool fv = plausibleFloat(v), fs = plausibleFloat(s);
  // A float that only makes sense one way round (unless the other way is a byte-sized int).
  if (fs && !fv && mv > 8)
    return 1;
  if (fv && !fs && ms > 8)
    return -1;
  // An integer much smaller one way round.
  if (ms + 8 <= mv && !fv)
    return 1;
  if (mv + 8 <= ms && !fs)
    return -1;
  return 0;
}

}  // namespace

// The first wasm frame below the load hook: "name" and its code offset.
EM_JS(int, endian_js_caller, (char* out, int cap), {
  var lines = (new Error().stack || "").split("\n");
  var i = 0;
  for (; i < lines.length; i++)
    if (lines[i].indexOf("__sanitizer_cov_load") >= 0) break;
  var line = lines[i + 1] || "";
  line = line.trim();
  if (line.indexOf("at ") == 0) line = line.substring(3);
  stringToUTF8(line, out, cap);
  var k = line.lastIndexOf(":0x");
  return k >= 0 ? parseInt(line.substring(k + 1), 16) : 0;
});

namespace {

void identify(Site& s) {
  char frame[sizeof(s.func) + 96];
  s.wasmOffset = (uint32_t)endian_js_caller(frame, sizeof(frame));
  // "fsa.wasm.NAME (url:wasm-function[N]:0xOFF)" -> NAME
  char* name = frame;
  if (!strncmp(name, "fsa.wasm.", 9))
    name += 9;
  char* paren = strstr(name, " (");
  if (paren)
    *paren = 0;
  snprintf(s.func, sizeof(s.func), "%s", name);
}

void check(const void* p, int size) {
  uint32_t a = (uint32_t)(uintptr_t)p;
  if (!g_pageRefs[a >> kPageShift] || g_inHook)
    return;
  int ri = findRange(a);
  if (ri < 0)
    return;
  g_inHook = true;
  const Range& r = g_ranges[ri];
  const uint8_t* b = (const uint8_t*)p;
  uint32_t v = size == 2 ? (uint32_t)(b[0] | b[1] << 8) : (uint32_t)(b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24);
  uint32_t key = g_guard * 2 + (size == 4);
  uint32_t h = (key * 2654435761u) & (kSites - 1);
  for (uint32_t n = 0; n < kSites; n++, h = (h + 1) & (kSites - 1)) {
    Site& s = g_sites[h];
    if (s.guard == 0) {
      if (g_siteCount >= kSites - 1)
        break;
      g_siteCount++;
      s.guard = key | 0x80000000u;
      s.size = size;
      s.firstValue = v;
      s.offset = a - r.start;
      s.fourcc = r.fourcc;
      memcpy(s.rangeName, r.name, sizeof(s.rangeName));
      identify(s);
    } else if (s.guard != (key | 0x80000000u)) {
      continue;
    }
    s.count++;
    int c = classify(v, size);
    if (c > 0 && s.swapped++ == 0)
      s.swappedValue = v;
    else if (c < 0)
      s.plain++;
    break;
  }
  g_inHook = false;
}

void jsonString(char*& o, char* end, const char* s) {
  if (o < end) *o++ = '"';
  for (; *s && o + 7 < end; s++) {
    unsigned char c = *s;
    if (c == '"' || c == '\\') {
      *o++ = '\\';
      *o++ = c;
    } else if (c < 0x20) {
      o += snprintf(o, end - o, "\\u%04x", c);
    } else {
      *o++ = c;
    }
  }
  if (o < end) *o++ = '"';
}

const char* formatOf(const Site& s, char* buf) {
  if (s.fourcc) {
    for (int i = 0; i < 4; i++) {
      char c = (char)(s.fourcc >> (24 - 8 * i));
      buf[i] = (c >= 0x20 && c < 0x7F) ? c : '?';
    }
    buf[4] = 0;
    return buf;
  }
  const char* dot = strrchr(s.rangeName, '.');
  return dot ? dot + 1 : "";
}

}  // namespace

extern "C" {

void __sanitizer_cov_trace_pc_guard_init(uint32_t* start, uint32_t* stop) {
  if (start == stop || *start)
    return;
  for (uint32_t* g = start; g < stop; g++)
    *g = ++g_guardCount;
}
void __sanitizer_cov_trace_pc_guard(uint32_t* guard) { g_guard = *guard; }

void __sanitizer_cov_load1(uint8_t*) {}
void __sanitizer_cov_load2(uint16_t* p) { check(p, 2); }
void __sanitizer_cov_load4(uint32_t* p) { check(p, 4); }
void __sanitizer_cov_load8(uint64_t*) {}
void __sanitizer_cov_load16(void*) {}
void __sanitizer_cov_store1(uint8_t*) {}
void __sanitizer_cov_store2(uint16_t*) {}
void __sanitizer_cov_store4(uint32_t*) {}
void __sanitizer_cov_store8(uint64_t*) {}
void __sanitizer_cov_store16(void*) {}

void fsa_endian_range(const void* p, unsigned int size, unsigned int fourcc, const char* name) {
  uint32_t start = (uint32_t)(uintptr_t)p, end = start + size;
  if (!p || size < 2 || end < start)
    return;
  for (int i = 0; i < g_rangeCount; i++) {
    Range& r = g_ranges[i];
    if (r.start == start && r.end == end) {  // re-registered: relabel
      if (fourcc || name) {
        r.fourcc = fourcc;
        snprintf(r.name, sizeof(r.name), "%s", name ? name : "");
      }
      return;
    }
  }
  if (g_rangeCount == kMaxRanges)
    removeAt(0);
  Range& r = g_ranges[g_rangeCount];
  r.start = start;
  r.end = end;
  r.fourcc = fourcc;
  r.leaf = true;
  snprintf(r.name, sizeof(r.name), "%s", name ? name : "");
  for (int i = 0; i < g_rangeCount; i++) {
    Range& o = g_ranges[i];
    if (o.start <= start && o.end >= end)
      o.leaf = false;
    else if (start <= o.start && end >= o.end)
      r.leaf = false;
  }
  g_rangeCount++;
  pages(start, end, +1);
  g_cached = -1;
}

void fsa_endian_unrange(const void* p, unsigned int size) {
  uint32_t start = (uint32_t)(uintptr_t)p, end = start + size;
  for (int i = g_rangeCount - 1; i >= 0; i--)
    if (g_ranges[i].start >= start && g_ranges[i].start < end)
      removeAt(i);
}

// The registered ranges, to the "endian_ranges" stream (Module._fsa_endian_ranges()).
EMSCRIPTEN_KEEPALIVE void fsa_endian_ranges(void) {
  char line[256], fmt[8];
  for (int i = 0; i < g_rangeCount; i++) {
    const Range& r = g_ranges[i];
    Site tmp;
    tmp.fourcc = r.fourcc;
    memcpy(tmp.rangeName, r.name, sizeof(tmp.rangeName));
    char* o = line;
    char* end = line + sizeof(line) - 1;
    o += snprintf(o, end - o, "{\"start\":\"0x%x\",\"size\":%u,\"leaf\":%d,\"format\":", r.start, r.end - r.start, r.leaf);
    jsonString(o, end, formatOf(tmp, fmt));
    o += snprintf(o, end - o, ",\"name\":");
    jsonString(o, end, r.name);
    o += snprintf(o, end - o, "}");
    *o = 0;
    fsa_diag("endian_ranges", line);
  }
}

EMSCRIPTEN_KEEPALIVE void fsa_endian_report(void) {
  char line[1024], fmt[8];
  for (uint32_t i = 0; i < kSites; i++) {
    Site& s = g_sites[i];
    if (!s.guard || s.count == s.reported)
      continue;
    s.reported = s.count;
    char* o = line;
    char* end = line + sizeof(line) - 1;
    o += snprintf(o, end - o, "{\"function\":");
    jsonString(o, end, s.func);
    o += snprintf(o, end - o, ",\"file\":\"\",\"line\":0,\"unit\":\"\",\"format\":");
    jsonString(o, end, formatOf(s, fmt));
    char field[96];
    snprintf(field, sizeof(field), "+0x%X in %s", s.offset, s.rangeName[0] ? s.rangeName : "(file buffer)");
    o += snprintf(o, end - o, ",\"field\":");
    jsonString(o, end, field);
    uint32_t sample = s.swapped ? s.swappedValue : s.firstValue;
    uint32_t sw = s.size == 2 ? __builtin_bswap16((uint16_t)sample) : __builtin_bswap32(sample);
    char detail[200];
    snprintf(detail, sizeof(detail), "u%d raw load: %u swapped-looking, %u plain-looking of %u; %s 0x%0*X (swapped 0x%0*X)",
             s.size * 8, s.swapped, s.plain, s.count, s.swapped ? "e.g." : "first", s.size * 2, sample, s.size * 2, sw);
    o += snprintf(o, end - o, ",\"evidence\":\"runtime\",\"detail\":");
    jsonString(o, end, detail);
    o += snprintf(o, end - o,
                  ",\"count\":%u,\"size\":%d,\"swapped\":%u,\"plain\":%u,\"wasm\":\"0x%x\",\"site\":%u}",
                  s.count, s.size, s.swapped, s.plain, s.wasmOffset, (s.guard & 0x7FFFFFFF) >> 1);
    *o = 0;
    fsa_diag("endian", line);
  }
}

}  // extern "C"

#endif  // FSA_ENDIAN_DIAG
