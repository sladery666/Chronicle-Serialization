#pragma once
#include "chronicle/binary_reader.hpp"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
namespace chronicle {
using TypeId = std::uint32_t;
using FieldId = std::uint32_t;
inline constexpr TypeId sceneType = 1;
inline constexpr TypeId playerType = 2;
inline constexpr TypeId vec3Type = 3;
inline constexpr TypeId itemType = 4;
inline constexpr TypeId statsType = 5;
enum class WireType : std::uint8_t {
    u64 = 1,
    i32 = 2,
    i64 = 3,
    f32 = 4,
    string = 5,
    object = 6,
    array = 7
};
struct FieldSchema {
    FieldId id;
    std::string name;
    WireType wire;
    WireType elementWire = WireType::object;
    bool operator==(const FieldSchema&) const = default;
};
struct TypeSchema {
    TypeId id;
    std::uint16_t version;
    std::string name;
    std::vector<FieldSchema> fields;
    bool operator==(const TypeSchema&) const = default;
};
class SchemaRegistry {
  public:
    Status registerType(TypeSchema schema);
    const TypeSchema* find(TypeId id, std::uint16_t version) const noexcept;

  private:
    std::vector<TypeSchema> types_;
};
const SchemaRegistry& builtinSchemas();
struct FieldRecord {
    FieldId id;
    WireType wire;
    ByteBuffer payload;
    std::size_t offset = 0;
};
struct ObjectRecord {
    TypeId type;
    std::uint16_t version;
    std::vector<FieldRecord> fields;
    std::size_t offset = 0;
};
// Views borrow the reader input; invalidated when its owner dies or mutates.
// deserialize() uses them only during parsing and returns owned records.
struct FieldView {
    FieldId id;
    WireType wire;
    std::span<const std::byte> payload;
    std::size_t offset = 0;
};
struct ObjectView {
    TypeId type;
    std::uint16_t version;
    std::vector<FieldView> fields;
    std::size_t offset = 0;
};
ObjectView decodeObjectView(BinaryReader& reader);
ObjectRecord ownObject(const ObjectView& object);
ByteBuffer encodeObject(ObjectRecord object);
ObjectRecord decodeObject(BinaryReader& reader);
} // namespace chronicle
