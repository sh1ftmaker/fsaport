// The disc: the user's own image, read from the page (a dropped File or a URL)
// through platform/web/fsa_disc.js. Paths resolve through the disc's FST.
//
// Reads suspend the calling coroutine (JSPI) until the bytes arrive. Async
// reads go to a worker thread so they can be issued from callbacks too, and
// complete with their callback run as a deferred "interrupt".
#include "platform.h"

#include <emscripten.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

EM_ASYNC_JS(int, fsa_js_disc_read, (void* dst, double offset, int length), {
  const bytes = await Module.fsaDisc.read(offset, length);
  HEAPU8.set(bytes, dst);
  return bytes.length;
});
EM_JS(double, fsa_js_disc_size, (), { return Module.fsaDisc ? Module.fsaDisc.size : 0; });

namespace {

struct FstEntry {
  bool isDir;
  u32 nameOffset;
  u32 offsetOrParent;  // file: disc offset; dir: parent index
  u32 lengthOrNext;    // file: length; dir: index after its last child
};

std::vector<FstEntry> g_fst;
std::vector<char> g_names;
DVDDiskID g_diskId;
u32 g_currentDir = 0;

u32 be32(const u8* p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

void read_disc(void* dst, u32 offset, u32 length) {
  if (length)
    fsa_js_disc_read(dst, (double)offset, (int)length);
}

const char* entry_name(u32 i) { return g_names.data() + g_fst[i].nameOffset; }

bool name_eq(const char* a, size_t alen, const char* b) {
  for (size_t i = 0; i < alen; i++, b++)
    if (!*b || tolower((unsigned char)a[i]) != tolower((unsigned char)*b))
      return false;
  return *b == '\0';
}

s32 resolve(const char* path) {
  u32 dir = path[0] == '/' ? 0 : g_currentDir;
  const char* p = path;
  while (*p) {
    while (*p == '/')
      p++;
    if (!*p)
      break;
    const char* end = p;
    while (*end && *end != '/')
      end++;
    size_t len = (size_t)(end - p);
    if (len == 1 && p[0] == '.') {
    } else if (len == 2 && p[0] == '.' && p[1] == '.') {
      dir = g_fst[dir].offsetOrParent;
    } else {
      s32 found = -1;
      for (u32 i = dir + 1; i < g_fst[dir].lengthOrNext;) {
        if (name_eq(p, len, entry_name(i))) {
          found = (s32)i;
          break;
        }
        i = g_fst[i].isDir ? g_fst[i].lengthOrNext : i + 1;
      }
      if (found < 0)
        return -1;
      if (*end == '\0')
        return found;
      if (!g_fst[found].isDir)
        return -1;
      dir = (u32)found;
    }
    p = end;
  }
  return (s32)dir;
}

// --- Async reads: a worker thread performs them ------------------------------
struct Request {
  DVDCommandBlock* block;
  DVDFileInfo* file;  // non-null: complete with the file callback
  void* addr;
  u32 discOffset;
  u32 length;
};
std::deque<Request> g_requests;
OSThread g_worker;
OSMessageQueue g_workerQueue;
OSMessage g_workerMsgs[1];

void complete(void* arg) {
  auto* req = static_cast<Request*>(arg);
  Request copy = *req;
  {
    HostAllocScope host;
    delete req;
  }
  req = &copy;
  DVDCommandBlock* b = req->block;
  b->transferredSize = req->length;
  b->state = DVD_STATE_END;
  if (req->file && req->file->callback)
    req->file->callback((s32)req->length, req->file);
  else if (!req->file && b->callback)
    b->callback((s32)req->length, b);
}

void* worker_main(void*) {
  for (;;) {
    while (g_requests.empty()) {
      OSMessage m;
      OSReceiveMessage(&g_workerQueue, &m, OS_MESSAGE_BLOCK);
    }
    Request req = g_requests.front();
    g_requests.pop_front();
    if (req.block->state == DVD_STATE_CANCELED)
      continue;
    read_disc(req.addr, req.discOffset, req.length);
    HostAllocScope host;
    platform_post_callback(complete, new Request(req));
  }
}

BOOL queue_read(DVDCommandBlock* block, DVDFileInfo* file, void* addr, u32 discOffset, u32 length) {
  block->state = DVD_STATE_BUSY;
  block->addr = addr;
  block->length = length;
  block->offset = discOffset;
  block->transferredSize = 0;
  HostAllocScope host;
  g_requests.push_back({block, file, addr, discOffset, length});
  OSSendMessage(&g_workerQueue, nullptr, OS_MESSAGE_NOBLOCK);
  return TRUE;
}

}  // namespace

void platform_dvd_init() {
  HostAllocScope host;
  u8 header[0x440];
  read_disc(header, 0, sizeof(header));
  memcpy(&g_diskId, header, sizeof(g_diskId));
  u32 fstOffset = be32(header + 0x424);
  u32 fstSize = be32(header + 0x428);
  std::vector<u8> fst(fstSize);
  read_disc(fst.data(), fstOffset, fstSize);
  u32 count = be32(fst.data() + 8);
  g_fst.resize(count);
  for (u32 i = 0; i < count; i++) {
    const u8* e = fst.data() + i * 12;
    g_fst[i] = {e[0] != 0, be32(e) & 0xFFFFFF, be32(e + 4), be32(e + 8)};
  }
  g_names.assign((const char*)fst.data() + count * 12, (const char*)fst.data() + fstSize);
  g_names.push_back('\0');
  platform_log("[dvd] %.6s, %u FST entries\n", g_diskId.gameName, count);

  OSInitMessageQueue(&g_workerQueue, g_workerMsgs, 1);
  static u8 workerStack[0x1000];
  OSCreateThread(&g_worker, worker_main, nullptr, workerStack + sizeof(workerStack), sizeof(workerStack), 1, 0);
  OSResumeThread(&g_worker);
}

// Module._fsa_set_dvd_log(1) from the console logs every path the game resolves
// and every archive lookup that fails; 2 also logs the ones that succeed.
extern "C" int fsa_dvd_log;
int fsa_dvd_log = 0;

extern "C" {

EMSCRIPTEN_KEEPALIVE void fsa_set_dvd_log(int on) { fsa_dvd_log = on; }

void DVDInit() {}
BOOL DVDSetAutoFatalMessaging(BOOL) { return FALSE; }
BOOL DVDSetAutoInvalidation(BOOL) { return TRUE; }
void DVDPause() {}
void DVDResume() {}
void DVDReset() {}
int DVDCheckDisk() { return 1; }
s32 DVDGetDriveStatus() {
  platform_service_interrupts();
  platform_poll_point();
  return g_requests.empty() ? DVD_STATE_END : DVD_STATE_BUSY;
}
DVDDiskID* DVDGetCurrentDiskID() { return &g_diskId; }


s32 DVDConvertPathToEntrynum(const char* path) {
  s32 e = resolve(path);
  if (fsa_dvd_log)
    platform_log("[dvd] %s -> %d\n", path, e);
  return e;
}

BOOL DVDFastOpen(s32 entrynum, DVDFileInfo* fileInfo) {
  if (entrynum < 0 || (u32)entrynum >= g_fst.size() || g_fst[entrynum].isDir)
    return FALSE;
  memset(fileInfo, 0, sizeof(*fileInfo));
  fileInfo->startAddr = g_fst[entrynum].offsetOrParent;
  fileInfo->length = g_fst[entrynum].lengthOrNext;
  fileInfo->cb.state = DVD_STATE_END;
  return TRUE;
}

BOOL DVDOpen(const char* fileName, DVDFileInfo* fileInfo) {
  s32 e = resolve(fileName);
  if (fsa_dvd_log)
    platform_log("[dvd] open %s -> %d\n", fileName, e);
  if (e < 0) {
    platform_log("[dvd] not found: %s\n", fileName);
    return FALSE;
  }
  return DVDFastOpen(e, fileInfo);
}

BOOL DVDClose(DVDFileInfo*) { return TRUE; }

BOOL DVDGetCurrentDir(char* path, u32 maxlen) {
  snprintf(path, maxlen, "/");
  return TRUE;
}
BOOL DVDChangeDir(char* dirName) {
  s32 e = resolve(dirName);
  if (e < 0 || !g_fst[e].isDir)
    return FALSE;
  g_currentDir = (u32)e;
  return TRUE;
}

BOOL DVDFastOpenDir(s32 entrynum, DVDDir* dir) {
  if (entrynum < 0 || (u32)entrynum >= g_fst.size() || !g_fst[entrynum].isDir)
    return FALSE;
  dir->entryNum = (u32)entrynum;
  dir->location = (u32)entrynum + 1;
  dir->next = g_fst[entrynum].lengthOrNext;
  return TRUE;
}
BOOL DVDOpenDir(char* dirName, DVDDir* dir) { return DVDFastOpenDir(resolve(dirName), dir); }
BOOL DVDReadDir(DVDDir* dir, DVDDirEntry* dirent) {
  u32 i = dir->location;
  if (i <= dir->entryNum || i >= dir->next)
    return FALSE;
  dirent->entryNum = i;
  dirent->isDir = g_fst[i].isDir;
  dirent->name = (char*)entry_name(i);
  dir->location = g_fst[i].isDir ? g_fst[i].lengthOrNext : i + 1;
  return TRUE;
}
BOOL DVDCloseDir(DVDDir*) { return TRUE; }

s32 DVDReadPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, s32) {
  fileInfo->cb.state = DVD_STATE_BUSY;
  read_disc(addr, fileInfo->startAddr + (u32)offset, (u32)length);
  fileInfo->cb.state = DVD_STATE_END;
  fileInfo->cb.transferredSize = (u32)length;
  return length;
}

BOOL DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, DVDCallback callback, s32) {
  fileInfo->callback = callback;
  return queue_read(&fileInfo->cb, fileInfo, addr, fileInfo->startAddr + (u32)offset, (u32)length);
}

BOOL DVDReadAbsAsyncPrio(DVDCommandBlock* block, void* addr, s32 length, s32 offset, DVDCBCallback callback, s32) {
  block->callback = callback;
  return queue_read(block, nullptr, addr, (u32)offset, (u32)length);
}

s32 DVDGetCommandBlockStatus(const DVDCommandBlock* block) {
  platform_service_interrupts();
  platform_poll_point();
  return block->state;
}
s32 DVDGetFileInfoStatus(DVDFileInfo* fileInfo) { return DVDGetCommandBlockStatus(&fileInfo->cb); }

s32 DVDCancel(DVDCommandBlock* block) {
  if (block->state == DVD_STATE_BUSY)
    block->state = DVD_STATE_CANCELED;
  return 0;
}
BOOL DVDCancelAsync(DVDCommandBlock* block, DVDCBCallback callback) {
  DVDCancel(block);
  if (callback)
    callback(0, block);
  return TRUE;
}
s32 DVDCancelAll() { return 0; }

BOOL DVDReadDiskID(DVDCommandBlock* block, DVDDiskID* diskID, DVDCBCallback callback) {
  memcpy(diskID, &g_diskId, sizeof(g_diskId));
  block->state = DVD_STATE_END;
  if (callback)
    callback(sizeof(g_diskId), block);
  return TRUE;
}
BOOL DVDCompareDiskID(DVDDiskID* a, DVDDiskID* b) { return memcmp(a, b, 8) == 0; }

}  // extern "C"
