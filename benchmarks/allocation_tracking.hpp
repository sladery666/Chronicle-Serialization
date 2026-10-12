#pragma once
#include <cstddef>
namespace chronicle::benchmark {
struct AllocationStats {
    std::size_t count = 0;
    std::size_t totalBytes = 0;
    std::size_t liveBytes = 0;
    std::size_t peakBytes = 0;
};
// Tracks requested C++ allocation bytes, not allocator overhead or process RSS.
void beginAllocationTracking() noexcept;
AllocationStats endAllocationTracking() noexcept;
} // namespace chronicle::benchmark
