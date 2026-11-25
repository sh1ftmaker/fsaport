// The GBA link, answered natively (deluxe; docs/MULTIPLAYER.md §2-3).
//
// The SDK's GBA library (SIProbe's GBA type, GBAJoyBoot, GBAGetStatus,
// GBARead, GBAWrite, GBAReset) as a native endpoint per channel. Each endpoint
// plays the part of the GBA client program the game boots ("client_thread.bin"),
// at the level of what the game's client thread (game/src/game/tu_803006E8.cpp,
// fn_80300E1C and fn_80300A8C) checks, word by word:
//
//   boot      GBAJoyBoot succeeds at once; the GBA then holds its game code
//             (header bytes 0xAC-0xAF of the program the game patched) for the
//             game to read: status 0x28 (PSF1 | SEND).
//   handshake read the code -> 0x20; the game writes its disc id -> 0x30 (PSF1 |
//             PSF0, "session up"); command 0x04000000: the GBA sends its 8-word
//             session packet (0x38 while words wait); command 0x02000000 + 8
//             words: the GBA takes the game's packet and sends it back.
//   tick      the first word after a packet is the game's tick: echoed back
//             (fn_80300A8C compares it and then marks the client connected,
//             field_0xd8 = 1).
//   frame     cls_804AA8C8::pollInputs_8030A43C posts message 10 each frame:
//             the game writes the player's record (lbl_80506DE8[chan], 12-26
//             words) and then reads one key word. The record is handed to the
//             peer when the key word is read.
//   key word  (GBA keys 0-7) << 16 | (keys 8-9) << 8 | check byte
//             (fn_803006E8 over bits 8-23), and an acknowledgement in the top
//             byte: 2 once a file transfer (records of kind 6, 7, 8) has
//             arrived, 3 once a message (kinds 10, 11, 12) has.
//
// The session packet the GBA starts with is the one the game itself fills in
// at GBAJoyBoot (byte 0 = 1, its port, byte 3 = 1, the boot tick), which is
// what a freshly booted client reports, so the first comparison holds.
//
// Words cross this boundary as the game's memory holds them (the port's host
// order): the game writes `u32 cmd = 0x4000000` and reads its key word into a
// u32, so the endpoint deals in host u32 values throughout.
//
// With nothing attached every call answers as the old stub did: no GBA.
#include "gba_link.h"

#include "platform.h"

#include <dolphin/pad.h>
#include <emscripten.h>

#include <cstdint>
#include <cstring>

extern "C" uint32_t (*aurora_si_probe_hook)(int32_t chan);  // extern/aurora/lib/dolphin/si/si.cpp

namespace {
constexpr u32 kSiGba = 0x00040000;  // SI_GBA
constexpr u8 kPsf1 = 0x20, kPsf0 = 0x10, kSend = 0x08;
constexpr u32 kCmdPacketIn = 0x02000000, kCmdAck = 0x03000000, kCmdPacketOut = 0x04000000;
constexpr int kMaxRecordWords = 0x1A;

enum Mode : u8 { kOff, kCode, kDiscId, kCmd, kRxPacket, kFrame };

struct Endpoint {
  FsaGbaPeer* peer = nullptr;
  bool booted = false;
  Mode mode = kOff;
  u8 psf = 0;
  bool expectTick = false;
  u32 out[16];
  int outHead = 0, outCount = 0;
  u32 code = 0;      // the program's game code (header 0xAC)
  u32 bootTick = 0;  // the program's boot tick (header 0xC8)
  u32 packet[8];
  int rx = 0;
  u32 frame[kMaxRecordWords];
  int frameCount = 0;
  u8 ack = 0;
  // File transfer being assembled (kinds 6, 7, 8).
  bool fileOpen = false;
  u32 fileAddr = 0;
  int fileFlag = 0;
  u32 fileSize = 0;
  // Statistics, for the page's panel.
  u32 frames = 0, files = 0, boots = 0, lastKeys = 0;
};

Endpoint g_ep[FSA_GBA_CHANNELS];
PADStatus g_pads[FSA_GBA_CHANNELS];
u16 g_pageKeys[FSA_GBA_CHANNELS];
u8 g_fileBuf[FSA_GBA_CHANNELS][0x40000];  // GBA_JOYBOOT_PROGRAM_SIZE_MAX: the largest file the GBA can hold

void push(Endpoint& e, u32 w) {
  if (e.outCount < 16)
    e.out[(e.outHead + e.outCount++) & 15] = w;
}
u32 pop(Endpoint& e) {
  u32 w = e.out[e.outHead];
  e.outHead = (e.outHead + 1) & 15;
  e.outCount--;
  return w;
}
u8 status_of(const Endpoint& e) { return (u8)(e.psf | (e.outCount ? kSend : 0)); }

u32 bswap(u32 v) { return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24); }

// fn_803006E8: the key word's check byte over its bits 8-23 (see GbaClient.h).
u8 check_byte(u32 data) {
  u32 crc = 0;
  for (int half = 0; half < 2; half++, data >>= 8) {
    for (u32 mask = 0x80; mask != 0; mask >>= 1) {
      crc <<= 1;
      if (data & mask) {
        if (crc & 0x100)
          crc ^= 0xCC;
        else
          crc += 1;
      } else if (crc & 0x100) {
        crc ^= 0xCD;
      }
    }
  }
  for (int i = 0; i < 8; i++) {
    crc <<= 1;
    if (crc & 0x100)
      crc ^= 0xCD;
  }
  return (u8)crc;
}

// The record's kind: byte 3 of word 0 holds mKind (4 bits) under the port's
// (clang, LSB-first) bitfield layout of cls_804AA858Sub11C (game/Map.h).
int record_kind(u32 w0) { return (w0 >> 24) & 0xF; }

// A file word as the game composed it (`src[0] << 16 | src[1]` over the
// file's bytes read as host u16) back to the file's four bytes.
void file_bytes(u32 v, u8* b) {
  b[0] = (u8)(v >> 16);
  b[1] = (u8)(v >> 24);
  b[2] = (u8)v;
  b[3] = (u8)(v >> 8);
}

void commit_frame(int chan) {
  Endpoint& e = g_ep[chan];
  if (e.frameCount == 0)
    return;
  e.frames++;
  const u32* w = e.frame;
  int n = e.frameCount;
  int kind = record_kind(w[0]);
  switch (kind) {
  case 6:  // file header: words 2, 3 = byte-swapped flag and GBA address
    e.ack = 0;
    e.fileOpen = n >= 4;
    e.fileFlag = n >= 3 ? (int)bswap(w[2]) : 0;
    e.fileAddr = n >= 4 ? bswap(w[3]) : 0;
    e.fileSize = 0;
    break;
  case 7:
  case 8:  // file words from word 2 on; 8 is the last chunk
    if (e.fileOpen) {
      for (int i = 2; i < n && e.fileSize + 4 <= sizeof(g_fileBuf[0]); i++, e.fileSize += 4)
        file_bytes(w[i], g_fileBuf[chan] + e.fileSize);
      if (kind == 8) {
        e.fileOpen = false;
        e.files++;
        e.ack = 2;
        if (e.peer && e.peer->file)
          e.peer->file(e.peer, chan, e.fileAddr, g_fileBuf[chan], e.fileSize, e.fileFlag);
      }
    } else if (kind == 8) {
      e.ack = 2;  // a resend's tail after the header was missed: acknowledge, the game moves on
    }
    break;
  case 10:
    e.ack = 0;
    break;
  case 12:
    e.ack = 3;
    break;
  default: break;
  }
  if (e.peer && e.peer->record)
    e.peer->record(e.peer, chan, w, n);
  e.frameCount = 0;
}

u32 key_word(int chan) {
  Endpoint& e = g_ep[chan];
  u16 keys = e.peer && e.peer->keys ? e.peer->keys(e.peer, chan) : 0;
  u32 word = ((u32)(keys & 0xFF) << 16) | ((u32)(keys & 0x300)) | ((u32)e.ack << 24);
  word |= check_byte((word & 0xFF00) | ((word >> 16) & 0xFF));
  e.lastKeys = word;
  return word;
}

void reset_protocol(Endpoint& e) {
  e.outHead = e.outCount = 0;
  e.expectTick = false;
  e.rx = 0;
  e.frameCount = 0;
  e.fileOpen = false;
  if (e.booted) {
    e.mode = kCode;
    e.psf = kPsf1;
    push(e, e.code);
  } else {
    e.mode = kOff;
    e.psf = 0;
  }
}

bool valid(s32 chan) { return chan >= 0 && chan < FSA_GBA_CHANNELS && g_ep[chan].peer != nullptr; }

s32 not_ready(u8* status) {
  if (status)
    *status = 0;
  return 1;  // GBA_NOT_READY
}

// --- The local peer: keys from the pad of the same port, view in this page ---
u16 local_keys(FsaGbaPeer*, int chan) {
  const PADStatus& p = g_pads[chan];
  u16 k = g_pageKeys[chan];
  if (p.err == PAD_ERR_NONE) {
    u16 b = p.button;
    if (b & PAD_BUTTON_A) k |= 0x001;
    if (b & PAD_BUTTON_B) k |= 0x002;
    if (b & PAD_BUTTON_X) k |= 0x004;  // Select (the game maps it like the pad's X)
    if (b & PAD_BUTTON_START) k |= 0x008;
    if ((b & PAD_BUTTON_RIGHT) || p.stickX > 40) k |= 0x010;
    if ((b & PAD_BUTTON_LEFT) || p.stickX < -40) k |= 0x020;
    if ((b & PAD_BUTTON_UP) || p.stickY > 40) k |= 0x040;
    if ((b & PAD_BUTTON_DOWN) || p.stickY < -40) k |= 0x080;
    if ((b & PAD_TRIGGER_R) || p.triggerRight > 140) k |= 0x100;
    if ((b & (PAD_TRIGGER_L | PAD_BUTTON_Y)) || p.triggerLeft > 140) k |= 0x200;
  }
  return k;
}
void local_record(FsaGbaPeer*, int chan, const u32* w, int n) { FsaGbaViewRecord(chan, w, n); }
void local_file(FsaGbaPeer*, int chan, u32 addr, const u8* b, u32 size, int flag) { FsaGbaViewFile(chan, addr, b, size, flag); }
FsaGbaPeer g_local = {"local", local_keys, local_record, local_file, nullptr};

uint32_t probe_hook(int32_t chan) { return valid(chan) ? kSiGba : 0; }
}  // namespace

extern "C" {

int FsaGbaAttach(int chan, FsaGbaPeer* peer) {
  if (chan < 0 || chan >= FSA_GBA_CHANNELS)
    return 0;
  Endpoint& e = g_ep[chan];
  e = Endpoint();
  e.peer = peer ? peer : &g_local;
  aurora_si_probe_hook = probe_hook;
  FsaGbaViewReset(chan);
  platform_log("[gba] port %d: GBA plugged in (%s peer)\n", chan, e.peer->name);
  return 1;
}

int FsaGbaDetach(int chan) {
  if (!valid(chan))
    return 0;
  g_ep[chan] = Endpoint();
  platform_log("[gba] port %d: GBA unplugged\n", chan);
  return 1;
}

int FsaGbaPresent(int chan) { return valid(chan) ? 1 : 0; }

void FsaGbaPadsRead(const PADStatus* status) { memcpy(g_pads, status, sizeof(g_pads)); }

// --- The SDK's GBA library -----------------------------------------------------
void GBAInit() {}

s32 GBAGetStatus(s32 chan, u8* status) {
  if (!valid(chan))
    return not_ready(status);
  // The client thread polls the status in loops that end on a timeout
  // (OSGetTime): yield there like the SDK's other polling calls, so a stuck
  // exchange cannot spin forever in lockstep, where time stands still
  // within a frame.
  platform_poll_point();
  Endpoint& e = g_ep[chan];
  if (status)
    *status = status_of(e);
  return 0;
}

s32 GBAReset(s32 chan, u8* status) {
  if (!valid(chan))
    return not_ready(status);
  Endpoint& e = g_ep[chan];
  reset_protocol(e);
  if (status)
    *status = status_of(e);
  return 0;
}

s32 GBAJoyBoot(s32 chan, s32, s32, u8* program, u32 length, u8* status) {
  if (!valid(chan))
    return not_ready(status);
  Endpoint& e = g_ep[chan];
  if (program && length >= 0xCC) {
    memcpy(&e.code, program + 0xAC, 4);
    memcpy(&e.bootTick, program + 0xC8, 4);
  }
  e.booted = true;
  e.boots++;
  e.ack = 0;
  memset(e.packet, 0, sizeof(e.packet));
  u8* p = (u8*)e.packet;  // GbaPacket_80300A8C, as the game fills it at boot
  p[0] = 1;
  p[1] = (u8)chan;
  p[3] = 1;
  memcpy(p + 4, &e.bootTick, 4);
  reset_protocol(e);
  FsaGbaViewReset(chan);
  if (status)
    *status = 0;
  return 0;
}

s32 GBARead(s32 chan, u8* dst, u8* status) {
  if (!valid(chan))
    return not_ready(status);
  Endpoint& e = g_ep[chan];
  u32 w;
  if (e.outCount) {
    w = pop(e);
    if (e.mode == kCode)
      e.mode = kDiscId;
  } else {
    // Nothing waiting: the key word, which also ends a frame.
    if (e.mode == kFrame) {
      commit_frame(chan);
      e.mode = kCmd;
    }
    w = key_word(chan);
  }
  memcpy(dst, &w, 4);
  if (status)
    *status = status_of(e);
  return 0;
}

s32 GBAWrite(s32 chan, u8* src, u8* status) {
  if (!valid(chan))
    return not_ready(status);
  Endpoint& e = g_ep[chan];
  u32 w;
  memcpy(&w, src, 4);
  switch (e.mode) {
  case kOff:
  case kCode: break;
  case kDiscId:
    e.psf = kPsf1 | kPsf0;
    e.mode = kCmd;
    break;
  case kRxPacket:
    e.packet[e.rx++] = w;
    if (e.rx == 8) {
      for (int i = 0; i < 8; i++)
        push(e, e.packet[i]);
      e.expectTick = true;
      e.mode = kCmd;
    }
    break;
  case kFrame:
    if (e.frameCount < kMaxRecordWords) {
      e.frame[e.frameCount++] = w;
      break;
    }
    commit_frame(chan);  // a frame whose key read never came: start over
    e.mode = kCmd;
    [[fallthrough]];
  case kCmd:
    if (w == kCmdPacketIn) {
      e.mode = kRxPacket;
      e.rx = 0;
      e.expectTick = false;
    } else if (e.expectTick) {
      e.expectTick = false;
      push(e, w);  // the tick, echoed
    } else if (w == kCmdPacketOut) {
      for (int i = 0; i < 8; i++)
        push(e, e.packet[i]);
      e.expectTick = true;
    } else if (w == kCmdAck) {
    } else {
      e.mode = kFrame;
      e.frame[0] = w;
      e.frameCount = 1;
    }
    break;
  }
  if (status)
    *status = status_of(e);
  return 0;
}

// --- The page ---------------------------------------------------------------------
// Plug (on = 1) or unplug a GBA with the local peer; hot-plugging works, the
// game's client thread probes every port each frame.
EMSCRIPTEN_KEEPALIVE int fsa_gba_attach(int chan, int on) { return on ? FsaGbaAttach(chan, nullptr) : FsaGbaDetach(chan); }

// Extra GBA keys (GBA_* bits) from the page for one port, ORed with the pad's
// (live play only: they are not in the harness's input stream).
EMSCRIPTEN_KEEPALIVE void fsa_gba_page_keys(int chan, int keys) {
  if (chan >= 0 && chan < FSA_GBA_CHANNELS)
    g_pageKeys[chan] = (u16)keys;
}

// The link state for the panel: present, booted, mode, psf, frames, files,
// boots, last key word, ack.
EMSCRIPTEN_KEEPALIVE int fsa_gba_link_info(int chan, u32* out) {
  if (chan < 0 || chan >= FSA_GBA_CHANNELS)
    return 0;
  const Endpoint& e = g_ep[chan];
  out[0] = e.peer != nullptr;
  out[1] = e.booted;
  out[2] = e.mode;
  out[3] = e.psf;
  out[4] = e.frames;
  out[5] = e.files;
  out[6] = e.boots;
  out[7] = e.lastKeys;
  out[8] = e.ack;
  return 9;
}

}  // extern "C"
