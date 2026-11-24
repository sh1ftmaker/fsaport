# Port diagnostics

Runtime diagnostics for the decomp. Game code writes JSON lines with
`fsa_diag(name, json)` (platform/diag.cpp). The page batches them and
`tools/serve.py --diag DIR` stores them in `DIR/<run>/<name>.jsonl`, one run
per page load.

## Uninitialised reads (`uninit`)

A diagnostics build that finds game fields which are read before anything
writes them, and fields that a constructor leaves unset. The motivating case
was `GameRootSub04_801F856C`. Its constructor left `field_0x2c` and
`field_0x30` unset on most stages because the decomp was missing an else
branch. The destructor then deleted a junk pointer, since the port's heap
memory is not zero.

### Building and running

```sh
cmake -S . -B build/web-uninit -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=.../Emscripten.cmake -DFSA_ASSETS=... -DFSA_GENERATED=... \
  -DFSA_DIAG_UNINIT=ON -DFSA_DECOMP_SYMBOLS=/path/to/fsa-decomp/config/G4SE01/symbols.txt
ninja -C build/web-uninit fsa
python3 tools/serve.py --dir build/web-uninit --iso ... --diag logs/diag
tools/uninit_summary.py logs/diag/<run>/uninit.jsonl [more runs] [--all] [--json merged.jsonl]
```

The option is `OFF` by default. In a normal build none of this code is
compiled or linked: there is no poison, no instrumentation, and no
`fsa.uninit.json`.

### How it works

1. **Poison.** `JKRHeap::alloc` fills every new block with `0xA5` bytes. That
   covers every JKR heap type, `operator new`, and `new (heap, align)`. It
   also records the block in an address map (`platform/diag_uninit.cpp`), and
   `JKRHeap::free` releases the record. `malloc` (host code, Aurora, libc++)
   is not poisoned.
2. **Load checks.** After the link, `tools/uninit_instrument.py` rewrites
   `fsa.wasm`. It only changes functions that the linker map places in the
   game's unit libraries (game/src, JSystem, SDK). Each `i32.load`, `f32.load`,
   `i64.load` and `i32.load16_*` gets an inline compare of the loaded value
   with the poison pattern. On a match it calls `fsa_uninit_hit(addr, site)`,
   where `site` is the reading function. The fast path is a tee, a compare and
   a branch, with no call.
3. **Constructor brackets.** Each constructor (Itanium C1/C2, `this` in local
   0) gets a wrapper that calls `fsa_uninit_ctor_enter/exit`, and every direct
   call is sent through the wrapper. When the outermost constructor of a fresh
   block returns, two things happen:
   - The block is tagged with the class, so later reads can name it.
   - The block's bytes that are still `0xA5` are recorded as runs.

   These records are reported when the next block is allocated or
   constructed. If a later constructor runs inside the block (an array
   element, or a placement new), the scan is cut at that point.
4. **Allocation sites.** Each call to `operator new` or `JKRHeap::alloc`
   first stores the calling function's index. A block without a constructor
   (POD, buffers) is then named by the function that allocated it.
5. **Inlining off.** The diagnostics build compiles the game with
   `-fno-inline` and links with `-O1` (no wasm-opt inlining). Otherwise most
   constructors would be inlined into their callers and could not be
   bracketed (64 bracketed instead of 1158).
6. **Names.** The instrumenter writes `fsa.uninit.json` (function index to
   Itanium, demangled and MWCC names plus the original address).
   `platform/web/fsa_uninit.js` uses it to turn the runtime's numbers into the
   records below. MWCC names are generated from the demangled signature and
   kept only when they exist in `symbols.txt`; about 2/3 of the game
   functions match.

### Records

One JSON line per finding:

```json
{"evidence":"read-before-write","class":"GameRootSub04_801F856C",
 "ctor":"GameRootSub04_801F856C::GameRootSub04_801F856C(JKRHeap*)",
 "ctor_mwcc":"__ct__22GameRootSub04_801F856CFP7JKRHeap","ctor_addr":"0x8013A538",
 "field":"0x2c","size":4,"where":"GameRootSub04_801F856C::fn_80139C84()",
 "where_mwcc":"fn_80139C84__22GameRootSub04_801F856CFv","where_addr":"0x80139C84",
 "load":"u32/f32","alloc_size":56,"count":1,
 "scene":{"mode":7,"stage":8,"level":3,"in_stage":1},"key":"1:21926:21917:0:44:4:4"}
```

- `evidence` is one of:
  - `read-before-write`: game code loaded a value that is still the
    untouched poison of a heap block. The field was read before anything
    wrote it. This is proof of a read. `where` is the function that did the
    read.
  - `never-written-after-ctor`: bytes of the block that were still poisoned
    when its outermost constructor returned. `field` is the start of the run
    and `size` its length in bytes. `objects` is the number of objects that
    constructor built, and `count` is how many of them had this run. Many of
    these are fine: padding, fields set by an `init()` later, buffers. They
    are leads, not bugs.
  - `poison-value-outside-heap`: a poison value was loaded from memory that
    is not a known heap block, such as a stack or static copy of an unset
    field. `address` replaces `field`.
- `class` is the most derived constructor that finished on the block, or that
  is still running on it. It is `null` for a block without a constructor,
  which `alloc_site` (the allocating function) identifies instead.
- `field` is the byte offset in the port's layout. That matches the decomp's
  `field_0xNN` wherever the class layout is the same under clang. Watch for
  vtable placement and Aurora's larger GX structures.
- If one function reads more than 8 offsets of one class (a loop over an
  array), the rest are folded into one record with `"field":"many"` and
  `fields_from`/`fields_to`.
- A record is repeated whenever its count changes (every 5 s). Keep the last
  line per `key`, which `tools/uninit_summary.py` does. `key` uses wasm
  function indices, so it is only stable within one build.
- `scene` is taken from the game when the finding is first seen (root mode,
  save stage and level, whether a stage is loaded). During the root object's
  own construction its `mode` can itself be poison (-1515870811).
- Not caught: reads of 1-byte fields (0xA5 is too plausible as a value),
  stack variables, `malloc` memory, and fields of objects whose memory was
  freed and reused without a new allocation (pool allocators such as JPA
  particles are poisoned only when the pool is allocated).

### Verification

With the `GameRootSub04` else branch removed again in a scratch build, the
adventure opening reports:

- `never-written-after-ctor GameRootSub04_801F856C +0x28 size 16`;
- `read-before-write +0x2c in GameRootSub04_801F856C::fn_80139C84()`.

Then the port crashes exactly as the bug did (`StaffRollLoader::fn_8044B8EC`
on the poison pointer). With the fix in place, only `+0x28` and `+0x34`
(4 bytes each) remain as never-written.

### Cost

In stage 1, game time is about 1.45x the normal build (4.3 vs 3.0 ms/frame).
That is the load checks plus `-fno-inline`. Every allocation's poison fill
adds to load times. The address map and the records take about 100 MB of
host memory.
