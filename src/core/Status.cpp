#include "core/Status.h"

#include <utility>

namespace manumesh {

Status::Status() = default;

Status::Status(StatusCode code, std::string message)
    : code_(code), message_(std::move(message)) {
}

Status Status::success() {
  return {};
}

Status Status::invalidArgument(std::string message) {
  return {StatusCode::InvalidArgument, std::move(message)};
}

Status Status::topologyError(std::string message) {
  return {StatusCode::TopologyError, std::move(message)};
}

bool Status::ok() const {
  return code_ == StatusCode::Ok;
}

StatusCode Status::code() const {
  return code_;
}

const std::string& Status::message() const {
  return message_;
}

} // namespace manumesh
