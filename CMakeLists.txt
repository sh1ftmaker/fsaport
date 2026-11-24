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
target_link_options(fsa PRIVATE
  -sJSPI=1
  "-sJSPI_EXPORTS=['fsa_thread_entry']"
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 -sMAXIMUM_MEMORY=2147483648
  -sSTACK_SIZE=4194304
  -sEXIT_RUNTIME=0 -sFORCE_FILESYSTEM=1 -sINVOKE_RUN=0 -sENVIRONMENT=web
  "-sEXPORTED_RUNTIME_METHODS=['callMain','FS','HEAPU8','UTF8ToString']"
  "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_fsa_thread_entry','_fsa_retrace','_fsa_timer','_fsa_next_timer_ms','_fsa_dump_threads','_fsa_set_dvd_log','_fsa_debug_state','_fsa_touch_set','_fsa_debug_nodark','_fsa_print_stack','_fsa_debug_set_stage','_fsa_debug_thp','_fsa_diag']"
  "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/platform/web/fsa_sched.js"
  "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/platform/web/fsa_disc.js"
  "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/platform/web/fsa_save.js"
  "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/platform/web/fsa_audio.js"
  "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/platform/web/fsa_harness.js"
  -lidbfs.js
  --profiling-funcs -Wl,--error-limit=0 -Wl,--Map=fsa.map
  $<$<BOOL:${FSA_DEBUG_LINES}>:-g -O1>)
set_property(TARGET fsa APPEND PROPERTY LINK_DEPENDS
  ${CMAKE_SOURCE_DIR}/platform/web/fsa_sched.js ${CMAKE_SOURCE_DIR}/platform/web/fsa_disc.js
  ${CMAKE_SOURCE_DIR}/platform/web/fsa_save.js ${CMAKE_SOURCE_DIR}/platform/web/fsa_audio.js
  ${CMAKE_SOURCE_DIR}/platform/web/fsa_harness.js)
add_custom_command(TARGET fsa POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/platform/web/index.html ${CMAKE_BINARY_DIR}/index.html
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/platform/web/touch.js ${CMAKE_BINARY_DIR}/touch.js
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/platform/web/gputrace.js ${CMAKE_BINARY_DIR}/gputrace.js)
