#pragma once
// fsaport: the game's operator new (JKRHeap.cpp) puts every allocation made
// while fsa_host_alloc_depth == 0 into the game's JKR heaps (MEM1). Aurora is
// called from game code, so its own allocations would land there too; netplay's
// rollback restores MEM1 and would roll them back under Aurora's feet
// (docs/NETPLAY.md). Entry points that may allocate hold one of these.
extern "C" int fsa_host_alloc_depth;
namespace aurora {
struct HostAllocScope {
  HostAllocScope() noexcept { ++fsa_host_alloc_depth; }
  ~HostAllocScope() { --fsa_host_alloc_depth; }
  HostAllocScope(const HostAllocScope&) = delete;
  HostAllocScope& operator=(const HostAllocScope&) = delete;
};
} // namespace aurora
#define AURORA_HOST_ALLOC ::aurora::HostAllocScope auroraHostAlloc_
