// Entry point: bring up Aurora (WebGPU), the OS layer and the disc, then run
// the game's own main() (renamed fsa_game_main in the port) on the main thread.
#include "platform.h"

#include <aurora/aurora.h>
#include <emscripten.h>

#include <cstdio>

int fsa_game_main();

namespace {
void log_callback(AuroraLogLevel level, const char* module, const char* message, unsigned int) {
  static const char* names[] = {"debug", "info", "warn", "error", "fatal"};
  if (level >= LOG_INFO)
    printf("[aurora %s] %s: %s\n", names[level], module, message);
}
}  // namespace

int main(int argc, char** argv) {
  {
    HostAllocScope host;  // everything here is host memory, not the game's heaps
    AuroraConfig config{};
    config.appName = "fsaport";
    config.desiredBackend = BACKEND_WEBGPU;
    config.vsync = true;
    config.windowWidth = 640;
    config.windowHeight = 480;
    config.logCallback = log_callback;
    config.logLevel = LOG_INFO;
    config.mem1Size = 64 * 1024 * 1024;  // the GameCube had 24 MB; host structs are larger
    config.mem2Size = 16 * 1024 * 1024;  // ARAM
    aurora_initialize(argc, argv, &config);

    platform_threads_init();
    OSInit();
    platform_dvd_init();
  }
  printf("[fsaport] starting the game\n");
  return fsa_game_main();
}
