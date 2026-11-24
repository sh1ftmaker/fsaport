/* Port: AddressSanitizer view of the JKR heaps (cmake -DFSA_SANITIZE=address).
 * The game carves its heaps out of one big MEM1 allocation, so ASan by itself
 * only sees that outer block. The heap implementations (JKRExpHeap,
 * JKRSolidHeap, JKRUnitHeap; built without -fsanitize=address so they may
 * touch their own block headers) poison everything they manage and unpoison
 * exactly the bytes each alloc() asked for: game code then gets
 * overflow-past-request and use-after-free/stale-pointer checks on every JKR
 * block. Not game code; empty unless FSA_PORT_ASAN. */
#ifndef FSA_PORT_ASAN_H
#define FSA_PORT_ASAN_H
#ifdef FSA_PORT_ASAN
#ifdef __cplusplus
extern "C" {
#endif
void __asan_poison_memory_region(void const volatile *addr, unsigned long size);
void __asan_unpoison_memory_region(void const volatile *addr, unsigned long size);
#ifdef __cplusplus
}
#endif
#define FSA_ASAN_POISON(p, n) __asan_poison_memory_region((p), (unsigned long)(n))
#define FSA_ASAN_UNPOISON(p, n) __asan_unpoison_memory_region((p), (unsigned long)(n))
#else
#define FSA_ASAN_POISON(p, n) ((void)0)
#define FSA_ASAN_UNPOISON(p, n) ((void)0)
#endif
#endif
