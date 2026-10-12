#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace chronicle {
struct Vec3 {
    float x = 0;
    float y = 0;
    float z = 0;
    bool operator==(const Vec3&) const = default;
};
struct Item {
    std::uint64_t id = 0;
    std::string name;
    bool operator==(const Item&) const = default;
};
struct Stats {
    std::int64_t health = 100;
    std::int64_t maxHealth = 100;
    bool operator==(const Stats&) const = default;
};
struct Player {
    std::uint64_t id = 0;
    std::string name = "Unknown";
    Stats stats;
    Vec3 position;
    std::vector<Item> inventory;
    bool operator==(const Player&) const = default;
};
struct Scene {
    std::string name;
    Player player;
    bool operator==(const Scene&) const = default;
};
inline Scene demoScene() {
    return {"Meadow",
            {42,
             "Ada",
             {100, 100},
             {1.5F, -2.0F, 3.25F},
             {{7, "Sword"}, {8, "Potion"}}}};
}
} // namespace chronicle
