#ifndef _JSYSTEM_RESTIMG_H
#define _JSYSTEM_RESTIMG_H

#include <dolphin/gx.h>
#include "types.h"
#include "port/be.h"

#ifndef _JUTTransparency
typedef u8 _JUTTransparency;
#endif

// Port: a file image (BTI); multi-byte fields are big-endian (port/be.h).
struct ResTIMG
{
    inline BOOL isMIPmapEnabled() const { return (mIsMIPmapEnabled > 0); }

    inline u16 getWidth() const { return mSizeX; }
    inline u16 getHeight() const { return mSizeY; }

    u8 mTextureFormat;              // _00
    _JUTTransparency mTransparency; // _01
    BE<u16> mSizeX;                     // _02
    BE<u16> mSizeY;                     // _04
    u8 mWrapS;                      // _06
    u8 mWrapT;                      // _07
    u8 mPaletteFormat;              // _08
    u8 mLutFormat;                  // _09
    BE<u16> mPaletteEntryCount;         // _0A
    BE<u32> mPaletteOffset;             // _0C
    GXBool mIsMIPmapEnabled;        // _10
    GXBool mDoEdgeLOD;              // _11
    GXBool mIsBiasClamp;            // _12
    GXBool mIsMaxAnisotropy;        // _13
    u8 mMinFilterType;              // _14
    u8 mMagFilterType;              // _15
    s8 mMinLOD;                     // _16
    s8 mMaxLOD;                     // _17
    u8 mTotalImageCount;            // _18
    u8 _19;                         // _19, unknown
    BE<s16> mLODBias;                   // _1A
    BE<u32> mImageDataOffset;           // _1C
};

struct ResTIMGPair
{
    ResTIMG _00;
    ResTIMG _20;
};

#endif