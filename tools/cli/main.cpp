#include "commands.hpp"
#include <iostream>
#include <vector>
int main(int argc, char** argv) {
    try {
        std::vector<std::string_view> args;
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }
        return chronicle::cli::run(args, std::cout, std::cerr);
    } catch (const std::exception& error) {
        std::cerr << "Unexpected failure: " << error.what() << '\n';
        return 2;
    }
}
