#include "allocation_tracking.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
namespace {
struct alignas(std::max_align_t) Header {
    void* raw;
    std::size_t bytes;
    std::uint64_t generation;
};
thread_local bool tracking = false;
thread_local std::uint64_t generation = 0;
thread_local chronicle::benchmark::AllocationStats stats;
void* allocate(std::size_t bytes, std::size_t alignment) {
    bytes = std::max(bytes, std::size_t{1});
    alignment = std::max(alignment, alignof(Header));
    const auto overhead = sizeof(Header) + alignment - 1;
    if (bytes > std::numeric_limits<std::size_t>::max() - overhead) {
        throw std::bad_alloc();
    }
    auto* raw = std::malloc(bytes + overhead);
    if (!raw) {
        throw std::bad_alloc();
    }
    void* aligned = static_cast<std::byte*>(raw) + sizeof(Header);
    auto space = bytes + alignment - 1;
    if (!std::align(alignment, bytes, aligned, space)) {
        std::free(raw);
        throw std::bad_alloc();
    }
    auto* header = reinterpret_cast<Header*>(static_cast<std::byte*>(aligned) -
                                             sizeof(Header));
    new (header) Header{raw, bytes, tracking ? generation : 0};
    if (tracking) {
        ++stats.count;
        stats.totalBytes += bytes;
        stats.liveBytes += bytes;
        stats.peakBytes = std::max(stats.peakBytes, stats.liveBytes);
    }
    return aligned;
}
void release(void* pointer) noexcept {
    if (!pointer) {
        return;
    }
    auto* header = reinterpret_cast<Header*>(static_cast<std::byte*>(pointer) -
                                             sizeof(Header));
    if (tracking && header->generation == generation) {
        stats.liveBytes -= header->bytes;
    }
    std::free(header->raw);
}
} // namespace
namespace chronicle::benchmark {
void beginAllocationTracking() noexcept {
    ++generation;
    stats = {};
    tracking = true;
}
AllocationStats endAllocationTracking() noexcept {
    tracking = false;
    return stats;
}
} // namespace chronicle::benchmark
void* operator new(std::size_t bytes) {
    return allocate(bytes, alignof(std::max_align_t));
}
void* operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}
void operator delete(void* pointer) noexcept {
    release(pointer);
}
void operator delete[](void* pointer) noexcept {
    release(pointer);
}
void operator delete(void* pointer, std::size_t) noexcept {
    release(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept {
    release(pointer);
}
void* operator new(std::size_t bytes, std::align_val_t alignment) {
    return allocate(bytes, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t bytes, std::align_val_t alignment) {
    return ::operator new(bytes, alignment);
}
void operator delete(void* pointer, std::align_val_t) noexcept {
    release(pointer);
}
void operator delete[](void* pointer, std::align_val_t) noexcept {
    release(pointer);
}
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept {
    release(pointer);
}
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept {
    release(pointer);
}
void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(bytes);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t bytes, const std::nothrow_t&) noexcept {
    try {
        return ::operator new[](bytes);
    } catch (...) {
        return nullptr;
    }
}
void operator delete(void* pointer, const std::nothrow_t&) noexcept {
    release(pointer);
}
void operator delete[](void* pointer, const std::nothrow_t&) noexcept {
    release(pointer);
}
void* operator new(std::size_t bytes, std::align_val_t alignment,
                   const std::nothrow_t&) noexcept {
    try {
        return ::operator new(bytes, alignment);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t bytes, std::align_val_t alignment,
                     const std::nothrow_t&) noexcept {
    try {
        return ::operator new[](bytes, alignment);
    } catch (...) {
        return nullptr;
    }
}
void operator delete(void* pointer, std::align_val_t,
                     const std::nothrow_t&) noexcept {
    release(pointer);
}
void operator delete[](void* pointer, std::align_val_t,
                       const std::nothrow_t&) noexcept {
    release(pointer);
}
