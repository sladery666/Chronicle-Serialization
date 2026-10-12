#pragma once
#include "chronicle/migration.hpp"
#include "chronicle/type_binding.hpp"
namespace chronicle {
template <> struct TypeBinding<Vec3> {
    static constexpr TypeId typeId = vec3Type;
    static constexpr std::string_view name = "Vec3";
    static constexpr std::uint16_t minVersion = 1, maxVersion = 1;
    static constexpr auto fields() {
        return std::tuple{bindField(1, "x", &Vec3::x),
                          bindField(2, "y", &Vec3::y),
                          bindField(3, "z", &Vec3::z)};
    }
};
template <> struct TypeBinding<Item> {
    static constexpr TypeId typeId = itemType;
    static constexpr std::string_view name = "Item";
    static constexpr std::uint16_t minVersion = 1, maxVersion = 1;
    static constexpr auto fields() {
        return std::tuple{bindField(1, "id", &Item::id),
                          bindField(2, "name", &Item::name)};
    }
};
template <> struct TypeBinding<Stats> {
    static constexpr TypeId typeId = statsType;
    static constexpr std::string_view name = "Stats";
    static constexpr std::uint16_t minVersion = 1, maxVersion = 1;
    static constexpr auto fields() {
        return std::tuple{bindField(1, "health", &Stats::health),
                          bindField(2, "maxHealth", &Stats::maxHealth)};
    }
    static void validate(const Stats& stats) {
        if (stats.maxHealth <= 0 || stats.health > stats.maxHealth) {
            fail(ErrorCode::invalid_format, 0, "Player.stats",
                 "maxHealth must be positive and health must not exceed "
                 "maxHealth");
        }
    }
};
template <> struct TypeBinding<PlayerV1> {
    static constexpr TypeId typeId = playerType;
    static constexpr std::string_view name = "Player";
    static constexpr std::uint16_t minVersion = 1, maxVersion = 1;
    static constexpr auto fields() {
        return std::tuple{bindField(1, "id", &PlayerV1::id),
                          bindField(3, "health", &PlayerV1::health),
                          bindField(4, "position", &PlayerV1::position),
                          bindField(5, "inventory", &PlayerV1::inventory)};
    }
};
template <> struct TypeBinding<PlayerV2> {
    static constexpr TypeId typeId = playerType;
    static constexpr std::string_view name = "Player";
    static constexpr std::uint16_t minVersion = 2, maxVersion = 2;
    static constexpr auto fields() {
        return std::tuple{bindField(1, "id", &PlayerV2::id),
                          bindField(2, "name", &PlayerV2::name),
                          bindField(3, "health", &PlayerV2::health),
                          bindField(4, "position", &PlayerV2::position),
                          bindField(5, "inventory", &PlayerV2::inventory)};
    }
};
template <> struct TypeBinding<Player> {
    static constexpr TypeId typeId = playerType;
    static constexpr std::string_view name = "Player";
    static constexpr std::uint16_t minVersion = 3, maxVersion = 3;
    static std::uint16_t version(const BindingOptions& options) {
        return options.playerVersion;
    }
    static constexpr auto fields() {
        return std::tuple{bindField(1, "id", &Player::id),
                          bindField(2, "name", &Player::name),
                          bindField(4, "position", &Player::position),
                          bindField(5, "inventory", &Player::inventory),
                          bindField(6, "stats", &Player::stats)};
    }
};
template <> struct TypeBinding<Scene> {
    static constexpr TypeId typeId = sceneType;
    static constexpr std::string_view name = "Scene";
    static constexpr std::uint16_t minVersion = 1, maxVersion = 1;
    static constexpr auto fields() {
        return std::tuple{bindField(1, "name", &Scene::name),
                          bindField(2, "player", &Scene::player)};
    }
};
template <> struct TypeBinding<SceneV1> {
    static constexpr TypeId typeId = sceneType;
    static constexpr std::string_view name = "Scene";
    static constexpr std::uint16_t minVersion = 1, maxVersion = 1;
    static constexpr auto fields() {
        return std::tuple{bindField(1, "name", &SceneV1::name),
                          bindField(2, "player", &SceneV1::player)};
    }
};
template <> struct TypeBinding<SceneV2> {
    static constexpr TypeId typeId = sceneType;
    static constexpr std::string_view name = "Scene";
    static constexpr std::uint16_t minVersion = 1, maxVersion = 1;
    static constexpr auto fields() {
        return std::tuple{bindField(1, "name", &SceneV2::name),
                          bindField(2, "player", &SceneV2::player)};
    }
};
} // namespace chronicle
