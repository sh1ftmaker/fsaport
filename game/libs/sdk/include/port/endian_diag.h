/* Endian trap (diagnostics build, cmake -DFSA_ENDIAN_DIAG=ON; see docs/DIAGNOSTICS.md).
 *
 * The loaders record the address ranges that hold file bytes (disc reads,
 * decompression output, ARAM-to-RAM copies, archive resources) and forget them
 * when the heap block is freed. The instrumented units' multi-byte loads are
 * checked against those ranges by platform/endian_diag.cpp; a raw load from
 * file bytes is a read that bypasses the big-endian helpers.
 *
 * In a normal build the macros expand to nothing.
 */
#ifndef FSA_PORT_ENDIAN_DIAG_H
#define FSA_PORT_ENDIAN_DIAG_H

#ifdef FSA_ENDIAN_DIAG
#ifdef __cplusplus
extern "C" {
#endif
/* [p, p + size) holds file bytes; fourcc (0 if unknown) and name label it. */
void fsa_endian_range(const void* p, unsigned int size, unsigned int fourcc, const char* name);
/* Forget every range that starts in [p, p + size). */
void fsa_endian_unrange(const void* p, unsigned int size);
/* Write the changed sites to the "endian" diagnostics stream. */
void fsa_endian_report(void);
#ifdef __cplusplus
}
#endif
#define FSA_ENDIAN_RANGE(p, size, fourcc, name) fsa_endian_range((p), (size), (fourcc), (name))
#define FSA_ENDIAN_UNRANGE(p, size) fsa_endian_unrange((p), (size))
#else
#define FSA_ENDIAN_RANGE(p, size, fourcc, name) ((void)0)
#define FSA_ENDIAN_UNRANGE(p, size) ((void)0)
#endif

#endif
