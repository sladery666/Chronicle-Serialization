#include "allocation_tracking.hpp"
#include "chronicle/serialization.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>
namespace {
using namespace chronicle;
using namespace chronicle::benchmark;
using Clock = std::chrono::steady_clock;
constexpr unsigned samples = 31;
struct Measurement {
    double median = 0;
    AllocationStats allocations;
};
template <typename F> Measurement measure(F operation) {
    // Separate runs keep counter updates out of the timed allocation path.
    operation();
    std::array<double, samples> times{};
    for (auto& time : times) {
        const auto start = Clock::now();
        operation();
        time = std::chrono::duration<double, std::milli>(Clock::now() - start)
                   .count();
    }
    std::sort(times.begin(), times.end());
    beginAllocationTracking();
    try {
        operation();
    } catch (...) {
        endAllocationTracking();
        throw;
    }
    return {times[samples / 2], endAllocationTracking()};
}
void report(std::string_view name, const Measurement& result) {
    std::cout << name << "_median_ms=" << result.median << '\n'
              << name << "_allocations=" << result.allocations.count << '\n'
              << name << "_requested_bytes=" << result.allocations.totalBytes
              << '\n'
              << name << "_peak_live_bytes=" << result.allocations.peakBytes
              << '\n';
    if (result.allocations.liveBytes != 0) {
        throw std::runtime_error("operation retained tracked allocations");
    }
}
void run(std::size_t count, const std::filesystem::path& file) {
    auto scene = demoScene();
    scene.player.inventory.clear();
    scene.player.inventory.reserve(count);
    std::mt19937 random(20261011);
    for (std::size_t i = 0; i < count; ++i) {
        scene.player.inventory.push_back({static_cast<std::uint64_t>(i),
                                          "item-" + std::to_string(random())});
    }
    auto bytes = serialize(scene);
    if (!bytes) {
        throw Failure(bytes.error());
    }
    const auto encode = measure([&] {
        auto result = serialize(scene);
        if (!result) {
            throw Failure(result.error());
        }
        if (result.value().size() != bytes.value().size() ||
            !std::equal(result.value().view().begin(),
                        result.value().view().end(),
                        bytes.value().view().begin())) {
            throw std::runtime_error("non-deterministic encoding");
        }
    });
    const auto decode = measure([&] {
        auto result = deserialize(bytes.value().view());
        if (!result) {
            throw Failure(result.error());
        }
        if (result.value().scene != scene) {
            throw std::runtime_error("round-trip mismatch");
        }
    });
    const auto save = measure([&] {
        auto result = saveScene(file, scene);
        if (!result) {
            throw Failure(result.error());
        }
    });
    const auto load = measure([&] {
        auto result = loadScene(file);
        if (!result) {
            throw Failure(result.error());
        }
        if (result.value().scene != scene) {
            throw std::runtime_error("file round-trip mismatch");
        }
    });
    std::cout << "seed=20261011 items=" << count << " samples=" << samples
              << " bytes=" << bytes.value().size() << '\n';
    report("encode", encode);
    report("decode", decode);
    report("save", save);
    report("load", load);
}
bool checkAllocations() {
    void* existing = ::operator new(16);
    beginAllocationTracking();
    ::operator delete(existing);
    void* a = ::operator new(16);
    void* b = ::operator new(32, std::align_val_t{64});
    ::operator delete(a);
    const auto first = endAllocationTracking();
    beginAllocationTracking();
    ::operator delete(b, std::align_val_t{64});
    const auto second = endAllocationTracking();
    return first.count == 2 && first.totalBytes == 48 &&
           first.peakBytes == 48 && first.liveBytes == 32 &&
           second.count == 0 && second.liveBytes == 0;
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--check-allocations") {
            return checkAllocations() ? 0 : 1;
        }
        const bool suite = argc == 2 && std::string_view(argv[1]) == "--suite";
        if (argc > 2 || (argc == 2 && !suite)) {
            std::cerr << "chronicle_benchmark [--suite|--check-allocations]\n";
            return 64;
        }
        struct Scratch {
            std::filesystem::path path;
            ~Scratch() {
                std::error_code error;
                std::filesystem::remove_all(path, error);
            }
        } scratch;
        const auto candidate =
            std::filesystem::temp_directory_path() /
            ("chronicle-benchmark-" +
             std::to_string(Clock::now().time_since_epoch().count()));
        if (!std::filesystem::create_directory(candidate)) {
            return 1;
        }
        scratch.path = candidate;
#if defined(__clang__)
        std::cout << "compiler=Clang " << __clang_version__ << '\n';
#elif defined(__GNUC__)
        std::cout << "compiler=GCC " << __VERSION__ << '\n';
#else
        std::cout << "compiler=MSVC\n";
#endif
#ifdef NDEBUG
        std::cout << "build=Release\n";
#else
        std::cout << "build=Debug\n";
#endif
        std::cout << "hardware_threads=" << std::thread::hardware_concurrency()
                  << "\nfile_io=warm_cache\nmemory_metric=operator_new_"
                     "requested_bytes\nmeasurement_scope=operation_including_"
                     "result_destruction_and_validation\n";
        if (suite) {
            for (const auto count : {10U, 1000U, 10000U, 100000U}) {
                run(count, scratch.path / "scene.bin");
            }
        } else {
            run(10000, scratch.path / "scene.bin");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
