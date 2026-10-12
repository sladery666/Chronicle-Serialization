#include "support.hpp"
#include <fstream>
namespace chronicle::test {
TEST(File, SaveLoadAndAtomicReplacement) {
    TempDirectory temp;
    const auto path = temp.path / "scene.bin";
    ASSERT_TRUE(saveScene(path, demoScene()));
    auto scene = demoScene();
    scene.player.stats.health = 80;
    ASSERT_TRUE(saveScene(path, scene));
    auto loaded = loadScene(path);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.value().scene, scene);
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(temp.path),
                            std::filesystem::directory_iterator{}),
              1);
}
TEST(File, MissingAndOversizedFiles) {
    TempDirectory temp;
    auto missing = readFile(temp.path / "absent");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, ErrorCode::io_error);
    auto path = temp.path / "huge";
    std::ofstream file(path, std::ios::binary);
    file.seekp(static_cast<std::streamoff>(maxFileBytes));
    file.put('x');
    file.close();
    auto result = readFile(path);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::limit_exceeded);
}
TEST(Migration, V1ToV2SourcePreservedAndValidated) {
    TempDirectory temp;
    const auto output = temp.path / "v2.bin";
    auto original = readFile(golden("scene_v1.bin"));
    ASSERT_TRUE(original);
    ASSERT_TRUE(migrateFile(golden("scene_v1.bin"), output, 2));
    auto loaded = loadScene(output);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.value().playerVersion, 2);
    EXPECT_EQ(loaded.value().scene.player.name, "Unknown");
    EXPECT_EQ(loaded.value().scene.player.stats.health, 100);
    auto source = readFile(golden("scene_v1.bin"));
    ASSERT_TRUE(source);
    EXPECT_EQ(vectorBytes(original.value().view()),
              vectorBytes(source.value().view()));
}
TEST(Migration, FailedWritesAndExistingDestinationDoNotDestroyFiles) {
    TempDirectory temp;
    const auto output = temp.path / "existing";
    ASSERT_TRUE(saveScene(output, demoScene()));
    auto before = readFile(output);
    ASSERT_TRUE(before);
    EXPECT_FALSE(migrateFile(golden("scene_v1.bin"), output));
    auto after = readFile(output);
    ASSERT_TRUE(after);
    EXPECT_EQ(vectorBytes(before.value().view()),
              vectorBytes(after.value().view()));
    EXPECT_FALSE(
        migrateFile(golden("scene_v1.bin"), temp.path / "missing" / "out.bin"));
    EXPECT_FALSE(migrateFile(golden("bad_magic.bin"), temp.path / "bad.bin"));
    EXPECT_FALSE(std::filesystem::exists(temp.path / "bad.bin"));
    EXPECT_FALSE(migrateFile(golden("scene_v2.bin"), temp.path / "v2.bin", 2));
    EXPECT_FALSE(std::filesystem::exists(temp.path / "v2.bin"));
    EXPECT_FALSE(migrateFile(golden("scene_v1.bin"), golden("scene_v1.bin")));
    const auto directory = temp.path / "directory";
    std::filesystem::create_directory(directory);
    EXPECT_FALSE(saveScene(directory, demoScene()));
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(temp.path),
                            std::filesystem::directory_iterator{}),
              2);
}
TEST(Migration, AliasedDestinationIsRejected) {
    TempDirectory temp;
    const auto source = temp.path / "v1.bin";
    const auto alias = temp.path / "alias.bin";
    ASSERT_TRUE(saveScene(source, demoScene(), 1));
    std::filesystem::create_hard_link(source, alias);
    EXPECT_FALSE(migrateFile(source, alias));
    auto result = loadScene(source);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().playerVersion, 1);
}
} // namespace chronicle::test
namespace chronicle::test {
TEST(Migration, InvalidSceneCannotReplaceValidSave) {
    TempDirectory temp;
    const auto file = temp.path / "save.bin";
    ASSERT_TRUE(saveScene(file, demoScene()));
    auto before = readFile(file);
    ASSERT_TRUE(before);
    auto scene = demoScene();
    scene.player.name = "\xFF";
    EXPECT_FALSE(saveScene(file, scene));
    auto after = readFile(file);
    ASSERT_TRUE(after);
    EXPECT_EQ(vectorBytes(before.value().view()),
              vectorBytes(after.value().view()));
}
} // namespace chronicle::test
