#include "chronicle/bound_serialization.hpp"
#include "commands.hpp"
#include "model.hpp"
#include <iostream>
int main(int argc, char** argv) {
    using namespace chronicle;
    using example::WorldSettings;
    if (argc != 3) {
        std::cerr << "type_binding_demo save|load|inspect FILE\n";
        return 64;
    }
    auto report = [](const Error& error) {
        std::cerr << error.context << " at offset " << error.offset << ": "
                  << error.message << '\n';
        return cli::errorExit(error);
    };
    const std::string_view command = argv[1];
    if (command == "save") {
        auto result = saveBound(argv[2], example::demoSettings());
        if (!result) {
            return report(result.error());
        }
        std::cout << "Saved WorldSettings schema=1\n";
        return 0;
    }
    if (command != "load" && command != "inspect") {
        return 64;
    }
    auto result = loadBound<WorldSettings>(argv[2]);
    if (!result) {
        return report(result.error());
    }
    if (command == "load") {
        const auto& settings = result.value().value;
        std::cout << "WorldSettings seed=" << settings.seed
                  << " difficulty=" << settings.difficulty
                  << " quest=" << settings.quest.title
                  << " progress=" << settings.quest.progress
                  << " checkpoints=" << settings.quest.checkpoints.size()
                  << " rewards=" << settings.rewards.size() << '\n';
    } else {
        SchemaRegistry registry;
        auto status = registerBinding<WorldSettings>(registry);
        if (!status) {
            return report(status.error());
        }
        std::cout << cli::inspectRecord(result.value().root, registry);
    }
    return 0;
}
