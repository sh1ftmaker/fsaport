// The GBA link seam (deluxe; docs/MULTIPLAYER.md §2).
//
// The game is unmodified above the SDK's GBA library: its client threads
// (game/src/game/tu_803006E8.cpp) call SIProbe / GBAJoyBoot / GBAGetStatus /
// GBARead / GBAWrite / GBAReset exactly as on a GameCube. platform/gba_link.cpp
// implements those calls with a native endpoint per channel that speaks the
// client program's side of the protocol (the handshake, the 8-word session
// packet, the tick echo, the per-frame record and the key word), so the game
// sees a connected GBA without any GBA code running.
//
// What is swappable is the *peer* behind each endpoint: who presses that GBA's
// buttons and who looks at its screen. A peer answers two questions and is
// told two things:
//
//   keys(chan)            the GBA's buttons now (GBA_* bits, 10 used); called
//                         once per per-frame transfer, from the game's client
//                         thread, so it must answer at once (buffer it).
//   record(chan, r, n)    a per-frame record arrived: n words (0x68 bytes max)
//                         in the port's memory order, as the game built it
//                         (lbl_80506DE8[chan]); after any file/message it
//                         completes, those are delivered first.
//   file(chan, addr, b, n) a file transfer completed: GBA address, bytes in
//                         file order (still Yaz0 when the game flagged it).
//
// The local peer (platform/gba_link.cpp, the default) takes the keys from
// PADStatus[chan] as the game read it at the latest JUTGamePad::read (so the
// harness drives all four GBAs, and lockstep runs stay deterministic) and
// feeds the native GBA view (platform/gba_view.cpp). A network peer would
// buffer keys from a socket and forward records/files to a remote screen.
//
// Plain C; the page's JavaScript binding is the fsa_gba_* exports in
// gba_link.cpp, not this header.
#pragma once

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FSA_GBA_CHANNELS 4

struct FsaGbaPeer {
  const char* name;
  u16 (*keys)(struct FsaGbaPeer* p, int chan);
  void (*record)(struct FsaGbaPeer* p, int chan, const u32* words, int count);
  void (*file)(struct FsaGbaPeer* p, int chan, u32 address, const u8* bytes, u32 size, int flag);
  void* user;
};

// Plug a GBA into `chan` driven by `peer` (NULL: the local peer), or unplug it
// (attach = 0). With nothing attached the GBA calls answer as before this
// existed: no GBA, GBA_NOT_READY.
int FsaGbaAttach(int chan, struct FsaGbaPeer* peer);
int FsaGbaDetach(int chan);
int FsaGbaPresent(int chan);

// The local peer's input: the four PADStatus of the latest pad read (called by
// fsa_pad_after_read after the harness has replaced or recorded them).
struct PADStatus;
void FsaGbaPadsRead(const struct PADStatus* status);

// The native view (gba_view.cpp), fed by the local peer.
void FsaGbaViewReset(int chan);
void FsaGbaViewRecord(int chan, const u32* words, int count);
void FsaGbaViewFile(int chan, u32 address, const u8* bytes, u32 size, int flag);

#ifdef __cplusplus
}
#endif
