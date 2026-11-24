#include <string.h>
#include "JSystem/JUtility/JUTDbg.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "port/endian_diag.h"

JKRHeap *JKRHeap::sSystemHeap;
JKRHeap *JKRHeap::sCurrentHeap;
JKRHeap *JKRHeap::sRootHeap;
JKRHeapErrorHandler *JKRHeap::mErrorHandler;
bool JKRHeap::sDefaultFillCheckFlag;
void *JKRHeap::mCodeStart;
void *JKRHeap::mCodeEnd;
void *JKRHeap::mUserRamStart;
void *JKRHeap::mUserRamEnd;
u32 JKRHeap::mMemorySize;
bool JKRHeap::TState::bVerbose_;

bool JKRHeap::sDefaultFillFlag = true;

JKRHeap::JKRHeap(void *data, u32 size, JKRHeap *heap, bool errorFlag) : JKRDisposer(),
                                                                        mHeapTree(this),
                                                                        mDisposerList()
{
    OSInitMutex(&mMutex);
    mHeapSize = size;
    mStartAddress = (u8 *)data;
    mEndAddress = ((u8 *)data + size);
    if (heap == nullptr)
    {
        becomeSystemHeap();
        becomeCurrentHeap();
    }
    else
    {
        heap->mHeapTree.appendChild(&mHeapTree);
        if (sSystemHeap == sRootHeap)
            becomeSystemHeap();
        if (sCurrentHeap == sRootHeap)
            becomeCurrentHeap();
    }
    mErrorFlag = errorFlag;
    if (mErrorFlag == true && mErrorHandler == nullptr)
        mErrorHandler = JKRDefaultMemoryErrorRoutine;

    mIsDebugFill = sDefaultFillFlag;
    mFillCheckFlag = sDefaultFillCheckFlag;
    mInitFlag = false;
}

JKRHeap::~JKRHeap()
{
    mHeapTree.getParent()->removeChild(&mHeapTree);
    JSUTree<JKRHeap> *nextRootHeap = sRootHeap->mHeapTree.getFirstChild();
    if (sCurrentHeap == this)
        sCurrentHeap = !nextRootHeap ? sRootHeap : nextRootHeap->getObject();

    if (sSystemHeap == this)
        sSystemHeap = !nextRootHeap ? sRootHeap : nextRootHeap->getObject();
}

bool JKRHeap::initArena(char **outUserRamStart, u32 *outUserRamSize, int numHeaps)
{
    void *arenaLo = OSGetArenaLo();
    void *arenaHi = OSGetArenaHi();
    if (arenaLo == arenaHi)
    {
        return false;
    }
    void *arenaStart = OSInitAlloc(arenaLo, arenaHi, numHeaps);
    arenaHi = (u8 *)OSRoundDown32B(arenaHi);
    arenaLo = (u8 *)OSRoundUp32B(arenaStart);
    u8 *start = (u8 *)OSPhysicalToCached(0);
    mCodeStart = (u8 *)start;
    mCodeEnd = (u8 *)arenaLo;
    mUserRamStart = (u8 *)arenaLo;
    mUserRamEnd = (u8 *)arenaHi;
    mMemorySize = *(u32 *)((start + 0x28));
    OSSetArenaLo(arenaHi);
    OSSetArenaHi(arenaHi);
    *outUserRamStart = (char *)arenaLo;
    *outUserRamSize = (u32)arenaHi - (u32)arenaLo;
    return true;
}

JKRHeap *JKRHeap::becomeSystemHeap()
{
    JKRHeap *old = sSystemHeap;
    sSystemHeap = this;
    return old;
}

JKRHeap *JKRHeap::becomeCurrentHeap()
{
    JKRHeap *old = sCurrentHeap;
    sCurrentHeap = this;
    return old;
}

void JKRHeap::destroy(JKRHeap *heap)
{
#line 200
    JUT_ASSERT(heap != 0);
    heap->destroy();
}

void JKRHeap::destroy()
{
    FSA_ENDIAN_UNRANGE(mStartAddress, (u32)mEndAddress - (u32)mStartAddress);
    do_destroy();
}

#ifdef FSA_PORT_TRACE
extern "C" int fsa_host_alloc_depth;
extern "C" void fsa_debug_stack(const char *);
#endif
#ifdef FSA_DIAG_UNINIT
extern "C" void fsa_uninit_alloc(void *p, u32 size);
extern "C" void fsa_uninit_free(void *p);
#endif
void *JKRHeap::alloc(u32 byteCount, int alignment, JKRHeap *heap)
{
    void *memory;
    if (heap)
    {
        return heap->alloc(byteCount, alignment);
    }
    else if (sCurrentHeap)
    {
        return sCurrentHeap->alloc(byteCount, alignment);
    }
    return nullptr;
}

void *JKRHeap::alloc(u32 byteCount, int alignment)
{
#line 317
    JUT_WARNING_F(!mInitFlag, "alloc %x byte in heap %x", byteCount, this);
#ifdef FSA_PORT
    // Port: the game sizes some heaps for the GameCube's structures; report when
    // a host-sized structure no longer fits instead of failing silently.
#ifdef FSA_PORT_TRACE
    u32 freeBefore = (u32)getFreeSize();
#endif
    void *p = do_alloc(byteCount, alignment);
#ifdef FSA_DIAG_UNINIT
    // Diagnostics build: poison the block (platform/diag_uninit.cpp).
    fsa_uninit_alloc(p, byteCount);
#else
    // Port: hand out zeroed blocks. Many constructors leave fields to the
    // zeroed memory the GameCube gives them (MEM1 cleared at boot, heaps carved
    // from it); the decomp checked them against the original (EffectSystem,
    // MessageManager, ... are byte-identical there). Zeroing every block also
    // covers memory a heap reuses.
    if (p != NULL) {
        memset(p, 0, byteCount);
    }
#endif
#ifdef FSA_PORT_TRACE
    static u32 sCount = 0;
    if (this == (JKRHeap *)0x8d40e0 && sCount == 3000 && byteCount == 0x100) {
        fsa_debug_stack("main heap 0x100 alloc #3000");
    }
    if (this == (JKRHeap *)0x8d40e0 && (++sCount % 500) == 0)
        OSReport("[heap] main: %u allocs, free 0x%x, last alloc 0x%x\n", sCount, (u32)getFreeSize(), byteCount);
    if (freeBefore > (u32)getFreeSize() + 0x100000)
        OSReport("[heap] DROP in %p: alloc 0x%x align %d -> %p, free 0x%x -> 0x%x\n", this, byteCount, alignment, p,
                 freeBefore, (u32)getFreeSize());
    if (byteCount >= 0x8000)
        OSReport("[heap] alloc 0x%x in heap %p -> %p (free now 0x%x)\n", byteCount, this, p, (u32)getFreeSize());
    static int sFlood = 0;
    if (p && byteCount < 0x40000 && (u32)getFreeSize() < 0x3000000 && (u32)getFreeSize() > 0x100000 && sFlood < 30) {
        sFlood++;
        OSReport("[heap] small alloc 0x%x align %d in %p (free 0x%x) host=%d\n", byteCount, alignment, this,
                 (u32)getFreeSize(), fsa_host_alloc_depth);
    }
#endif
    if (!p && byteCount)
        OSReport("[heap] alloc of 0x%x (align %d) failed in heap %p (free 0x%x, total 0x%x)\n", byteCount, alignment,
                 this, (u32)getFreeSize(), (u32)((u8 *)mEndAddress - (u8 *)mStartAddress));
    return p;
#else
    return do_alloc(byteCount, alignment);
#endif
}

void JKRHeap::free(void *memory, JKRHeap *heap)
{
    if ((heap) || (heap = findFromRoot(memory), heap))
    {
        heap->free(memory);
    }
}

void JKRHeap::free(void *memory)
{
#line 365
    JUT_WARNING_F(!mInitFlag, "free %x in heap %x", memory, this);
    FSA_ENDIAN_UNRANGE(memory, memory && getSize(memory) > 0 ? (u32)getSize(memory) : 0);
#ifdef FSA_DIAG_UNINIT
    fsa_uninit_free(memory);
#endif
    do_free(memory);
}

void JKRHeap::callAllDisposer()
{
    JSUListIterator<JKRDisposer> iterator;
    while (iterator = mDisposerList.getFirst(), iterator != mDisposerList.getEnd())
    {
        iterator->~JKRDisposer();
    }
}

void JKRHeap::freeAll()
{
#line 417
    JUT_WARNING_F(!mInitFlag, "freeAll in heap %x", this);
    FSA_ENDIAN_UNRANGE(mStartAddress, (u32)mEndAddress - (u32)mStartAddress);
    do_freeAll();
}

void JKRHeap::freeTail()
{
#line 431
    JUT_WARNING_F(!mInitFlag, "freeTail in heap %x", this);
    do_freeTail();
}

// fabricated
void JKRHeap::fillFreeArea()
{
#line 461
    JUT_WARNING_F(!mInitFlag, "fillFreeArea in heap %x", this);
    do_fillFreeArea();
}

int JKRHeap::resize(void *memoryBlock, u32 newSize)
{
#line 491
    JUT_WARNING_F(!mInitFlag, "resize block %x into %x in heap %x", memoryBlock, newSize, this);
#ifdef FSA_PORT_ASAN
    // Port (fsa_asan.h): the block's addressable bytes follow the new size.
    int oldSize = do_getSize(memoryBlock);
    int result = do_resize(memoryBlock, newSize);
    if (result >= 0 && oldSize >= 0)
    {
        FSA_ASAN_POISON(memoryBlock, oldSize > result ? oldSize : result);
        FSA_ASAN_UNPOISON(memoryBlock, newSize);
    }
    return result;
#else
    return do_resize(memoryBlock, newSize);
#endif
}
s32 JKRHeap::getSize(void *ptr) { return do_getSize(ptr); }
s32 JKRHeap::getFreeSize() { return do_getFreeSize(); }
u32 JKRHeap::getTotalFreeSize() { return do_getTotalFreeSize(); }

u8 JKRHeap::changeGroupID(u8 newGroupID)
{
#line 570
    JUT_WARNING_F(!mInitFlag, "change heap ID into %x in heap %x", newGroupID, this);
    return do_changeGroupID(newGroupID);
}

u8 JKRHeap::getCurrentGroupId() { return do_getCurrentGroupId(); }

u32 JKRHeap::getMaxAllocatableSize(int p1)
{
    u32 maxFreeBlock = (u32)do_getMaxFreeBlock();
    u32 freeSize = do_getFreeSize();
    return ~(p1 - 1) & freeSize - (p1 - 1 & p1 - ((u32)maxFreeBlock & 0xf));
}

JKRHeap *JKRHeap::findFromRoot(void *ptr)
{
    if (sRootHeap == nullptr)
    {
        return nullptr;
    }

    if (sRootHeap->mStartAddress <= ptr && ptr < sRootHeap->mEndAddress)
    {
        return sRootHeap->find(ptr);
    }

    return sRootHeap->findAllHeap(ptr);
}

JKRHeap *JKRHeap::find(void *memory) const
{
    if ((mStartAddress <= memory) && (memory < mEndAddress))
    {
        if (mHeapTree.getNumChildren() != 0)
        {
#ifdef FSA_PORT_TRACE
            int guard = 0;
#endif
            for (JSUTreeIterator<JKRHeap> iterator(mHeapTree.getFirstChild()); iterator != mHeapTree.getEndChild(); ++iterator)
            {
#ifdef FSA_PORT_TRACE
                if (++guard > 64) {
                    OSReport("[trace] heap %p: child list loops (numChildren %d)\n", this, (int)mHeapTree.getNumChildren());
                    int k = 0;
                    for (JSUTree<JKRHeap>* t = mHeapTree.getFirstChild(); t && k < 8; t = t->getNextChild(), k++)
                        OSReport("[trace]   child tree %p obj %p next %p\n", t, t->getObject(), t->getNextChild());
                    for (;;) OSSuspendThread(OSGetCurrentThread());
                }
#endif
                JKRHeap *result = iterator->find(memory);
                if (result)
                {
                    return result;
                }
            }
        }
        return const_cast<JKRHeap *>(this);
    }
    return nullptr;
}

JKRHeap *JKRHeap::findAllHeap(void *memory) const
{
    if (mHeapTree.getNumChildren() != 0)
    {
        for (JSUTreeIterator<JKRHeap> iterator(mHeapTree.getFirstChild()); iterator != mHeapTree.getEndChild(); ++iterator)
        {
            JKRHeap *result = iterator->findAllHeap(memory);
            if (result)
            {
                return result;
            }
        }
    }

    if (mStartAddress <= memory && memory < mEndAddress)
    {
        return const_cast<JKRHeap *>(this);
    }

    return nullptr;
}

void JKRHeap::dispose_subroutine(u32 begin, u32 end)
{
    JSUListIterator<JKRDisposer> last_iterator;
    for (JSUListIterator<JKRDisposer> iterator(mDisposerList.getFirst()); iterator != mDisposerList.getEnd();)
    {
        JKRDisposer *disposer = iterator.getObject();

        if ((void *)begin <= disposer && disposer < (void *)end)
        {
            disposer->~JKRDisposer();
            if (last_iterator == nullptr)
            {
                iterator = mDisposerList.getFirst();
            }
            else
            {
                iterator = last_iterator;
                iterator++;
            }
        }
        else
        {
            last_iterator = iterator;
            iterator++;
        }
    }
}

bool JKRHeap::dispose(void *memory, u32 size)
{
    u32 begin = (u32)memory;
    u32 end = (u32)memory + size;
    dispose_subroutine(begin, end);
    return false;
}

void JKRHeap::dispose(void *begin, void *end)
{
    dispose_subroutine((u32)begin, (u32)end);
}

void JKRHeap::dispose()
{
    JSUListIterator<JKRDisposer> iterator;
    while (iterator = mDisposerList.getFirst(), iterator != mDisposerList.getEnd())
    {
        iterator->~JKRDisposer();
    }
}

void JKRHeap::copyMemory(void *dst, void *src, u32 size)
{
    u32 count = (size + 3) / 4;

    u32 *dst_32 = (u32 *)dst;
    u32 *src_32 = (u32 *)src;
    while (count > 0)
    {
        *dst_32 = *src_32;
        dst_32++;
        src_32++;
        count--;
    }
}

void JKRDefaultMemoryErrorRoutine(void *heap, u32 size, int alignment)
{
#line 830
    JUT_REPORT_MSG("Error: Cannot allocate memory %d(0x%x)byte in %d byte alignment from %08x\n", size, size, alignment, heap);
    JUT_PANIC("abort\n");
}

JKRHeapErrorHandler *JKRHeap::setErrorHandler(JKRHeapErrorHandler *newHandler)
{
    JKRHeapErrorHandler *oldHandler = mErrorHandler;
    if (!newHandler)
    {
        newHandler = JKRDefaultMemoryErrorRoutine;
    }
    mErrorHandler = newHandler;
    return oldHandler;
}

// attempt to reconstruct function but lazy
void JKRHeap::checkMemoryFilled(u8 *address, u32 size, u8 p3)
{
    for (int i = 0; i < size; i++)
    {
#line 999
        JUT_WARNING_F(p3 == address[i], "**** checkMemoryFilled:\n address %08x size %x:\n (%08x = %02x)\n", address, size, address[i], address[i]);
    }
}

bool JKRHeap::isSubHeap(JKRHeap *heap) const
{
    if (!heap)
        return false;

    if (mHeapTree.getNumChildren() != 0)
    {

        for (JSUTreeIterator<JKRHeap> iterator(mHeapTree.getFirstChild()); iterator != mHeapTree.getEndChild(); ++iterator)
        {
            if (iterator.getObject() == heap)
            {
                return true;
            }

            if (iterator.getObject()->isSubHeap(heap))
            {
                return true;
            }
        }
    }

    return false;
}

#ifdef FSA_PORT
extern "C" int fsa_host_alloc_depth;
// Port: JSystem's global operator new/delete replace C++'s for the whole wasm
// module, but only the game's own allocations belong in JKR heaps. Host code
// (libc++, Aurora, the platform layer) marks itself with fsa_host_alloc_depth,
// and anything before the first heap exists goes to malloc too. delete frees by
// address: memory inside the root heap goes back to its JKR heap.
extern "C" {
extern int fsa_host_alloc_depth;
void *malloc(size_t);
void free(void *);
int posix_memalign(void **, size_t, size_t);
}
namespace {
bool use_jkr() { return fsa_host_alloc_depth == 0 && JKRHeap::getCurrentHeap() != nullptr; }
bool in_jkr(void *p)
{
    JKRHeap *root = JKRHeap::getRootHeap();
    return root && p >= root->getStartAddr() && p < root->getEndAddr();
}
void *host_alloc(size_t n, size_t align)
{
    void *p = nullptr;
    if (align <= 8)
        return malloc(n ? n : 1);
    posix_memalign(&p, align, n ? n : 1);
    return p;
}
void release(void *p)
{
    if (!p)
        return;
    if (in_jkr(p))
        JKRHeap::free(p, nullptr);
    else
        free(p);
}
} // namespace

void *operator new(size_t byteCount) { return use_jkr() ? JKRHeap::alloc(byteCount, 4, nullptr) : host_alloc(byteCount, 8); }
void *operator new(size_t byteCount, int alignment)
{
    return use_jkr() ? JKRHeap::alloc(byteCount, alignment, nullptr) : host_alloc(byteCount, alignment < 0 ? -alignment : alignment);
}
void *operator new(size_t byteCount, JKRHeap *heap, int alignment) { return JKRHeap::alloc(byteCount, alignment, heap); }
void *operator new[](size_t byteCount) { return operator new(byteCount); }
void *operator new[](size_t byteCount, int alignment) { return operator new(byteCount, alignment); }
void *operator new[](size_t byteCount, JKRHeap *heap, int alignment) { return JKRHeap::alloc(byteCount, alignment, heap); }
void operator delete(void *memory) { release(memory); }
void operator delete[](void *memory) { release(memory); }
void operator delete(void *memory, size_t) { release(memory); }
void operator delete[](void *memory, size_t) { release(memory); }
namespace std { enum class align_val_t : size_t; }
void *operator new(size_t n, std::align_val_t a) { return use_jkr() ? JKRHeap::alloc(n, (int)a, nullptr) : host_alloc(n, (size_t)a); }
void *operator new[](size_t n, std::align_val_t a) { return operator new(n, a); }
void operator delete(void *memory, std::align_val_t) { release(memory); }
void operator delete[](void *memory, std::align_val_t) { release(memory); }
void operator delete(void *memory, size_t, std::align_val_t) { release(memory); }
void operator delete[](void *memory, size_t, std::align_val_t) { release(memory); }
#else
void *operator new(u32 byteCount)
{
    return JKRHeap::alloc(byteCount, 4, nullptr);
}
void *operator new(u32 byteCount, int alignment)
{
    return JKRHeap::alloc(byteCount, alignment, nullptr);
}
void *operator new(u32 byteCount, JKRHeap *heap, int alignment)
{
    return JKRHeap::alloc(byteCount, alignment, heap);
}

void *operator new[](u32 byteCount)
{
    return JKRHeap::alloc(byteCount, 4, nullptr);
}
void *operator new[](u32 byteCount, int alignment)
{
    return JKRHeap::alloc(byteCount, alignment, nullptr);
}
void *operator new[](u32 byteCount, JKRHeap *heap, int alignment)
{
    return JKRHeap::alloc(byteCount, alignment, heap);
}

void operator delete(void *memory) { JKRHeap::free(memory, nullptr); }
void operator delete[](void *memory) { JKRHeap::free(memory, nullptr); }
#endif

JKRHeap::TState::TState(const JKRHeap::TState::TArgument &arg, const JKRHeap::TState::TLocation &location)
    : mArgument(arg), mLocation(location)
{
    // UNUSED FUNCTION
}

JKRHeap::TState::TState(const JKRHeap::TState &other, bool p2)
    : mArgument(other.getHeap(), 0, p2)
{
    // UNUSED FUNCTION
}

JKRHeap::TState::TState(const JKRHeap::TState &other, const JKRHeap::TState::TLocation &location, bool p3)
    : mArgument(other.getHeap(), 0, p3), mLocation()
{
    // UNUSED FUNCTION
}

JKRHeap::TState::~TState()
{
    // Unused
    if (isCompareOnDestructed())
    {
        TState state(getHeap(), getId(), false);
        if (mArgument.mHeap->state_compare(*this, state))
        {
            if (isVerbose())
            {
                JUT_REPORT_MSG("heap unchanged");
            }
        }
        else
        {
            state_dumpDifference(*this, state);

            if (isVerbose())
            {
                char *str = mLocation._00;
                u32 addr = mLocation._04;
                JUT_REPORT_MSG("**** heap changed ****");
                JUT_REPORT_MSG("location   : [%s:%d]", str, addr);
                JUT_REPORT_MSG("**** heap changed : old ****");
                dump();
                JUT_REPORT_MSG("**** heap changed : new ****");
                state.dump();
            }
        }
    }
}

void JKRHeap::state_register(JKRHeap::TState *p, u32) const
{
#line 1132
    JUT_ASSERT(p != 0);
    JUT_ASSERT(p->getHeap() == this);
}

bool JKRHeap::state_compare(const JKRHeap::TState &r1, const JKRHeap::TState &r2) const
{
#line 1141
    JUT_ASSERT(r1.getHeap() == r2.getHeap());
    return (r1.getCheckCode() == r2.getCheckCode());
}

// fabricated, but probably matches(except for line numbers)
void JKRHeap::state_dumpDifference(const JKRHeap::TState &r1, const JKRHeap::TState &r2)
{
    JUT_LOG_F("heap       : %p / %p", r1.getHeap(), r2.getHeap());
    JUT_LOG_F("check-code : 0x%08x / 0x%08x", r1.getCheckCode(), r2.getCheckCode());
    JUT_LOG_F("id         : 0x%08x / 0x%08x", r1.getId(), r2.getId());
    JUT_LOG_F("used size  : %10u / %10u", r1.getUsedSize(), r2.getUsedSize());
}

void JKRHeap::state_dump(const TState &state) const
{
#line 1165
    JUT_LOG_F("check-code : 0x%08x", state.getCheckCode());
    JUT_LOG_F("id         : 0x%08x", state.getId());
    JUT_LOG_F("used size  : %u", state.getUsedSize());
}
