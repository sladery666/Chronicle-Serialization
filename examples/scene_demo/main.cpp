#include "chronicle/serialization.hpp"
#include <iostream>
int main(int argc, char** argv) {
    using namespace chronicle;
    if (argc < 3 || argc > 4) {
        std::cerr << "scene_demo save FILE [1|2|3]\nscene_demo load FILE\n";
        return 64;
    }
    const std::string_view command = argv[1];
    if (command == "save") {
        if (argc == 4 && std::string_view(argv[3]) != "1" &&
            std::string_view(argv[3]) != "2" &&
            std::string_view(argv[3]) != "3") {
            return 64;
        }
        const std::uint16_t version =
            argc == 4 ? static_cast<std::uint16_t>(argv[3][0] - '0') : 3;
        const auto result = saveScene(argv[2], demoScene(), version);
        if (!result) {
            std::cerr << result.error().message << '\n';
            return 2;
        }
        std::cout << "Saved Player V" << version << '\n';
        return 0;
    }
    if (command == "load" && argc == 3) {
        auto result = loadScene(argv[2]);
        if (!result) {
            std::cerr << result.error().context << ": "
                      << result.error().message << '\n';
            return 2;
        }
        const auto& scene = result.value().scene;
        std::cout << scene.name << ": " << scene.player.name
                  << ", health=" << scene.player.stats.health
                  << ", maxHealth=" << scene.player.stats.maxHealth
                  << ", inventory=" << scene.player.inventory.size() << '\n';
        return 0;
    }
    return 64;
}
