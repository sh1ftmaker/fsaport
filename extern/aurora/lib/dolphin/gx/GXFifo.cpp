#include "../../host_alloc.hpp"
#include "gx.hpp"

extern "C" {

static GXFifoObj* GPFifo;
static GXFifoObj* CPUFifo;

void GXGetGPStatus(GXBool* overhi, GXBool* underlow, GXBool* readIdle, GXBool* cmdIdle, GXBool* brkpt) {
  AURORA_HOST_ALLOC;
  *overhi = *underlow = *readIdle = *cmdIdle = *brkpt = false;
  *readIdle = true;
}

// TODO GXGetFifoStatus

void GXGetFifoPtrs(GXFifoObj* fifo, void** readPtr, void** writePtr) {
  AURORA_HOST_ALLOC;
  *readPtr = NULL;
  *writePtr = NULL;
}

GXFifoObj* GXGetCPUFifo() {
  AURORA_HOST_ALLOC; return CPUFifo; }

GXFifoObj* GXGetGPFifo() {
  AURORA_HOST_ALLOC; return GPFifo; }

// TODO GXGetFifoBase
// TODO GXGetFifoSize
// TODO GXGetFifoLimits
// TODO GXSetBreakPtCallback
// TODO GXEnableBreakPt
// TODO GXDisableBreakPt

void GXInitFifoBase(GXFifoObj* fifo, void* base, u32 size) {
  AURORA_HOST_ALLOC;}

void GXInitFifoPtrs(GXFifoObj* fifo, void* readPtr, void* writePtr) {
  AURORA_HOST_ALLOC;}

// TODO GXInitFifoLimits

void GXSetCPUFifo(GXFifoObj* fifo) {
  AURORA_HOST_ALLOC; CPUFifo = fifo; }

void GXSetGPFifo(GXFifoObj* fifo) {
  AURORA_HOST_ALLOC; GPFifo = fifo; }

void GXSaveCPUFifo(GXFifoObj* fifo) {
  AURORA_HOST_ALLOC;}

// TODO GXSaveGPFifo
// TODO GXRedirectWriteGatherPipe
// TODO GXRestoreWriteGatherPipe
// TODO GXSetCurrentGXThread
// TODO GXGetCurrentGXThread
// TODO GXGetOverflowCount
// TODO GXResetOverflowCount
}