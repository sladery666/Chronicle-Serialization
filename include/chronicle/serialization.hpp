#pragma once
#include "chronicle/bound_serialization.hpp"
#include "chronicle/scene_bindings.hpp"
#include "chronicle/schema.hpp"
#include <filesystem>
namespace chronicle {
struct LoadedScene {
    Scene scene;
    std::uint16_t playerVersion;
    ObjectRecord root;
};
Result<ByteBuffer> serialize(const Scene& scene,
                             std::uint16_t playerVersion = 3);
Result<LoadedScene> deserialize(std::span<const std::byte> bytes);
Status saveScene(const std::filesystem::path& path, const Scene& scene,
                 std::uint16_t playerVersion = 3);
Result<LoadedScene> loadScene(const std::filesystem::path& path);
Status migrateFile(const std::filesystem::path& source,
                   const std::filesystem::path& destination,
                   std::uint16_t targetVersion = 3);
Result<ByteBuffer> migrateArchive(std::span<const std::byte> bytes,
                                  std::uint16_t targetVersion = 3);
} // namespace chronicle
