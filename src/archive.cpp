#include "chronicle/archive.hpp"
#include <array>
namespace chronicle {
namespace {
constexpr std::array<std::byte, 8> magic{
    std::byte{'C'}, std::byte{'H'}, std::byte{'R'}, std::byte{'N'},
    std::byte{'L'}, std::byte{'I'}, std::byte{'T'}, std::byte{'E'}};
}
ByteBuffer encodeArchive(TypeId rootType, std::span<const std::byte> payload) {
    BinaryWriter writer;
    writer.reserve(24 + payload.size());
    writer.writeBytes(magic);
    writer.writeU16(1);
    writer.writeU16(0);
    writer.writeU32(rootType);
    writer.writeU64(payload.size());
    writer.writeBytes(payload);
    return std::move(writer).take();
}
ObjectView decodeArchive(std::span<const std::byte> bytes,
                         TypeId expectedRoot) {
    if (bytes.size() > maxFileBytes) {
        fail(ErrorCode::limit_exceeded, 0, "header",
             "file size limit exceeded");
    }
    BinaryReader reader(bytes, 0, "header");
    auto inputMagic = reader.readBytes(magic.size());
    if (!std::equal(inputMagic.begin(), inputMagic.end(), magic.begin())) {
        fail(ErrorCode::invalid_format, 0, "header.magic", "expected CHRNLITE");
    }
    if (reader.readU16() != 1) {
        fail(ErrorCode::unsupported_version, 8, "header.version",
             "unsupported file format version");
    }
    if (reader.readU16() != 0) {
        fail(ErrorCode::invalid_format, 10, "header.reserved",
             "reserved bits must be zero");
    }
    if (reader.readU32() != expectedRoot) {
        fail(ErrorCode::invalid_format, 12, "header.root",
             "unexpected root type ID");
    }
    const auto size = reader.readU64();
    if (size > maxFileBytes - 24) {
        fail(ErrorCode::limit_exceeded, 16, "header.payload",
             "payload size limit exceeded");
    }
    if (size > reader.remaining()) {
        fail(ErrorCode::truncated, 24, "header.payload",
             "payload exceeds remaining input");
    }
    if (size != reader.remaining()) {
        fail(ErrorCode::invalid_format, 16, "header.payload",
             "payload size does not match file");
    }
    BinaryReader rootReader(reader.readBytes(static_cast<std::size_t>(size)),
                            24, "root");
    return decodeObjectView(rootReader);
}
} // namespace chronicle
