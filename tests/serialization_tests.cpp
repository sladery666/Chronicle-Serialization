#include "support.hpp"
#include <limits>
#include <random>
namespace chronicle::test {
TEST(Scene, DeterministicRoundTripAndGoldenV2) {
    auto a = serialize(demoScene(), 2);
    auto b = serialize(demoScene(), 2);
    auto goldenFile = readFile(golden("scene_v2.bin"));
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    ASSERT_TRUE(goldenFile);
    EXPECT_EQ(vectorBytes(a.value().view()), vectorBytes(b.value().view()));
    EXPECT_EQ(vectorBytes(a.value().view()),
              vectorBytes(goldenFile.value().view()));
    auto loaded = deserialize(a.value().view());
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.value().scene, demoScene());
}
TEST(Compatibility, GoldenV1WideningAndDefaultName) {
    auto loaded = loadScene(golden("scene_v1.bin"));
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.value().playerVersion, 1);
    auto expected = demoScene();
    expected.player.name = "Unknown";
    EXPECT_EQ(loaded.value().scene, expected);
    auto encoded = serialize(demoScene(), 1);
    auto goldenFile = readFile(golden("scene_v1.bin"));
    ASSERT_TRUE(encoded);
    ASSERT_TRUE(goldenFile);
    EXPECT_EQ(vectorBytes(encoded.value().view()),
              vectorBytes(goldenFile.value().view()));
    auto scene = demoScene();
    scene.player.stats.health = INT32_MIN;
    auto bytes = serialize(scene, 1);
    ASSERT_TRUE(bytes);
    auto negative = deserialize(bytes.value().view());
    ASSERT_TRUE(negative);
    EXPECT_EQ(negative.value().scene.player.stats.health, INT32_MIN);
}
TEST(Scene, EmptyAndLargeIntegersRoundTrip) {
    Scene scene;
    scene.player.id = UINT64_MAX;
    scene.player.stats.health = INT64_MIN;
    scene.player.name = "存档";
    auto bytes = serialize(scene);
    ASSERT_TRUE(bytes);
    auto loaded = deserialize(bytes.value().view());
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.value().scene, scene);
}
TEST(Compatibility, MissingFieldsUseDefaults) {
    auto bytes = wrap({sceneType, 1, {}, 0});
    auto loaded = deserialize(bytes.view());
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.value().scene, Scene{});
    auto original = loadScene(golden("scene_v2.bin"));
    ASSERT_TRUE(original);
    auto player = playerRecord(original.value());
    player.fields.clear();
    auto missing = replacePlayer(original.value(), player);
    auto result = deserialize(missing.view());
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().scene.player, Player{});
}
TEST(Compatibility, UnknownFieldsAndWireTypesAreSkipped) {
    auto loaded = loadScene(golden("scene_v2.bin"));
    ASSERT_TRUE(loaded);
    FieldRecord unknown{999, static_cast<WireType>(255), {}, 0};
    unknown.payload.push(std::byte{0xFF});
    loaded.value().root.fields.push_back(unknown);
    auto player = playerRecord(loaded.value());
    player.fields.push_back(unknown);
    auto bytes = replacePlayer(loaded.value(), player);
    auto result = deserialize(bytes.view());
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().scene, demoScene());
}
TEST(Schema, RegistrationValidationAndCanonicalOrder) {
    SchemaRegistry registry;
    EXPECT_TRUE(registry.registerType(
        {10,
         1,
         "A",
         {{2, "second", WireType::string}, {1, "first", WireType::u64}}}));
    ASSERT_NE(registry.find(10, 1), nullptr);
    EXPECT_EQ(registry.find(10, 1)->fields.front().id, 1U);
    EXPECT_FALSE(registry.registerType({10, 1, "A", {}}));
    EXPECT_FALSE(registry.registerType({10, 2, "B", {}}));
    EXPECT_FALSE(registry.registerType({11, 1, "A", {}}));
    EXPECT_FALSE(registry.registerType(
        {12, 1, "C", {{1, "a", WireType::u64}, {1, "b", WireType::u64}}}));
    EXPECT_TRUE(registry.registerType({10, 2, "A", {}}));
    auto loaded = loadScene(golden("scene_v2.bin"));
    ASSERT_TRUE(loaded);
    auto root = loaded.value().root;
    auto first = encodeObject(root);
    std::reverse(root.fields.begin(), root.fields.end());
    auto second = encodeObject(root);
    EXPECT_EQ(vectorBytes(first.view()), vectorBytes(second.view()));
    root.fields.push_back(root.fields.front());
    EXPECT_THROW(encodeObject(root), Failure);
}
TEST(Malformed, EveryTruncationIsRejected) {
    auto bytes = serialize(demoScene());
    ASSERT_TRUE(bytes);
    for (std::size_t size = 0; size < bytes.value().size(); ++size) {
        auto result = deserialize(bytes.value().view().first(size));
        EXPECT_FALSE(result) << "size=" << size;
    }
}
TEST(Malformed, HeaderVersionsLengthsAndTrailingBytes) {
    auto bytes = serialize(demoScene());
    ASSERT_TRUE(bytes);
    auto original = vectorBytes(bytes.value().view());
    for (const auto& change :
         std::vector<std::pair<std::size_t, std::uint64_t>>{
             {0, 0}, {8, 2}, {10, 1}, {12, 2}, {16, UINT64_MAX}}) {
        auto mutated = original;
        put(mutated, change.first, change.second, change.first == 16 ? 8 : 1);
        EXPECT_FALSE(deserialize(mutated));
    }
    original.push_back(std::byte{0});
    EXPECT_FALSE(deserialize(original));
}
TEST(Malformed, FieldWireDuplicateFlagsAndSchemaVersions) {
    auto loaded = loadScene(golden("scene_v2.bin"));
    ASSERT_TRUE(loaded);
    auto root = loaded.value().root;
    root.fields.front().wire = WireType::u64;
    auto bad = wrap(root);
    EXPECT_FALSE(deserialize(bad.view()));
    root = loaded.value().root;
    root.version = 99;
    auto future = wrap(root);
    auto result = deserialize(future.view());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::unsupported_version);
    auto bytes = serialize(demoScene());
    ASSERT_TRUE(bytes);
    auto data = vectorBytes(bytes.value().view());
    put(data, 37, 1, 1);
    EXPECT_FALSE(deserialize(data));
    data = vectorBytes(bytes.value().view());
    put(data, 58, 1, 4);
    EXPECT_FALSE(deserialize(data));
    auto player = playerRecord(loaded.value());
    player.type = itemType;
    auto wrong = replacePlayer(loaded.value(), player);
    EXPECT_FALSE(deserialize(wrong.view()));
}
TEST(Malformed, StringsArraysAndScalarPayloadLengths) {
    auto loaded = loadScene(golden("scene_v2.bin"));
    ASSERT_TRUE(loaded);
    auto player = playerRecord(loaded.value());
    for (auto& f : player.fields) {
        if (f.id == 5) {
            BinaryWriter w;
            w.writeU32(UINT32_MAX);
            f.payload = std::move(w).take();
        }
    }
    auto bytes = replacePlayer(loaded.value(), player);
    auto result = deserialize(bytes.view());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::limit_exceeded);
    player = playerRecord(loaded.value());
    for (auto& f : player.fields) {
        if (f.id == 1) {
            f.payload.push(std::byte{0});
        }
    }
    auto trailing = replacePlayer(loaded.value(), player);
    EXPECT_FALSE(deserialize(trailing.view()));
    auto root = loaded.value().root;
    root.fields[0].payload = ByteBuffer{};
    auto empty = wrap(root);
    EXPECT_FALSE(deserialize(empty.view()));
}
TEST(Malformed, SeededMutationSmoke) {
    auto bytes = serialize(demoScene());
    ASSERT_TRUE(bytes);
    std::mt19937 random(42);
    for (unsigned i = 0; i < 1000; ++i) {
        auto changed = vectorBytes(bytes.value().view());
        const auto index = random() % changed.size();
        changed[index] = std::byte(static_cast<std::uint8_t>(random()));
        auto result = deserialize(changed);
        if (result) {
            auto encoded = serialize(result.value().scene);
            EXPECT_TRUE(encoded);
        }
    }
}
TEST(Scene, RejectNonFiniteOverflowAndUnsupportedVersions) {
    auto scene = demoScene();
    scene.player.position.x = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(serialize(scene));
    scene = demoScene();
    scene.player.stats = {INT64_MAX, INT64_MAX};
    EXPECT_FALSE(serialize(scene, 1));
    EXPECT_TRUE(serialize(scene, 2));
    EXPECT_FALSE(serialize(scene, 4));
    scene.player.inventory.resize(maxArrayElements + 1);
    EXPECT_FALSE(serialize(scene));
}
} // namespace chronicle::test
namespace chronicle::test {
TEST(Malformed, NestedInvalidLengthsUtf8AndNaN) {
    auto original = loadScene(golden("scene_v2.bin"));
    ASSERT_TRUE(original);
    auto root = original.value().root;
    BinaryWriter badString;
    badString.writeU32(1);
    badString.writeU8(0xFF);
    root.fields[0].payload = std::move(badString).take();
    auto invalidUtf8 = wrap(root);
    EXPECT_FALSE(deserialize(invalidUtf8.view()));
    auto player = playerRecord(original.value());
    for (auto& f : player.fields) {
        if (f.id == 4) {
            BinaryReader reader(f.payload.view());
            auto position = decodeObject(reader);
            BinaryWriter nan;
            nan.writeFloat(std::numeric_limits<float>::quiet_NaN());
            position.fields[0].payload = std::move(nan).take();
            f.payload = encodeObject(position);
        }
    }
    auto invalidPosition = replacePlayer(original.value(), player);
    EXPECT_FALSE(deserialize(invalidPosition.view()));
    for (const auto count : {0U, 1U, maxArrayElements}) {
        player = playerRecord(original.value());
        for (auto& f : player.fields) {
            if (f.id == 5) {
                BinaryWriter w;
                w.writeU32(count);
                w.writeU64(UINT64_MAX);
                f.payload = std::move(w).take();
            }
        }
        auto invalidArray = replacePlayer(original.value(), player);
        EXPECT_FALSE(deserialize(invalidArray.view()));
    }
    auto bytes = serialize(demoScene());
    ASSERT_TRUE(bytes);
    for (const auto offset : {40U, 66U}) {
        auto mutated = vectorBytes(bytes.value().view());
        put(mutated, offset, UINT64_MAX, 8);
        auto result = deserialize(mutated);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, ErrorCode::limit_exceeded);
    }
}
} // namespace chronicle::test
namespace chronicle::test {
TEST(Ownership, BorrowedRecordsAndOwnedSnapshot) {
    auto file = readFile(golden("scene_v2.bin"));
    ASSERT_TRUE(file);
    auto bytes = vectorBytes(file.value().view());
    BinaryReader reader(std::span(bytes).subspan(24), 24);
    auto view = decodeObjectView(reader);
    ASSERT_EQ(view.fields.size(), 2U);
    EXPECT_EQ(view.fields[0].payload.data(),
              bytes.data() + view.fields[0].offset);
    auto owned = ownObject(view);
    const auto copied = vectorBytes(owned.fields[0].payload.view());
    bytes[view.fields[0].offset] = std::byte{0};
    EXPECT_EQ(view.fields[0].payload[0], std::byte{0});
    EXPECT_EQ(vectorBytes(owned.fields[0].payload.view()), copied);
}
TEST(Ownership, LoadedSceneOutlivesInputAndCopyMove) {
    auto loaded = [] {
        auto bytes = serialize(demoScene());
        if (!bytes) {
            throw Failure(bytes.error());
        }
        return deserialize(bytes.value().view());
    }();
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.value().scene, demoScene());
    auto copy = loaded.value();
    auto moved = std::move(loaded.value());
    auto copyBytes = wrap(copy.root);
    auto movedBytes = wrap(moved.root);
    EXPECT_EQ(vectorBytes(copyBytes.view()), vectorBytes(movedBytes.view()));
    auto result = deserialize(copyBytes.view());
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().scene, demoScene());
}
TEST(Ownership, MaximumFieldCountAndLateDuplicate) {
    ObjectRecord record{sceneType, 1, {}, 0};
    for (FieldId id = 100; id < 1124; ++id) {
        record.fields.push_back({id, static_cast<WireType>(255), {}, 0});
    }
    auto bytes = encodeObject(record);
    BinaryReader reader(bytes.view());
    auto view = decodeObjectView(reader);
    EXPECT_EQ(view.fields.size(), 1024U);
    auto changed = vectorBytes(bytes.view());
    put(changed, 8 + 1023 * 16, 100, 4);
    BinaryReader duplicate(changed);
    EXPECT_THROW(decodeObjectView(duplicate), Failure);
    record.fields.push_back({1124, WireType::u64, {}, 0});
    EXPECT_THROW(encodeObject(record), Failure);
}
} // namespace chronicle::test
