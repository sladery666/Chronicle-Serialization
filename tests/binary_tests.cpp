#include "support.hpp"
#include <array>
#include <limits>
#include <memory>
namespace chronicle::test {
TEST(Result, MoveOnlyValueAndError) {
    Result<std::unique_ptr<int>> value(std::make_unique<int>(7));
    ASSERT_TRUE(value);
    EXPECT_EQ(*value.value(), 7);
    auto moved = std::move(value).value();
    EXPECT_EQ(*moved, 7);
    Result<int> error(Error{ErrorCode::truncated, 12, "field", "short"});
    ASSERT_FALSE(error);
    EXPECT_EQ(error.error().offset, 12U);
}
TEST(ByteBuffer, EmptyAndSelfAppend) {
    ByteBuffer buffer;
    EXPECT_TRUE(buffer.empty());
    buffer.append({});
    buffer.push(std::byte{42});
    buffer.append(buffer.view());
    ASSERT_EQ(buffer.size(), 2U);
    EXPECT_EQ(buffer.view()[1], std::byte{42});
}
TEST(Binary, KnownLittleEndianBytes) {
    BinaryWriter w;
    w.writeU8(0xAB);
    w.writeU16(0x1234);
    w.writeU32(0x12345678);
    w.writeU64(0x0102030405060708);
    const std::vector<std::byte> expected{
        std::byte{0xAB}, std::byte{0x34}, std::byte{0x12}, std::byte{0x78},
        std::byte{0x56}, std::byte{0x34}, std::byte{0x12}, std::byte{8},
        std::byte{7},    std::byte{6},    std::byte{5},    std::byte{4},
        std::byte{3},    std::byte{2},    std::byte{1}};
    EXPECT_EQ(vectorBytes(w.view()), expected);
    BinaryReader r(w.view());
    EXPECT_EQ(r.readU8(), 0xAB);
    EXPECT_EQ(r.readU16(), 0x1234);
    EXPECT_EQ(r.readU32(), 0x12345678U);
    EXPECT_EQ(r.readU64(), 0x0102030405060708ULL);
    r.requireEnd();
}
TEST(Binary, ExtremesAndSignedFloats) {
    BinaryWriter w;
    w.writeU8(0);
    w.writeU8(255);
    w.writeU16(65535);
    w.writeU32(UINT32_MAX);
    w.writeU64(UINT64_MAX);
    w.writeI32(INT32_MIN);
    w.writeI32(INT32_MAX);
    w.writeI64(INT64_MIN);
    w.writeI64(INT64_MAX);
    w.writeFloat(-1.25F);
    w.writeDouble(3.125);
    BinaryReader r(w.view());
    EXPECT_EQ(r.readU8(), 0);
    EXPECT_EQ(r.readU8(), 255);
    EXPECT_EQ(r.readU16(), 65535);
    EXPECT_EQ(r.readU32(), UINT32_MAX);
    EXPECT_EQ(r.readU64(), UINT64_MAX);
    EXPECT_EQ(r.readI32(), INT32_MIN);
    EXPECT_EQ(r.readI32(), INT32_MAX);
    EXPECT_EQ(r.readI64(), INT64_MIN);
    EXPECT_EQ(r.readI64(), INT64_MAX);
    EXPECT_EQ(r.readFloat(), -1.25F);
    EXPECT_EQ(r.readDouble(), 3.125);
    r.requireEnd();
}
TEST(Binary, StringsUtf8AndEmbeddedNull) {
    BinaryWriter w;
    w.writeString("");
    w.writeString("存档🎮");
    w.writeString(std::string("a\0b", 3));
    BinaryReader r(w.view());
    EXPECT_EQ(r.readString(), "");
    EXPECT_EQ(r.readString(), "存档🎮");
    EXPECT_EQ(r.readString(), std::string("a\0b", 3));
    r.requireEnd();
    EXPECT_FALSE(validUtf8("\xC0\x80"));
    EXPECT_FALSE(validUtf8("\xED\xA0\x80"));
    EXPECT_FALSE(validUtf8("\xF4\x90\x80\x80"));
    EXPECT_FALSE(validUtf8("\xE2\x82"));
}
TEST(Binary, BoundsOverflowAndFailureDoesNotAdvance) {
    const std::array<std::byte, 3> bytes{};
    BinaryReader r(bytes, 100, "test");
    EXPECT_THROW(r.readU32(), Failure);
    EXPECT_EQ(r.offset(), 100U);
    EXPECT_THROW(r.readBytes(std::numeric_limits<std::size_t>::max()), Failure);
    EXPECT_EQ(r.offset(), 100U);
    r.readBytes(3);
    EXPECT_THROW(r.readU8(), Failure);
    BinaryReader empty({});
    EXPECT_THROW(empty.readU64(), Failure);
}
TEST(Binary, StringLimitsAndTrailingBytes) {
    BinaryWriter w;
    w.writeU32(UINT32_MAX);
    BinaryReader r(w.view());
    EXPECT_THROW(r.readString(), Failure);
    BinaryWriter bad;
    bad.writeU32(1);
    bad.writeU8(0xFF);
    BinaryReader invalid(bad.view());
    EXPECT_THROW(invalid.readString(), Failure);
    BinaryReader trailing(w.view());
    EXPECT_THROW(trailing.requireEnd(), Failure);
    EXPECT_THROW(w.writeString(std::string(maxStringBytes + 1, 'x')), Failure);
    EXPECT_THROW(w.writeString("\xFF"), Failure);
}
} // namespace chronicle::test
namespace chronicle::test {
TEST(ByteBuffer, ExternalOwnershipAndPartialSelfAppend) {
    std::array source{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    ByteBuffer buffer;
    buffer.append(source);
    source[0] = std::byte{99};
    EXPECT_EQ(buffer.view()[0], std::byte{1});
    buffer.append(buffer.view().subspan(1, 2));
    const std::vector expected{std::byte{1}, std::byte{2}, std::byte{3},
                               std::byte{4}, std::byte{2}, std::byte{3}};
    EXPECT_EQ(vectorBytes(buffer.view()), expected);
    buffer.reserve(64);
    buffer.append(buffer.view());
    EXPECT_EQ(buffer.size(), 12U);
    EXPECT_EQ(buffer.view()[6], std::byte{1});
    buffer.append(buffer.view().last(2));
    EXPECT_EQ(buffer.view().back(), std::byte{3});
}
TEST(ByteBuffer, AdoptsMovedStorage) {
    std::vector<std::byte> bytes(4096, std::byte{7});
    const auto* original = bytes.data();
    ByteBuffer buffer(std::move(bytes));
    EXPECT_EQ(buffer.view().data(), original);
    EXPECT_EQ(buffer.size(), 4096U);
}
TEST(Binary, ReserveKeepsLimitAndBytes) {
    BinaryWriter writer;
    writer.reserve(64);
    writer.writeU64(UINT64_MAX);
    const auto before = vectorBytes(writer.view());
    EXPECT_THROW(writer.reserve(maxFileBytes + 1), Failure);
    EXPECT_EQ(vectorBytes(writer.view()), before);
    std::vector<std::byte> fill(maxFileBytes - writer.view().size());
    writer.writeBytes(fill);
    EXPECT_THROW(writer.writeU8(1), Failure);
    EXPECT_EQ(writer.view().size(), maxFileBytes);
}
} // namespace chronicle::test
