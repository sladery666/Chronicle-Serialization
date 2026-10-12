#pragma once
#include "chronicle/schema.hpp"
#include <cmath>
#include <concepts>
#include <limits>
#include <tuple>
#include <type_traits>
namespace chronicle {
// Specialize with stable IDs, a version range, and fields() returning a tuple.
template <typename T> struct TypeBinding;
struct BindingOptions {
    std::uint16_t playerVersion = 3;
};
inline constexpr unsigned maxBindingDepth = 32;
template <typename T>
concept BoundObject = std::default_initializable<T> && requires {
    { TypeBinding<T>::typeId } -> std::convertible_to<TypeId>;
    { TypeBinding<T>::name } -> std::convertible_to<std::string_view>;
    { TypeBinding<T>::minVersion } -> std::convertible_to<std::uint16_t>;
    { TypeBinding<T>::maxVersion } -> std::convertible_to<std::uint16_t>;
    TypeBinding<T>::fields();
};
namespace binding_detail {
template <typename T> struct VectorTraits {
    static constexpr bool value = false;
};
template <typename T, typename A> struct VectorTraits<std::vector<T, A>> {
    static constexpr bool value = true;
    using Element = T;
};
template <typename T> consteval bool supported() {
    if constexpr (std::same_as<T, std::uint64_t> ||
                  std::same_as<T, std::int32_t> ||
                  std::same_as<T, std::int64_t> || std::same_as<T, float> ||
                  std::same_as<T, std::string> || BoundObject<T>) {
        return true;
    } else if constexpr (VectorTraits<T>::value) {
        return std::default_initializable<T> &&
               supported<typename VectorTraits<T>::Element>();
    } else {
        return false;
    }
}
} // namespace binding_detail
template <typename T>
concept BoundValue = binding_detail::supported<T>();
template <typename O, typename M> struct MemberBinding {
    using Object = O;
    using Member = M;
    FieldId id;
    std::string_view name;
    M O::* member;
    std::uint16_t since = 1;
    bool legacyInt32 = false;
    constexpr bool active(std::uint16_t version) const {
        return version >= since;
    }
};
template <typename O, BoundValue M>
constexpr MemberBinding<O, M> bindField(FieldId id, std::string_view name,
                                        M O::* member,
                                        std::uint16_t since = 1) {
    return {id, name, member, since, false};
}
template <typename O>
constexpr MemberBinding<O, std::int64_t>
bindLegacyInt32(FieldId id, std::string_view name, std::int64_t O::* member) {
    return {id, name, member, 1, true};
}
namespace binding_detail {
template <BoundValue T> constexpr WireType wire() {
    if constexpr (std::same_as<T, std::uint64_t>) {
        return WireType::u64;
    } else if constexpr (std::same_as<T, std::int32_t>) {
        return WireType::i32;
    } else if constexpr (std::same_as<T, std::int64_t>) {
        return WireType::i64;
    } else if constexpr (std::same_as<T, float>) {
        return WireType::f32;
    } else if constexpr (std::same_as<T, std::string>) {
        return WireType::string;
    } else if constexpr (BoundObject<T>) {
        return WireType::object;
    } else {
        return WireType::array;
    }
}
template <typename F>
constexpr WireType fieldWire(const F& field, std::uint16_t version) {
    return field.legacyInt32 && version == 1 ? WireType::i32
                                             : wire<typename F::Member>();
}
inline void checkDepth(unsigned depth) {
    if (depth > maxBindingDepth) {
        fail(ErrorCode::limit_exceeded, 0, "binding",
             "nesting depth limit exceeded");
    }
}
template <BoundObject T> TypeSchema makeSchema(std::uint16_t version) {
    static_assert(TypeBinding<T>::minVersion > 0 &&
                  TypeBinding<T>::minVersion <= TypeBinding<T>::maxVersion);
    TypeSchema schema{
        TypeBinding<T>::typeId, version, std::string(TypeBinding<T>::name), {}};
    std::apply(
        [&](const auto&... fields) {
            [[maybe_unused]] auto add = [&](const auto& field) {
                using F = std::decay_t<decltype(field)>;
                static_assert(std::same_as<typename F::Object, T>,
                              "field member belongs to a different type");
                if (!field.since || field.since > TypeBinding<T>::maxVersion ||
                    (field.legacyInt32 &&
                     !std::same_as<typename F::Member, std::int64_t>)) {
                    fail(ErrorCode::schema_error, 0, schema.name,
                         "invalid field binding options");
                }
                if (field.active(version)) {
                    WireType element = WireType::object;
                    if constexpr (VectorTraits<typename F::Member>::value) {
                        element = wire<typename VectorTraits<
                            typename F::Member>::Element>();
                    }
                    schema.fields.push_back({field.id, std::string(field.name),
                                             fieldWire(field, version),
                                             element});
                }
            };
            (add(fields), ...);
        },
        TypeBinding<T>::fields());
    return schema;
}
template <BoundObject T> const TypeSchema& schema(std::uint16_t version) {
    // Metadata validation runs once, independently of hot-path object parsing.
    static const SchemaRegistry registry = [] {
        SchemaRegistry result;
        for (std::uint32_t v = TypeBinding<T>::minVersion;
             v <= TypeBinding<T>::maxVersion; ++v) {
            auto status = result.registerType(
                makeSchema<T>(static_cast<std::uint16_t>(v)));
            if (!status) {
                throw Failure(status.error());
            }
        }
        return result;
    }();
    const auto* result = registry.find(TypeBinding<T>::typeId, version);
    if (!result) {
        fail(ErrorCode::unsupported_version, 0,
             std::string(TypeBinding<T>::name), "unsupported schema version");
    }
    return *result;
}
template <BoundObject T>
ObjectRecord encode(const T&, const BindingOptions&, unsigned);
template <BoundObject T> T decode(const ObjectView&, unsigned);
template <BoundValue T>
void writeValue(BinaryWriter& writer, const T& value,
                const BindingOptions& options, unsigned depth) {
    checkDepth(depth);
    if constexpr (std::same_as<T, std::uint64_t>) {
        writer.writeU64(value);
    } else if constexpr (std::same_as<T, std::int32_t>) {
        writer.writeI32(value);
    } else if constexpr (std::same_as<T, std::int64_t>) {
        writer.writeI64(value);
    } else if constexpr (std::same_as<T, float>) {
        if (!std::isfinite(value)) {
            fail(ErrorCode::invalid_format, writer.view().size(), "float",
                 "value must be finite");
        }
        writer.writeFloat(value);
    } else if constexpr (std::same_as<T, std::string>) {
        writer.writeString(value);
    } else if constexpr (BoundObject<T>) {
        auto bytes = encodeObject(encode(value, options, depth));
        writer.writeBytes(bytes.view());
    } else {
        if (value.size() > maxArrayElements) {
            fail(ErrorCode::limit_exceeded, writer.view().size(), "array",
                 "element count limit exceeded");
        }
        writer.writeU32(static_cast<std::uint32_t>(value.size()));
        for (const auto& element : value) {
            BinaryWriter payload;
            writeValue(payload, element, options, depth + 1);
            writer.writeU64(payload.view().size());
            writer.writeBytes(payload.view());
        }
    }
}
template <BoundValue T> constexpr std::size_t minimumBytes() {
    if constexpr (std::same_as<T, std::int32_t> || std::same_as<T, float> ||
                  std::same_as<T, std::string> || VectorTraits<T>::value) {
        return 4;
    } else {
        return 8;
    }
}
template <BoundValue T> T readValue(BinaryReader& reader, unsigned depth) {
    checkDepth(depth);
    if constexpr (std::same_as<T, std::uint64_t>) {
        return reader.readU64();
    } else if constexpr (std::same_as<T, std::int32_t>) {
        return reader.readI32();
    } else if constexpr (std::same_as<T, std::int64_t>) {
        return reader.readI64();
    } else if constexpr (std::same_as<T, float>) {
        const auto offset = reader.offset();
        const auto value = reader.readFloat();
        if (!std::isfinite(value)) {
            fail(ErrorCode::invalid_format, offset, "float",
                 "value must be finite");
        }
        return value;
    } else if constexpr (std::same_as<T, std::string>) {
        return reader.readString();
    } else if constexpr (BoundObject<T>) {
        return decode<T>(decodeObjectView(reader), depth);
    } else {
        using E = typename VectorTraits<T>::Element;
        const auto offset = reader.offset();
        const auto count = reader.readU32();
        if (count > maxArrayElements) {
            fail(ErrorCode::limit_exceeded, offset, "array",
                 "element count limit exceeded");
        }
        if (count > reader.remaining() / (8 + minimumBytes<E>())) {
            fail(ErrorCode::truncated, reader.offset(), "array",
                 "array entries do not fit input");
        }
        T result;
        result.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto size = reader.readU64();
            if (size > maxFileBytes) {
                fail(ErrorCode::limit_exceeded, reader.offset() - 8, "array",
                     "element size limit exceeded");
            }
            const auto base = reader.offset();
            BinaryReader input(reader.readBytes(static_cast<std::size_t>(size)),
                               base, "array element");
            result.push_back(readValue<E>(input, depth + 1));
            input.requireEnd();
        }
        return result;
    }
}
template <BoundObject T>
ObjectRecord encode(const T& value, const BindingOptions& options,
                    unsigned depth) {
    checkDepth(depth);
    auto version = static_cast<std::uint16_t>(TypeBinding<T>::maxVersion);
    if constexpr (requires { TypeBinding<T>::version(options); }) {
        version = TypeBinding<T>::version(options);
    }
    const auto& metadata = schema<T>(version);
    if constexpr (requires { TypeBinding<T>::validate(value); }) {
        TypeBinding<T>::validate(value);
    }
    ObjectRecord result{metadata.id, version, {}, 0};
    result.fields.reserve(metadata.fields.size());
    std::apply(
        [&](const auto&... fields) {
            [[maybe_unused]] auto add = [&](const auto& field) {
                if (!field.active(version)) {
                    return;
                }
                using M = typename std::decay_t<decltype(field)>::Member;
                BinaryWriter writer;
                if constexpr (std::same_as<M, std::int64_t>) {
                    if (field.legacyInt32 && version == 1) {
                        const auto number = value.*(field.member);
                        if (number < INT32_MIN || number > INT32_MAX) {
                            fail(ErrorCode::invalid_argument, 0,
                                 metadata.name + "." + std::string(field.name),
                                 "V1 value does not fit int32");
                        }
                        writer.writeI32(static_cast<std::int32_t>(number));
                    } else {
                        writeValue(writer, value.*(field.member), options,
                                   depth + 1);
                    }
                } else {
                    writeValue(writer, value.*(field.member), options,
                               depth + 1);
                }
                result.fields.push_back({field.id, fieldWire(field, version),
                                         std::move(writer).take(), 0});
            };
            (add(fields), ...);
        },
        TypeBinding<T>::fields());
    return result;
}
template <BoundObject T> T decode(const ObjectView& object, unsigned depth) {
    checkDepth(depth);
    if (object.type != TypeBinding<T>::typeId) {
        fail(ErrorCode::invalid_format, object.offset,
             std::string(TypeBinding<T>::name), "unexpected type ID");
    }
    if (object.version < TypeBinding<T>::minVersion ||
        object.version > TypeBinding<T>::maxVersion) {
        fail(ErrorCode::unsupported_version, object.offset + 4,
             std::string(TypeBinding<T>::name), "unsupported schema version");
    }
    const auto& metadata = schema<T>(object.version);
    T result{};
    for (const auto& record : object.fields) {
        std::apply(
            [&](const auto&... fields) {
                [[maybe_unused]] auto read = [&](const auto& field) {
                    if (field.id != record.id ||
                        !field.active(object.version)) {
                        return;
                    }
                    const auto context =
                        metadata.name + "." + std::string(field.name);
                    if (fieldWire(field, object.version) != record.wire) {
                        fail(ErrorCode::invalid_format, record.offset - 12,
                             context, "wire type does not match schema");
                    }
                    BinaryReader reader(record.payload, record.offset, context);
                    using M = typename std::decay_t<decltype(field)>::Member;
                    if constexpr (std::same_as<M, std::int64_t>) {
                        if (field.legacyInt32 && object.version == 1) {
                            result.*(field.member) = reader.readI32();
                        } else {
                            result.*(field.member) =
                                readValue<M>(reader, depth + 1);
                        }
                    } else {
                        result.*(field.member) =
                            readValue<M>(reader, depth + 1);
                    }
                    reader.requireEnd();
                };
                (read(fields), ...);
            },
            TypeBinding<T>::fields());
    }
    if constexpr (requires {
                      TypeBinding<T>::finish(std::move(result), object.version);
                  }) {
        result = TypeBinding<T>::finish(std::move(result), object.version);
    }
    if constexpr (requires { TypeBinding<T>::validate(result); }) {
        TypeBinding<T>::validate(result);
    }
    return result;
}
template <BoundValue T> void registerDependencies(SchemaRegistry& registry);
template <BoundObject T> void registerType(SchemaRegistry& registry) {
    for (std::uint32_t v = TypeBinding<T>::minVersion;
         v <= TypeBinding<T>::maxVersion; ++v) {
        const auto version = static_cast<std::uint16_t>(v);
        const auto& metadata = schema<T>(version);
        const auto* existing = registry.find(metadata.id, version);
        if (existing) {
            if (*existing != metadata) {
                fail(ErrorCode::schema_error, 0, metadata.name,
                     "type binding collision");
            }
        } else {
            auto status = registry.registerType(metadata);
            if (!status) {
                throw Failure(status.error());
            }
        }
    }
    std::apply(
        [&](const auto&... fields) {
            (registerDependencies<
                 typename std::decay_t<decltype(fields)>::Member>(registry),
             ...);
        },
        TypeBinding<T>::fields());
}
template <BoundValue T> void registerDependencies(SchemaRegistry& registry) {
    if constexpr (BoundObject<T>) {
        registerType<T>(registry);
    } else if constexpr (VectorTraits<T>::value) {
        registerDependencies<typename VectorTraits<T>::Element>(registry);
    }
}
} // namespace binding_detail
template <BoundObject T> Status registerBinding(SchemaRegistry& registry) {
    try {
        auto candidate = registry;
        binding_detail::registerType<T>(candidate);
        registry = std::move(candidate);
        return std::monostate{};
    } catch (const Failure& failure) {
        return failure.error();
    }
}
template <BoundObject T>
ObjectRecord encodeBoundObject(const T& value,
                               const BindingOptions& options = {}) {
    return binding_detail::encode(value, options, 0);
}
template <BoundObject T> T decodeBoundObject(const ObjectView& object) {
    return binding_detail::decode<T>(object, 0);
}
} // namespace chronicle
