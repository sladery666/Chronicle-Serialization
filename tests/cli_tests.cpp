#include "commands.hpp"
#include "support.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
namespace chronicle::test {
struct CommandResult {
    int code;
    std::string out;
    std::string err;
};
CommandResult command(std::initializer_list<std::string> args) {
    std::vector<std::string_view> views;
    for (const auto& arg : args) {
        views.push_back(arg);
    }
    std::ostringstream out, err;
    const auto code = cli::run(views, out, err);
    return {code, out.str(), err.str()};
}
TEST(Cli, HelpUsageAndVersion) {
    EXPECT_EQ(command({"--help"}).code, 0);
    EXPECT_EQ(command({"--version"}).code, 0);
    EXPECT_EQ(command({}).code, 64);
    EXPECT_EQ(command({"unknown"}).code, 64);
    EXPECT_EQ(command({"validate"}).code, 64);
}
TEST(Cli, ValidateAndInspect) {
    auto valid = command({"validate", golden("scene_v1.bin").string()});
    EXPECT_EQ(valid.code, 0);
    EXPECT_NE(valid.out.find("Player V1"), std::string::npos);
    auto inspected = command({"inspect", golden("scene_v2.bin").string()});
    EXPECT_EQ(inspected.code, 0);
    EXPECT_NE(inspected.out.find("Player type=2 schema=2"), std::string::npos);
    EXPECT_NE(inspected.out.find("offset=0x"), std::string::npos);
    EXPECT_NE(inspected.out.find("Item type=4"), std::string::npos);
}
TEST(Cli, JsonIsValidAndPreservesStringsAndIntegers) {
    auto output = command({"to-json", golden("scene_v2.bin").string()});
    ASSERT_EQ(output.code, 0);
    auto json = nlohmann::json::parse(output.out);
    EXPECT_EQ(json["player"]["name"], "Ada");
    EXPECT_EQ(json["player"]["stats"]["health"], 100);
    auto scene = demoScene();
    scene.name = "quote\"\n\t\\存档";
    scene.player.stats.health = INT64_MIN;
    scene.player.id = UINT64_MAX;
    auto parsed = nlohmann::json::parse(cli::toJson(scene));
    EXPECT_EQ(parsed["name"], scene.name);
    EXPECT_EQ(parsed["player"]["stats"]["health"].get<std::int64_t>(),
              INT64_MIN);
    EXPECT_EQ(parsed["player"]["id"].get<std::uint64_t>(), UINT64_MAX);
}
TEST(Cli, SemanticDiffIncludesHealthInventoryAndSchema) {
    TempDirectory temp;
    auto scene = demoScene();
    scene.player.stats.health = 80;
    scene.player.inventory.pop_back();
    const auto changed = temp.path / "changed.bin";
    ASSERT_TRUE(saveScene(changed, scene));
    auto result =
        command({"diff", golden("scene_v2.bin").string(), changed.string()});
    EXPECT_EQ(result.code, 1);
    EXPECT_NE(result.out.find("player.stats.health: 100 -> 80"),
              std::string::npos);
    EXPECT_NE(result.out.find("item removed"), std::string::npos);
    EXPECT_EQ(command({"diff", changed.string(), changed.string()}).code, 0);
    auto schema = command({"diff", golden("scene_v1.bin").string(),
                           golden("scene_v2.bin").string()});
    EXPECT_EQ(schema.code, 1);
    EXPECT_NE(schema.out.find("field added"), std::string::npos);
}
TEST(Cli, CompleteMigrationWorkflow) {
    TempDirectory temp;
    const auto target = temp.path / "migrated.bin";
    EXPECT_EQ(command({"inspect", golden("scene_v1.bin").string()}).code, 0);
    EXPECT_EQ(command({"validate", golden("scene_v1.bin").string()}).code, 0);
    EXPECT_EQ(
        command({"migrate", golden("scene_v1.bin").string(), target.string()})
            .code,
        0);
    EXPECT_EQ(command({"validate", target.string()}).code, 0);
    EXPECT_EQ(
        command({"diff", golden("scene_v1.bin").string(), target.string()})
            .code,
        1);
    auto json = command({"to-json", target.string()});
    ASSERT_EQ(json.code, 0);
    EXPECT_EQ(nlohmann::json::parse(json.out)["player"]["name"], "Unknown");
}
TEST(Cli, ErrorCodesAndOffsetContext) {
    auto bad = command({"validate", golden("bad_magic.bin").string()});
    EXPECT_EQ(bad.code, 2);
    EXPECT_NE(bad.err.find("header.magic at offset 0x0"), std::string::npos);
    EXPECT_EQ(command({"validate", golden("truncated.bin").string()}).code, 2);
    TempDirectory temp;
    EXPECT_EQ(command({"validate", (temp.path / "absent").string()}).code, 3);
    auto bytes = readFile(golden("scene_v2.bin"));
    ASSERT_TRUE(bytes);
    auto mutated = vectorBytes(bytes.value().view());
    put(mutated, 8, 9, 2);
    const auto future = temp.path / "future.bin";
    ASSERT_TRUE(writeFile(future, mutated));
    EXPECT_EQ(command({"validate", future.string()}).code, 4);
    EXPECT_EQ(command({"migrate", golden("scene_v1.bin").string(),
                       golden("scene_v1.bin").string()})
                  .code,
              64);
}
} // namespace chronicle::test
