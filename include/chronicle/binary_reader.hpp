#pragma once
#include "chronicle/binary_writer.hpp"
#include "chronicle/result.hpp"
#include <string>
namespace chronicle {
class BinaryReader {
  public:
    explicit BinaryReader(std::span<const std::byte> bytes,
                          std::size_t base = 0, std::string context = "binary")
        : bytes_(bytes), base_(base), context_(std::move(context)) {}
    std::uint8_t readU8();
    std::uint16_t readU16();
    std::uint32_t readU32();
    std::uint64_t readU64();
    std::int32_t readI32();
    std::int64_t readI64();
    float readFloat();
    double readDouble();
    std::string readString();
    std::span<const std::byte> readBytes(std::size_t count);
    std::size_t remaining() const noexcept {
        return bytes_.size() - cursor_;
    }
    std::size_t offset() const noexcept {
        return base_ + cursor_;
    }
    void requireEnd() const;

  private:
    std::span<const std::byte> bytes_;
    std::size_t cursor_ = 0;
    std::size_t base_;
    std::string context_;
};
bool validUtf8(std::string_view value) noexcept;
} // namespace chronicle
