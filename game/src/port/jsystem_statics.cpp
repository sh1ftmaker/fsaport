// JSystem definitions the port needs that no compiled unit provides.
//
// - JASGlobalInstance<T>::sInstance: the sources spell these `template <> T*
//   JASGlobalInstance<T>::sInstance;`, which MWCC took as a definition and
//   standard C++ takes as a declaration only. One definition each here.
// - Functions FSA links whose source was never written (the matching build takes
//   them from the original DOL).
#include "JSystem/JAudio/System/JASGadget.h"

// Before the headers below: some of them use getInstance() inline, which would
// instantiate the primary template's member first.
class JAISoundInfo;
class JAISeMgr;
class JAUSoundInfo;
class JASAudioThread;
class JAIStreamMgr;
class JAISeqMgr;
class JASDefaultBankTable;
class JAISoundStarter;
template <> JAISoundInfo* JASGlobalInstance<JAISoundInfo>::sInstance = nullptr;
template <> JAISeMgr* JASGlobalInstance<JAISeMgr>::sInstance = nullptr;
template <> JAUSoundInfo* JASGlobalInstance<JAUSoundInfo>::sInstance = nullptr;
template <> JASAudioThread* JASGlobalInstance<JASAudioThread>::sInstance = nullptr;
template <> JAIStreamMgr* JASGlobalInstance<JAIStreamMgr>::sInstance = nullptr;
template <> JAISeqMgr* JASGlobalInstance<JAISeqMgr>::sInstance = nullptr;
template <> JASDefaultBankTable* JASGlobalInstance<JASDefaultBankTable>::sInstance = nullptr;
template <> JAISoundStarter* JASGlobalInstance<JAISoundStarter>::sInstance = nullptr;

#include "JSystem/JAudio/Interface/JAISound.h"
#include "JSystem/JAudio/Interface/JAISeMgr.h"
#include "JSystem/JAudio/Interface/JAISeqMgr.h"
#include "JSystem/JAudio/Interface/JAIStreamMgr.h"
#include "JSystem/JAudio/JAUAudience.h"
#include "JSystem/JAudio/System/JASAudioThread.h"
#include "JSystem/JAudio/System/JASBankTable.h"
#include "JSystem/J3D/J3DTransform.h"
#include "JSystem/JKernel/JKRFileCache.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRThread.h"
#include "JSystem/JMessage/data.h"
#include "JSystem/JSupport/JSUStream.h"
#include <dolphin/mtx.h>
#include <string.h>

// JMessage's file signatures, compared with memcmp against the first 4 bytes of a
// BMG file ("MESGbmg1") and its colour block ("MGCL"): the bytes in memory must
// read "MESG" / "MGCL", so the int's value depends on the host's byte order.
namespace JMessage {
namespace data {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
const int ga4cSignature = 'GSEM';
const int ga4cSignature_color = 'LCGM';
#else
const int ga4cSignature = 'MESG';
const int ga4cSignature_color = 'MGCL';
#endif
} // namespace data
} // namespace JMessage

// mAB[i] = mA * mB[i] for count matrices (tp's J3DTransform.cpp, without paired singles).
void J3DPSMtxArrayConcat(f32 (*mA)[4], f32 (*mB)[4], f32 (*mAB)[4], u32 count)
{
    Mtx* b = (Mtx*)mB;
    Mtx* ab = (Mtx*)mAB;
    for (u32 n = 0; n < count; n++)
    {
        Mtx tmp;
        for (int r = 0; r < 3; r++)
        {
            for (int c = 0; c < 4; c++)
            {
                f32 v = mA[r][0] * b[n][0][c] + mA[r][1] * b[n][1][c] + mA[r][2] * b[n][2][c];
                if (c == 3)
                    v += mA[r][3];
                tmp[r][c] = v;
            }
        }
        memcpy(ab[n], tmp, sizeof(Mtx));
    }
}

// FSA never mounts a JKRFileCache (see JKRFileCache.cpp); getFirstFile only needs a
// path it can hand to JKRDvdFinder and free from the system heap.
char* JKRFileCache::getDvdPathName(const char* path) const
{
    size_t length = strlen(path) + 1;
    char* dvdPathName = (char*)JKRHeap::getSystemHeap()->alloc(length, 1);
    memcpy(dvdPathName, path, length);
    return dvdPathName;
}

// tp: the one-argument draw prints to OSReport, i.e. the two-argument one without a console.
void JKRThreadSwitch::draw(JKRThreadName_* thread_name_list)
{
    draw(thread_name_list, nullptr);
}

JKRThreadSwitch::~JKRThreadSwitch() {}

// "No calls to this appear to have survived" (JSUStream.h): an empty slot.
void JSUOutputStream::_0C() {}
