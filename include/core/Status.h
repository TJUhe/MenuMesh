/**
 * @file include/core/Status.h
 * @brief Declares status facilities for ManuMesh's core-mesh module.
 * @ingroup manumesh_core
 *
 * @details Core types establish the storage, validation, tolerance, topology, and status contracts consumed by every algorithm module.
 */

#pragma once

#include "Export.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace manumesh {

/// Stable status codes used by public APIs that should not throw exceptions.
enum class StatusCode {
    Ok = 0,
    InvalidArgument,
    IoError,
    TopologyError,
    AlgorithmError,
    OutOfMemory,
};

/// Lightweight status object for operation results and validation diagnostics.
///
/// Valid calls on non-conforming data return Status or Result<T>. Exceptions
/// are reserved for programmer contract violations and never cross the C ABI.
/// See `documentation/design/error_handling_policy.md` before adding an entry
/// point.
class Status {
public:
    /// Constructs a successful status.
    MANUMESH_API Status();
    /// @param[in] code Stable error category.
    /// @param[in] message Human-readable diagnostic; not a stable API token.
    MANUMESH_API Status(StatusCode code, std::string message);

    MANUMESH_API static Status success();
    MANUMESH_API static Status invalidArgument(std::string message);
    MANUMESH_API static Status topologyError(std::string message);

    /// @return true only when code() is StatusCode::Ok.
    MANUMESH_API bool ok() const;
    /// @return Stable status category.
    MANUMESH_API StatusCode code() const;
    /// @return Diagnostic string owned by this status.
    MANUMESH_API const std::string& message() const;

private:
    StatusCode code_ = StatusCode::Ok;
    std::string message_;
};

/// Minimal value-or-status carrier for future APIs that avoid exceptions.
///
/// Values are stored in std::optional, so T is only constructed for success
/// results and does not need to be default constructible.
template <typename T> class Result {
public:
    /// Constructs a successful result by copying a value.
    Result(const T& value)
        : value_(value),
          status_(Status::success()) {}
    /// Constructs a successful result by moving a value.
    Result(T&& value)
        : value_(std::move(value)),
          status_(Status::success()) {}
    /// Constructs a result without a value.
    /// @param[in] status Failure status; callers should not pass success().
    Result(Status status)
        : status_(std::move(status)) {}

    /// @return true when the stored status is successful.
    bool ok() const { return status_.ok(); }
    /// @return true when a value is present.
    bool hasValue() const { return value_.has_value(); }
    /// @return Stored status by reference.
    const Status& status() const { return status_; }
    /// @return Immutable stored value.
    /// @throws std::logic_error when no value is present.
    const T& value() const {
        if (!value_.has_value()) {
            throw std::logic_error(status_.message().empty() ? "Result has no value." : status_.message());
        }
        return *value_;
    }
    /// @return Mutable stored value.
    /// @throws std::logic_error when no value is present.
    T& value() {
        if (!value_.has_value()) {
            throw std::logic_error(status_.message().empty() ? "Result has no value." : status_.message());
        }
        return *value_;
    }

private:
    std::optional<T> value_;
    Status status_;
};

} // namespace manumesh
