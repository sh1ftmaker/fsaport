// The picture: aspect ratio and zoom (docs/VIEW.md). Plain C declarations, so
// game units (which cannot include emscripten.h) and the platform layer can both
// include it. Implemented in platform/view.cpp.
//
// The game keeps simulating the 4:3 view it always had: the camera's box, its
// clamp at area edges and every "is it on screen" test are untouched. Only the
// drawing of the world is widened (or cropped and scaled): the camera's GX
// projection and viewport are replaced, at the moment they are loaded, by the
// render box that fsa_view_render_box() derives from the camera's own box.
// With the default view (4:3, zoom 1) nothing here does anything, and the
// game draws exactly what it always drew.
#ifndef FSA_PORT_FSA_VIEW_H
#define FSA_PORT_FSA_VIEW_H

#ifdef __cplusplus
extern "C" {
#endif

// A box in the camera's world plane: x right, y down (the camera's view box).
typedef struct FsaViewBox {
    float x0, y0, x1, y1;
} FsaViewBox;

// Non-zero when the view is not the default one (anything to widen or crop).
int fsa_view_active(void);

// The world's render box for the camera box `sim` (what the game's 4:3 picture
// shows), the bounds the camera keeps to (`bounds`: the area) and the players'
// box (`focus`, may be empty: x0 == x1). Returns 0, leaving out alone, when the
// world is to be drawn as the game set it up. Also records the pair for
// fsa_view_last_boxes() and marks the frame as one with a widened world.
int fsa_view_render_box(const FsaViewBox* sim, const FsaViewBox* bounds, const FsaViewBox* focus, FsaViewBox* out);

// The sim box and render box of the last fsa_view_render_box() that returned
// non-zero in this frame; 0 when there was none.
int fsa_view_last_boxes(FsaViewBox* sim, FsaViewBox* render);

// The render target in pixels (the canvas backing store).
void fsa_view_target_size(unsigned* width, unsigned* height);

// Whether 2D drawn on the game's full-screen port may bleed into the side bars
// (fades and other full-screen quads): only while the world is widened.
int fsa_view_bleed(void);
// While non-zero, fsa_view_bleed() says no (2D that must stay in the 4:3 middle).
void fsa_view_bleed_suppress(int on);
// A full-screen 2D quad starting at *x, *w wide (in the full-screen port's
// logical units), widened to the whole picture while the port bleeds, when it
// covered the logical frame's width (*x <= 0 and *x + *w >= logicalWidth).
// For the game's fades, which start at x = 0 and so leave the left bar uncovered.
void fsa_view_cover(float* x, float* w, float logicalWidth, float logicalHeight);

// The J2DOrthoGraph that bleeds: GameRoot2's full-screen port (field_0x10).
extern void* fsa_view_bleed_graf;

// The logical x range [*x0, *x1] that covers the whole render target, for the
// full-screen port of width `logicalWidth` (the game's 608).
void fsa_view_bleed_range(float logicalWidth, float logicalHeight, float* x0, float* x1);

// A point the game computed in its 4:3 screen (Camera worldToScreen_8030C5F0,
// logical units of a logicalWidth x logicalHeight frame) moved to where the
// widened or zoomed picture shows that world position, in the same units
// (outside 0..logicalWidth in the side bars). Unchanged when the world is
// drawn as the game set it up. For drawing only: the HUD's world-anchored
// markers and its "is a player under this corner" tests.
void fsa_view_hud_point(float* x, float* y, float logicalWidth, float logicalHeight);
#ifdef __cplusplus
#define FSA_VIEW_HUD_POINT(v)                                                                    \
    fsa_view_hud_point(&(v).x, &(v).y, (float)(int)JUTVideo::getManager()->getFbWidth(),        \
                       (float)(int)JUTVideo::getManager()->getEfbHeight())
#endif

// Aurora's (extern/aurora/include/dolphin/gx/GXAurora.h): the viewport and
// scissor in render target pixels, past the logical 608x448 frame. The game's
// own GX headers do not declare them.
void GXSetViewportRender(float left, float top, float wd, float ht, float nearz, float farz);
void GXSetScissorRender(unsigned left, unsigned top, unsigned wd, unsigned ht);

#ifdef __cplusplus
}
#endif

#endif
