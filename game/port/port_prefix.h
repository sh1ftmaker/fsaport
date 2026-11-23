/* Force-included first in every unit of the port build (tools/compile_check.py).
 * Compatibility for Metrowerks CodeWarrior-isms the decompiled sources rely on. */
#ifndef FSA_PORT_PREFIX_H
#define FSA_PORT_PREFIX_H

#include "ppc_intrinsics.h"

/* __declspec(weak) and __declspec(section "...") only steer the Metrowerks linker. */
#define __declspec(x)


/* Metrowerks gives 5-8 character constants ('NIN_Logo', J2D pane tags) 64-bit
 * values; clang keeps only the last 4 bytes. tools/fix_tag64.py rewrote them as
 * FSA_TAG64("NIN_Logo"): the characters packed big-endian into a u64. */
#ifdef __cplusplus
constexpr unsigned long long fsa_tag64(const char* s, unsigned long n) {
  return n == 0 ? 0ULL : (fsa_tag64(s, n - 1) << 8) | (unsigned char)s[n - 1];
}
#define FSA_TAG64(s) (fsa_tag64(s, sizeof(s) - 1))
#endif

#endif
