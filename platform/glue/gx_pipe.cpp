// The GameCube's write-gather pipe (GXWGFifo, 0xCC008000) as Aurora's command
// FIFO: every store the game makes appends the value big-endian, and Aurora's
// command processor decodes the stream as the GPU did.
//
// Built against Aurora's headers only (not the decomp's SDK headers); the game
// side is declared in game/libs/sdk/include/dolphin/gx/GXVert.h.
#include "gx/fifo.hpp"

using namespace aurora::gx;

extern "C" {
volatile unsigned char fsa_gx_pipe[16];
void fsa_gxpipe_u8(unsigned char v) { fifo::write_u8(v); }
void fsa_gxpipe_u16(unsigned short v) { fifo::write_u16(v); }
void fsa_gxpipe_u32(unsigned int v) { fifo::write_u32(v); }
void fsa_gxpipe_f32(float v) { fifo::write_f32(v); }
}
