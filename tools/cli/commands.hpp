#pragma once
#include "chronicle/serialization.hpp"
#include <iosfwd>
#include <span>
#include <string_view>
namespace chronicle::cli {
std::string toJson(const Scene& scene);
std::string inspect(const LoadedScene& scene);
std::string inspectRecord(const ObjectRecord& root,
                          const SchemaRegistry& registry);
std::string diff(const LoadedScene& before, const LoadedScene& after);
int errorExit(const Error& error);
int run(std::span<const std::string_view> args, std::ostream& out,
        std::ostream& err);
} // namespace chronicle::cli
