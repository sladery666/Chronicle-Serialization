#pragma once
#include "chronicle/archive.hpp"
#include "chronicle/type_binding.hpp"
namespace chronicle {
template <BoundObject T> struct LoadedObject {
    T value;
    ObjectRecord root;
};
template <BoundObject T>
Result<ByteBuffer> serializeBound(const T& value,
                                  const BindingOptions& options = {}) {
    try {
        auto payload = encodeObject(encodeBoundObject(value, options));
        return encodeArchive(TypeBinding<T>::typeId, payload.view());
    } catch (const Failure& failure) {
        return failure.error();
    }
}
template <BoundObject T>
Result<LoadedObject<T>> deserializeBound(std::span<const std::byte> bytes) {
    try {
        auto view = decodeArchive(bytes, TypeBinding<T>::typeId);
        auto value = decodeBoundObject<T>(view);
        return LoadedObject<T>{std::move(value), ownObject(view)};
    } catch (const Failure& failure) {
        return failure.error();
    }
}
template <BoundObject T>
Status saveBound(const std::filesystem::path& path, const T& value,
                 const BindingOptions& options = {}) {
    auto bytes = serializeBound(value, options);
    if (!bytes) {
        return bytes.error();
    }
    return writeFile(path, bytes.value().view());
}
template <BoundObject T>
Result<LoadedObject<T>> loadBound(const std::filesystem::path& path) {
    auto bytes = readFile(path);
    if (!bytes) {
        return bytes.error();
    }
    return deserializeBound<T>(bytes.value().view());
}
} // namespace chronicle
