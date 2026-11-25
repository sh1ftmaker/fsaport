// Branch coverage build (-DFSA_COVERAGE=ON, docs/DIAGNOSTICS.md "Branch
// coverage"): the game units are compiled with clang's source-based coverage
// (-fprofile-instr-generate -fcoverage-mapping). Their counters live in the
// wasm data (__llvm_prf_cnts); compiler-rt's profile runtime (part of
// emscripten's libcompiler_rt) serialises them as a .profraw image.
//
// fsa_cov_dump() writes that image to the diagnostics stream
// `coverage_profraw`, base64 in chunks (one JSON line per chunk):
//   {"dump":k,"part":i,"parts":n,"size":S,"reads":R,"b64":"..."}
// The page calls it from fsaDiagFlush() and on pagehide (index.html);
// tools/coverage_report.py reassembles the last complete dump of a run.
// Counts are cumulative for the page load (the runtime never resets them).
#include <emscripten.h>
#include <stdint.h>
#include <stdlib.h>

extern "C" uint64_t __llvm_profile_get_size_for_buffer(void);
extern "C" int __llvm_profile_write_buffer(char* buffer);
extern "C" unsigned fsa_harness_reads(void);  // platform/harness.cpp

extern "C" EMSCRIPTEN_KEEPALIVE int fsa_cov_dump(void) {
  static int seq;
  uint64_t size = __llvm_profile_get_size_for_buffer();
  char* buf = (char*)malloc((size_t)size);
  if (!buf)
    return -1;
  if (__llvm_profile_write_buffer(buf) != 0) {
    free(buf);
    return -2;
  }
  int dump = seq++;
  EM_ASM({
    const d = Module.fsaDiag || (typeof window !== 'undefined' && window.fsaDiag);
    if (!d) return;
    const bytes = HEAPU8.subarray($0, $0 + $1);
    const CHUNK = 3 << 19;  // 1.5 MB raw, 2 MB of base64 per line
    const parts = Math.max(1, Math.ceil($1 / CHUNK));
    for (let p = 0; p < parts; p++) {
      const c = bytes.subarray(p * CHUNK, Math.min($1, (p + 1) * CHUNK));
      let s = '';
      for (let i = 0; i < c.length; i += 0x8000)
        s += String.fromCharCode.apply(null, c.subarray(i, i + 0x8000));
      d('coverage_profraw', JSON.stringify({ dump: $2, part: p, parts, size: $1, reads: $3, b64: btoa(s) }));
    }
  }, buf, (int)size, dump, fsa_harness_reads());
  free(buf);
  return (int)size;
}
