#pragma once
#include "chronicle/byte_buffer.hpp"
#include <cstdint>
#include <string_view>
#include <utility>
namespace chronicle {
inline constexpr std::size_t maxFileBytes = 64 * 1024 * 1024;
inline constexpr std::size_t maxStringBytes = 1024 * 1024;
inline constexpr std::uint32_t maxArrayElements = 100000;
class BinaryWriter {
  public:
    void reserve(std::size_t capacity);
    void writeU8(std::uint8_t value);
    void writeU16(std::uint16_t value);
    void writeU32(std::uint32_t value);
    void writeU64(std::uint64_t value);
    void writeI32(std::int32_t value);
    void writeI64(std::int64_t value);
    void writeFloat(float value);
    void writeDouble(double value);
    void writeString(std::string_view value);
    void writeBytes(std::span<const std::byte> bytes);
    std::span<const std::byte> view() const noexcept {
        return buffer_.view();
    }
    ByteBuffer take() && {
        return std::move(buffer_);
    }

  private:
    ByteBuffer buffer_;
};
} // namespace chronicle
