#include "chronicle/serialization.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
namespace chronicle {
namespace {
Error ioError(const std::filesystem::path& path, std::string message) {
    return {ErrorCode::io_error, 0, path.string(), std::move(message)};
}
struct TemporaryDirectory {
    std::filesystem::path path;
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};
} // namespace
Result<ByteBuffer> readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        return ioError(path, "cannot open file for reading");
    }
    const auto end = stream.tellg();
    if (end < 0) {
        return ioError(path, "cannot determine file size");
    }
    if (static_cast<std::uint64_t>(end) > maxFileBytes) {
        return Error{ErrorCode::limit_exceeded, 0, path.string(),
                     "file size limit exceeded"};
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    stream.seekg(0);
    if (!bytes.empty()) {
        stream.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream || stream.peek() != std::char_traits<char>::eof()) {
        return ioError(path, "file changed or failed during read");
    }
    return ByteBuffer(std::move(bytes));
}
Status writeFile(const std::filesystem::path& path,
                 std::span<const std::byte> bytes, bool replace) {
    if (bytes.size() > maxFileBytes) {
        return Error{ErrorCode::limit_exceeded, 0, path.string(),
                     "file size limit exceeded"};
    }
    static std::atomic<std::uint64_t> sequence{0};
    std::error_code error;
    auto parent = path.parent_path();
    if (parent.empty()) {
        parent = ".";
    }
    TemporaryDirectory temporary;
    bool created = false;
    for (unsigned attempt = 0; attempt < 64; ++attempt) {
        const auto stamp =
            std::chrono::steady_clock::now().time_since_epoch().count();
        temporary.path = parent / (".chronicle-" + std::to_string(stamp) + "-" +
                                   std::to_string(sequence.fetch_add(1)));
        created = std::filesystem::create_directory(temporary.path, error);
        if (created || error) {
            break;
        }
    }
    if (!created) {
        temporary.path.clear();
        return ioError(path,
                       "cannot create temporary directory: " + error.message());
    }
    const auto staging = temporary.path / "data";
    std::ofstream stream(staging, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return ioError(path, "cannot open temporary file");
    }
    if (!bytes.empty()) {
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    stream.flush();
    if (!stream) {
        return ioError(path, "failed to write temporary file");
    }
    stream.close();
    if (stream.fail()) {
        return ioError(path, "failed to close temporary file");
    }
    if (replace) {
        std::filesystem::rename(staging, path, error);
    } else {
        // Link installation is atomic and fails if the destination already
        // exists.
        std::filesystem::create_hard_link(staging, path, error);
    }
    if (error) {
        return ioError(path,
                       "cannot install completed file: " + error.message());
    }
    return std::monostate{};
}
Status saveScene(const std::filesystem::path& path, const Scene& scene,
                 std::uint16_t playerVersion) {
    auto bytes = serialize(scene, playerVersion);
    if (!bytes) {
        return bytes.error();
    }
    return writeFile(path, bytes.value().view());
}
Result<LoadedScene> loadScene(const std::filesystem::path& path) {
    auto bytes = readFile(path);
    if (!bytes) {
        return bytes.error();
    }
    return deserialize(bytes.value().view());
}
Status migrateFile(const std::filesystem::path& source,
                   const std::filesystem::path& destination,
                   std::uint16_t targetVersion) {
    std::error_code error;
    const auto sourceAbsolute =
        std::filesystem::absolute(source, error).lexically_normal();
    if (error) {
        return ioError(source, error.message());
    }
    const auto destinationAbsolute =
        std::filesystem::absolute(destination, error).lexically_normal();
    if (error) {
        return ioError(destination, error.message());
    }
    if (sourceAbsolute == destinationAbsolute) {
        return Error{ErrorCode::invalid_argument, 0, destination.string(),
                     "migration requires a new destination"};
    }
    auto original = readFile(source);
    if (!original) {
        return original.error();
    }
    auto bytes = migrateArchive(original.value().view(), targetVersion);
    if (!bytes) {
        return bytes.error();
    }
    return writeFile(destination, bytes.value().view(), false);
}
} // namespace chronicle
