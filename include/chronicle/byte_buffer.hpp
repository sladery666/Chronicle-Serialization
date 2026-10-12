#pragma once
#include <cstddef>
#include <functional>
#include <span>
#include <utility>
#include <vector>
namespace chronicle {
class ByteBuffer {
  public:
    ByteBuffer() = default;
    explicit ByteBuffer(std::vector<std::byte> bytes)
        : bytes_(std::move(bytes)) {}
    void reserve(std::size_t capacity) {
        bytes_.reserve(capacity);
    }
    std::size_t size() const noexcept {
        return bytes_.size();
    }
    bool empty() const noexcept {
        return bytes_.empty();
    }
    std::span<const std::byte> view() const noexcept {
        return bytes_;
    }
    void append(std::span<const std::byte> bytes) {
        if (bytes.empty()) {
            return;
        }
        const auto less = std::less<const std::byte*>{};
        const bool aliases = !bytes_.empty() &&
                             !less(bytes.data(), bytes_.data()) &&
                             less(bytes.data(), bytes_.data() + bytes_.size());
        if (aliases) {
            // Snapshot only self-append; insertion may invalidate its source.
            const std::vector<std::byte> copy(bytes.begin(), bytes.end());
            bytes_.insert(bytes_.end(), copy.begin(), copy.end());
        } else {
            bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
        }
    }
    void push(std::byte byte) {
        bytes_.push_back(byte);
    }

  private:
    std::vector<std::byte> bytes_;
};
} // namespace chronicle
