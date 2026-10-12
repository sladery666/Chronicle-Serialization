#include "chronicle/migration.hpp"
#include "chronicle/result.hpp"
#include <algorithm>
#include <limits>
#include <utility>
namespace chronicle {
std::int64_t inferredMaxHealth(std::int64_t health) noexcept {
    return std::max(std::int64_t{100}, health);
}
PlayerV2 migratePlayerV1ToV2(PlayerV1 player) {
    return {player.id, "Unknown", static_cast<std::int64_t>(player.health),
            player.position, std::move(player.inventory)};
}
Player migratePlayerV2ToV3(PlayerV2 player) {
    return {player.id,
            std::move(player.name),
            {player.health, inferredMaxHealth(player.health)},
            player.position,
            std::move(player.inventory)};
}
Player migratePlayer(PlayerV1 player) {
    return migratePlayerV2ToV3(migratePlayerV1ToV2(std::move(player)));
}
SceneV2 migrateSceneV1ToV2(SceneV1 scene) {
    return {std::move(scene.name),
            migratePlayerV1ToV2(std::move(scene.player))};
}
Scene migrateSceneV2ToV3(SceneV2 scene) {
    return {std::move(scene.name),
            migratePlayerV2ToV3(std::move(scene.player))};
}
SceneV2 projectSceneV2(const Scene& scene) {
    if (scene.player.stats.maxHealth !=
        inferredMaxHealth(scene.player.stats.health)) {
        fail(ErrorCode::invalid_argument, 0, "Player.stats.maxHealth",
             "legacy save would lose custom maxHealth");
    }
    return {scene.name,
            {scene.player.id, scene.player.name, scene.player.stats.health,
             scene.player.position, scene.player.inventory}};
}
SceneV1 projectSceneV1(const Scene& scene) {
    const auto health = scene.player.stats.health;
    auto v2 = projectSceneV2(scene);
    if (health < INT32_MIN || health > INT32_MAX) {
        fail(ErrorCode::invalid_argument, 0, "Player.health",
             "V1 value does not fit int32");
    }
    return {std::move(v2.name),
            {v2.player.id, static_cast<std::int32_t>(health),
             v2.player.position, std::move(v2.player.inventory)}};
}
} // namespace chronicle
