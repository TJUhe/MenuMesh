/**
 * @file src/core/Status.cpp
 * @brief 实现 ManuMesh 核心网格模块的状态设施。
 * @ingroup manumesh_core
 *
 * @details 核心类型建立所有算法模块共同使用的存储、校验、容差、拓扑和状态契约。
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
