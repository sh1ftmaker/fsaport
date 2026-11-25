# fsaport: Four Swords Adventures on WebAssembly + WebGPU.
#
#   source ../fsadecomp/tools/emsdk/emsdk_env.sh
#   python3 tools/extract_assets.py path/to/main.dol build/assets
#   emcmake cmake -S . -B build/web -G Ninja -DCMAKE_BUILD_TYPE=Release
#   ninja -C build/web fsa
cmake_minimum_required(VERSION 3.25)
project(fsaport C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(FSA_GAME ${CMAKE_SOURCE_DIR}/game)
option(FSA_DEBUG_LINES "keep DWARF line tables in fsa.wasm (llvm-symbolizer; links with -O1)" OFF)
set(FSA_ASSETS ${CMAKE_SOURCE_DIR}/build/assets CACHE PATH "assets extracted by tools/extract_assets.py")

# The emscripten toolchain can leave CMAKE_AR empty in subdirectory scopes
# ("" qc ... -> exit 127); Aurora's CMakeLists reads it back from the cache.
set(CMAKE_AR "$ENV{EMSDK}/upstream/bin/llvm-ar" CACHE FILEPATH "" FORCE)

# --- Aurora (GX -> WebGPU, VI, PAD, CARD, OS memory) -------------------------
set(AURORA_DAWN_PROVIDER emscripten CACHE STRING "" FORCE)
set(AURORA_SDL3_PROVIDER vendor CACHE STRING "" FORCE)
set(AURORA_ENABLE_DVD OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_RMLUI OFF CACHE BOOL "" FORCE)
set(AURORA_CACHE_USE_ZSTD OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/aurora EXCLUDE_FROM_ALL)

# --- The decompiled game and JSystem ------------------------------------------
# Flags every unit gets; each unit's own include paths come from game/units.txt.
add_library(fsa_game_flags INTERFACE)
target_compile_options(fsa_game_flags INTERFACE
  -nostdinc -fno-rtti -fno-exceptions -fsigned-char -ffp-contract=off -gline-tables-only
  # MWCC let functions fall off the end without a return; clang would treat
  # those paths as unreachable (that miscompiled into hangs).
  -fno-strict-return
  # The decomp reinterprets memory across types everywhere (MWCC never
  # exploited aliasing rules); with them on, clang dropped stores.
  -fno-strict-aliasing
  -Wno-everything "SHELL:-include ${FSA_GAME}/port/port_prefix.h"
  $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>
  $<$<COMPILE_LANGUAGE:C>:-std=gnu99>)
target_compile_definitions(fsa_game_flags INTERFACE FSA_PORT=1 $<$<BOOL:${FSA_TRACE}>:FSA_PORT_TRACE=1> $<$<BOOL:${FSA_NO_AUDIO}>:FSA_PORT_NO_AUDIO=1>)
option(FSA_NO_AUDIO "skip JAudio initialisation (boot without sound)" OFF)
option(FSA_TRACE "[trace] reports and checks in the game code" OFF)

# --- Sanitizer diagnostics build (docs/DIAGNOSTICS.md "Sanitizer run") ---------
# undefined: UBSan over the game units, recovering; platform/sanitize/sanitize.cpp
#   is the handler runtime. address: ASan over the game units, the JKR heaps
#   poisoning their blocks (game/port/fsa_asan.h). Use a separate build dir.
set(FSA_SANITIZE "" CACHE STRING "diagnostics build: undefined | address (empty: normal build)")
set_property(CACHE FSA_SANITIZE PROPERTY STRINGS "" undefined address)
if(FSA_SANITIZE STREQUAL "undefined")
  # vptr/function need RTTI; return would trap where the game falls off the end
  # of a function (that is why the build has -fno-strict-return), so missing
  # returns are reported at compile time instead (-Wreturn-type, see the doc).
  target_compile_options(fsa_game_flags INTERFACE -fsanitize=undefined -fno-sanitize=vptr,function,return
    -fsanitize-recover=all -Wreturn-type)
elseif(FSA_SANITIZE STREQUAL "address")
  target_compile_options(fsa_game_flags INTERFACE -fsanitize=address -fsanitize-recover=address)
  target_compile_definitions(fsa_game_flags INTERFACE FSA_PORT_ASAN=1)
  # The heap implementations manage the poisoned memory themselves.
  set_source_files_properties(
    ${FSA_GAME}/libs/jsystem/JSystem/JKernel/JKRHeap.cpp ${FSA_GAME}/libs/jsystem/JSystem/JKernel/JKRExpHeap.cpp
    ${FSA_GAME}/libs/jsystem/JSystem/JKernel/JKRSolidHeap.cpp ${FSA_GAME}/libs/jsystem/JSystem/JKernel/JKRUnitHeap.cpp
    PROPERTIES COMPILE_OPTIONS -fno-sanitize=address)
elseif(NOT FSA_SANITIZE STREQUAL "")
  message(FATAL_ERROR "FSA_SANITIZE must be undefined, address or empty")
endif()

execute_process(COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/gen_units_cmake.py
                ${FSA_GAME}/units.txt ${CMAKE_BINARY_DIR}/fsa_units.cmake)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${FSA_GAME}/units.txt)
set(FSA_UNIT_LIBS)
include(${CMAKE_BINARY_DIR}/fsa_units.cmake)
# The units.txt flags carry --std=...; the flags target's -std wins (it comes later).

# --- Platform layer: what the GameCube SDK did, for the browser -----------------
file(GLOB FSA_PLATFORM_SOURCES CONFIGURE_DEPENDS platform/*.cpp platform/*.c)
add_library(fsa_platform STATIC ${FSA_PLATFORM_SOURCES})
target_include_directories(fsa_platform PRIVATE platform
  ${FSA_GAME}/libs/sdk/include ${FSA_GAME}/include)
target_compile_definitions(fsa_platform PRIVATE FSA_PORT=1 FSA_PLATFORM=1)
target_compile_options(fsa_platform PRIVATE "SHELL:-include ${FSA_GAME}/port/port_prefix.h" -Wno-everything -gline-tables-only)
target_link_libraries(fsa_platform PRIVATE aurora::core)

# --- Endian trap (diagnostics; docs/DIAGNOSTICS.md, "Endian trap") -------------
# The matching units call __sanitizer_cov_load2/4 before every load
# (platform/endian_diag.cpp checks them against the ranges that hold file bytes).
option(FSA_ENDIAN_DIAG "endian trap: report raw multi-byte loads from file data (slow; diagnostics only)" OFF)
set(FSA_ENDIAN_DIAG_UNITS "/game/src/game/" CACHE STRING "regex of the source paths the endian trap instruments")
if(FSA_ENDIAN_DIAG)
  target_compile_definitions(fsa_game_flags INTERFACE FSA_ENDIAN_DIAG=1)
  target_compile_definitions(fsa_platform PRIVATE FSA_ENDIAN_DIAG=1)
  foreach(lib ${FSA_UNIT_LIBS})
    get_target_property(srcs ${lib} SOURCES)
    foreach(s ${srcs})
      if(s MATCHES "${FSA_ENDIAN_DIAG_UNITS}")
        set_property(SOURCE ${s} APPEND PROPERTY COMPILE_OPTIONS "-fsanitize-coverage=trace-pc-guard,trace-loads")
      endif()
    endforeach()
  endforeach()
endif()

# Glue built against Aurora's own headers (not the decomp's SDK headers).
add_library(fsa_glue STATIC platform/glue/gx_pipe.cpp)
target_include_directories(fsa_glue PRIVATE extern/aurora/lib)
target_link_libraries(fsa_glue PRIVATE aurora::core aurora::gx)

# The game calls the paired-single matrix names; Aurora provides them with this.
target_compile_definitions(aurora_mtx PRIVATE MTX_USE_PS=1)

add_executable(fsa platform/web/empty.c)
target_link_libraries(fsa PRIVATE
  -Wl,--start-group ${FSA_UNIT_LIBS} fsa_platform fsa_glue -Wl,--end-group
  aurora::core aurora::gx aurora::gd aurora::si aurora::vi aurora::pad aurora::mtx aurora::os aurora::card)
set_target_properties(fsa PROPERTIES SUFFIX ".js")
set(FSA_INITIAL_MEMORY 268435456)
if(FSA_SANITIZE)
  target_sources(fsa PRIVATE platform/sanitize/sanitize.cpp)
  string(TOUPPER ${FSA_SANITIZE} _san)
  set_source_files_properties(platform/sanitize/sanitize.cpp PROPERTIES COMPILE_DEFINITIONS FSA_SAN_${_san}=1
    COMPILE_OPTIONS "-Wno-everything")
  set(FSA_DECOMP_SYMBOLS "${CMAKE_SOURCE_DIR}/../fsadecomp/fsa-decomp/config/G4SE01/symbols.txt"
      CACHE FILEPATH "the decomp's config/G4SE01/symbols.txt (join key for the diagnostics)")
  add_custom_command(TARGET fsa POST_BUILD
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/san_symtab.py ${CMAKE_BINARY_DIR}/fsa.map ${FSA_GAME}/units.txt
            ${CMAKE_BINARY_DIR}/fsa_symtab.json ${FSA_DECOMP_SYMBOLS})
endif()
# --- Branch coverage (diagnostics; docs/DIAGNOSTICS.md, "Branch coverage") -----
# clang source-based coverage of the game's own code: the matching units get
# -fprofile-instr-generate -fcoverage-mapping (a counter increment per region,
# the region map in the wasm's __llvm_covmap/__llvm_covfun custom sections).
# platform/coverage/profile_dump.cpp posts the counters (a .profraw image) to
# the coverage_profraw stream; tools/coverage_report.py turns them into TSV.
option(FSA_COVERAGE "branch coverage build: clang source-based coverage of the game code (diagnostics only)" OFF)
set(FSA_COVERAGE_UNITS "/game/src/game/" CACHE STRING "regex of the source paths the coverage build instruments")
if(FSA_COVERAGE)
  foreach(lib ${FSA_UNIT_LIBS})
    get_target_property(srcs ${lib} SOURCES)
    foreach(s ${srcs})
      if(s MATCHES "${FSA_COVERAGE_UNITS}")
        set_property(SOURCE ${s} APPEND PROPERTY COMPILE_OPTIONS "-fprofile-instr-generate;-fcoverage-mapping")
      endif()
    endforeach()
  endforeach()
  target_sources(fsa PRIVATE platform/coverage/profile_dump.cpp)
  set_source_files_properties(platform/coverage/profile_dump.cpp PROPERTIES COMPILE_OPTIONS "-Wno-everything")
  # compiler-rt's profile runtime (in emscripten's libcompiler_rt).
  target_link_options(fsa PRIVATE -fprofile-instr-generate)
endif()
if(FSA_SANITIZE STREQUAL "address")
  # Shadow memory is 1/8 of the address space; the redzones grow every block.
  set(FSA_INITIAL_MEMORY 1073741824)
  target_link_options(fsa PRIVATE -fsanitize=address -fsanitize-recover=address -gsource-map
    "SHELL:--js-library ${CMAKE_SOURCE_DIR}/platform/sanitize/san_lib.js")
  set_property(TARGET fsa APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/platform/sanitize/san_lib.js)
endif()
target_link_options(fsa PRIVATE
  -sJSPI=1
  "-sJSPI_EXPORTS=['fsa_thread_entry']"
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=${FSA_INITIAL_MEMORY} -sMAXIMUM_MEMORY=2147483648
  -sSTACK_SIZE=4194304
  -sEXIT_RUNTIME=0 -sFORCE_FILESYSTEM=1 -sINVOKE_RUN=0 -sENVIRONMENT=web
  "-sEXPORTED_RUNTIME_METHODS=['callMain','FS','HEAPU8','UTF8ToString']"
  "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_fsa_thread_entry','_fsa_retrace','_fsa_timer','_fsa_next_timer_ms','_fsa_dump_threads','_fsa_set_dvd_log','_fsa_debug_state','_fsa_touch_set','_fsa_debug_nodark','_fsa_print_stack','_fsa_debug_set_stage','_fsa_debug_thp','_fsa_diag']"
  "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/platform/web/fsa_sched.js"
  "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/platform/web/fsa_disc.js"
  "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/platform/web/fsa_save.js"
  "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/platform/web/fsa_audio.js"
  "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/platform/web/fsa_harness.js"
  "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/platform/web/fsa_gba.js"
  "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/platform/web/fsa_net.js"
  "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/platform/web/fsa_net_room.js"
  -lidbfs.js
  --profiling-funcs -Wl,--error-limit=0 -Wl,--Map=fsa.map
  $<$<BOOL:${FSA_DEBUG_LINES}>:-g -O1>)
set_property(TARGET fsa APPEND PROPERTY LINK_DEPENDS
  ${CMAKE_SOURCE_DIR}/platform/web/fsa_sched.js ${CMAKE_SOURCE_DIR}/platform/web/fsa_disc.js
  ${CMAKE_SOURCE_DIR}/platform/web/fsa_save.js ${CMAKE_SOURCE_DIR}/platform/web/fsa_audio.js
  ${CMAKE_SOURCE_DIR}/platform/web/fsa_harness.js ${CMAKE_SOURCE_DIR}/platform/web/fsa_gba.js
  ${CMAKE_SOURCE_DIR}/platform/web/fsa_net.js ${CMAKE_SOURCE_DIR}/platform/web/fsa_net_room.js)
# Netplay (deluxe, docs/NETPLAY.md): the static memory a rollback snapshot covers.
add_custom_command(TARGET fsa POST_BUILD
  COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/net_regions.py ${CMAKE_BINARY_DIR}/fsa.map ${CMAKE_BINARY_DIR}/fsa.netregions.json
  VERBATIM)

# --- Diagnostics build (docs/DIAGNOSTICS.md): function coverage and stub hits ---
# Every game/JSystem unit and the platform layer are built with
# -finstrument-functions (platform/diag_cov.cpp counts the entries); after the
# link tools/diag_symtab.py writes fsa.diag.tsv (wasm table index -> decomp
# symbol, demangled name, unit, kind) next to fsa.wasm. Use a separate build
# directory (build/diag); the normal build is unchanged when this is OFF.
option(FSA_DIAG "diagnostics build: -finstrument-functions coverage and stub-hit streams" OFF)
set(FSA_DECOMP_SYMBOLS "${CMAKE_SOURCE_DIR}/../fsadecomp/fsa-decomp/config/G4SE01/symbols.txt"
    CACHE FILEPATH "the decomp's config/G4SE01/symbols.txt (join key for the diagnostics)")
if(FSA_DIAG)
  # -finstrument-functions-after-inlining instead counts only out-of-line calls
  # (cheaper, but inlined functions then never show up).
  set(FSA_DIAG_INSTRUMENT "-finstrument-functions" CACHE STRING "instrumentation flag for the diagnostics build")
  target_compile_options(fsa_game_flags INTERFACE ${FSA_DIAG_INSTRUMENT})
  target_compile_definitions(fsa_game_flags INTERFACE FSA_DIAG=1)
  target_compile_options(fsa_platform PRIVATE ${FSA_DIAG_INSTRUMENT})
  target_compile_definitions(fsa_platform PRIVATE FSA_DIAG=1)
  # Mangled names in the wasm name section (the join key needs them).
  target_link_options(fsa PRIVATE -Wl,--no-demangle)
  get_filename_component(FSA_LLVM_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
  add_custom_command(TARGET fsa POST_BUILD
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/diag_symtab.py --build ${CMAKE_BINARY_DIR}
            --source ${CMAKE_SOURCE_DIR} --symbols ${FSA_DECOMP_SYMBOLS}
            --llvm-bin ${FSA_LLVM_BIN}/../bin --out ${CMAKE_BINARY_DIR}/fsa.diag.tsv
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR} VERBATIM)
  # Virtual dispatch audit (platform/diag_vdispatch.cpp): marks every
  # call_indirect and passes each function's first argument to the entry hook;
  # writes fsa.diag.vt.tsv (the port's vtables). Runs after diag_symtab.py,
  # which reads the unmodified function bodies.
  option(FSA_DIAG_VDISPATCH "rewrite fsa.wasm for the vdispatch stream (FSA_DIAG builds)" ON)
  if(FSA_DIAG_VDISPATCH)
    add_custom_command(TARGET fsa POST_BUILD
      COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/vdispatch_instrument.py ${CMAKE_BINARY_DIR}/fsa.wasm
              ${CMAKE_BINARY_DIR}/fsa.map --llvm-bin ${FSA_LLVM_BIN}/../bin
      VERBATIM)
  endif()
  # Function traces (platform/diag_fntrace.cpp, platform/web/fsa_fntrace.js):
  # every instrumented function gets a one-byte "selected?" test on entry and
  # exit; the page selects functions at run time (?trace=URL). Runs last.
  option(FSA_DIAG_FNTRACE "rewrite fsa.wasm for the fntrace stream (FSA_DIAG builds)" ON)
  if(FSA_DIAG_FNTRACE)
    target_link_options(fsa PRIVATE "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/platform/web/fsa_fntrace.js")
    set_property(TARGET fsa APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/platform/web/fsa_fntrace.js)
    add_custom_command(TARGET fsa POST_BUILD
      COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/fntrace_instrument.py ${CMAKE_BINARY_DIR}/fsa.wasm
              ${CMAKE_BINARY_DIR}/fsa.map
      VERBATIM)
  endif()
endif()

add_custom_command(TARGET fsa POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/platform/web/index.html ${CMAKE_BINARY_DIR}/index.html
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/platform/web/touch.js ${CMAKE_BINARY_DIR}/touch.js
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/platform/web/gputrace.js ${CMAKE_BINARY_DIR}/gputrace.js)

# --- Diagnostics build: uninitialised reads (docs/DIAGNOSTICS.md) --------------
# Fresh JKR heap blocks are filled with 0xA5; after the link,
# tools/uninit_instrument.py adds poison checks to the game's loads and brackets
# its constructors. Findings go to the "uninit" diagnostics stream.
option(FSA_DIAG_UNINIT "poison fresh heap blocks and report uninitialised reads (slow; diagnostics only)" OFF)
# (FSA_DECOMP_SYMBOLS, the decomp's symbols.txt, is set with FSA_DIAG above.)
if(FSA_DIAG_UNINIT)
  target_compile_definitions(fsa_game_flags INTERFACE FSA_DIAG_UNINIT=1)
  # Constructors must stay functions for the instrumenter to bracket them:
  # no inlining in the game code, and none by wasm-opt at the link (-O1).
  target_compile_options(fsa_game_flags INTERFACE -fno-inline)
  target_compile_definitions(fsa_platform PRIVATE FSA_DIAG_UNINIT=1)
  target_link_options(fsa PRIVATE -O1 "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/platform/web/fsa_uninit.js")
  set_property(TARGET fsa APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/platform/web/fsa_uninit.js)
  add_custom_command(TARGET fsa POST_BUILD
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/uninit_instrument.py ${CMAKE_BINARY_DIR}/fsa.wasm
            ${CMAKE_BINARY_DIR}/fsa.map --symbols ${FSA_DECOMP_SYMBOLS}
    VERBATIM)
endif()
