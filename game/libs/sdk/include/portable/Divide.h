#ifndef PORTABLE_DIVIDE_H
#define PORTABLE_DIVIDE_H

// Integer division by a count that comes from game data and can be zero. The
// GameCube's divw gives 0 for a zero divisor and the game relies on it (the
// first particle of a fixed-interval JPA sphere emission divides by
// mVolumeAngleMax - 1 == 0); most other CPUs trap. Under MWCC this is the
// plain division, so the code it generates is unchanged. See docs/PORTABLE.md.
#ifdef __MWERKS__
#define gc_div_s32(a, b) ((a) / (b))
#else
static inline int gc_div_s32(int a, int b) { return b != 0 ? a / b : 0; }
#endif

#endif
