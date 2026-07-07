#pragma once

#include "manumesh/Export.h"

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
template <typename T> class Result {
public:
  Result(const T& value) : value_(value), status_(Status::success()), hasValue_(true) {}
  Result(T&& value)
      : value_(std::move(value)), status_(Status::success()), hasValue_(true) {}
  Result(Status status) : status_(std::move(status)), hasValue_(false) {}

  bool ok() const { return status_.ok(); }
  const Status& status() const { return status_; }
  const T& value() const { return value_; }
  T& value() { return value_; }

private:
  T value_{};
  Status status_;
  bool hasValue_ = false;
};

} // namespace manumesh
