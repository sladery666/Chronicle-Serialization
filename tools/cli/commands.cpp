#include "commands.hpp"
#include <iomanip>
#include <ostream>
namespace chronicle::cli {
int errorExit(const Error& error) {
    switch (error.code) {
    case ErrorCode::io_error:
        return 3;
    case ErrorCode::unsupported_version:
        return 4;
    case ErrorCode::invalid_argument:
        return 64;
    default:
        return 2;
    }
}
int run(std::span<const std::string_view> args, std::ostream& out,
        std::ostream& err) {
    auto report = [&](const Error& error) {
        err << error.context << " at offset 0x" << std::hex << error.offset
            << std::dec << ": " << error.message << '\n';
        return errorExit(error);
    };
    if (args.size() == 1 && (args[0] == "--help" || args[0] == "-h")) {
        out << "chronicle inspect|validate|to-json FILE\nchronicle diff OLD "
               "NEW (exit 1 when different)\nchronicle migrate SOURCE NEW "
               "[--to-version 2|3] "
               "(destination must not exist)\n";
        return 0;
    }
    if (args.size() == 1 && args[0] == "--version") {
        out << "Chronicle Lite 1.0.0\n";
        return 0;
    }
    if (args.empty()) {
        err << "Use chronicle --help\n";
        return 64;
    }
    const auto command = args[0];
    const bool single =
        command == "inspect" || command == "validate" || command == "to-json";
    const bool pair = command == "diff" || command == "migrate";
    const bool migrationOption = command == "migrate" && args.size() == 5 &&
                                 args[3] == "--to-version" &&
                                 (args[4] == "2" || args[4] == "3");
    if ((!single && !pair) ||
        (args.size() != (single ? 2U : 3U) && !migrationOption)) {
        err << "Invalid command or argument count; use chronicle --help\n";
        return 64;
    }
    if (command == "migrate") {
        const std::uint16_t target = migrationOption && args[4] == "2" ? 2 : 3;
        auto status = migrateFile(std::filesystem::path(args[1]),
                                  std::filesystem::path(args[2]), target);
        if (!status) {
            return report(status.error());
        }
        out << "Migrated to Player V" << target << ": " << args[2] << '\n';
        return 0;
    }
    auto loaded = loadScene(std::filesystem::path(args[1]));
    if (!loaded) {
        return report(loaded.error());
    }
    if (command == "validate") {
        out << "Valid: format=1 Player V" << loaded.value().playerVersion
            << '\n';
    } else if (command == "inspect") {
        out << inspect(loaded.value());
    } else if (command == "to-json") {
        out << toJson(loaded.value().scene);
    } else {
        auto other = loadScene(std::filesystem::path(args[2]));
        if (!other) {
            return report(other.error());
        }
        const auto changes = diff(loaded.value(), other.value());
        out << (changes.empty() ? "No semantic differences\n" : changes);
        return changes.empty() ? 0 : 1;
    }
    return 0;
}
} // namespace chronicle::cli
