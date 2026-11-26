// The game's data with no source form, copied from the player's main.dol.
//
// The build carries no game bytes: tools/gen_globals.py emit defines the
// storage of each object (zero-filled, at its size and alignment) and
// fsa_dol_blobs[], a table of (storage, DOL address, size, element widths).
// Before the runtime starts, platform/web/fsa_disc.js reads main.dol from the
// disc the player chose and checks its SHA-256 (Module.fsaDolLoad); this
// constructor, the first to run (priority 101, before every game and JSystem
// constructor), copies each object's bytes and makes them little-endian per
// element (the widths repeat over the object; "1" leaves bytes as they are).
#include <emscripten.h>
#include <stdint.h>

extern "C" {
struct fsa_dol_blob {
  void* dst;
  uint32_t addr;
  uint32_t size;
  const char* widths;
};
extern const fsa_dol_blob fsa_dol_blobs[];
extern const unsigned fsa_dol_nblobs;
}

// Copies DOL bytes [addr, addr + size) (a virtual address in one of the DOL's
// sections) to dst. 0 when main.dol is not loaded or the range is outside it.
EM_JS(int, fsa_dol_read, (void* dst, uint32_t addr, uint32_t size), {
  // (EM_JS passes uint32_t as a signed number: DOL addresses are 0x80000000+.)
  return Module.fsaDolRead ? Module.fsaDolRead(dst >>> 0, addr >>> 0, size >>> 0) : 0;
});

EM_JS(void, fsa_dol_fail, (uint32_t addr, uint32_t size), {
  abort('main.dol: 0x' + (addr >>> 0).toString(16) + '+0x' + (size >>> 0).toString(16) +
        ' could not be read (no disc chosen, or not the G4SE01 main.dol)');
});

__attribute__((constructor(101))) static void fsa_dol_fill() {
  for (unsigned i = 0; i < fsa_dol_nblobs; i++) {
    const fsa_dol_blob& b = fsa_dol_blobs[i];
    if (!fsa_dol_read(b.dst, b.addr, b.size)) {
      fsa_dol_fail(b.addr, b.size);
      return;
    }
    const char* w = b.widths;
    if (w[0] == '1' && w[1] == 0)
      continue;
    uint8_t* p = static_cast<uint8_t*>(b.dst);
    uint8_t* end = p + b.size;
    for (const char* k = w; p < end; k = *(k + 1) ? k + 1 : w) {
      unsigned n = (unsigned)(*k - '0');
      for (unsigned lo = 0, hi = n - 1; lo < hi; lo++, hi--) {
        uint8_t t = p[lo];
        p[lo] = p[hi];
        p[hi] = t;
      }
      p += n;
    }
  }
}
