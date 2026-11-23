#include "game/GameAudio.h"
#include "game/Camera.h"
#include "game/GameGlobals.h"
#include "game/GameView.h"
#include "game/Map.h"
#include <dolphin/gx.h>
#include <string.h>
#include "JSystem/JUtility/JUTVideo.h"

static inline bool contains(const JGeometry::TBox2f& b, const JGeometry::TVec2f& p) {
    return p.x >= b.i.x && p.y >= b.i.y && p.x < b.f.x && p.y < b.f.y;
}

CameraEntry_804AA8E8 lbl_804AA8E8[7] = {
    {104.0f, 142.0f, 32.0f, 3.0f, 3.0f, 64.0f, 240, 0.1f, 1},
    {86.0f, 130.0f, 32.0f, 3.0f, 3.0f, 128.0f, 60, 0.1f, 0},
    {64.0f, 130.0f, 32.0f, 3.0f, 1.0f, 32.0f, 60, 0.1f, 0},
    {0.0f, 0.0f, 0.0f, 3.0f, 3.0f, 32.0f, 0, 0.1f, 0},
    {104.0f, 142.0f, 112.0f, 3.0f, 1.0f, 0.0f, 0, 0.2f, 1},
    {96.0f, 130.0f, 96.0f, 3.0f, 1.0f, 0.0f, 0, 0.2f, 1},
    {64.0f, 130.0f, 64.0f, 3.0f, 3.0f, 64.0f, 1200, 0.1f, 0},
};

Camera_804AA9E4::Camera_804AA9E4() {}

Camera_804AA9E4::~Camera_804AA9E4() {}

// FIXME: 99.83 %, scheduling+regalloc; behaviour clean.
void Camera_804AA9E4::fn_8030E5BC() {
    mVisibleBox_0x154.i.x = 0.0f;
    mVisibleBox_0x154.i.y = 0.0f;
    mVisibleBox_0x154.f.x = 512.0f;
    mVisibleBox_0x154.f.y = 384.0f;
    mInTransition_0x1e8 = 0;
    mPrevFocusBox_0x1b4.i.y = 0.0f;
    mPrevFocusBox_0x1b4.i.x = 0.0f;
    mPrevFocusBox_0x1b4.f.y = 0.0f;
    mPrevFocusBox_0x1b4.f.x = 0.0f;
    mOffset_0x1dc.z = 0.0f;
    mOffset_0x1dc.y = 0.0f;
    mOffset_0x1dc.x = 0.0f;
    mShrinkDelay_0x1c4[0] = 0;
    mShrinkDelay_0x1c4[1] = 0;
    mShrinkDelay_0x1c4[2] = 0;
    mShrinkDelay_0x1c4[3] = 0;
    mMargin_0x1d4 = -1.0f;
    mMinHalfWidth_0x1d8 = -1.0f;
    JGeometry::TBox2f box;
    box.i.x = 0.0f;
    box.i.y = 0.0f;
    box.f.x = 256.0f;
    box.f.y = 192.0f;
    fn_8030CB10(&box, 1024.0f);
    mFocusPointNum_0x7f0 = 0;
    mFocusOverride_0x7f4 = 0;
    mShowWholeArea_0x7f5 = 0;
    mWholeAreaTimer_0x838 = -1;
    mOverrideBlend_0x7f6 = 0;
    mOverrideTimer_0x83c = -1;
    mBlendBoxOn_0x840 = 0;
    u16 width = JUTVideo::getManager()->getFbWidth();
    u16 height = JUTVideo::getManager()->getEfbHeight();
    mScreenRect_0x144.i.x = 0;
    mScreenRect_0x144.i.y = 0;
    mScreenRect_0x144.f.x = width;
    mScreenRect_0x144.f.y = height;
    mZoom_0x858 = 1.0f;
    mEntryIndex_0x174 = 0;
    field_0x178 = 0;
    field_0x179 = 0;
    field_0x17a = 0;
    field_0x17b = 0;
    mWidenBounds_0x17c = 0;
    mSkipUpdate_0x17d = 0;
    mUseRequestedBox_0x17e = 0;
}

// The per-frame update: picks the table entry for the pending request (flags 0x178-0x17B), lets
// fn_8030CD18 work out the target box and depth, blends towards it while a transition is armed,
// sets the view (fn_8030CB10) and the zoom, and tells the audio manager where the camera is.
// FIXME(shotgun): 99.82 %, operands; behaviour clean.
// (audio center construction)
void Camera_804AA9E4::fn_8030E154() {
    JGeometry::TBox2f box = mVisibleBox_0x154;
    if (gGameRoot_8053AAF8->field_0x7e != 0) {
        if (field_0x179 != 0) {
            mEntryIndex_0x174 = 3;
        } else {
            mEntryIndex_0x174 = 1;
        }
    } else if (field_0x179 != 0) {
        mEntryIndex_0x174 = 3;
    } else if (field_0x17a != 0) {
        mEntryIndex_0x174 = 6;
    } else if (field_0x178 != 0) {
        mEntryIndex_0x174 = 2;
    } else if (field_0x17b != 0) {
        mEntryIndex_0x174 = 5;
    } else if (mFocusPointNum_0x7f0 == 0) {
        mEntryIndex_0x174 = 4;
    } else {
        mEntryIndex_0x174 = 0;
    }
    field_0x179 = 0;
    field_0x178 = 0;
    field_0x17a = 0;
    field_0x17b = 0;
    if (mPrevFocusBox_0x1b4.i.x == 0.0f && mPrevFocusBox_0x1b4.i.y == 0.0f && mPrevFocusBox_0x1b4.f.x == 0.0f &&
        mPrevFocusBox_0x1b4.f.y == 0.0f) {
        mShowWholeArea_0x7f5 = 0;
    }
    const CameraEntry_804AA8E8* entry = &lbl_804AA8E8[mEntryIndex_0x174];
    if (mWidenBounds_0x17c != 0) {
        f32 w = box.f.x - box.i.x;
        f32 h = box.f.y - box.i.y;
        box.i.x = box.i.x - w;
        box.i.y = box.i.y - h;
        box.f.x = box.f.x + w;
        box.f.y = box.f.y + h;
    }
    if (mSkipUpdate_0x17d == 0) {
        JGeometry::TBox2f target;
        f32 depth;
        fn_8030CD18(&box, entry, &mPrevFocusBox_0x1b4, &target, &depth);
        if (mInTransition_0x1e8 != 0) {
            f32 t = (f32)mTransitionTimer_0x1ec / 30.0f;
            t = t * t;
            f32 u = 1.0f - t;
            target.i.x = t * target.i.x + u * mTransitionEndBox_0x1a0.i.x;
            target.i.y = t * target.i.y + u * mTransitionEndBox_0x1a0.i.y;
            target.f.x = t * target.f.x + u * mTransitionEndBox_0x1a0.f.x;
            target.f.y = t * target.f.y + u * mTransitionEndBox_0x1a0.f.y;
            depth = t * depth + u * mTransitionEndDepth_0x1b0;
            if (mTransitionTimer_0x1ec == 0) {
                fn_8030C85C();  // the reset of the armed state (inlined in the target)
            } else {
                mTransitionTimer_0x1ec--;
            }
        }
        if (mUseRequestedBox_0x17e != 0) {
            mPrevFocusBox_0x1b4.i.y = 0.0f;
            mPrevFocusBox_0x1b4.i.x = 0.0f;
            mPrevFocusBox_0x1b4.f.y = 0.0f;
            mPrevFocusBox_0x1b4.f.x = 0.0f;
            target.i.x = mRequestedBox_0x180.i.x;
            target.i.y = mRequestedBox_0x180.i.y;
            target.f.x = mRequestedBox_0x180.f.x;
            target.f.y = mRequestedBox_0x180.f.y;
            depth = 0.5f * (target.i.y + target.f.y) + 3072.0f;
            mUseRequestedBox_0x17e = 0;
        }
        fn_8030CB10(&target, depth);
        mZoom_0x858 = (f32)(s32)JUTVideo::sManager->getFbWidth() / (target.f.x - target.i.x);
        JGeometry::TVec3f a(target.f.x, target.i.y, depth);
        JGeometry::TVec3f b(target.i.x, target.i.y, depth);
        JGeometry::TVec3f center;
        center.add(a, b);
        center.scale(0.5f);
        gAudioMgr_8053AF80->fn_80459624(&center, mViewMtx_0x4, 0);
    }
    mFocusPointNum_0x7f0 = 0;
    mFocusOverride_0x7f4 = 0;
    mShowWholeArea_0x7f5 = 0;
    mBlendBoxOn_0x840 = 0;
    mWidenBounds_0x17c = 0;
    mSkipUpdate_0x17d = 0;
}

void Camera_804AA9E4::fn_8030E150() {}

// Loads the projection / view into the GX state (taking the pending matrices first when copy is
// set) and sets the viewport and scissor to the screen rect.
void Camera_804AA9E4::loadGXState_8030DFE8(bool copy) {
    if (copy) {
        JMath::gekko_ps_copy12(mLoadedViewMtx_0x74, mViewMtx_0x4);
        JMath::gekko_ps_copy16(mLoadedProjMtx_0xa4, mProjMtx_0x34);
    }
    GXSetProjection(mLoadedProjMtx_0xa4, GX_ORTHOGRAPHIC);
    GXLoadPosMtxImm(mLoadedViewMtx_0x74, GX_PNMTX8);
    GXLoadNrmMtxImm(mLoadedViewMtx_0x74, GX_PNMTX8);
    GXSetCurrentMtx(GX_PNMTX8);
    setScreenRectViewport();
}

// The 2D view of the box: the view matrix straight into the GX one, the projection from the box's
// size, and (when light is set) the light matrices; then the GX state of loadGXState_8030DFE8.
// FIXME(shotgun): 82.01 %, missing/extra + reloc-name; behaviour drift.
// (the target re-reads i.y, f.x and f.y of the box after the copy; ours keeps all four)
void Camera_804AA9E4::fn_8030DD8C(const JGeometry::TBox2f* box, bool light) {
    mViewBox_0x164.set(*box);
    // The view looks down on the box: the rotation first, then the translation to -eye.
    mLoadedViewMtx_0x74[0][0] = 1.0f;
    mLoadedViewMtx_0x74[0][1] = 0.0f;
    mLoadedViewMtx_0x74[0][2] = 0.0f;
    mLoadedViewMtx_0x74[0][3] = 0.0f;
    mLoadedViewMtx_0x74[1][0] = 0.0f;
    mLoadedViewMtx_0x74[1][1] = 1.0f;
    mLoadedViewMtx_0x74[1][2] = -1.0f;
    mLoadedViewMtx_0x74[1][3] = 0.0f;
    mLoadedViewMtx_0x74[2][0] = 0.0f;
    mLoadedViewMtx_0x74[2][1] = 1.0f;
    mLoadedViewMtx_0x74[2][2] = 0.0f;
    mLoadedViewMtx_0x74[2][3] = 0.0f;
    JGeometry::TVec3f eye(0.5f * (box->i.x + box->f.x), 0.5f * (box->i.y + box->f.y), 2048.0f);
    f32 halfW = 0.5f * (box->f.x - box->i.x);
    f32 halfH = 0.5f * (box->f.y - box->i.y);
    eye.negate();
    mLoadedViewMtx_0x74[0][3] = eye.x;
    mLoadedViewMtx_0x74[1][3] = eye.y;
    mLoadedViewMtx_0x74[2][3] = eye.z;
    C_MTXOrtho(mLoadedProjMtx_0xa4, -halfH, halfH, -halfW, halfW, 0.0f, 4096.0f);
    if (light) {
        PSMTXCopy(mLoadedViewMtx_0x74, mDrawInfo_0xe4.mCamMtx);
        C_MTXLightOrtho(mDrawInfo_0xe4.mPrjMtx, halfH, -halfH, -halfW, halfW, 0.5f, 0.5f, 0.5f, 0.5f);
    }
    loadGXState_8030DFE8(false);
}

void Camera_804AA9E4::fn_8030DD68() {
    GXSetCurrentMtx(GX_PNMTX8);
}

void Camera_804AA9E4::fn_8030DD20(const Mtx m) {
    Mtx tmp;
    PSMTXConcat(mLoadedViewMtx_0x74, m, tmp);
    GXSetCurrentMtx(GX_PNMTX9);
    GXLoadPosMtxImm(tmp, GX_PNMTX9);
    GXLoadNrmMtxImm(tmp, GX_PNMTX9);
}

// FIXME(shotgun): 88.47 %, missing/extra + reloc-name; behaviour drift.
// + regalloc
void Camera_804AA9E4::fn_8030CD18(const JGeometry::TBox2f* bounds, const CameraEntry_804AA8E8* entry,
                                  JGeometry::TBox2f* prevBox, JGeometry::TBox2f* outBox, f32* outDepth) {
    bool first = true;
    s32 focusCount = 0;
    f32 aspect = (f32)(s32)JUTVideo::sManager->getEfbHeight() / (f32)(s32)JUTVideo::sManager->getFbWidth();

    if (mMargin_0x1d4 < 0.0f) {
        mMargin_0x1d4 = entry->field_0x0;
    } else {
        f32 d = 0.1f * (entry->field_0x0 - mMargin_0x1d4);
        if (d < 0.0f) {
            if (d < -3.0f) {
                d = -3.0f;
            }
        } else {
            if (d > 3.0f) {
                d = 3.0f;
            }
        }
        mMargin_0x1d4 += d;
    }

    if (mMinHalfWidth_0x1d8 < 0.0f) {
        mMinHalfWidth_0x1d8 = entry->field_0x4;
    } else {
        f32 d = 0.1f * (entry->field_0x4 - mMinHalfWidth_0x1d8);
        if (d < 0.0f) {
            if (d < -3.0f) {
                d = -3.0f;
            }
        } else {
            if (d > 3.0f) {
                d = 3.0f;
            }
        }
        mMinHalfWidth_0x1d8 += d;
    }

    f32 xMin, xMax, yMin, yMax, yMinOrig, yMaxOrig;
    if (prevBox != NULL &&
        (prevBox->i.x != 0.0f || prevBox->f.x != 0.0f || prevBox->i.y != 0.0f || prevBox->f.y != 0.0f)) {
        focusCount = mFocusPointNum_0x7f0;
        if (mFocusOverride_0x7f4 != 0) {
            if (mFocusMode_0x7f8 == 4) {
                first = false;
                yMin = mFocusPos_0x7fc.y;
                xMin = mFocusPos_0x7fc.x;
                yMax = yMin;
                xMax = xMin;
                yMinOrig = yMin;
                yMaxOrig = yMin;
            } else if (mFocusMode_0x7f8 == 5) {
                first = false;
                yMinOrig = mFocusBox_0x808.i.y;
                yMaxOrig = mFocusBox_0x808.f.y;
                yMin = yMinOrig;
                xMin = mFocusBox_0x808.i.x;
                yMax = yMaxOrig;
                xMax = mFocusBox_0x808.f.x;
            }
            mShrinkDelay_0x1c4[0] = 0;
            mShrinkDelay_0x1c4[1] = 0;
            mShrinkDelay_0x1c4[2] = 0;
            mShrinkDelay_0x1c4[3] = 0;
        }
    } else {
        mFocusOverride_0x7f4 = 0;
    }

    s32 totalPoints = focusCount + 4;
    for (s32 i = 0; i < totalPoints; i++) {
        if (mFocusOverride_0x7f4 != 0 && mFocusMode_0x7f8 >= 0) {
            if (mFocusMode_0x7f8 != i || i >= 4) {
                if (mFocusMode_0x7f8 != 6 || i < 4) {
                    continue;
                }
            }
        }
        f32 px, py;
        if (i < 4) {
            if (!isPlayerActive_8023F264(i)) {
                continue;
            }
            s32 layer = getPlayerLayer_8023DE58(i);
            if (entry->field_0x20 != 0 &&
                !(fn_8022CAF8(0) == 4 && mInTransition_0x1e8 == 0 && !fn_80234134(0))) {
                if (i != 0 || (fn_8022CAF8(0) == 4 && !fn_80234134(0))) {
                    if (mInTransition_0x1e8 == 0 || fn_8022CAF8(0) != 4 || i != getRepresentativePlayer_8022EA0C(0)) {
                        continue;
                    }
                }
                if (layer <= 8) {
                    const JGeometry::TVec3f* pos = fn_8023E3B8(i);
                    px = pos->x;
                    py = pos->y;
                } else {
                    const JGeometry::TVec3f* pos = getPlayerLastTvPos_8023E32C(i);
                    px = pos->x;
                    py = pos->y;
                }
            } else {
                if (layer <= 8) {
                    const JGeometry::TVec3f* pos = fn_8023E3B8(i);
                    px = pos->x;
                    py = pos->y;
                } else {
                    if (i != 0 && gGameRoot_8053AAF8->field_0x7e == 0) {
                        continue;
                    }
                    const JGeometry::TVec3f* pos = getPlayerLastTvPos_8023E32C(i);
                    px = pos->x;
                    py = pos->y;
                }
            }
        } else {
            const JGeometry::TVec3f& pt = mFocusPoints_0x1f0[i - 4];
            if (entry->field_0x20 != 0 && !first) {
                f32 margin = mMargin_0x1d4 - entry->field_0x8;
                px = pt.x;
                py = pt.y;
                if (margin != 0.0f) {
                    if (px < xMin) {
                        if (px < xMin - margin) {
                            px += margin;
                        } else {
                            px = xMin;
                        }
                    } else if (px > xMax) {
                        if (px > xMax + margin) {
                            px -= margin;
                        } else {
                            px = xMax;
                        }
                    }
                    if (py < yMin) {
                        if (py < yMin - margin) {
                            py += margin;
                        } else {
                            py = yMin;
                        }
                    } else if (py > yMax) {
                        if (py > yMax + margin) {
                            py -= margin;
                        } else {
                            py = yMax;
                        }
                    }
                }
            } else {
                px = pt.x;
                py = pt.y;
            }
        }

        if (first) {
            first = false;
            xMin = px;
            xMax = px;
            yMin = py;
            yMax = py;
            yMinOrig = py;
            yMaxOrig = py;
        } else {
            if (xMin > px) {
                xMin = px;
            }
            if (yMin > py) {
                yMin = py;
            }
            if (yMinOrig > py) {
                yMinOrig = py;
            }
            if (xMax < px) {
                xMax = px;
            }
            if (yMax < py) {
                yMax = py;
            }
            if (yMaxOrig < py) {
                yMaxOrig = py;
            }
        }
    }

    if (mShowWholeArea_0x7f5 != 0) {
        yMinOrig = bounds->i.y;
        f32 pad = entry->field_0x0;
        yMaxOrig = bounds->f.y;
        yMin = yMinOrig + pad;
        yMax = yMaxOrig - pad;
        xMin = bounds->i.x + pad;
        xMax = bounds->f.x - pad;
    } else {
        mWholeAreaTimer_0x838 = -1;
    }

    if (mFocusOverride_0x7f4 == 0) {
        mOverrideTimer_0x83c = -1;
    }

    if (prevBox != NULL &&
        (prevBox->i.x != 0.0f || prevBox->f.x != 0.0f || prevBox->i.y != 0.0f || prevBox->f.y != 0.0f)) {
        f32 step;
        if (mFocusOverride_0x7f4 != 0 || focusCount > 0) {
            step = 0.0f;
        } else {
            step = entry->field_0x14;
        }
        if (xMin > prevBox->i.x) {
            if (xMin < prevBox->i.x + step) {
                xMin = prevBox->i.x;
            } else {
                xMin -= step;
            }
        }
        if (xMax < prevBox->f.x) {
            if (xMax > prevBox->f.x - step) {
                xMax = prevBox->f.x;
            } else {
                xMax += step;
            }
        }
        if (yMin > prevBox->i.y) {
            if (yMin < prevBox->i.y + step) {
                yMin = prevBox->i.y;
            } else {
                yMin -= step;
            }
        }
        if (yMax < prevBox->f.y) {
            if (yMax > prevBox->f.y - step) {
                yMax = prevBox->f.y;
            } else {
                yMax += step;
            }
        }

        bool blended = false;
        if (mShowWholeArea_0x7f5 != 0) {
            if (mWholeAreaTimer_0x838 < 0) {
                mWholeAreaTimer_0x838 = 70;
                mWholeAreaFromBox_0x818 = *prevBox;
            }
            if (mWholeAreaTimer_0x838 > 0) {
                f32 a0 = (f32)mWholeAreaTimer_0x838 * 180.0f * (1.0f / 70.0f);
                f32 a1 = 180.0f * (1.0f - (0.5f * JMath::sincosTable_.cosDegree(a0) + 0.5f));
                blended = true;
                mWholeAreaTimer_0x838--;
                f32 w = 0.5f * JMath::sincosTable_.cosDegree(a1) + 0.5f;
                f32 u = 1.0f - w;
                xMin = mWholeAreaFromBox_0x818.i.x * u + xMin * w;
                yMin = mWholeAreaFromBox_0x818.i.y * u + yMin * w;
                xMax = mWholeAreaFromBox_0x818.f.x * u + xMax * w;
                yMax = mWholeAreaFromBox_0x818.f.y * u + yMax * w;
            }
        }

        if (!blended && mFocusOverride_0x7f4 != 0 && mOverrideBlend_0x7f6 != 0) {
            if (mOverrideTimer_0x83c < 0) {
                mOverrideTimer_0x83c = 40;
                mOverrideFromBox_0x828 = *prevBox;
            }
            if (mOverrideTimer_0x83c > 0) {
                f32 a0 = (f32)mOverrideTimer_0x83c * 180.0f * 0.025f;
                f32 a1 = 180.0f * (1.0f - (0.5f * JMath::sincosTable_.cosDegree(a0) + 0.5f));
                blended = true;
                mOverrideTimer_0x83c--;
                f32 w = 0.5f * JMath::sincosTable_.cosDegree(a1) + 0.5f;
                f32 u = 1.0f - w;
                xMin = mOverrideFromBox_0x828.i.x * u + xMin * w;
                yMin = mOverrideFromBox_0x828.i.y * u + yMin * w;
                xMax = mOverrideFromBox_0x828.f.x * u + xMax * w;
                yMax = mOverrideFromBox_0x828.f.y * u + yMax * w;
            }
        }

        if (!blended) {
            f32 diff[4];
            diff[0] = prevBox->i.x - xMin;
            diff[1] = xMax - prevBox->f.x;
            diff[2] = prevBox->i.y - yMin;
            diff[3] = yMax - prevBox->f.y;

            f32 maxPos = 0.0f;
            if (diff[0] > maxPos) maxPos = diff[0];
            if (diff[1] > maxPos) maxPos = diff[1];
            if (diff[2] > maxPos) maxPos = diff[2];
            if (diff[3] > maxPos) maxPos = diff[3];

            f32 minNeg = 0.0f;
            if (diff[0] < minNeg) minNeg = diff[0];
            if (diff[1] < minNeg) minNeg = diff[1];
            if (diff[2] < minNeg) minNeg = diff[2];
            if (diff[3] < minNeg) minNeg = diff[3];

            f32 posLim[4];
            for (s32 i = 0; i < 4; i++) {
                if (diff[i] < 0.0f) {
                    if (mShrinkDelay_0x1c4[i] == 0 || diff[i ^ 1] > 0.0f) {
                        if (diff[i ^ 1] < 0.1f) {
                            posLim[i] = entry->field_0x10 * diff[i] / minNeg;
                        } else {
                            posLim[i] = entry->field_0xc * diff[i] / minNeg;
                        }
                    } else {
                        posLim[i] = 0.0f;
                    }
                } else {
                    posLim[i] = 0.0f;
                }
                if (mShrinkDelay_0x1c4[i] > 0) {
                    mShrinkDelay_0x1c4[i]--;
                }
            }

            f32 negLim[4];
            for (s32 i = 0; i < 4; i++) {
                if (diff[i] > 0.0f) {
                    negLim[i] = entry->field_0xc * diff[i] / maxPos;
                    if (mShrinkDelay_0x1c4[i] < entry->field_0x18) {
                        mShrinkDelay_0x1c4[i] = entry->field_0x18;
                    }
                } else {
                    negLim[i] = 0.0f;
                }
            }

            f32 dx0 = entry->field_0x1c * (xMin - prevBox->i.x);
            if (dx0 < 0.0f) {
                if (dx0 < -negLim[0]) dx0 = -negLim[0];
            } else {
                if (dx0 > posLim[0]) dx0 = posLim[0];
            }
            xMin = prevBox->i.x + dx0;

            f32 dx1 = entry->field_0x1c * (xMax - prevBox->f.x);
            if (dx1 < 0.0f) {
                if (dx1 < -posLim[1]) dx1 = -posLim[1];
            } else {
                if (dx1 > negLim[1]) dx1 = negLim[1];
            }
            xMax = prevBox->f.x + dx1;

            f32 dy0 = entry->field_0x1c * (yMin - prevBox->i.y);
            if (dy0 < 0.0f) {
                if (dy0 < -negLim[2]) dy0 = -negLim[2];
            } else {
                if (dy0 > posLim[2]) dy0 = posLim[2];
            }
            yMin = prevBox->i.y + dy0;

            f32 dy1 = entry->field_0x1c * (yMax - prevBox->f.y);
            if (dy1 < 0.0f) {
                if (dy1 < -posLim[3]) dy1 = -posLim[3];
            } else {
                if (dy1 > negLim[3]) dy1 = negLim[3];
            }
            yMax = prevBox->f.y + dy1;
        }
    }

    if (mBlendBoxOn_0x840 != 0) {
        f32 t = mBlendWeight_0x854;
        f32 u = 1.0f - t;
        yMin = mBlendBox_0x844.i.y * t + yMin * u;
        yMax = mBlendBox_0x844.f.y * t + yMax * u;
        xMin = mBlendBox_0x844.i.x * t + xMin * u;
        yMinOrig = yMin;
        yMaxOrig = yMax;
        xMax = mBlendBox_0x844.f.x * t + xMax * u;
    }

    if (prevBox != NULL) {
        prevBox->i.x = xMin;
        prevBox->i.y = yMin;
        prevBox->f.x = xMax;
        prevBox->f.y = yMax;
    }

    f32 pad = mMargin_0x1d4;
    xMin -= pad;
    yMin -= pad;
    xMax += pad;
    yMax += pad;

    if (xMin < bounds->i.x) xMin = bounds->i.x;
    if (yMin < bounds->i.y) yMin = bounds->i.y;
    if (xMax > bounds->f.x) xMax = bounds->f.x;
    if (yMax > bounds->f.y) yMax = bounds->f.y;

    f32 minW = 2.0f * mMinHalfWidth_0x1d8;
    if (xMax - xMin < minW) {
        f32 cx = 0.5f * (xMax + xMin);
        xMin = cx - mMinHalfWidth_0x1d8;
        xMax = cx + mMinHalfWidth_0x1d8;
        if (xMin < bounds->i.x) {
            f32 shift = bounds->i.x - xMin;
            xMin = bounds->i.x;
            xMax += shift;
        }
        if (xMax > bounds->f.x) {
            f32 shift = xMax - bounds->f.x;
            xMax = bounds->f.x;
            xMin -= shift;
        }
    }

    if (yMax - yMin < minW * aspect) {
        f32 cy = 0.5f * (yMax + yMin);
        f32 halfH = mMinHalfWidth_0x1d8 * aspect;
        yMin = cy - halfH;
        yMax = cy + halfH;
        if (yMin < bounds->i.y) {
            f32 shift = bounds->i.y - yMin;
            yMin = bounds->i.y;
            yMax += shift;
        }
        if (yMax > bounds->f.y) {
            f32 shift = yMax - bounds->f.y;
            yMax = bounds->f.y;
            yMin -= shift;
        }
    }

    f32 fbW = (f32)(s32)JUTVideo::sManager->getFbWidth();
    f32 efbH = (f32)(s32)JUTVideo::sManager->getEfbHeight();
    if ((yMax - yMin) * fbW > (bounds->f.x - bounds->i.x) * efbH) {
        f32 cy = 0.5f * (yMin + yMax);
        f32 halfH = aspect * (0.5f * (bounds->f.x - bounds->i.x));
        yMin = cy - halfH;
        yMax = cy + halfH;
    }

    if ((xMax - xMin) * (f32)(s32)JUTVideo::sManager->getEfbHeight() >
        (yMax - yMin) * (f32)(s32)JUTVideo::sManager->getFbWidth()) {
        f32 cy = 0.5f * (yMin + yMax);
        f32 halfH = (0.5f * (xMax - xMin)) * aspect;
        yMin = cy - halfH;
        yMax = cy + halfH;
        if (yMin < bounds->i.y) {
            f32 shift = bounds->i.y - yMin;
            yMin = bounds->i.y;
            yMax += shift;
            if (yMax > bounds->f.y) {
                yMax = bounds->f.y;
            }
        }
        if (yMax > bounds->f.y) {
            f32 shift = yMax - bounds->f.y;
            yMax = bounds->f.y;
            yMin -= shift;
            if (yMin < bounds->i.y) {
                yMin = bounds->i.y;
            }
        }
    } else {
        f32 cx = 0.5f * (xMin + xMax);
        f32 halfW = (0.5f * (yMax - yMin)) / aspect;
        xMin = cx - halfW;
        xMax = cx + halfW;
        if (xMin < bounds->i.x) {
            f32 shift = bounds->i.x - xMin;
            xMin = bounds->i.x;
            xMax += shift;
            if (xMax > bounds->f.x) {
                xMax = bounds->f.x;
            }
        }
        if (xMax > bounds->f.x) {
            f32 shift = xMax - bounds->f.x;
            xMax = bounds->f.x;
            xMin -= shift;
            if (xMin < bounds->i.x) {
                xMin = bounds->i.x;
            }
        }
    }

    xMin += mOffset_0x1dc.x;
    yMin += mOffset_0x1dc.y;
    xMax += mOffset_0x1dc.x;
    yMax += mOffset_0x1dc.y;
    if (xMin < bounds->i.x) {
        f32 shift = bounds->i.x - xMin;
        xMin = bounds->i.x;
        xMax += shift;
    }
    if (yMin < bounds->i.y) {
        f32 shift = bounds->i.y - yMin;
        yMin = bounds->i.y;
        yMax += shift;
    }
    if (xMax > bounds->f.x) {
        f32 shift = xMax - bounds->f.x;
        xMax = bounds->f.x;
        xMin -= shift;
        if (xMin < bounds->i.x) {
            f32 shift2 = bounds->i.x - xMin;
            xMin += shift2;
            xMax = bounds->f.x + shift2;
        }
    }
    if (yMax > bounds->f.y) {
        f32 shift = yMax - bounds->f.y;
        yMax = bounds->f.y;
        yMin -= shift;
        if (yMin < bounds->i.y) {
            f32 shift2 = bounds->i.y - yMin;
            yMin += shift2;
            yMax = bounds->f.y + shift2;
        }
    }

    if (outBox != NULL) {
        outBox->i.x = xMin;
        outBox->i.y = yMin;
        outBox->f.x = xMax;
        outBox->f.y = yMax;
    }
    if (outDepth != NULL) {
        *outDepth = 0.5f * (yMinOrig + yMaxOrig) + 3072.0f;
    }
}

// Not inlined into its caller fn_8030E5BC in the original.
#pragma auto_inline off
// FIXME(shotgun): 98.97 %, scheduling+regalloc; behaviour clean.
// (the target keeps the box corners in float registers; it also pre-zeroes
// the translation column and negates the two centres as one ps_neg pair)
void Camera_804AA9E4::fn_8030CB10(const JGeometry::TBox2f* box, f32 depth) {
    JGeometry::TVec3f trans;
    trans.x = 0.5f * (box->i.x + box->f.x);
    trans.y = 0.5f * (box->i.y + box->f.y);
    trans.z = depth;
    trans.negate();

    // The view: x and y shifted to the box's centre, z the depth axis.
    mViewMtx_0x4[0][0] = 1.0f;
    mViewMtx_0x4[0][1] = 0.0f;
    mViewMtx_0x4[0][2] = 0.0f;
    mViewMtx_0x4[0][3] = 0.0f;
    mViewMtx_0x4[1][0] = 0.0f;
    mViewMtx_0x4[1][1] = 1.0f;
    mViewMtx_0x4[1][2] = -1.0f;
    mViewMtx_0x4[1][3] = 0.0f;
    mViewMtx_0x4[2][0] = 0.0f;
    mViewMtx_0x4[2][1] = 1.0f;
    mViewMtx_0x4[2][2] = 0.0f;
    mViewMtx_0x4[2][3] = 0.0f;
    mViewMtx_0x4[0][3] = trans.x;
    mViewMtx_0x4[1][3] = trans.y;
    mViewMtx_0x4[2][3] = trans.z;

    f32 halfW = 0.5f * box->getWidth();
    f32 halfH = 0.5f * box->getHeight();
    C_MTXOrtho(mProjMtx_0x34, -halfH, halfH, -halfW, halfW, 0.0f, 5120.0f);
    PSMTXCopy(mViewMtx_0x4, mDrawInfo_0xe4.mCamMtx);
    C_MTXLightOrtho(mDrawInfo_0xe4.mPrjMtx, halfH, -halfH, -halfW, halfW, 0.5f, 0.5f, 0.5f, 0.5f);
    JMath::gekko_ps_copy12(mLoadedViewMtx_0x74, mViewMtx_0x4);
    JMath::gekko_ps_copy16(mLoadedProjMtx_0xa4, mProjMtx_0x34);
    mViewBox_0x164.i.x = box->i.x;
    mViewBox_0x164.i.y = box->i.y;
    mViewBox_0x164.f.x = box->f.x;
    mViewBox_0x164.f.y = box->f.y;
}
#pragma auto_inline reset

// Starts the focus transition to the new box: arms the camera, clears the damping, builds the
// focus state from the table entry for mEntryIndex_0x174 (fn_8030CD18), then the same bookkeeping as
// setNextVisibleBox_8030C928.
// FIXME(shotgun): 93.47 %, missing/extra; behaviour near.
// (box copy order in inlined setNextVisibleBox_8030C928)
void Camera_804AA9E4::fn_8030C9E4(const JGeometry::TBox2f* box) {
    mInTransition_0x1e8 = 1;
    mTransitionTimer_0x1ec = 30;
    mShowWholeArea_0x7f5 = 0;
    mMargin_0x1d4 = -1.0f;
    mMinHalfWidth_0x1d8 = -1.0f;
    fn_8030CD18(box, &lbl_804AA8E8[mEntryIndex_0x174], NULL, &mTransitionEndBox_0x1a0, &mTransitionEndDepth_0x1b0);
    setNextVisibleBox_8030C928(box);
}

// FIXME(shotgun): 93.70 %, missing/extra; behaviour near.
// (box copy order)
void Camera_804AA9E4::setNextVisibleBox_8030C928(const JGeometry::TBox2f* box) {
    mNextVisibleBox_0x190.set(box->i.x, box->i.y, box->f.x, box->f.y);
    for (s32 i = 0; i < 4; i++) {
        gGameRoot2_8053AB10->field_0x20->mLayer2Visible_0x2360[i] = gGameRoot2_8053AB10->field_0x20->mLayerVisible_0x235c[i];
    }
    gGameRoot2_8053AB10->field_0x20->mLayer2Origin_0x2374 = gGameRoot2_8053AB10->field_0x20->mLayerOrigin_0x2368;
    gGameRoot2_8053AB10->field_0x20->mLayerOrigin_0x2368 = JGeometry::TVec3f(box->i.x, box->i.y, 0.0f);
}

void Camera_804AA9E4::fn_8030C85C() {
    mInTransition_0x1e8 = 0;
    for (s32 i = 0; i < 4; i++) {
        if (gGameRoot2_8053AB10->field_0x20->mLayerVisible_0x235c[i]) {
            gGameRoot2_8053AB10->field_0x20->mLayer2Visible_0x2360[i] = 0;
        }
    }
    getGameView_802D6A9C()->fn_802DCDF4();
    mVisibleBox_0x154.set(mNextVisibleBox_0x190);
    mPrevFocusBox_0x1b4.i.y = 0.0f;
    mPrevFocusBox_0x1b4.i.x = 0.0f;
    mPrevFocusBox_0x1b4.f.y = 0.0f;
    mPrevFocusBox_0x1b4.f.x = 0.0f;
}

// The camera's box of the given width around center (the height keeps the screen's aspect).
void Camera_804AA9E4::requestBox_8030C7C4(const JGeometry::TVec2f* center, f32 size) {
    f32 halfY = 0.5f * (size * (f32)(s32)JUTVideo::sManager->getEfbHeight() /
                        (f32)(s32)JUTVideo::sManager->getFbWidth());
    Camera_804AA9E4* cam = gGameRoot2_8053AB10->field_0x24;
    cam->mRequestedBox_0x180.set(center->x - 0.5f * size, center->y - halfY, center->x + 0.5f * size,
                         center->y + halfY);
    cam->mUseRequestedBox_0x17e = 1;
}

// One row of a homogeneous (w) projection.
static inline f32 projectRow(const f32* row, f32 x, f32 y, f32 z, f32 w) {
    return x * row[0] + y * row[1] + z * row[2] + w * row[3];
}

// Not inlined into its caller isOnScreen_8030C2A8 in the original (bl worldToScreen_8030C5F0).
#pragma auto_inline off
// A world position's screen position: normalised (0..1) when normalised is set, else in the
// screen rect (inRect) or the whole screen. z is cleared.
void Camera_804AA9E4::worldToScreen_8030C5F0(const Vec* pos, bool inRect, bool normalised, Vec* out) {
    Vec v;
    PSMTXMultVec(mLoadedViewMtx_0x74, pos, &v);
    f32 y = projectRow(mLoadedProjMtx_0xa4[1], v.x, v.y, v.z, 1.0f);
    f32 x = projectRow(mLoadedProjMtx_0xa4[0], v.x, v.y, v.z, 1.0f);
    if (normalised) {
        out->x = 0.5f * (1.0f + x);
        out->y = 0.5f * (1.0f - y);
    } else if (inRect) {
        out->x = 0.5f * (1.0f + x) * (f32)(mScreenRect_0x144.f.x - mScreenRect_0x144.i.x) + (f32)mScreenRect_0x144.i.x;
        out->y = 0.5f * (1.0f - y) * (f32)(mScreenRect_0x144.f.y - mScreenRect_0x144.i.y) + (f32)mScreenRect_0x144.i.y;
    } else {
        out->x = 0.5f * (1.0f + x) * (f32)(s32)JUTVideo::sManager->getFbWidth();
        out->y = 0.5f * (1.0f - y) * (f32)(s32)JUTVideo::sManager->getEfbHeight();
    }
    out->z = 0.0f;
}
#pragma auto_inline reset

void Camera_804AA9E4::fn_8030C4A0(const JGeometry::TVec2f* in, bool flag, JGeometry::TVec3f* out) {
    if (flag) {
        out->set(mViewBox_0x164.i.x + (in->x - (f32)mScreenRect_0x144.i.x) * (mViewBox_0x164.f.x - mViewBox_0x164.i.x) /
                                       (f32)(mScreenRect_0x144.f.x - mScreenRect_0x144.i.x),
                 mViewBox_0x164.i.y + (in->y - (f32)mScreenRect_0x144.i.y) * (mViewBox_0x164.f.y - mViewBox_0x164.i.y) /
                                       (f32)(mScreenRect_0x144.f.y - mScreenRect_0x144.i.y),
                 0.0f);
    } else {
        out->set(mViewBox_0x164.i.x + in->x * (mViewBox_0x164.f.x - mViewBox_0x164.i.x) / (f32)(s32)JUTVideo::sManager->getFbWidth(),
                 mViewBox_0x164.i.y + in->y * (mViewBox_0x164.f.y - mViewBox_0x164.i.y) / (f32)(s32)JUTVideo::sManager->getEfbHeight(),
                 0.0f);
    }
}

// The camera box's world positions of the screen's top-left and bottom-right corners.
void Camera_804AA9E4::fn_8030C380(JGeometry::TVec3f* out) {
    JGeometry::TVec2f topLeft(0.0f, 0.0f);
    fn_8030C4A0(&topLeft, false, &out[0]);
    JGeometry::TVec2f bottomRight((f32)(s32)JUTVideo::sManager->getFbWidth(),
                                  (f32)(s32)JUTVideo::sManager->getEfbHeight());
    fn_8030C4A0(&bottomRight, false, &out[1]);
}

// A plain function that loadGXState_8030DFE8 inlines and whose own copy the linker dropped: the original's
// .sdata2 has 1.0f between isOnScreen_8030C2A8's unsigned int -> float bias and fn_8030C380's signed one,
// so it was generated here (tools/pool.py). The viewport and scissor of the screen rect, with the
// default depth range.
// FAKEMATCH(crack-pass): pool helper failing R2 (1 call site, 7-line definition): split out only where the .sdata2 order needs it.
void Camera_804AA9E4::setScreenRectViewport() {
    u32 w = mScreenRect_0x144.f.x - mScreenRect_0x144.i.x;
    u32 h = mScreenRect_0x144.f.y - mScreenRect_0x144.i.y;
    GXSetViewport((f32)mScreenRect_0x144.i.x, (f32)mScreenRect_0x144.i.y, (f32)w, (f32)h, 0.0f, 1.0f);
    GXSetScissor(mScreenRect_0x144.i.x, mScreenRect_0x144.i.y, w, h);
    GXSetZScaleOffset(1.0f, 0.0f);
}

// Whether the point's screen position lies inside the screen rect shrunk by 14 on every side.
bool Camera_804AA9E4::isOnScreen_8030C2A8(const JGeometry::TVec3f* pos) {
    Vec screen;
    worldToScreen_8030C5F0(pos, true, false, &screen);
    return screen.x >= (f32)(mScreenRect_0x144.i.x + 14) && screen.x < (f32)(mScreenRect_0x144.f.x - 14) &&
           screen.y >= (f32)(mScreenRect_0x144.i.y + 14) && screen.y < (f32)(mScreenRect_0x144.f.y - 14);
}

u8 Camera_804AA9E4::addFocusPoint_8030C210(const JGeometry::TVec3f* pos) {
    s32 result = 0;
    s32 count = mFocusPointNum_0x7f0;
    if (count < 0x80) {
        JGeometry::TVec2f p(pos->x, pos->y);
        if (contains(mVisibleBox_0x154, p)) {
            JGeometry::TVec3f* dst = &mFocusPoints_0x1f0[count];
            dst->x = p.x;
            dst->y = pos->y;
            dst->z = pos->z;
            mFocusPointNum_0x7f0++;
            result = true;
        }
    }
    return result;
}

// Adds the box's two corners as focus points when both lie inside the camera box and there is
// room for two.
u8 Camera_804AA9E4::fn_8030C058(const JGeometry::TBox2f* box) {
    s32 result = false;
    if (mFocusPointNum_0x7f0 + 1 < 0x80) {
        if (contains(mVisibleBox_0x154, box->i) && contains(mVisibleBox_0x154, box->f)) {
            JGeometry::TVec3f topLeft;
            topLeft.x = box->i.x;
            topLeft.y = box->i.y;
            topLeft.z = 0.0f;
            addFocusPoint_8030C210(&topLeft);
            JGeometry::TVec3f bottomRight;
            bottomRight.x = box->f.x;
            bottomRight.y = box->f.y;
            bottomRight.z = 0.0f;
            addFocusPoint_8030C210(&bottomRight);
            result = true;
        }
    }
    return result;
}
