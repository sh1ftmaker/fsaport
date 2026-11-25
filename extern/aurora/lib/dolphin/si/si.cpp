#include "../../host_alloc.hpp"
#include <dolphin/si.h>
#include <dolphin/pad.h>
#include "../../input.hpp"

#include <SDL3/SDL_power.h>

// fsaport (deluxe): the port's GBA link (platform/gba_link.cpp) answers for the
// ports it has a GBA plugged into (SI_GBA); 0 = not one of them.
extern "C" uint32_t (*aurora_si_probe_hook)(int32_t chan);
uint32_t (*aurora_si_probe_hook)(int32_t chan) = nullptr;

uint32_t SIProbe(int32_t chan) {
  AURORA_HOST_ALLOC;
  if (aurora_si_probe_hook != nullptr) {
    if (const uint32_t type = aurora_si_probe_hook(chan)) {
      return type;
    }
  }
  auto* const controller = aurora::input::get_controller_for_player(chan);
  if (controller == nullptr) {
    // A port with keyboard bindings is a controller to PADRead, so it must be
    // one to SIProbe too: games that probe before reading (FSA's GBA client
    // does) otherwise drop the keyboard's input. (fsaport change)
    u32 count = 0;
    if (chan >= 0 && PADGetKeyButtonBindings(static_cast<u32>(chan), &count) != nullptr) {
      return SI_GC_CONTROLLER;
    }
    return SI_ERROR_NO_RESPONSE;
  }

  if (controller->m_isGameCube) {
    auto level = SDL_GetJoystickPowerInfo(SDL_GetGamepadJoystick(controller->m_controller), nullptr);
    if (level == SDL_POWERSTATE_UNKNOWN) {
      return SI_GC_WAVEBIRD;
    }
  }

  return SI_GC_CONTROLLER;
}
