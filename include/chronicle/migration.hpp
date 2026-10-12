#pragma once
#include "chronicle/scene.hpp"
namespace chronicle {
struct PlayerV1 {
    std::uint64_t id = 0;
    std::int32_t health = 100;
    Vec3 position;
    std::vector<Item> inventory;
    bool operator==(const PlayerV1&) const = default;
};
struct PlayerV2 {
    std::uint64_t id = 0;
    std::string name = "Unknown";
    std::int64_t health = 100;
    Vec3 position;
    std::vector<Item> inventory;
    bool operator==(const PlayerV2&) const = default;
};
struct SceneV1 {
    std::string name;
    PlayerV1 player;
    bool operator==(const SceneV1&) const = default;
};
struct SceneV2 {
    std::string name;
    PlayerV2 player;
    bool operator==(const SceneV2&) const = default;
};
std::int64_t inferredMaxHealth(std::int64_t health) noexcept;
PlayerV2 migratePlayerV1ToV2(PlayerV1 player);
Player migratePlayerV2ToV3(PlayerV2 player);
SceneV2 migrateSceneV1ToV2(SceneV1 scene);
Scene migrateSceneV2ToV3(SceneV2 scene);
// Existing name now returns the current V3 Player through both explicit edges.
Player migratePlayer(PlayerV1 player);
SceneV2 projectSceneV2(const Scene& scene);
SceneV1 projectSceneV1(const Scene& scene);
} // namespace chronicle
