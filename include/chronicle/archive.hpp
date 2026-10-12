#pragma once
#include "chronicle/schema.hpp"
#include <filesystem>
namespace chronicle {
ByteBuffer encodeArchive(TypeId rootType, std::span<const std::byte> payload);
// Returned view borrows bytes; callers must own or convert it before returning.
ObjectView decodeArchive(std::span<const std::byte> bytes, TypeId expectedRoot);
Result<ByteBuffer> readFile(const std::filesystem::path& path);
Status writeFile(const std::filesystem::path& path,
                 std::span<const std::byte> bytes, bool replace = true);
} // namespace chronicle
