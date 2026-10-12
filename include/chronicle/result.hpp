#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
namespace chronicle {
enum class ErrorCode {
    truncated,
    invalid_format,
    unsupported_version,
    limit_exceeded,
    schema_error,
    io_error,
    invalid_argument
};
struct Error {
    ErrorCode code;
    std::size_t offset;
    std::string context;
    std::string message;
};
class Failure : public std::runtime_error {
  public:
    explicit Failure(Error error)
        : std::runtime_error(error.message), error_(std::move(error)) {}
    const Error& error() const noexcept {
        return error_;
    }

  private:
    Error error_;
};
[[noreturn]] inline void fail(ErrorCode code, std::size_t offset,
                              std::string context, std::string message) {
    throw Failure({code, offset, std::move(context), std::move(message)});
}
template <typename T> class Result {
  public:
    Result(T value) : data_(std::move(value)) {}
    Result(Error error) : data_(std::move(error)) {}
    bool hasValue() const noexcept {
        return std::holds_alternative<T>(data_);
    }
    explicit operator bool() const noexcept {
        return hasValue();
    }
    T& value() & {
        return std::get<T>(data_);
    }
    const T& value() const& {
        return std::get<T>(data_);
    }
    T&& value() && {
        return std::get<T>(std::move(data_));
    }
    const Error& error() const {
        return std::get<Error>(data_);
    }

  private:
    std::variant<T, Error> data_;
};
using Status = Result<std::monostate>;
} // namespace chronicle
