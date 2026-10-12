#include "chronicle/binary_reader.hpp"
#include <array>
#include <bit>
#include <limits>
#include <type_traits>
namespace chronicle {
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
bool validUtf8(std::string_view value) noexcept {
    std::size_t i = 0;
    while (i < value.size()) {
        const auto first = static_cast<unsigned char>(value[i++]);
        if (first < 0x80) {
            continue;
        }
        unsigned count = 0;
        std::uint32_t code = 0;
        std::uint32_t minimum = 0;
        if (first >= 0xC2 && first <= 0xDF) {
            count = 1;
            code = first & 0x1FU;
            minimum = 0x80;
        } else if (first >= 0xE0 && first <= 0xEF) {
            count = 2;
            code = first & 0x0FU;
            minimum = 0x800;
        } else if (first >= 0xF0 && first <= 0xF4) {
            count = 3;
            code = first & 0x07U;
            minimum = 0x10000;
        } else {
            return false;
        }
        if (count > value.size() - i) {
            return false;
        }
        for (unsigned j = 0; j < count; ++j) {
            const auto next = static_cast<unsigned char>(value[i++]);
            if ((next & 0xC0U) != 0x80U) {
                return false;
            }
            code = (code << 6U) | (next & 0x3FU);
        }
        if (code < minimum || code > 0x10FFFF ||
            (code >= 0xD800 && code <= 0xDFFF)) {
            return false;
        }
    }
    return true;
}
void BinaryWriter::reserve(std::size_t capacity) {
    if (capacity > maxFileBytes) {
        fail(ErrorCode::limit_exceeded, buffer_.size(), "writer",
             "file size limit exceeded");
    }
    buffer_.reserve(capacity);
}
void BinaryWriter::writeBytes(std::span<const std::byte> bytes) {
    if (bytes.size() > maxFileBytes - buffer_.size()) {
        fail(ErrorCode::limit_exceeded, buffer_.size(), "writer",
             "file size limit exceeded");
    }
    buffer_.append(bytes);
}
namespace {
template <typename T> void writeUnsigned(BinaryWriter& writer, T value) {
    std::array<std::byte, sizeof(T)> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = std::byte(static_cast<std::uint8_t>(value >> (i * 8U)));
    }
    writer.writeBytes(bytes);
}
} // namespace
void BinaryWriter::writeU8(std::uint8_t value) {
    writeUnsigned(*this, value);
}
void BinaryWriter::writeU16(std::uint16_t value) {
    writeUnsigned(*this, value);
}
void BinaryWriter::writeU32(std::uint32_t value) {
    writeUnsigned(*this, value);
}
void BinaryWriter::writeU64(std::uint64_t value) {
    writeUnsigned(*this, value);
}
void BinaryWriter::writeI32(std::int32_t value) {
    writeU32(std::bit_cast<std::uint32_t>(value));
}
void BinaryWriter::writeI64(std::int64_t value) {
    writeU64(std::bit_cast<std::uint64_t>(value));
}
void BinaryWriter::writeFloat(float value) {
    writeU32(std::bit_cast<std::uint32_t>(value));
}
void BinaryWriter::writeDouble(double value) {
    writeU64(std::bit_cast<std::uint64_t>(value));
}
void BinaryWriter::writeString(std::string_view value) {
    if (value.size() > maxStringBytes) {
        fail(ErrorCode::limit_exceeded, buffer_.size(), "string",
             "string size limit exceeded");
    }
    if (!validUtf8(value)) {
        fail(ErrorCode::invalid_format, buffer_.size(), "string",
             "invalid UTF-8");
    }
    writeU32(static_cast<std::uint32_t>(value.size()));
    writeBytes(std::as_bytes(std::span(value.data(), value.size())));
}
std::span<const std::byte> BinaryReader::readBytes(std::size_t count) {
    if (count > remaining()) {
        fail(ErrorCode::truncated, offset(), context_,
             "requested bytes exceed remaining input");
    }
    auto result = bytes_.subspan(cursor_, count);
    cursor_ += count;
    return result;
}
std::uint8_t BinaryReader::readU8() {
    return std::to_integer<std::uint8_t>(readBytes(1)[0]);
}
std::uint16_t BinaryReader::readU16() {
    const auto bytes = readBytes(2);
    std::uint16_t result = 0;
    for (unsigned i = 0; i < 2; ++i) {
        result = static_cast<std::uint16_t>(
            result | (std::to_integer<std::uint16_t>(bytes[i]) << (i * 8U)));
    }
    return result;
}
std::uint32_t BinaryReader::readU32() {
    const auto bytes = readBytes(4);
    std::uint32_t result = 0;
    for (unsigned i = 0; i < 4; ++i) {
        result |= std::to_integer<std::uint32_t>(bytes[i]) << (i * 8U);
    }
    return result;
}
std::uint64_t BinaryReader::readU64() {
    const auto bytes = readBytes(8);
    std::uint64_t result = 0;
    for (unsigned i = 0; i < 8; ++i) {
        result |= std::to_integer<std::uint64_t>(bytes[i]) << (i * 8U);
    }
    return result;
}
std::int32_t BinaryReader::readI32() {
    return std::bit_cast<std::int32_t>(readU32());
}
std::int64_t BinaryReader::readI64() {
    return std::bit_cast<std::int64_t>(readU64());
}
float BinaryReader::readFloat() {
    return std::bit_cast<float>(readU32());
}
double BinaryReader::readDouble() {
    return std::bit_cast<double>(readU64());
}
std::string BinaryReader::readString() {
    const auto length = readU32();
    if (length > maxStringBytes) {
        fail(ErrorCode::limit_exceeded, offset() - 4, context_,
             "string size limit exceeded");
    }
    auto bytes = readBytes(length);
    std::string result;
    if (!bytes.empty()) {
        result.assign(reinterpret_cast<const char*>(bytes.data()),
                      bytes.size());
    }
    if (!validUtf8(result)) {
        fail(ErrorCode::invalid_format, offset() - length, context_,
             "invalid UTF-8");
    }
    return result;
}
void BinaryReader::requireEnd() const {
    if (remaining() != 0) {
        fail(ErrorCode::invalid_format, offset(), context_,
             "unexpected trailing bytes");
    }
}
} // namespace chronicle
