// The picture: aspect ratio and zoom (docs/VIEW.md, game/port/fsa_view.h).
//
// The page picks an aspect (4:3, 16:10, 16:9, the window's own) and a zoom
// with Module._fsa_view_set; the canvas backing store becomes 480 * aspect by
// 480 pixels, Aurora pillarboxes everything the game draws in its 608x448
// logical frame into the 4:3 box in the middle (AuroraSetContentAspect), and
// the camera (game/src/game/camera_8030C058.cpp) draws the world over the whole
// width through fsa_view_render_box(). The default (4:3, zoom 1) changes
// nothing: no resize, no pillarbox mapping, no camera override.
#include "platform.h"
#include "../game/port/fsa_view.h"

#include <emscripten.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

extern "C" void VISetWindowSize(uint32_t width, uint32_t height);
extern "C" void AuroraSetContentAspect(float aspect);
extern "C" void AuroraGetRenderSize(uint32_t* width, uint32_t* height);
extern u32 g_framesBegun;  // platform/vi.cpp

namespace {
constexpr float kNativeAspect = 4.0f / 3.0f;
constexpr unsigned kHeight = 480;

enum Edge { kEdgeFit = 0, kEdgeFree = 1 };

struct Settings {
  float aspect = kNativeAspect;
  float zoom = 1.0f;
  int edge = kEdgeFit;
};

Settings g_cur;            // in effect
Settings g_pending;        // set by the page, applied at the next frame start
bool g_dirty = false;
bool g_active = false;     // g_cur differs from the default
unsigned g_width = 640, g_height = kHeight;

u32 g_lastWidenFrame = ~0u;
FsaViewBox g_lastSim{}, g_lastRender{};

bool is_default(const Settings& s) {
  return std::fabs(s.aspect - kNativeAspect) < 1e-3f && std::fabs(s.zoom - 1.0f) < 1e-3f;
}

void apply(const Settings& s) {
  g_cur = s;
  g_active = !is_default(s);
  unsigned w = 640;
  if (std::fabs(s.aspect - kNativeAspect) >= 1e-3f)
    w = (unsigned)std::lround(kHeight * s.aspect) & ~1u;
  if (w != g_width) {
    g_width = w;
    VISetWindowSize(w, kHeight);
  }
  // Pillarbox only when the canvas is wider than 4:3; at 640x480 the plain
  // stretch of 608x448 over it is the shipped mapping.
  AuroraSetContentAspect(w != 640 ? kNativeAspect : 0.0f);
  platform_log("[view] aspect %.4f (%ux%u), zoom %.3f, edge %s\n", s.aspect, w, kHeight, s.zoom,
               s.edge == kEdgeFree ? "free" : "fit");
}

float width(const FsaViewBox& b) { return b.x1 - b.x0; }
float height(const FsaViewBox& b) { return b.y1 - b.y0; }

// Moves [lo, hi] (keeping its length) so that it lies inside [min, max], or
// centres it on [min, max] when it is longer.
void clamp_span(float& lo, float& hi, float min, float max) {
  const float len = hi - lo;
  if (len >= max - min) {
    const float c = 0.5f * (min + max);
    lo = c - 0.5f * len;
    hi = c + 0.5f * len;
  } else if (lo < min) {
    lo = min;
    hi = min + len;
  } else if (hi > max) {
    hi = max;
    lo = max - len;
  }
}

// Moves [lo, hi] (keeping its length) so that it contains [a, b] when it can,
// else centres it on [a, b].
void contain_span(float& lo, float& hi, float a, float b) {
  const float len = hi - lo;
  if (len <= b - a) {
    const float c = 0.5f * (a + b);
    lo = c - 0.5f * len;
    hi = c + 0.5f * len;
  } else if (a < lo) {
    lo = a;
    hi = a + len;
  } else if (b > hi) {
    hi = b;
    lo = b - len;
  }
}
}  // namespace

extern "C" {

void* fsa_view_bleed_graf = nullptr;

// Called by the page. aspect: width / height of the picture (clamped to
// 4:3 .. 3:1); zoom: > 1 crops and enlarges, < 1 shows more; edge: 0 keeps
// the widened picture inside the camera's area (cropping instead where the
// area is too narrow), 1 widens symmetrically and shows what lies past it.
EMSCRIPTEN_KEEPALIVE void fsa_view_set(float aspect, float zoom, int edge) {
  Settings s;
  s.aspect = std::isfinite(aspect) ? std::clamp(aspect, kNativeAspect, 3.0f) : kNativeAspect;
  s.zoom = std::isfinite(zoom) ? std::clamp(zoom, 0.5f, 3.0f) : 1.0f;
  s.edge = edge == kEdgeFree ? kEdgeFree : kEdgeFit;
  g_pending = s;
  g_dirty = true;
}

void fsa_view_frame_begin() {
  if (g_dirty) {
    g_dirty = false;
    apply(g_pending);
  }
}

int fsa_view_active(void) { return g_active; }

void fsa_view_target_size(unsigned* w, unsigned* h) {
  uint32_t tw = 0, th = 0;
  AuroraGetRenderSize(&tw, &th);
  if (tw == 0 || th == 0) {
    tw = g_width;
    th = g_height;
  }
  *w = tw;
  *h = th;
}

int fsa_view_render_box(const FsaViewBox* sim, const FsaViewBox* bounds, const FsaViewBox* focus, FsaViewBox* out) {
  if (!g_active || width(*sim) <= 0.0f || height(*sim) <= 0.0f)
    return 0;
  unsigned tw, th;
  fsa_view_target_size(&tw, &th);
  // The sim box fills the 4:3 box in the middle (contentW pixels wide); keep
  // its world-per-pixel scale on each axis, so the extra width is more world.
  const float contentW = std::min((float)th * kNativeAspect, (float)tw);
  const float sx = width(*sim) / contentW, sy = height(*sim) / (float)th;
  float w = (float)tw * sx / g_cur.zoom, h = (float)th * sy / g_cur.zoom;
  const float cx = 0.5f * (sim->x0 + sim->x1), cy = 0.5f * (sim->y0 + sim->y1);
  const bool haveFocus = focus && width(*focus) > 0.0f && height(*focus) >= 0.0f;
  constexpr float kPad = 24.0f;  // world units kept around the players when cropping

  if (g_cur.edge == kEdgeFit && bounds && width(*bounds) > 0.0f && height(*bounds) > 0.0f) {
    // Shrink (zoom in) until the picture fits the area, but never so far that
    // the players' box (plus a margin) no longer fits: past that, show the void.
    float k = std::min({1.0f, width(*bounds) / w, height(*bounds) / h});
    if (haveFocus) {
      k = std::max(k, std::min(1.0f, (width(*focus) + 2 * kPad) / w));
      k = std::max(k, std::min(1.0f, (height(*focus) + 2 * kPad) / h));
    }
    w *= k;
    h *= k;
  }
  FsaViewBox r{cx - 0.5f * w, cy - 0.5f * h, cx + 0.5f * w, cy + 0.5f * h};
  if (haveFocus) {
    contain_span(r.x0, r.x1, focus->x0 - kPad, focus->x1 + kPad);
    contain_span(r.y0, r.y1, focus->y0 - kPad, focus->y1 + kPad);
  }
  if (g_cur.edge == kEdgeFit && bounds && width(*bounds) > 0.0f && height(*bounds) > 0.0f) {
    clamp_span(r.x0, r.x1, bounds->x0, bounds->x1);
    clamp_span(r.y0, r.y1, bounds->y0, bounds->y1);
  }
  *out = r;
  g_lastSim = *sim;
  g_lastRender = r;
  g_lastWidenFrame = g_framesBegun;
  return 1;
}

int fsa_view_last_boxes(FsaViewBox* sim, FsaViewBox* render) {
  if (!g_active || g_lastWidenFrame != g_framesBegun)
    return 0;
  if (sim)
    *sim = g_lastSim;
  if (render)
    *render = g_lastRender;
  return 1;
}

void fsa_view_hud_point(float* x, float* y, float logicalWidth, float logicalHeight) {
  // The boxes of this frame or the one before: HUD logic can run before the
  // world is drawn in a frame.
  if (!g_active || g_lastWidenFrame == ~0u || g_framesBegun - g_lastWidenFrame > 1)
    return;
  if (logicalWidth <= 0 || logicalHeight <= 0 || width(g_lastRender) <= 0 || height(g_lastRender) <= 0)
    return;
  unsigned tw, th;
  fsa_view_target_size(&tw, &th);
  const float contentW = std::min((float)th * kNativeAspect, (float)tw);
  const float off = std::round(((float)tw - contentW) * 0.5f);
  // sim screen -> world -> render target pixels -> the pillarboxed logical frame
  const float wx = g_lastSim.x0 + *x / logicalWidth * width(g_lastSim);
  const float wy = g_lastSim.y0 + *y / logicalHeight * height(g_lastSim);
  const float px = (wx - g_lastRender.x0) / width(g_lastRender) * (float)tw;
  const float py = (wy - g_lastRender.y0) / height(g_lastRender) * (float)th;
  *x = (px - off) * logicalWidth / contentW;
  *y = py * logicalHeight / (float)th;
}

static int g_bleedSuppressed = 0;
void fsa_view_bleed_suppress(int on) { g_bleedSuppressed = on; }

int fsa_view_bleed(void) {
  return !g_bleedSuppressed && g_active && g_width != 640 && g_lastWidenFrame != ~0u && g_framesBegun - g_lastWidenFrame <= 1;
}

void fsa_view_cover(float* x, float* w, float logicalWidth, float logicalHeight) {
  if (!fsa_view_bleed() || *x > 0.0f || *x + *w < logicalWidth)
    return;
  float x0, x1;
  fsa_view_bleed_range(logicalWidth, logicalHeight, &x0, &x1);
  *x = std::min(*x, x0);
  *w = std::max(*x + *w, x1) - *x;
}

void fsa_view_bleed_range(float logicalWidth, float, float* x0, float* x1) {
  unsigned tw, th;
  fsa_view_target_size(&tw, &th);
  const float contentW = std::min((float)th * kNativeAspect, (float)tw);
  const float scale = contentW / logicalWidth;  // target pixels per logical unit
  const float off = std::round(((float)tw - contentW) * 0.5f);
  *x0 = -off / scale;
  *x1 = ((float)tw - off) / scale;
}

}  // extern "C"
