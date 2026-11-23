#ifndef _JSYSTEM_JPA_JPABLOCK_H
#define _JSYSTEM_JPA_JPABLOCK_H

#include "JSystem/JGeometry.h"
#include "types.h"
#include "port/be.h"

#include "JSystem/JParticle/JPAField.h"

struct JKRHeap;
struct JPAEmitterWorkData;
struct JPABaseParticle;

typedef void JPAVolumeFunc(JPAEmitterWorkData *);

enum JPADynFlags
{
    JPADYN_FixedDensity = 0x1,
    JPADYN_FixedInterval = 0x2,
    JPADYN_InheritScale = 0x4,
    JPADYN_FollowEmtr = 0x8,
    JPADYN_FollowEmtrChld = 0x10,
};

enum JPAVolType
{
    JPAVOL_Cube = 0,
    JPAVOL_Sphere = 1,
    JPAVOL_Cylinder = 2,
    JPAVOL_Torus = 3,
    JPAVOL_Point = 4,
    JPAVOL_Circle = 5,
    JPAVOL_Line = 6,
};

enum JPAFieldBlockType
{
    JPAFIELD_Gravity = 0,
    JPAFIELD_Air = 1,
    JPAFIELD_Magnet = 2,
    JPAFIELD_Newton = 3,
    JPAFIELD_Vortex = 4,
    JPAFIELD_Random = 5,
    JPAFIELD_Drag = 6,
    JPAFIELD_Convection = 7,
    JPAFIELD_Spin = 8,
};

// Port: big-endian vectors in JPA file blocks, read as JGeometry vectors.
struct JPABEVec3f {
    BE<f32> x, y, z;
    operator JGeometry::TVec3f() const
    {
        JGeometry::TVec3f v;
        v.x = x;
        v.y = y;
        v.z = z;
        return v;
    }
};
struct JPABEVec3s {
    BE<s16> x, y, z;
    operator JGeometry::TVec3<s16>() const
    {
        JGeometry::TVec3<s16> v;
        v.x = x;
        v.y = y;
        v.z = z;
        return v;
    }
};

struct JPADynamicsBlockData
{
    // Representation of the contents in a .jpc file
    u8 mMagic[4]; // _00
    BE<u32> mSize;    // _04

    BE<u32> mFlags;                        // _08
    BE<u32> mResUserWork;                  // _0C
    JPABEVec3f mEmitterScl;     // _10
    JPABEVec3f mEmitterTrs;     // _1C
    JPABEVec3f mEmitterDir;     // _28
    BE<f32> mInitialVelOmni;               // _34
    BE<f32> mInitialVelAxis;               // _38
    BE<f32> mInitialVelRndm;               // _3C
    BE<f32> mInitialVelDir;                // _40
    BE<f32> mSpread;                       // _44
    BE<f32> mInitialVelRatio;              // _48
    BE<f32> mRate;                         // _4C
    BE<f32> mRateRndm;                     // _50
    BE<f32> mLifeTimeRndm;                 // _54
    BE<f32> mVolumeSweep;                  // _58
    BE<f32> mVolumeMinRad;                 // _5C
    BE<f32> mAirResist;                    // _60
    BE<f32> mMoment;                       // _64
    JPABEVec3s mEmitterRot; // _68
    BE<s16> mMaxFrame;                     // _6E
    BE<s16> mStartFrame;                   // _70
    BE<s16> mLifeTime;                     // _72
    BE<u16> mVolumeSize;                   // _74
    BE<u16> mDivNumber;                    // _76
    u8 mRateStep;                      // _78
    BE<u32> _7C;                           // _7C
};

/**
 * @size{0x8}
 */
struct JPADynamicsBlock
{

    JPADynamicsBlock(const u8 *);
    void init();
    void create(JPAEmitterWorkData *);

    /**
     * @fabricated
     */
    inline const JPADynamicsBlockData *castData() const { return reinterpret_cast<const JPADynamicsBlockData *>(mData); }

    void calc(JPAEmitterWorkData *work) { mVolumeFunction(work); }

    s16 getStartFrame() const { return mData->mStartFrame; }
    u32 getResUserWork() const { return mData->mResUserWork; }
    u32 getFlag() const { return mData->mFlags; }
    u32 getVolumeType() const { return (mData->mFlags >> 8) & 0x07; }
    u16 getDivNumber() const { return mData->mDivNumber; }
    f32 getRateRndm() const { return mData->mRateRndm; }
    void getEmitterScl(JGeometry::TVec3f *vec) const { vec->set(mData->mEmitterScl.x, mData->mEmitterScl.y, mData->mEmitterScl.z); }
    void getEmitterTrs(JGeometry::TVec3f *vec) const { vec->set(mData->mEmitterTrs.x, mData->mEmitterTrs.y, mData->mEmitterTrs.z); }
    void getEmitterDir(JGeometry::TVec3f *vec) const { vec->set(mData->mEmitterDir.x, mData->mEmitterDir.y, mData->mEmitterDir.z); }
    void getEmitterRot(JGeometry::TVec3<s16> *vec) const { vec->set(mData->mEmitterRot.x, mData->mEmitterRot.y, mData->mEmitterRot.z); }
    s16 getMaxFrame() { return mData->mMaxFrame; }
    s16 getLifetime() { return mData->mLifeTime; }
    u16 getVolumeSize() { return mData->mVolumeSize; }
    f32 getRate() { return mData->mRate; }
    u8 getRateStep() { return mData->mRateStep; }
    f32 getVolumeSweep() { return mData->mVolumeSweep; }
    f32 getVolumeMinRad() { return mData->mVolumeMinRad; }
    f32 getInitVelOmni() { return mData->mInitialVelOmni; }
    f32 getInitVelAxis() { return mData->mInitialVelAxis; }
    f32 getInitVelDir() { return mData->mInitialVelDir; }
    f32 getInitVelDirSp() { return mData->mSpread; }
    f32 getInitVelRndm() { return mData->mInitialVelRndm; }
    f32 getInitVelRatio() { return mData->mInitialVelRatio; }
    f32 getAirRes() { return mData->mAirResist; }
    f32 getMomentRndm() const { return mData->mMoment; }

    // unused/inlined:
    void init_jpa(const u8 *, JKRHeap *);

    const JPADynamicsBlockData *mData; // _00
    JPAVolumeFunc *mVolumeFunction;    // _04
};

/**
 * @size{0x2C}
 */
struct JPAFieldBlock
{
    /** @fabricated */
    struct Data
    {
        u8 _00[4];                   // _00
        BE<u32> _04;                     // _04
        BE<u32> _08;                     // _08
        JPABEVec3f mOffset;          // _0C
        JPABEVec3f mVelocity;        // _18
        BE<f32> mAmplitude;              // _24
        BE<f32> mMagRndm;                // _28
        BE<f32> mVal1;                   // _2C
        BE<f32> mFadeInTime;             // _30
        BE<f32> mFadeOutTime;            // _34
        BE<f32> mEnTime;                 // _38
        BE<f32> mDisTime;                // _3C
        u8 mCycle;                   // _40
    };


    JPAFieldBlock(const u8 *, JKRHeap *);

    void init(JKRHeap *);

    // unused/inlined:
    void init_jpa(const u8 *, JKRHeap *);

    inline u32 getSttFlag() const { return mData->_08 >> 16; }
    inline u32 getAddType() const { return (mData->_08 >> 8) & 0x3; }
    inline u32 getType() const { return mData->_08 & 0xF; }
    inline int checkStatus(u16 flag) { return getSttFlag() & flag; }

    inline const JGeometry::TVec3f &getDir() const { return mVelocity; }
    inline JGeometry::TVec3f &getPos()  { return mOffset; }

    inline f32 getMag() const { return mSpeed; }
    inline f32 getEnTime() const { return mData->mEnTime; }
    inline f32 getDisTime() const { return mData->mDisTime; }
    inline f32 getFadeOutTime() const { return mData->mFadeOutTime; }
    inline f32 getFadeInTime() const { return mData->mFadeInTime; }
    inline f32 getFadeOutRate() const { return mFadeOutRate; }
    inline f32 getFadeInRate() const { return mFadeInRate; }

    inline f32 getMagRndm() const { return mData->mMagRndm; }
    inline f32 getVal1() const { return mData->mVal1; }
    inline u16 getCycle() const { return mData->mCycle; }

    inline void getPosOrig(JGeometry::TVec3f *pos) const { pos->set((JGeometry::TVec3f)mData->mOffset); }
    inline void getDirOrig(JGeometry::TVec3f *dir) const { dir->set((JGeometry::TVec3f)mData->mVelocity); }
    inline f32 getMagOrig() const { return mData->mAmplitude; }

    inline void initOpParam() {
        getPosOrig(&mOffset);
        getDirOrig(&mVelocity);
        mSpeed = getMagOrig();
    }

    void prepare(JPAEmitterWorkData *work) { pFld->prepare(work, this); }
    void calc(JPAEmitterWorkData *work, JPABaseParticle *ptcl) { pFld->calc(work, this, ptcl); }

    const Data *mData;           // _00
    JPAFieldBase *pFld;          // _04
    f32 mFadeInRate;             // _08
    f32 mFadeOutRate;            // _0C
    JGeometry::TVec3f mOffset;   // _10
    JGeometry::TVec3f mVelocity; // _1C
    f32 mSpeed;                  // _28
};

struct JPAKeyBlockData
{
    BE<u32> mMagic;        // _00
    BE<u32> mBlockSize;    // _04
    u8 mFlag;          // _08
    u8 mKeyFrameCount; // _09
    u8 mIsLoopEnabled; // _0A
    u8 _0B;            // _0B
};

/**
 * @size{0x8}
 */
struct JPAKeyBlock
{
    JPAKeyBlock(const u8 *);

    f32 calc(f32);

    // Unused/inlined:
    void init_jpa(const u8 *, JKRHeap *);

    const JPAKeyBlockData *mDataStart; // _00
    const BE<f32> *mKeyFrameData;      // _04 (port: big-endian key frames in the jpc)
};

#endif
