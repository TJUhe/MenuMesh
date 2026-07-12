#pragma once

// Status/Result are the "data error" track of the ManuMesh error-handling
// policy: valid calls on non-conforming input return a Status (or Result<T>)
// instead of throwing; exceptions are reserved for API-contract violations
// and never cross the C ABI. The full one-page decision table lives in
// docs/design/error_handling_policy.md - consult it before adding any new
// public entry point.
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
class Status {
public:
    MANUMESH_API Status();
    MANUMESH_API Status(StatusCode code, std::string message);

    MANUMESH_API static Status success();
    MANUMESH_API static Status invalidArgument(std::string message);
    MANUMESH_API static Status topologyError(std::string message);

    MANUMESH_API bool ok() const;
    MANUMESH_API StatusCode code() const;
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
    Result(const T& value)
        : value_(value),
          status_(Status::success()) {}
    Result(T&& value)
        : value_(std::move(value)),
          status_(Status::success()) {}
    Result(Status status)
        : status_(std::move(status)) {}

    bool ok() const { return status_.ok(); }
    bool hasValue() const { return value_.has_value(); }
    const Status& status() const { return status_; }
    const T& value() const {
        if (!value_.has_value()) {
            throw std::logic_error(status_.message().empty() ? "Result has no value." : status_.message());
        }
        return *value_;
    }
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
