// OS threads, messages, mutexes, conditions and alarms for the browser.
//
// Every OSThread is a coroutine: its own wasm stack (a JSPI promising export,
// fsa_thread_entry) and its own region of emscripten's shadow stack. Switching
// suspends the current coroutine on a promise and resumes the target from a
// microtask (platform/web/fsa_sched.js), so only one runs at a time, as on the
// single-core GameCube. Scheduling is by priority (0 highest), like the SDK's,
// but cooperative: a thread runs until it blocks or an OS call readies a
// higher-priority thread. "Interrupts" (retrace, alarms, DVD completion) run
// from the browser's event loop and only make threads ready; when every thread
// is blocked the scheduler is idle and the next interrupt resumes one.
#include "platform.h"

#include <emscripten.h>
#include <emscripten/stack.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <unordered_map>
#include <vector>

extern "C" void _emscripten_stack_restore(uintptr_t sp);

#ifdef FSA_DIAG
// Diagnostics build: which shadow call stack (diag_cov.cpp) is live. Context
// n + 1 is coroutine n; 0 is the interrupt / host context.
extern "C" void fsa_diag_set_context(unsigned ctx);
extern "C" unsigned fsa_diag_context(void);
#define DIAG_CONTEXT(c) fsa_diag_set_context((unsigned)(c)->id + 1)
#else
#define DIAG_CONTEXT(c) ((void)0)
#endif

namespace {

constexpr size_t kThreadStackSize = 512 * 1024;

struct Coroutine {
  OSThread* thread = nullptr;
  int id = 0;
  bool started = false;
  uintptr_t sp = 0;       // saved shadow-stack pointer while switched out
  uintptr_t stackBase = 0; // lowest address (emscripten stacks grow down)
  uintptr_t stackTop = 0;
  OSThreadStartFunction func = nullptr;
  void* arg = nullptr;
  int suspend = 0;
  bool waiting = false;
  bool done = false;
  bool detached = false;
  void* exitVal = nullptr;
  OSTime wakeAt = 0;      // OSSleepTicks-style timed wait, 0 = none
};

std::vector<Coroutine*> g_coroutines;        // index = id
std::unordered_map<OSThread*, Coroutine*> g_byThread;
Coroutine* g_current = nullptr;               // null while idle
std::unordered_map<const void*, std::deque<Coroutine*>> g_waitQueues;
std::deque<Coroutine*> g_joinWaiters;
int g_schedulerDisabled = 0;
int g_interruptDepth = 0;
OSThread g_mainThread;

Coroutine* coroutine_of(OSThread* t) {
  auto it = g_byThread.find(t);
  return it == g_byThread.end() ? nullptr : it->second;
}

bool runnable(const Coroutine* c) {
  return c->started ? (!c->waiting && c->suspend <= 0 && !c->done) : (c->suspend <= 0 && !c->done);
}

// Highest-priority runnable coroutine; ties go to the one after `after` in id
// order, so equal priorities round-robin on yield.
Coroutine* pick(const Coroutine* after = nullptr) {
  Coroutine* best = nullptr;
  size_t n = g_coroutines.size();
  size_t start = after ? (size_t)after->id + 1 : 0;
  for (size_t k = 0; k < n; k++) {
    Coroutine* c = g_coroutines[(start + k) % n];
    if (!c || !runnable(c))
      continue;
    if (!best || c->thread->priority < best->thread->priority)
      best = c;
  }
  return best;
}

void set_stack(Coroutine* c) {
  emscripten_stack_set_limits((void*)c->stackTop, (void*)c->stackBase);
}

}  // namespace

// --- JS side of the switch (platform/web/fsa_sched.js holds the promise map) --
EM_ASYNC_JS(void, fsa_js_switch, (int from, int to, int start), {
  await Module.fsaSched.switchTo(from, to, start);
});
EM_ASYNC_JS(void, fsa_js_idle, (int from), {
  await Module.fsaSched.idle(from);
});
EM_JS(void, fsa_js_resume, (int to, int start), {
  Module.fsaSched.resume(to, start);
});

namespace {

// Switch from the current coroutine to `next` (or to idle when null). Returns
// when something switches back to the caller.
void switch_from_current(Coroutine* next) {
  Coroutine* self = g_current;
  if (next == self)
    return;
  self->sp = emscripten_stack_get_current();
  g_current = next;
  if (next) {
    bool start = !next->started;
    next->started = true;
    fsa_js_switch(self->id, next->id, start ? 1 : 0);
  } else {
    fsa_js_idle(self->id);
  }
  // Resumed: our shadow stack again.
  g_current = self;
  DIAG_CONTEXT(self);
  set_stack(self);
  _emscripten_stack_restore(self->sp);
}

// Called after state changes: run a higher-priority thread if one is ready.
void reschedule() {
  if (g_interruptDepth || g_schedulerDisabled || !g_current)
    return;
  Coroutine* best = pick();
  if (best && best != g_current && (!runnable(g_current) || best->thread->priority < g_current->thread->priority))
    switch_from_current(best);
}

// The current thread can no longer run (blocked, suspended, exited).
u32 g_blocks = 0;  // times a thread blocked (platform_poll_point's lockstep count)

void block_current() {
  g_blocks++;
  while (!runnable(g_current)) {
    platform_service_interrupts();
    Coroutine* next = pick();
    if (next == g_current)
      break;
    switch_from_current(next);  // next may be null: idle until an interrupt
  }
}

void wait_on(const void* key) {
  g_current->waiting = true;
  {
    HostAllocScope host;
    g_waitQueues[key].push_back(g_current);
  }
  block_current();
}

// Wake every thread waiting on key (OSWakeupThread semantics).
bool wake_all(const void* key) {
  auto it = g_waitQueues.find(key);
  if (it == g_waitQueues.end() || it->second.empty())
    return false;
  for (Coroutine* c : it->second)
    c->waiting = false;
  it->second.clear();
  return true;
}

bool wake_one(const void* key) {
  auto it = g_waitQueues.find(key);
  if (it == g_waitQueues.end() || it->second.empty())
    return false;
  // Highest priority first, like the SDK's priority-ordered thread queues.
  auto best = std::min_element(it->second.begin(), it->second.end(),
                               [](Coroutine* a, Coroutine* b) { return a->thread->priority < b->thread->priority; });
  (*best)->waiting = false;
  it->second.erase(best);
  return true;
}

Coroutine* make_coroutine(OSThread* t) {
  HostAllocScope host;
  auto* c = new Coroutine();
  c->thread = t;
  c->id = (int)g_coroutines.size();
  g_coroutines.push_back(c);
  g_byThread[t] = c;
  return c;
}

}  // namespace

// --- Entry for a new coroutine (a JSPI promising export) -----------------------
extern "C" EMSCRIPTEN_KEEPALIVE __attribute__((no_instrument_function)) void fsa_thread_entry(int id) {
  Coroutine* c = g_coroutines[id];
  g_current = c;
  DIAG_CONTEXT(c);
  set_stack(c);
  _emscripten_stack_restore(c->stackTop);
  void* ret = c->func(c->arg);
  OSExitThread(ret);
}

// --- Interrupt bracket: callbacks run here must not switch -------------------
#ifdef FSA_DIAG
static unsigned g_diagSavedContext;
__attribute__((no_instrument_function)) void platform_interrupt_enter() {
  if (g_interruptDepth++ == 0) {
    g_diagSavedContext = fsa_diag_context();
    fsa_diag_set_context(0);
  }
}
__attribute__((no_instrument_function)) void platform_interrupt_leave() {
  if (--g_interruptDepth == 0)
    fsa_diag_set_context(g_diagSavedContext);
}
#else
void platform_interrupt_enter() { g_interruptDepth++; }
void platform_interrupt_leave() { g_interruptDepth--; }
#endif

// Called from the browser's event loop (retrace, timers, DVD): if the scheduler
// is idle and a thread became ready, resume it.
void platform_kick() {
  if (g_current || g_interruptDepth)
    return;
  Coroutine* next = pick();
  if (!next)
    return;
  bool start = !next->started;
  next->started = true;
  g_current = next;
  fsa_js_resume(next->id, start ? 1 : 0);
}

bool platform_is_idle() { return g_current == nullptr; }

void platform_reschedule() { reschedule(); }

void platform_threads_init() {
  memset(&g_mainThread, 0, sizeof(g_mainThread));
  g_mainThread.priority = g_mainThread.base = 16;
  g_mainThread.state = OS_THREAD_STATE_RUNNING;
  Coroutine* c = make_coroutine(&g_mainThread);
  c->started = true;
  c->stackBase = emscripten_stack_get_end();
  c->stackTop = emscripten_stack_get_base();
  g_current = c;
  DIAG_CONTEXT(c);
}

// Wake timed sleepers whose time has come (called at interrupt points).
void platform_check_sleepers(OSTime now) {
  for (Coroutine* c : g_coroutines) {
    if (c && c->wakeAt && now >= c->wakeAt) {
      c->wakeAt = 0;
      c->waiting = false;
    }
  }
}

extern "C" {

// --- Threads ----------------------------------------------------------------
void OSInitThreadQueue(OSThreadQueue* queue) { queue->head = queue->tail = nullptr; }

OSThread* OSGetCurrentThread() { return g_current ? g_current->thread : &g_mainThread; }

BOOL OSIsThreadTerminated(OSThread* thread) {
  Coroutine* c = coroutine_of(thread);
  return !c || c->done;
}

s32 OSDisableScheduler() { return g_schedulerDisabled++; }
s32 OSEnableScheduler() { return g_schedulerDisabled--; }

void OSYieldThread() {
  platform_service_interrupts();
  if (!g_current)
    return;
  Coroutine* next = pick(g_current);
  if (next && next != g_current && next->thread->priority <= g_current->thread->priority)
    switch_from_current(next);
}

BOOL OSCreateThread(OSThread* thread, OSThreadStartFunction func, void* param, void* stack, u32 stackSize,
                    OSPriority prio, u16 attr) {
  if (prio < OS_PRIORITY_MIN || prio > OS_PRIORITY_MAX)
    return FALSE;
  if (Coroutine* old = coroutine_of(thread)) {
    // Reusing an OSThread object (JKRThread does after a thread ends).
    g_byThread.erase(thread);
    g_coroutines[old->id] = nullptr;
  }
  HostAllocScope host;
  memset(&thread->context, 0, sizeof(thread->context));
  thread->state = OS_THREAD_STATE_READY;
  thread->attr = attr;
  thread->suspend = 1;
  thread->priority = thread->base = prio;
  thread->val = nullptr;
  thread->queue = nullptr;
  thread->mutex = nullptr;
  thread->stackBase = (u8*)stack;
  thread->stackEnd = (u32*)((u8*)stack - stackSize);
  thread->error = 0;
  Coroutine* c = make_coroutine(thread);
  // The game's stack sizes are for PowerPC; the port's shadow stack is separate.
  void* mem = aligned_alloc(16, kThreadStackSize);
  c->stackBase = (uintptr_t)mem;
  c->stackTop = (uintptr_t)mem + kThreadStackSize;
  c->func = func;
  c->arg = param;
  c->suspend = 1;
  c->detached = (attr & OS_THREAD_ATTR_DETACH) != 0;
  return TRUE;
}

void OSExitThread(void* val) {
  Coroutine* self = g_current;
  self->done = true;
  self->exitVal = val;
  self->thread->val = val;
  self->thread->state = OS_THREAD_STATE_MORIBUND;
  wake_all(&self->thread->queueJoin);
  block_current();
  // A finished coroutine is never picked again; its promise stays pending.
  for (;;)
    fsa_js_idle(self->id);
}

void OSCancelThread(OSThread* thread) {
  Coroutine* c = coroutine_of(thread);
  if (!c)
    return;
  if (c == g_current) {
    OSExitThread(nullptr);
    return;
  }
  c->done = true;
  thread->state = OS_THREAD_STATE_MORIBUND;
  for (auto& [key, q] : g_waitQueues)
    q.erase(std::remove(q.begin(), q.end(), c), q.end());
  wake_all(&thread->queueJoin);
  reschedule();
}

void OSDetachThread(OSThread* thread) {
  if (Coroutine* c = coroutine_of(thread))
    c->detached = true;
}

BOOL OSJoinThread(OSThread* thread, void** val) {
  Coroutine* c = coroutine_of(thread);
  if (!c)
    return FALSE;
  while (!c->done)
    wait_on(&thread->queueJoin);
  if (val)
    *val = c->exitVal;
  return TRUE;
}

s32 OSResumeThread(OSThread* thread) {
  Coroutine* c = coroutine_of(thread);
  if (!c)
    return 0;
  s32 prev = c->suspend;
  if (c->suspend > 0 && --c->suspend == 0) {
    thread->suspend = 0;
    thread->state = OS_THREAD_STATE_READY;
    reschedule();
  }
  return prev;
}

s32 OSSuspendThread(OSThread* thread) {
  Coroutine* c = coroutine_of(thread);
  if (!c)
    return 0;
  s32 prev = c->suspend++;
  thread->suspend = c->suspend;
  if (c == g_current)
    block_current();
  return prev;
}

BOOL OSIsThreadSuspended(OSThread* thread) {
  Coroutine* c = coroutine_of(thread);
  return c && c->suspend > 0;
}

void OSSleepThread(OSThreadQueue* queue) { wait_on(queue); }

void OSWakeupThread(OSThreadQueue* queue) {
  if (wake_all(queue))
    reschedule();
}

void OSClearStack(u8) {}

OSPriority OSGetThreadPriority(OSThread* thread) { return thread->priority; }

BOOL OSSetThreadPriority(OSThread* thread, OSPriority prio) {
  if (prio < OS_PRIORITY_MIN || prio > OS_PRIORITY_MAX)
    return FALSE;
  thread->priority = thread->base = prio;
  reschedule();
  return TRUE;
}

OSSwitchThreadCallback OSSetSwitchThreadCallback(OSSwitchThreadCallback cb) { return cb; }
OSThread* OSSetIdleFunction(OSIdleFunction, void*, void*, u32) { return nullptr; }
OSThread* OSGetIdleFunction() { return nullptr; }
long OSCheckActiveThreads() { return (long)g_byThread.size(); }

// --- Messages ---------------------------------------------------------------
void OSInitMessageQueue(OSMessageQueue* mq, OSMessage* msgArray, s32 msgCount) {
  OSInitThreadQueue(&mq->queueSend);
  OSInitThreadQueue(&mq->queueReceive);
  mq->msgArray = msgArray;
  mq->msgCount = msgCount;
  mq->firstIndex = 0;
  mq->usedCount = 0;
}

BOOL OSSendMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) {
  while (mq->usedCount >= mq->msgCount) {
    if (!(flags & OS_MESSAGE_BLOCK) || g_interruptDepth || !g_current)
      return FALSE;
    wait_on(&mq->queueSend);
  }
  mq->msgArray[(mq->firstIndex + mq->usedCount) % mq->msgCount] = msg;
  mq->usedCount++;
  if (wake_one(&mq->queueReceive))
    reschedule();
  return TRUE;
}

BOOL OSJamMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) {
  while (mq->usedCount >= mq->msgCount) {
    if (!(flags & OS_MESSAGE_BLOCK) || g_interruptDepth || !g_current)
      return FALSE;
    wait_on(&mq->queueSend);
  }
  mq->firstIndex = (mq->firstIndex + mq->msgCount - 1) % mq->msgCount;
  mq->msgArray[mq->firstIndex] = msg;
  mq->usedCount++;
  if (wake_one(&mq->queueReceive))
    reschedule();
  return TRUE;
}

BOOL OSReceiveMessage(OSMessageQueue* mq, OSMessage* msg, s32 flags) {
  while (mq->usedCount == 0) {
    if (!(flags & OS_MESSAGE_BLOCK) || g_interruptDepth || !g_current)
      return FALSE;
    wait_on(&mq->queueReceive);
  }
  if (msg)
    *msg = mq->msgArray[mq->firstIndex];
  mq->firstIndex = (mq->firstIndex + 1) % mq->msgCount;
  mq->usedCount--;
  if (wake_one(&mq->queueSend))
    reschedule();
  return TRUE;
}

// --- Mutexes and conditions -------------------------------------------------
void OSInitMutex(OSMutex* mutex) {
  memset(mutex, 0, sizeof(*mutex));
}

void OSLockMutex(OSMutex* mutex) {
  OSThread* me = OSGetCurrentThread();
  while (mutex->thread && mutex->thread != me)
    wait_on(mutex);
  mutex->thread = me;
  mutex->count++;
}

void OSUnlockMutex(OSMutex* mutex) {
  if (mutex->thread != OSGetCurrentThread())
    return;
  if (--mutex->count == 0) {
    mutex->thread = nullptr;
    if (wake_one(mutex))
      reschedule();
  }
}

BOOL OSTryLockMutex(OSMutex* mutex) {
  OSThread* me = OSGetCurrentThread();
  if (mutex->thread && mutex->thread != me)
    return FALSE;
  mutex->thread = me;
  mutex->count++;
  return TRUE;
}

void OSInitCond(OSCond* cond) { OSInitThreadQueue(&cond->queue); }

void OSWaitCond(OSCond* cond, OSMutex* mutex) {
  OSThread* me = OSGetCurrentThread();
  if (mutex->thread != me)
    return;
  s32 count = mutex->count;
  mutex->count = 0;
  mutex->thread = nullptr;
  wake_one(mutex);
  wait_on(cond);
  while (mutex->thread && mutex->thread != me)
    wait_on(mutex);
  mutex->thread = me;
  mutex->count = count;
}

void OSSignalCond(OSCond* cond) {
  if (wake_all(cond))
    reschedule();
}

// --- Alarms -----------------------------------------------------------------
}  // extern "C"

namespace {
std::vector<OSAlarm*> g_alarms;

void alarm_unlink(OSAlarm* a) { g_alarms.erase(std::remove(g_alarms.begin(), g_alarms.end(), a), g_alarms.end()); }
}  // namespace

void platform_check_alarms(OSTime now) {
  // Handlers may set or cancel alarms, so take one due alarm at a time.
  for (int guard = 0; guard < 256; guard++) {
    OSAlarm* due = nullptr;
    for (OSAlarm* a : g_alarms)
      if (a->fire <= now && (!due || a->fire < due->fire))
        due = a;
    if (!due)
      return;
    OSAlarmHandler handler = due->handler;
    if (due->period > 0) {
      while (due->fire <= now)
        due->fire += due->period;
    } else {
      alarm_unlink(due);
      due->handler = nullptr;
    }
    if (handler) {
      platform_interrupt_enter();
      handler(due, nullptr);
      platform_interrupt_leave();
    }
  }
}

OSTime platform_next_alarm() {
  OSTime next = 0;
  for (OSAlarm* a : g_alarms)
    if (!next || a->fire < next)
      next = a->fire;
  for (Coroutine* c : g_coroutines)
    if (c && c->wakeAt && (!next || c->wakeAt < next))
      next = c->wakeAt;
  return next;
}

extern "C" {

void OSInitAlarm(void) {}
void OSCreateAlarm(OSAlarm* alarm) {
  memset(alarm, 0, sizeof(*alarm));
}
void OSSetAlarmTag(OSAlarm* alarm, u32 tag) { alarm->tag = tag; }

void OSSetAbsAlarm(OSAlarm* alarm, OSTime time, OSAlarmHandler handler) {
  HostAllocScope host;
  alarm_unlink(alarm);
  alarm->handler = handler;
  alarm->fire = time;
  alarm->period = 0;
  g_alarms.push_back(alarm);
}

void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler) {
  OSSetAbsAlarm(alarm, OSGetTime() + tick, handler);
}

void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler) {
  HostAllocScope host;
  alarm_unlink(alarm);
  alarm->handler = handler;
  alarm->start = start;
  alarm->period = period;
  alarm->fire = start;
  OSTime now = OSGetTime();
  while (alarm->fire < now)
    alarm->fire += period;
  g_alarms.push_back(alarm);
}

void OSCancelAlarm(OSAlarm* alarm) {
  alarm_unlink(alarm);
  alarm->handler = nullptr;
}

void OSCancelAlarms(u32 tag) {
  HostAllocScope host;
  std::vector<OSAlarm*> keep;
  for (OSAlarm* a : g_alarms)
    if (a->tag != tag)
      keep.push_back(a);
  g_alarms.swap(keep);
}

BOOL OSCheckAlarmQueue(void) { return TRUE; }

// --- Interrupts: one core, cooperative scheduling -> nothing to mask ----------
BOOL OSDisableInterrupts() { return TRUE; }
BOOL OSEnableInterrupts() { return TRUE; }
BOOL OSRestoreInterrupts(BOOL level) { return level; }

}  // extern "C"

namespace {
u32 g_pollCount = 0, g_pollBlocks = 0;  // platform_poll_point's lockstep counters (saved by platform_threads_save)
}  // namespace

// Polling functions (DVDGetDriveStatus, VIGetRetraceCount, ...) call this. A
// thread that polls for ~8 ms without blocking is waiting on something only the
// browser can deliver (a retrace, a timer, a disc read), so it sleeps briefly
// and lets the page's event loop run.
void platform_poll_point() {
  static double lastYield = 0;
  if (!g_current || g_interruptDepth)
    return;
  if (platform_lockstep()) {
    // Lockstep (docs/HARNESS.md): the same decision by call count, so that a
    // replay yields at the same points. 1000 polls with no thread blocking in
    // between is a spin loop; it sleeps until virtual time moves (a retrace).
    u32& polls = g_pollCount;
    u32& blocks = g_pollBlocks;
    if (blocks != g_blocks) {
      blocks = g_blocks;
      polls = 0;
    }
    if (++polls < 1000)
      return;
    polls = 0;
    platform_sleep_ticks(OS_TIMER_CLOCK / 1000);
    blocks = g_blocks;
    return;
  }
  double now = emscripten_get_now();
  if (lastYield == 0)
    lastYield = now;
  if (now - lastYield < 8.0)
    return;
  platform_sleep_ticks(OS_TIMER_CLOCK / 1000);  // 1 ms
  lastYield = emscripten_get_now();
}

// Timed sleep used by the platform (not an SDK call on the GameCube's 2003 SDK).
void platform_sleep_ticks(OSTime ticks) {
  g_current->wakeAt = OSGetTime() + ticks;
  g_current->waiting = true;
  block_current();
}

// Debugging: list every thread and what it waits on (call _fsa_dump_threads()
// from the console).
extern "C" EMSCRIPTEN_KEEPALIVE void fsa_dump_threads() {
  for (Coroutine* c : g_coroutines) {
    if (!c)
      continue;
    const void* queue = nullptr;
    for (auto& [key, waiters] : g_waitQueues)
      if (std::find(waiters.begin(), waiters.end(), c) != waiters.end())
        queue = key;
    platform_log("[thread] %d%s prio %d func %u started %d waiting %d suspend %d done %d wakeAt %lld queue %p\n", c->id,
                 c == g_current ? "*" : "", c->thread ? (int)c->thread->priority : -1,
                 (unsigned)(uintptr_t)c->func, c->started, c->waiting, c->suspend, c->done, (long long)c->wakeAt,
                 queue);
  }
}

// --- Rollback (docs/NETPLAY.md, platform/netplay.cpp) -------------------------
// The thread layer's part of a snapshot. A JSPI continuation cannot be copied,
// so a snapshot can only be restored while every coroutine is suspended at the
// same place as when it was taken: the same coroutines, each started/finished
// alike and switched out at the same shadow-stack pointer (the same call path).
// What may differ, and is restored, is the scheduling state around them: who
// waits on what, timed wake-ups, the alarm list, the lockstep poll counters.
// The coroutines' shadow stacks themselves are saved by netplay.cpp
// (platform_thread_stacks).
namespace {
struct SavedCoroutine {
  Coroutine* c;
  OSThread* thread;
  uintptr_t sp;
  OSTime wakeAt;
  void* exitVal;
  int suspend;
  u8 started, waiting, done, detached;
};
template <class T> void put(u8*& p, const T& v) { memcpy(p, &v, sizeof(T)); p += sizeof(T); }
template <class T> T get(const u8*& p) { T v; memcpy(&v, p, sizeof(T)); p += sizeof(T); return v; }
}  // namespace

// Bytes written (0 when cap is too small). Called on the game thread inside
// the pad-read hook; cur_sp is that thread's shadow-stack pointer there.
int platform_threads_save(u8* buf, int cap, uintptr_t cur_sp) {
  size_t need = 64 + g_coroutines.size() * sizeof(SavedCoroutine) + g_alarms.size() * sizeof(void*);
  for (auto& [key, q] : g_waitQueues)
    need += 16 + q.size() * sizeof(void*);
  if (need > (size_t)cap)
    return 0;
  u8* p = buf;
  put<u32>(p, (u32)g_coroutines.size());
  put<Coroutine*>(p, g_current);
  for (Coroutine* c : g_coroutines) {
    SavedCoroutine s{};
    s.c = c;
    if (c) {
      s.thread = c->thread;
      s.sp = c == g_current ? cur_sp : c->sp;
      s.wakeAt = c->wakeAt;
      s.exitVal = c->exitVal;
      s.suspend = c->suspend;
      s.started = c->started;
      s.waiting = c->waiting;
      s.done = c->done;
      s.detached = c->detached;
    }
    put(p, s);
  }
  put<u32>(p, (u32)g_alarms.size());
  for (OSAlarm* a : g_alarms)
    put(p, a);
  u32 queues = 0;
  for (auto& [key, q] : g_waitQueues)
    queues += q.empty() ? 0 : 1;
  put<u32>(p, queues);
  for (auto& [key, q] : g_waitQueues) {
    if (q.empty())
      continue;
    put<const void*>(p, key);
    put<u32>(p, (u32)q.size());
    for (Coroutine* c : q)
      put(p, c);
  }
  put<u32>(p, g_blocks);
  put<u32>(p, g_pollCount);
  put<u32>(p, g_pollBlocks);
  put<int>(p, g_schedulerDisabled);
  return (int)(p - buf);
}

// 0 when restored; otherwise nothing is changed and the result says why:
// -1 a coroutine of the snapshot is gone or was replaced, -2 one that was
// running (started, not finished) at the snapshot has finished since, -3 one
// is switched out at another place (its sp), -4 another thread is current,
// -5 malformed.
// Reversible, and undone here: coroutines created after the snapshot (dropped:
// their continuations are abandoned, the re-run creates them again), and ones
// that had not started at the snapshot but have since (they start afresh).
int platform_threads_restore(const u8* buf, int len, uintptr_t cur_sp) {
  const u8* p = buf;
  const u8* end = buf + len;
  if (len < 8)
    return -5;
  u32 n = get<u32>(p);
  Coroutine* current = get<Coroutine*>(p);
  if (n > g_coroutines.size())
    return -1;
  if (current != g_current)
    return -4;
  const u8* first = p;
  for (u32 i = 0; i < n; i++) {
    SavedCoroutine s = get<SavedCoroutine>(p);
    Coroutine* c = g_coroutines[i];
    if (s.c != c || (c && s.thread != c->thread))
      return -1;
    if (!c)
      continue;
    if (s.started && !s.done) {
      if (!c->started || c->done)
        return -2;
      uintptr_t sp = c == g_current ? cur_sp : c->sp;
      if (s.sp != sp)
        return -3;
    } else if (s.done && !c->done) {
      return -1;
    }
  }
  for (size_t i = n; i < g_coroutines.size(); i++)
    if (g_coroutines[i] == g_current)
      return -4;
  if (p > end)
    return -5;
  HostAllocScope host;
  // Coroutines newer than the snapshot: forgotten.
  for (size_t i = n; i < g_coroutines.size(); i++) {
    Coroutine* c = g_coroutines[i];
    if (!c)
      continue;
    auto it = g_byThread.find(c->thread);
    if (it != g_byThread.end() && it->second == c)
      g_byThread.erase(it);
    c->done = true;  // never picked again; its stack memory is leaked on purpose (a continuation may still point into it)
  }
  g_coroutines.resize(n);
  p = first;
  for (u32 i = 0; i < n; i++) {
    SavedCoroutine s = get<SavedCoroutine>(p);
    Coroutine* c = g_coroutines[i];
    if (!c)
      continue;
    if (!s.started && c->started) {
      // Started after the snapshot: it starts again from its entry point (a
      // fresh JSPI stack); the old continuation is abandoned.
      c->started = false;
      c->done = false;
      c->sp = 0;
    }
    c->wakeAt = s.wakeAt;
    c->exitVal = s.exitVal;
    c->suspend = s.suspend;
    c->waiting = s.waiting;
    c->detached = s.detached;
  }
  u32 alarms = get<u32>(p);
  g_alarms.clear();
  for (u32 i = 0; i < alarms; i++)
    g_alarms.push_back(get<OSAlarm*>(p));
  for (auto& [key, q] : g_waitQueues)
    q.clear();
  u32 queues = get<u32>(p);
  for (u32 i = 0; i < queues; i++) {
    const void* key = get<const void*>(p);
    u32 k = get<u32>(p);
    auto& q = g_waitQueues[key];
    for (u32 j = 0; j < k; j++)
      q.push_back(get<Coroutine*>(p));
  }
  g_blocks = get<u32>(p);
  g_pollCount = get<u32>(p);
  g_pollBlocks = get<u32>(p);
  g_schedulerDisabled = get<int>(p);
  return 0;
}

// Each coroutine as (id, switched-out sp, stack top), for the page's netplay
// policy (only coroutines that are running: started, not finished).
int platform_thread_layout(u32* out, int max, uintptr_t cur_sp) {
  int k = 0;
  for (Coroutine* c : g_coroutines) {
    if (!c || !c->started || c->done || k >= max)
      continue;
    out[3 * k] = (u32)c->id;
    out[3 * k + 1] = (u32)(c == g_current ? cur_sp : c->sp);
    out[3 * k + 2] = (u32)c->stackTop;
    k++;
  }
  return k;
}

// The live part of every coroutine's shadow stack, as (low, high) pairs: from
// its saved sp (cur_sp for the current one) to its top. Returns the pair count.
int platform_thread_stacks(uintptr_t* out, int max, uintptr_t cur_sp) {
  int k = 0;
  for (Coroutine* c : g_coroutines) {
    if (!c || !c->started || c->done || k >= max)
      continue;
    uintptr_t lo = c == g_current ? cur_sp : c->sp;
    if (lo < c->stackBase || lo > c->stackTop)
      continue;
    out[2 * k] = lo;
    out[2 * k + 1] = c->stackTop;
    k++;
  }
  return k;
}
