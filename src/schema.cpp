#include "chronicle/schema.hpp"
#include "chronicle/scene_bindings.hpp"
#include <array>
#include <limits>
#include <set>
namespace chronicle {
Status SchemaRegistry::registerType(TypeSchema schema) {
    if (schema.id == 0 || schema.version == 0 || schema.name.empty() ||
        schema.fields.size() > 1024 || find(schema.id, schema.version)) {
        return Error{ErrorCode::schema_error, 0, schema.name,
                     "invalid or duplicate type/version ID"};
    }
    for (const auto& type : types_) {
        if ((type.id == schema.id && type.name != schema.name) ||
            (type.name == schema.name && type.id != schema.id)) {
            return Error{ErrorCode::schema_error, 0, schema.name,
                         "type ID/name collision"};
        }
    }
    std::set<FieldId> ids;
    std::set<std::string> names;
    for (const auto& field : schema.fields) {
        const auto wire = static_cast<std::uint8_t>(field.wire);
        const auto element = static_cast<std::uint8_t>(field.elementWire);
        if (!field.id || field.name.empty() || !ids.insert(field.id).second ||
            !names.insert(field.name).second || wire < 1 || wire > 7 ||
            (field.wire == WireType::array && (element < 1 || element > 7))) {
            return Error{ErrorCode::schema_error, 0, schema.name,
                         "invalid or duplicate field"};
        }
    }
    std::sort(schema.fields.begin(), schema.fields.end(),
              [](const auto& a, const auto& b) { return a.id < b.id; });
    types_.push_back(std::move(schema));
    return std::monostate{};
}
const TypeSchema* SchemaRegistry::find(TypeId id,
                                       std::uint16_t version) const noexcept {
    for (const auto& schema : types_) {
        if (schema.id == id && schema.version == version) {
            return &schema;
        }
    }
    return nullptr;
}
const SchemaRegistry& builtinSchemas() {
    static const SchemaRegistry registry = [] {
        SchemaRegistry result;
        auto status = registerBinding<Scene>(result);
        if (!status) {
            throw Failure(status.error());
        }
        status = registerBinding<SceneV1>(result);
        if (!status) {
            throw Failure(status.error());
        }
        status = registerBinding<SceneV2>(result);
        if (!status) {
            throw Failure(status.error());
        }
        return result;
    }();
    return registry;
}
ByteBuffer encodeObject(ObjectRecord object) {
    if (object.fields.size() > 1024) {
        fail(ErrorCode::limit_exceeded, 0, "object",
             "field count limit exceeded");
    }
    std::sort(object.fields.begin(), object.fields.end(),
              [](const auto& a, const auto& b) { return a.id < b.id; });
    std::size_t total = 8;
    for (const auto& field : object.fields) {
        if (total > maxFileBytes - 16 ||
            field.payload.size() > maxFileBytes - total - 16) {
            fail(ErrorCode::limit_exceeded, 0, "object",
                 "object size limit exceeded");
        }
        total += 16 + field.payload.size();
    }
    BinaryWriter writer;
    writer.reserve(total);
    writer.writeU32(object.type);
    writer.writeU16(object.version);
    writer.writeU16(static_cast<std::uint16_t>(object.fields.size()));
    FieldId previous = 0;
    for (const auto& field : object.fields) {
        if (field.id == 0 || field.id == previous) {
            fail(ErrorCode::schema_error, 0, "object",
                 "invalid or duplicate field ID");
        }
        previous = field.id;
        writer.writeU32(field.id);
        writer.writeU8(static_cast<std::uint8_t>(field.wire));
        writer.writeU8(0);
        writer.writeU16(0);
        writer.writeU64(field.payload.size());
        writer.writeBytes(field.payload.view());
    }
    return std::move(writer).take();
}
ObjectView decodeObjectView(BinaryReader& reader) {
    const auto offset = reader.offset();
    ObjectView object{reader.readU32(), reader.readU16(), {}, offset};
    const auto count = reader.readU16();
    if (!object.type || !object.version) {
        fail(ErrorCode::invalid_format, offset, "object",
             "zero type or schema version");
    }
    if (count > 1024) {
        fail(ErrorCode::limit_exceeded, offset + 6, "object",
             "field count limit exceeded");
    }
    if (count > reader.remaining() / 16) {
        fail(ErrorCode::truncated, reader.offset(), "object",
             "field headers do not fit input");
    }
    object.fields.reserve(count);
    std::array<FieldId, 1024> seen;
    for (std::uint16_t i = 0; i < count; ++i) {
        const auto headerOffset = reader.offset();
        const auto id = reader.readU32();
        const auto wire = reader.readU8();
        const auto flags = reader.readU8();
        const auto reserved = reader.readU16();
        const auto size = reader.readU64();
        const std::string context = "type " + std::to_string(object.type) +
                                    ".field " + std::to_string(id);
        const auto seenEnd = seen.begin() + i;
        if (!id || std::find(seen.begin(), seenEnd, id) != seenEnd || flags ||
            reserved) {
            fail(ErrorCode::invalid_format, headerOffset, context,
                 "invalid field header or duplicate ID");
        }
        seen[i] = id;
        if (size > maxFileBytes) {
            fail(ErrorCode::limit_exceeded, headerOffset + 8, context,
                 "field size limit exceeded");
        }
        if (size > reader.remaining()) {
            fail(ErrorCode::truncated, reader.offset(), context,
                 "field payload exceeds remaining input");
        }
        const auto payloadOffset = reader.offset();
        object.fields.push_back(
            {id, static_cast<WireType>(wire),
             reader.readBytes(static_cast<std::size_t>(size)), payloadOffset});
    }
    reader.requireEnd();
    return object;
}
ObjectRecord ownObject(const ObjectView& view) {
    ObjectRecord object{view.type, view.version, {}, view.offset};
    object.fields.reserve(view.fields.size());
    for (const auto& field : view.fields) {
        FieldRecord owned{field.id, field.wire, {}, field.offset};
        owned.payload.append(field.payload);
        object.fields.push_back(std::move(owned));
    }
    return object;
}
ObjectRecord decodeObject(BinaryReader& reader) {
    return ownObject(decodeObjectView(reader));
}
} // namespace chronicle
