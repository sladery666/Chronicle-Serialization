#include "../examples/type_binding_demo/model.hpp"
#include "commands.hpp"
#include "support.hpp"
#include <map>
namespace chronicle::test {
struct Duplicate {
    std::uint64_t first = 0, second = 0;
};
struct Empty {};
struct Arrays {
    std::vector<std::uint64_t> ids;
    std::vector<std::int32_t> scores;
    std::vector<std::int64_t> health;
    std::vector<std::string> labels;
    std::vector<std::vector<std::int32_t>> grid;
    bool operator==(const Arrays&) const = default;
};
struct FutureField {
    std::uint64_t value = 0;
};
} // namespace chronicle::test
namespace chronicle {
template <> struct TypeBinding<test::Duplicate> {
    static constexpr TypeId typeId = 100;
    static constexpr std::string_view name = "Duplicate";
    static constexpr std::uint16_t minVersion = 1, maxVersion = 1;
    static constexpr auto fields() {
        return std::tuple{bindField(1, "first", &test::Duplicate::first),
                          bindField(1, "second", &test::Duplicate::second)};
    }
};
template <> struct TypeBinding<test::Empty> {
    static constexpr TypeId typeId = 101;
    static constexpr std::string_view name = "Empty";
    static constexpr std::uint16_t minVersion = 1, maxVersion = 1;
    static constexpr auto fields() {
        return std::tuple{};
    }
};
template <> struct TypeBinding<test::Arrays> {
    static constexpr TypeId typeId = 102;
    static constexpr std::string_view name = "Arrays";
    static constexpr std::uint16_t minVersion = 1, maxVersion = 1;
    static constexpr auto fields() {
        return std::tuple{bindField(1, "ids", &test::Arrays::ids),
                          bindField(2, "scores", &test::Arrays::scores),
                          bindField(3, "health", &test::Arrays::health),
                          bindField(4, "labels", &test::Arrays::labels),
                          bindField(5, "grid", &test::Arrays::grid)};
    }
};
template <> struct TypeBinding<test::FutureField> {
    static constexpr TypeId typeId = 103;
    static constexpr std::string_view name = "FutureField";
    static constexpr std::uint16_t minVersion = 1, maxVersion = 1;
    static constexpr auto fields() {
        return std::tuple{bindField(1, "value", &test::FutureField::value, 2)};
    }
};
} // namespace chronicle
namespace chronicle::test {
using example::QuestState;
using example::WorldSettings;
static_assert(BoundObject<WorldSettings>);
static_assert(BoundValue<std::vector<Item>>);
static_assert(!BoundValue<bool> && !BoundValue<double>);
static_assert(!BoundValue<int*> && !BoundValue<std::map<int, int>>);
static_assert(!BoundValue<std::vector<bool>>);
ByteBuffer archiveRecord(ObjectRecord record) {
    const auto type = record.type;
    auto payload = encodeObject(std::move(record));
    return encodeArchive(type, payload.view());
}
TEST(Binding, NewNestedTypesRoundTripAndOwnership) {
    auto loaded = [] {
        auto bytes = serializeBound(example::demoSettings());
        if (!bytes) {
            throw Failure(bytes.error());
        }
        return deserializeBound<WorldSettings>(bytes.value().view());
    }();
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.value().value, example::demoSettings());
    auto copy = loaded.value();
    auto moved = std::move(loaded.value());
    EXPECT_EQ(copy.value, moved.value);
    EXPECT_EQ(copy.root.type, 11U);
    auto bytes = archiveRecord(copy.root);
    auto again = deserializeBound<WorldSettings>(bytes.view());
    ASSERT_TRUE(again);
    EXPECT_EQ(again.value().value, copy.value);
}
TEST(Binding, MetadataDependencyRegistrationAndInspection) {
    SchemaRegistry registry;
    ASSERT_TRUE(registerBinding<WorldSettings>(registry));
    ASSERT_NE(registry.find(11, 1), nullptr);
    ASSERT_NE(registry.find(10, 1), nullptr);
    ASSERT_NE(registry.find(itemType, 1), nullptr);
    EXPECT_EQ(registry.find(11, 1)->fields.front().id, 1U);
    EXPECT_EQ(registry.find(10, 1)->fields.back().elementWire, WireType::f32);
    EXPECT_TRUE(registerBinding<WorldSettings>(registry));
    auto bytes = serializeBound(example::demoSettings());
    ASSERT_TRUE(bytes);
    auto loaded = deserializeBound<WorldSettings>(bytes.value().view());
    ASSERT_TRUE(loaded);
    const auto output = cli::inspectRecord(loaded.value().root, registry);
    EXPECT_NE(output.find("WorldSettings type=11 schema=1"), std::string::npos);
    EXPECT_NE(output.find("QuestState type=10 schema=1"), std::string::npos);
    EXPECT_NE(output.find("elements=2"), std::string::npos);
    EXPECT_NE(output.find("Item type=4"), std::string::npos);
}
TEST(Binding, ExistingSchemasGeneratedFromBindings) {
    auto* v1 = builtinSchemas().find(playerType, 1);
    auto* v2 = builtinSchemas().find(playerType, 2);
    ASSERT_NE(v1, nullptr);
    ASSERT_NE(v2, nullptr);
    EXPECT_EQ(v1->fields.size(), 4U);
    EXPECT_EQ(v2->fields.size(), 5U);
    EXPECT_EQ(v1->fields[1].wire, WireType::i32);
    EXPECT_EQ(v2->fields[2].wire, WireType::i64);
    auto bytes = serializeBound(projectSceneV1(demoScene()));
    ASSERT_TRUE(bytes);
    auto goldenFile = readFile(golden("scene_v1.bin"));
    ASSERT_TRUE(goldenFile);
    EXPECT_EQ(vectorBytes(bytes.value().view()),
              vectorBytes(goldenFile.value().view()));
}
TEST(Binding, OutOfOrderBindingProducesCanonicalRecords) {
    auto record = encodeBoundObject(example::demoSettings());
    ASSERT_EQ(record.fields.front().id, 4U);
    auto a = archiveRecord(record);
    std::reverse(record.fields.begin(), record.fields.end());
    auto b = archiveRecord(record);
    EXPECT_EQ(vectorBytes(a.view()), vectorBytes(b.view()));
    auto bytes = serializeBound(example::demoSettings());
    ASSERT_TRUE(bytes);
    EXPECT_EQ(vectorBytes(bytes.value().view()), vectorBytes(a.view()));
}
TEST(Binding, MissingDefaultsUnknownFieldsAndEmptyObjects) {
    auto bytes = archiveRecord({11, 1, {}, 0});
    auto loaded = deserializeBound<WorldSettings>(bytes.view());
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.value().value, WorldSettings{});
    auto record = encodeBoundObject(example::demoSettings());
    FieldRecord unknown{999, static_cast<WireType>(255), {}, 0};
    unknown.payload.push(std::byte{0xFF});
    record.fields.push_back(std::move(unknown));
    auto extra = archiveRecord(record);
    auto result = deserializeBound<WorldSettings>(extra.view());
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().value, example::demoSettings());
    auto empty = serializeBound(Empty{});
    ASSERT_TRUE(empty);
    EXPECT_TRUE(deserializeBound<Empty>(empty.value().view()));
}
TEST(Binding, DuplicateAndInvalidDescriptorsAreRejected) {
    SchemaRegistry registry;
    EXPECT_FALSE(registerBinding<Duplicate>(registry));
    EXPECT_EQ(registry.find(100, 1), nullptr);
    EXPECT_FALSE(serializeBound(Duplicate{}));
    EXPECT_FALSE(registerBinding<FutureField>(registry));
    EXPECT_FALSE(serializeBound(FutureField{}));
    EXPECT_FALSE(registry.registerType(
        {200,
         1,
         "BadArray",
         {{1, "items", WireType::array, static_cast<WireType>(0)}}}));
}
TEST(Binding, RegistrationCollisionLeavesRegistryUnchanged) {
    SchemaRegistry registry;
    ASSERT_TRUE(registry.registerType({10, 1, "Other", {}}));
    auto status = registerBinding<WorldSettings>(registry);
    ASSERT_FALSE(status);
    EXPECT_EQ(status.error().code, ErrorCode::schema_error);
    EXPECT_EQ(registry.find(11, 1), nullptr);
    EXPECT_EQ(registry.find(10, 1)->name, "Other");
}
TEST(Binding, KnownWireWrongTypeAndFutureVersionRejected) {
    auto record = encodeBoundObject(example::demoSettings());
    record.fields[0].wire = WireType::u64;
    auto bytes = archiveRecord(record);
    EXPECT_FALSE(deserializeBound<WorldSettings>(bytes.view()));
    record = encodeBoundObject(example::demoSettings());
    record.version = 9;
    auto future = archiveRecord(record);
    auto result = deserializeBound<WorldSettings>(future.view());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::unsupported_version);
    auto other = serializeBound(example::demoSettings());
    ASSERT_TRUE(other);
    EXPECT_FALSE(deserializeBound<QuestState>(other.value().view()));
}
TEST(Binding, ScalarArraysAndNestedArrays) {
    Arrays input{{0, UINT64_MAX},
                 {INT32_MIN, INT32_MAX},
                 {INT64_MIN, INT64_MAX},
                 {"", "存档"},
                 {{1, 2}, {}, {-1}}};
    auto bytes = serializeBound(input);
    ASSERT_TRUE(bytes);
    auto result = deserializeBound<Arrays>(bytes.value().view());
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().value, input);
}
TEST(Binding, ArrayLengthsAndElementTrailingBytesRejected) {
    auto record = encodeBoundObject(QuestState{7, "test", 1, {1.0F}});
    for (auto& f : record.fields) {
        if (f.id == 4) {
            BinaryWriter w;
            w.writeU32(1);
            w.writeU64(5);
            w.writeFloat(1);
            w.writeU8(0);
            f.payload = std::move(w).take();
        }
    }
    auto bytes = archiveRecord(record);
    EXPECT_FALSE(deserializeBound<QuestState>(bytes.view()));
    for (auto& f : record.fields) {
        if (f.id == 4) {
            BinaryWriter w;
            w.writeU32(UINT32_MAX);
            f.payload = std::move(w).take();
        }
    }
    auto large = archiveRecord(record);
    auto result = deserializeBound<QuestState>(large.view());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::limit_exceeded);
}
TEST(Binding, FloatStringAndVectorLimits) {
    auto input = example::demoSettings();
    input.quest.checkpoints[0] = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(serializeBound(input));
    input = example::demoSettings();
    input.difficulty = "\xFF";
    EXPECT_FALSE(serializeBound(input));
    input = example::demoSettings();
    input.quest.checkpoints.resize(maxArrayElements + 1);
    EXPECT_FALSE(serializeBound(input));
    EXPECT_THROW(binding_detail::encode(example::demoSettings(), {},
                                        maxBindingDepth + 1),
                 Failure);
}
TEST(Binding, SaveLoadAndWrongRootFile) {
    TempDirectory temp;
    const auto path = temp.path / "settings.bin";
    ASSERT_TRUE(saveBound(path, example::demoSettings()));
    auto result = loadBound<WorldSettings>(path);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().value, example::demoSettings());
    EXPECT_FALSE(loadScene(path));
    EXPECT_FALSE(loadBound<QuestState>(path));
}
TEST(Binding, EveryNewTypeTruncationRejected) {
    auto bytes = serializeBound(example::demoSettings());
    ASSERT_TRUE(bytes);
    for (std::size_t size = 0; size < bytes.value().size(); ++size) {
        EXPECT_FALSE(
            deserializeBound<WorldSettings>(bytes.value().view().first(size)))
            << size;
    }
}
} // namespace chronicle::test
