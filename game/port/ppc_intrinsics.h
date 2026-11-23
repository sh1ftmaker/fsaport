/* Host versions of the Metrowerks PowerPC intrinsics the decompiled code calls.
 * Behaviour, not bit-exactness: __fres/__frsqrte are exact here where Gekko's
 * are 12-bit estimates. */
#ifndef FSA_PORT_PPC_INTRINSICS_H
#define FSA_PORT_PPC_INTRINSICS_H

#ifdef __cplusplus
extern "C" {
#endif

static inline double __fabs(double x) { return __builtin_fabs(x); }
static inline float __fabsf(float x) { return __builtin_fabsf(x); }
static inline double __fnabs(double x) { return -__builtin_fabs(x); }
static inline float __fres(float x) { return 1.0f / x; }
static inline double __frsqrte(double x) { return 1.0 / __builtin_sqrt(x); }
static inline float __frsqrtes(double x) { return (float)(1.0 / __builtin_sqrt(x)); }
static inline double __fsel(double a, double b, double c) { return a >= 0.0 ? b : c; }
static inline float __fsels(float a, float b, float c) { return a >= 0.0f ? b : c; }
static inline int __abs(int x) { return x < 0 ? -x : x; }
static inline long __labs(long x) { return x < 0 ? -x : x; }
static inline int __cntlzw(unsigned int x) { return x ? __builtin_clz(x) : 32; }
static inline void* __memcpy(void* d, const void* s, unsigned long n) { return __builtin_memcpy(d, s, n); }
static inline void __dcbz(void* p, int off) { __builtin_memset((char*)p + off, 0, 32); }
static inline void __dcbf(void* p, int off) { (void)p; (void)off; }
static inline void __dcbst(void* p, int off) { (void)p; (void)off; }
static inline void __dcbi(void* p, int off) { (void)p; (void)off; }
static inline void __sync(void) {}
static inline void __eieio(void) {}
static inline void __isync(void) {}
static inline float sqrtf(float x) { return __builtin_sqrtf(x); }

#ifdef __cplusplus
}
#endif

#endif
