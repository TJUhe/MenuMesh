/**
 * @file src/core/Status.cpp
 * @brief 实现 Status 的状态码和诊断文本存储。
 * @ingroup manumesh_core
 *
 * @details 本文件只实现轻量状态值，不执行日志记录或异常转换。
 */

#include "core/Status.h"

#include <utility>

namespace manumesh {

Status::Status() = default;

Status::Status(StatusCode code, std::string message)
    : code_(code),
      message_(std::move(message)) {}

Status Status::success() { return {}; }

Status Status::invalidArgument(std::string message) { return {StatusCode::InvalidArgument, std::move(message)}; }

Status Status::topologyError(std::string message) { return {StatusCode::TopologyError, std::move(message)}; }

bool Status::ok() const { return code_ == StatusCode::Ok; }

StatusCode Status::code() const { return code_; }

const std::string& Status::message() const { return message_; }

} // 命名空间 manumesh
