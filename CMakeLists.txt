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
set(FSA_ASSETS ${CMAKE_SOURCE_DIR}/build/assets CACHE PATH "assets extracted by tools/extract_assets.py")

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
  -nostdinc -fno-rtti -fno-exceptions -fsigned-char -ffp-contract=off
  -Wno-everything -include ${FSA_GAME}/port/port_prefix.h
  $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>
  $<$<COMPILE_LANGUAGE:C>:-std=gnu99>)
target_compile_definitions(fsa_game_flags INTERFACE FSA_PORT=1)

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
target_compile_definitions(fsa_platform PRIVATE FSA_PORT=1)
target_compile_options(fsa_platform PRIVATE -include ${FSA_GAME}/port/port_prefix.h -Wno-everything)
target_link_libraries(fsa_platform PRIVATE aurora::core)

add_executable(fsa platform/web/empty.c)
target_link_libraries(fsa PRIVATE
  -Wl,--start-group ${FSA_UNIT_LIBS} fsa_platform -Wl,--end-group
  aurora::core aurora::gx aurora::gd aurora::si aurora::vi aurora::pad aurora::mtx aurora::os aurora::card)
set_target_properties(fsa PROPERTIES SUFFIX ".js")
target_link_options(fsa PRIVATE
  -sJSPI=1
  "-sJSPI_EXPORTS=['fsa_thread_entry']"
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 -sMAXIMUM_MEMORY=2147483648
  -sSTACK_SIZE=1048576
  -sEXIT_RUNTIME=0 -sFORCE_FILESYSTEM=1 -sINVOKE_RUN=0 -sENVIRONMENT=web
  "-sEXPORTED_RUNTIME_METHODS=['callMain','FS','HEAPU8']"
  "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_fsa_thread_entry','_fsa_retrace']"
  --profiling-funcs)
