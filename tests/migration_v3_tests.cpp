#include "commands.hpp"
#include "support.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
namespace chronicle::test {
ByteBuffer wrapStats(ObjectRecord stats) {
    auto loaded = loadScene(golden("scene_v3.bin"));
    if (!loaded) {
        throw Failure(loaded.error());
    }
    auto player = playerRecord(loaded.value());
    for (auto& f : player.fields) {
        if (f.id == 6) {
            f.payload = encodeObject(std::move(stats));
            break;
        }
    }
    return replacePlayer(loaded.value(), std::move(player));
}
ObjectRecord statsRecord(const LoadedScene& scene) {
    auto player = playerRecord(scene);
    for (const auto& f : player.fields) {
        if (f.id == 6) {
            BinaryReader reader(f.payload.view());
            return decodeObject(reader);
        }
    }
    throw std::runtime_error("missing Stats");
}
TEST(V3, DefaultRoundTripAndIndependentGolden) {
    auto bytes = serialize(demoScene());
    auto fixture = readFile(golden("scene_v3.bin"));
    ASSERT_TRUE(bytes);
    ASSERT_TRUE(fixture);
    EXPECT_EQ(vectorBytes(bytes.value().view()),
              vectorBytes(fixture.value().view()));
    auto loaded = deserialize(bytes.value().view());
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.value().playerVersion, 3);
    EXPECT_EQ(loaded.value().scene, demoScene());
    auto player = playerRecord(loaded.value());
    EXPECT_EQ(std::count_if(player.fields.begin(), player.fields.end(),
                            [](const auto& f) { return f.id == 3; }),
              0);
    EXPECT_EQ(std::count_if(player.fields.begin(), player.fields.end(),
                            [](const auto& f) {
                                return f.id == 6 && f.wire == WireType::object;
                            }),
              1);
    EXPECT_EQ(statsRecord(loaded.value()).type, statsType);
}
TEST(V3, LegacyGoldenLoadingAndWritingRemainCompatible) {
    for (auto version : {std::uint16_t{1}, std::uint16_t{2}}) {
        auto bytes = serialize(demoScene(), version);
        auto fixture =
            readFile(golden(version == 1 ? "scene_v1.bin" : "scene_v2.bin"));
        ASSERT_TRUE(bytes);
        ASSERT_TRUE(fixture);
        EXPECT_EQ(vectorBytes(bytes.value().view()),
                  vectorBytes(fixture.value().view()));
        auto loaded = deserialize(fixture.value().view());
        ASSERT_TRUE(loaded);
        EXPECT_EQ(loaded.value().playerVersion, version);
        EXPECT_EQ(loaded.value().scene.player.stats, (Stats{100, 100}));
    }
}
TEST(V3, ExplicitEdgesPreserveSignedBoundariesAndName) {
    PlayerV1 v1;
    v1.health = INT32_MIN;
    auto v2 = migratePlayerV1ToV2(v1);
    auto v3 = migratePlayerV2ToV3(v2);
    EXPECT_EQ(v2.health, INT32_MIN);
    EXPECT_EQ(v3.stats, (Stats{INT32_MIN, 100}));
    EXPECT_EQ(v3.name, "Unknown");
    for (const auto health :
         {INT64_MIN, std::int64_t{-1}, std::int64_t{0}, std::int64_t{100},
          std::int64_t{250}, INT64_MAX}) {
        PlayerV2 player{7, "保留名字", health, {}, {{1, "Potion"}}};
        auto current = migratePlayerV2ToV3(player);
        EXPECT_EQ(current.stats.health, health);
        EXPECT_EQ(current.stats.maxHealth, std::max(std::int64_t{100}, health));
        EXPECT_EQ(current.name, player.name);
        EXPECT_EQ(current.inventory, player.inventory);
        auto legacy = serializeBound(SceneV2{"Test", player});
        ASSERT_TRUE(legacy);
        auto loaded = deserialize(legacy.value().view());
        ASSERT_TRUE(loaded);
        EXPECT_EQ(loaded.value().scene.player, current);
    }
}
TEST(V3, DirectAndStagedMigrationProduceIdenticalBytes) {
    auto source = readFile(golden("scene_v1.bin"));
    ASSERT_TRUE(source);
    auto direct = migrateArchive(source.value().view(), 3);
    auto middle = migrateArchive(source.value().view(), 2);
    ASSERT_TRUE(direct);
    ASSERT_TRUE(middle);
    auto staged = migrateArchive(middle.value().view(), 3);
    ASSERT_TRUE(staged);
    EXPECT_EQ(vectorBytes(direct.value().view()),
              vectorBytes(staged.value().view()));
    auto loaded = deserialize(direct.value().view());
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.value().playerVersion, 3);
    EXPECT_EQ(loaded.value().scene.player.name, "Unknown");
    auto v2 = readFile(golden("scene_v2.bin"));
    ASSERT_TRUE(v2);
    auto upgraded = migrateArchive(v2.value().view());
    ASSERT_TRUE(upgraded);
    auto v3 = deserialize(upgraded.value().view());
    ASSERT_TRUE(v3);
    EXPECT_EQ(v3.value().scene, demoScene());
}
TEST(V3, InvalidStatsAndFutureSchemaAreRejected) {
    auto current = loadScene(golden("scene_v3.bin"));
    ASSERT_TRUE(current);
    for (const auto maxHealth :
         {std::int64_t{0}, std::int64_t{-1}, std::int64_t{99}}) {
        auto stats = statsRecord(current.value());
        BinaryWriter w;
        w.writeI64(maxHealth);
        stats.fields[1].payload = std::move(w).take();
        auto bytes = wrapStats(stats);
        EXPECT_FALSE(deserialize(bytes.view()));
        auto scene = demoScene();
        scene.player.stats.maxHealth = maxHealth;
        EXPECT_FALSE(serialize(scene));
    }
    auto stats = statsRecord(current.value());
    stats.version = 2;
    auto future = wrapStats(stats);
    auto result = deserialize(future.view());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::unsupported_version);
    stats = statsRecord(current.value());
    stats.fields[0].wire = WireType::i32;
    auto wrong = wrapStats(stats);
    EXPECT_FALSE(deserialize(wrong.view()));
}
TEST(V3, MissingDefaultsUnknownRetiredIdAndUnknownStatsFields) {
    auto loaded = loadScene(golden("scene_v3.bin"));
    ASSERT_TRUE(loaded);
    auto stats = statsRecord(loaded.value());
    stats.fields.clear();
    auto missing = wrapStats(stats);
    auto result = deserialize(missing.view());
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().scene.player.stats, Stats{});
    stats = statsRecord(loaded.value());
    stats.fields.push_back({999, static_cast<WireType>(255), {}, 0});
    auto unknown = wrapStats(stats);
    EXPECT_TRUE(deserialize(unknown.view()));
    auto player = playerRecord(loaded.value());
    BinaryWriter w;
    w.writeI64(999);
    player.fields.push_back({3, WireType::i64, std::move(w).take(), 0});
    auto retired = replacePlayer(loaded.value(), player);
    auto accepted = deserialize(retired.view());
    ASSERT_TRUE(accepted);
    EXPECT_EQ(accepted.value().scene.player.stats.health, 100);
}
TEST(V3, CustomMaxHealthRoundTripAndLossyProjectionRejected) {
    auto scene = demoScene();
    scene.player.stats = {80, 150};
    auto bytes = serialize(scene);
    ASSERT_TRUE(bytes);
    auto loaded = deserialize(bytes.value().view());
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.value().scene, scene);
    for (auto version : {std::uint16_t{1}, std::uint16_t{2}}) {
        auto old = serialize(scene, version);
        ASSERT_FALSE(old);
        EXPECT_EQ(old.error().code, ErrorCode::invalid_argument);
    }
    scene.player.stats = {INT64_MAX, INT64_MAX};
    EXPECT_TRUE(serialize(scene, 2));
    EXPECT_FALSE(serialize(scene, 1));
}
TEST(V3, IdentityDowngradeAndUnknownTargetsRejected) {
    auto v2 = readFile(golden("scene_v2.bin"));
    auto v3 = readFile(golden("scene_v3.bin"));
    ASSERT_TRUE(v2);
    ASSERT_TRUE(v3);
    EXPECT_FALSE(migrateArchive(v2.value().view(), 2));
    EXPECT_FALSE(migrateArchive(v3.value().view(), 3));
    EXPECT_FALSE(migrateArchive(v3.value().view(), 2));
    EXPECT_FALSE(migrateArchive(v2.value().view(), 1));
    EXPECT_FALSE(migrateArchive(v2.value().view(), 4));
}
TEST(V3, FileChainPreservesSourcesAndExistingTargets) {
    TempDirectory temp;
    auto original = readFile(golden("scene_v1.bin"));
    ASSERT_TRUE(original);
    const auto direct = temp.path / "direct.bin", middle = temp.path / "v2.bin",
               staged = temp.path / "staged.bin";
    ASSERT_TRUE(migrateFile(golden("scene_v1.bin"), direct));
    ASSERT_TRUE(migrateFile(golden("scene_v1.bin"), middle, 2));
    ASSERT_TRUE(migrateFile(middle, staged, 3));
    auto a = readFile(direct), b = readFile(staged);
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    EXPECT_EQ(vectorBytes(a.value().view()), vectorBytes(b.value().view()));
    EXPECT_FALSE(migrateFile(golden("scene_v2.bin"), direct));
    auto unchanged = readFile(direct);
    ASSERT_TRUE(unchanged);
    EXPECT_EQ(vectorBytes(a.value().view()),
              vectorBytes(unchanged.value().view()));
    auto source = readFile(golden("scene_v1.bin"));
    ASSERT_TRUE(source);
    EXPECT_EQ(vectorBytes(source.value().view()),
              vectorBytes(original.value().view()));
}
TEST(V3, MigrationFailuresLeaveNoDestinationOrTemporaryFiles) {
    TempDirectory temp;
    EXPECT_FALSE(migrateFile(golden("bad_magic.bin"), temp.path / "bad.bin"));
    EXPECT_FALSE(
        migrateFile(golden("truncated.bin"), temp.path / "truncated.bin"));
    EXPECT_FALSE(
        migrateFile(golden("scene_v3.bin"), temp.path / "identity.bin"));
    EXPECT_FALSE(
        migrateFile(golden("scene_v2.bin"), temp.path / "missing" / "out.bin"));
    EXPECT_TRUE(std::filesystem::is_empty(temp.path));
}
TEST(V3, CliWorkflowShowsStructureJsonAndMigrationOptions) {
    TempDirectory temp;
    auto run = [&](std::initializer_list<std::string> args) {
        std::vector<std::string_view> views;
        for (const auto& arg : args) {
            views.push_back(arg);
        }
        std::ostringstream out, err;
        const auto code = cli::run(views, out, err);
        return std::pair{code, out.str()};
    };
    const auto direct = (temp.path / "direct.bin").string(),
               middle = (temp.path / "v2.bin").string();
    EXPECT_EQ(run({"migrate", golden("scene_v1.bin").string(), direct}).first,
              0);
    EXPECT_EQ(run({"migrate", golden("scene_v1.bin").string(), middle,
                   "--to-version", "2"})
                  .first,
              0);
    auto inspect = run({"inspect", direct});
    EXPECT_EQ(inspect.first, 0);
    EXPECT_NE(inspect.second.find("Stats type=5 schema=1"), std::string::npos);
    auto json = run({"to-json", direct});
    ASSERT_EQ(json.first, 0);
    auto parsed = nlohmann::json::parse(json.second);
    EXPECT_EQ(parsed["player"]["stats"]["maxHealth"], 100);
    EXPECT_FALSE(parsed["player"].contains("health"));
    auto difference = run({"diff", middle, direct});
    EXPECT_EQ(difference.first, 1);
    EXPECT_NE(difference.second.find("field moved -> player.stats.health"),
              std::string::npos);
    EXPECT_EQ(run({"migrate", golden("scene_v1.bin").string(), direct,
                   "--to-version", "4"})
                  .first,
              64);
    EXPECT_EQ(run({"migrate", golden("scene_v1.bin").string(), direct,
                   "--bad-option", "2"})
                  .first,
              64);
}
} // namespace chronicle::test
