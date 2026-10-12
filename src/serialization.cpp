#include "chronicle/serialization.hpp"
namespace chronicle {
namespace {
std::uint16_t sourceVersion(std::span<const std::byte> bytes) {
    const auto root = decodeArchive(bytes, sceneType);
    if (root.type != sceneType) {
        fail(ErrorCode::invalid_format, root.offset, "Scene",
             "unexpected type ID");
    }
    if (root.version != 1) {
        fail(ErrorCode::unsupported_version, root.offset + 4, "Scene",
             "unsupported schema version");
    }
    for (const auto& field : root.fields) {
        if (field.id != 2) {
            continue;
        }
        if (field.wire != WireType::object) {
            fail(ErrorCode::invalid_format, field.offset - 12, "Scene.player",
                 "wire type does not match schema");
        }
        BinaryReader reader(field.payload, field.offset, "Scene.player");
        if (reader.readU32() != playerType) {
            fail(ErrorCode::invalid_format, field.offset, "Player",
                 "unexpected type ID");
        }
        const auto version = reader.readU16();
        if (version < 1 || version > 3) {
            fail(ErrorCode::unsupported_version, field.offset + 4, "Player",
                 "unsupported schema version");
        }
        return version;
    }
    return 3; // Missing Player uses the current model's defaults.
}
template <BoundObject T> Result<ByteBuffer> verifiedArchive(const T& value) {
    auto bytes = serializeBound(value);
    if (!bytes) {
        return bytes.error();
    }
    auto verified = deserializeBound<T>(bytes.value().view());
    if (!verified) {
        return verified.error();
    }
    if (verified.value().value != value) {
        return Error{ErrorCode::invalid_format, 0, "migration",
                     "converted data failed round-trip verification"};
    }
    return std::move(bytes).value();
}
Result<ByteBuffer> finishV3(SceneV2 scene) {
    return verifiedArchive(migrateSceneV2ToV3(std::move(scene)));
}
} // namespace
Result<ByteBuffer> serialize(const Scene& scene, std::uint16_t playerVersion) {
    try {
        TypeBinding<Stats>::validate(scene.player.stats);
        if (playerVersion == 1) {
            return serializeBound(projectSceneV1(scene));
        }
        if (playerVersion == 2) {
            return serializeBound(projectSceneV2(scene));
        }
        if (playerVersion == 3) {
            return serializeBound(scene);
        }
        return Error{ErrorCode::unsupported_version, 0, "Player",
                     "supported schema versions are 1, 2 and 3"};
    } catch (const Failure& failure) {
        return failure.error();
    }
}
Result<LoadedScene> deserialize(std::span<const std::byte> bytes) {
    try {
        const auto version = sourceVersion(bytes);
        if (version == 1) {
            auto loaded = deserializeBound<SceneV1>(bytes);
            if (!loaded) {
                return loaded.error();
            }
            auto object = std::move(loaded).value();
            return LoadedScene{
                migrateSceneV2ToV3(migrateSceneV1ToV2(std::move(object.value))),
                1, std::move(object.root)};
        }
        if (version == 2) {
            auto loaded = deserializeBound<SceneV2>(bytes);
            if (!loaded) {
                return loaded.error();
            }
            auto object = std::move(loaded).value();
            return LoadedScene{migrateSceneV2ToV3(std::move(object.value)), 2,
                               std::move(object.root)};
        }
        auto loaded = deserializeBound<Scene>(bytes);
        if (!loaded) {
            return loaded.error();
        }
        auto object = std::move(loaded).value();
        return LoadedScene{std::move(object.value), 3, std::move(object.root)};
    } catch (const Failure& failure) {
        return failure.error();
    }
}
Result<ByteBuffer> migrateArchive(std::span<const std::byte> bytes,
                                  std::uint16_t targetVersion) {
    try {
        if (targetVersion != 2 && targetVersion != 3) {
            return Error{ErrorCode::unsupported_version, 0, "migration",
                         "supported migration targets are 2 and 3"};
        }
        const auto version = sourceVersion(bytes);
        if (version == 1) {
            auto source = deserializeBound<SceneV1>(bytes);
            if (!source) {
                return source.error();
            }
            auto v2 = migrateSceneV1ToV2(std::move(source).value().value);
            auto intermediate = verifiedArchive(v2);
            if (!intermediate) {
                return intermediate.error();
            }
            if (targetVersion == 2) {
                return std::move(intermediate).value();
            }
            // Materialize the verified intermediate before the next edge.
            auto verifiedV2 =
                deserializeBound<SceneV2>(intermediate.value().view());
            if (!verifiedV2) {
                return verifiedV2.error();
            }
            return finishV3(std::move(verifiedV2).value().value);
        }
        if (version == 2) {
            auto source = deserializeBound<SceneV2>(bytes);
            if (!source) {
                return source.error();
            }
            if (targetVersion == 3) {
                return finishV3(std::move(source).value().value);
            }
        } else {
            auto source = deserializeBound<Scene>(bytes);
            if (!source) {
                return source.error();
            }
        }
        return Error{ErrorCode::unsupported_version, 0, "migration",
                     "only forward upgrades are supported; source is already "
                     "at or above target"};
    } catch (const Failure& failure) {
        return failure.error();
    }
}
} // namespace chronicle
