// The paired-single matrix library (PSMTX*, PSVEC*) as Aurora's C versions.
#include <dolphin/mtx.h>

extern "C" {
void C_MTXConcat(const Mtx a, const Mtx b, Mtx ab);
void C_MTXCopy(const Mtx src, Mtx dst);
void C_MTXIdentity(Mtx m);
u32 C_MTXInverse(const Mtx src, Mtx inv);
void C_MTXMultVec(const Mtx m, const Vec* src, Vec* dst);
void C_MTXMultVecArraySR(const Mtx m, const Vec* srcBase, Vec* dstBase, u32 count);
void C_MTXMultVecSR(const Mtx m, const Vec* src, Vec* dst);
void C_MTXRotAxisRad(Mtx m, const Vec* axis, f32 rad);
void C_MTXRotRad(Mtx m, char axis, f32 rad);
void C_MTXScale(Mtx m, f32 x, f32 y, f32 z);
void C_MTXScaleApply(const Mtx src, Mtx dst, f32 x, f32 y, f32 z);
void C_MTXTrans(Mtx m, f32 x, f32 y, f32 z);
void C_MTXTransApply(const Mtx src, Mtx dst, f32 x, f32 y, f32 z);
void C_VECAdd(const Vec* a, const Vec* b, Vec* ab);
void C_VECCrossProduct(const Vec* a, const Vec* b, Vec* axb);

void PSMTXConcat(const Mtx a, const Mtx b, Mtx ab) { C_MTXConcat(a, b, ab); }
void PSMTXCopy(const Mtx src, Mtx dst) { C_MTXCopy(src, dst); }
void PSMTXIdentity(Mtx m) { C_MTXIdentity(m); }
u32 PSMTXInverse(const Mtx src, Mtx inv) { return C_MTXInverse(src, inv); }
void PSMTXMultVec(const Mtx m, const Vec* src, Vec* dst) { C_MTXMultVec(m, src, dst); }
void PSMTXMultVecArraySR(const Mtx m, const Vec* s, Vec* d, u32 n) { C_MTXMultVecArraySR(m, s, d, n); }
void PSMTXMultVecSR(const Mtx m, const Vec* src, Vec* dst) { C_MTXMultVecSR(m, src, dst); }
void PSMTXRotAxisRad(Mtx m, const Vec* axis, f32 rad) { C_MTXRotAxisRad(m, axis, rad); }
void PSMTXRotRad(Mtx m, char axis, f32 rad) { C_MTXRotRad(m, axis, rad); }
void PSMTXScale(Mtx m, f32 x, f32 y, f32 z) { C_MTXScale(m, x, y, z); }
void PSMTXScaleApply(const Mtx s, Mtx d, f32 x, f32 y, f32 z) { C_MTXScaleApply(s, d, x, y, z); }
void PSMTXTrans(Mtx m, f32 x, f32 y, f32 z) { C_MTXTrans(m, x, y, z); }
void PSMTXTransApply(const Mtx s, Mtx d, f32 x, f32 y, f32 z) { C_MTXTransApply(s, d, x, y, z); }
void PSVECAdd(const Vec* a, const Vec* b, Vec* ab) { C_VECAdd(a, b, ab); }
void PSVECCrossProduct(const Vec* a, const Vec* b, Vec* axb) { C_VECCrossProduct(a, b, axb); }
}
