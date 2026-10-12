#pragma once
#include "chronicle/scene_bindings.hpp"
namespace chronicle::example {
struct QuestState {
    std::uint64_t id = 0;
    std::string title = "Untitled";
    std::int32_t progress = 0;
    std::vector<float> checkpoints;
    bool operator==(const QuestState&) const = default;
};
struct WorldSettings {
    std::uint64_t seed = 0;
    std::string difficulty = "Normal";
    QuestState quest;
    std::vector<Item> rewards;
    bool operator==(const WorldSettings&) const = default;
};
inline WorldSettings demoSettings() {
    return {20261012,
            "Hard",
            {7, "Find the archive", 3, {0.25F, 0.5F}},
            {{8, "Potion"}}};
}
} // namespace chronicle::example
namespace chronicle {
template <> struct TypeBinding<example::QuestState> {
    static constexpr TypeId typeId = 10;
    static constexpr std::string_view name = "QuestState";
    static constexpr std::uint16_t minVersion = 1, maxVersion = 1;
    static constexpr auto fields() {
        return std::tuple{
            bindField(1, "id", &example::QuestState::id),
            bindField(2, "title", &example::QuestState::title),
            bindField(3, "progress", &example::QuestState::progress),
            bindField(4, "checkpoints", &example::QuestState::checkpoints)};
    }
};
template <> struct TypeBinding<example::WorldSettings> {
    static constexpr TypeId typeId = 11;
    static constexpr std::string_view name = "WorldSettings";
    static constexpr std::uint16_t minVersion = 1, maxVersion = 1;
    static constexpr auto fields() {
        // Deliberately not in ID order: output still uses canonical field
        // order.
        return std::tuple{
            bindField(4, "rewards", &example::WorldSettings::rewards),
            bindField(2, "difficulty", &example::WorldSettings::difficulty),
            bindField(3, "quest", &example::WorldSettings::quest),
            bindField(1, "seed", &example::WorldSettings::seed)};
    }
};
} // namespace chronicle
