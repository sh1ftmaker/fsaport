#include "../../host_alloc.hpp"
#include <dolphin/vi.h>

#include "../../window.hpp"
#include "aurora/math.hpp"
#include "vi_internal.hpp"

#include <optional>

namespace aurora::vi {
std::optional<GXRenderModeObj> g_renderMode;

Vec2<uint32_t> render_mode_size() noexcept {
  if (!g_renderMode) {
    return {640, 480};
  }
  return {g_renderMode->fbWidth, g_renderMode->efbHeight};
}

void configure(const GXRenderModeObj* rm) noexcept {
  const auto oldSize = render_mode_size();
  if (rm == nullptr) {
    g_renderMode.reset();
  } else {
    g_renderMode = *rm;
  }
  if (render_mode_size() != oldSize) {
    window::request_frame_buffer_resize();
  }
}

Vec2<uint32_t> configured_fb_size() noexcept {
  return render_mode_size();
}
} // namespace aurora::vi

extern "C" {
void VIInit() {
  AURORA_HOST_ALLOC;}
void VIConfigure(const GXRenderModeObj* rm) {
  AURORA_HOST_ALLOC; aurora::vi::configure(rm); }
void VIConfigurePan(u16 xOrg, u16 yOrg, u16 width, u16 height) {
  AURORA_HOST_ALLOC;
  (void)xOrg;
  (void)yOrg;
  (void)width;
  (void)height;
}
u32 VIGetTvFormat() {
  AURORA_HOST_ALLOC; return 0; }
void VIFlush() {
  AURORA_HOST_ALLOC;}

void VISetWindowTitle(const char* title) {
  AURORA_HOST_ALLOC; aurora::window::set_title(title); }
void VISetWindowFullscreen(bool fullscreen) {
  AURORA_HOST_ALLOC; aurora::window::set_fullscreen(fullscreen); }
bool VIGetWindowFullscreen() {
  AURORA_HOST_ALLOC; return aurora::window::get_fullscreen(); }
void VISetWindowSize(uint32_t width, uint32_t height) {
  AURORA_HOST_ALLOC; aurora::window::set_window_size(width, height); }
void VISetWindowPosition(uint32_t x, uint32_t y) {
  AURORA_HOST_ALLOC; aurora::window::set_window_position(x, y); }
void VICenterWindow() {
  AURORA_HOST_ALLOC; aurora::window::center_window(); }
void VISetFrameBufferScale(float scale) {
  AURORA_HOST_ALLOC; aurora::window::set_frame_buffer_scale(scale); }
}
