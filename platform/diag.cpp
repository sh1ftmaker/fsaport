// Port diagnostics output (for the decomp: coverage, uninitialised reads,
// sanitizer hits, ...). fsa_diag(name, json) appends one JSON line to the
// stream `name`; the page batches the lines and posts them to
// tools/serve.py --diag DIR, which writes DIR/<run>/<name>.jsonl (one run per
// page load). Game code declares it itself (it cannot include emscripten.h):
//   extern "C" void fsa_diag(const char* name, const char* json);
#include <emscripten.h>

extern "C" EMSCRIPTEN_KEEPALIVE void fsa_diag(const char* name, const char* json) {
  EM_ASM({
    if (Module.fsaDiag) Module.fsaDiag(UTF8ToString($0), UTF8ToString($1));
  }, name, json);
}
