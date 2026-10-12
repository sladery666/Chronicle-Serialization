#pragma once
#include "chronicle/serialization.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
namespace chronicle::test {
inline std::filesystem::path golden(std::string_view name) {
    return std::filesystem::path(CHRONICLE_GOLDEN_DIR) / name;
}
class TempDirectory {
  public:
    TempDirectory() {
        static std::atomic<unsigned> sequence{0};
        for (;;) {
            path = std::filesystem::temp_directory_path() /
                   ("chronicle-test-" +
                    std::to_string(std::chrono::steady_clock::now()
                                       .time_since_epoch()
                                       .count()) +
                    "-" + std::to_string(sequence.fetch_add(1)));
            if (std::filesystem::create_directory(path)) {
                break;
            }
        }
    }
    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    std::filesystem::path path;
};
inline std::vector<std::byte> vectorBytes(std::span<const std::byte> bytes) {
    return {bytes.begin(), bytes.end()};
}
inline void put(std::vector<std::byte>& bytes, std::size_t offset,
                std::uint64_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i) {
        bytes.at(offset + i) =
            std::byte(static_cast<std::uint8_t>(value >> (i * 8U)));
    }
}
inline ByteBuffer wrap(ObjectRecord root) {
    auto payload = encodeObject(std::move(root));
    BinaryWriter writer;
    writer.writeBytes(std::as_bytes(std::span("CHRNLITE", 8)));
    writer.writeU16(1);
    writer.writeU16(0);
    writer.writeU32(sceneType);
    writer.writeU64(payload.size());
    writer.writeBytes(payload.view());
    return std::move(writer).take();
}
inline ObjectRecord playerRecord(const LoadedScene& scene) {
    for (const auto& field : scene.root.fields) {
        if (field.id == 2) {
            BinaryReader reader(field.payload.view(), field.offset);
            return decodeObject(reader);
        }
    }
    throw std::runtime_error("no player");
}
inline ByteBuffer replacePlayer(LoadedScene scene, ObjectRecord player) {
    for (auto& field : scene.root.fields) {
        if (field.id == 2) {
            field.payload = encodeObject(std::move(player));
            break;
        }
    }
    return wrap(std::move(scene.root));
}
} // namespace chronicle::test
