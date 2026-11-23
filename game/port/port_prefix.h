/* Force-included first in every unit of the port build (tools/compile_check.py).
 * Compatibility for Metrowerks CodeWarrior-isms the decompiled sources rely on. */
#ifndef FSA_PORT_PREFIX_H
#define FSA_PORT_PREFIX_H

#include "ppc_intrinsics.h"

/* __declspec(weak) and __declspec(section "...") only steer the Metrowerks linker. */
#define __declspec(x)

#endif
