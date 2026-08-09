/**
 * @file include/core/Status.h
 * @brief 声明 ManuMesh 核心网格模块的状态设施。
 * @ingroup manumesh_core
 *
 * @details 核心类型建立所有算法模块共同使用的存储、校验、容差、拓扑和状态契约。
 */

#pragma once

#include "Export.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace manumesh {

/// 不应抛出异常的公共 API 所使用的稳定状态码。
enum class StatusCode {
    Ok = 0,
    InvalidArgument,
    IoError,
    TopologyError,
    AlgorithmError,
    OutOfMemory,
};

/// 用于操作结果和校验诊断的轻量状态对象。
///
/// 对不符合约定的数据执行有效调用时返回 Status 或 Result<T>。异常仅用于
/// 程序员违反契约的情况，绝不会跨越 C ABI。添加入口点前请参阅
/// `documentation/design/error_handling_policy.md`。
class Status {
public:
    /// 构造成功状态。
    MANUMESH_API Status();
    /// @param[in] code 稳定的错误类别。
    /// @param[in] message 面向用户的诊断文本；不是稳定的 API 令牌。
    MANUMESH_API Status(StatusCode code, std::string message);

    MANUMESH_API static Status success();
    MANUMESH_API static Status invalidArgument(std::string message);
    MANUMESH_API static Status topologyError(std::string message);

    /// @return 仅当 code() 为 StatusCode::Ok 时返回 true。
    MANUMESH_API bool ok() const;
    /// @return 稳定的状态类别。
    MANUMESH_API StatusCode code() const;
    /// @return 由此状态拥有的诊断字符串。
    MANUMESH_API const std::string& message() const;

private:
    StatusCode code_ = StatusCode::Ok;
    std::string message_;
};

/// 面向未来无异常 API 的最小值或状态承载器。
///
/// 值存储于 std::optional 中，因此仅在成功结果中构造 T，T 不必支持默认构造。
template <typename T> class Result {
public:
    /// 通过复制值构造成功结果。
    Result(const T& value)
        : value_(value),
          status_(Status::success()) {}
    /// 通过移动值构造成功结果。
    Result(T&& value)
        : value_(std::move(value)),
          status_(Status::success()) {}
    /// 构造不包含值的结果。
    /// @param[in] status 失败状态；调用方不应传入 success()。
    Result(Status status)
        : status_(std::move(status)) {}

    /// @return 存储的状态成功时返回 true。
    bool ok() const { return status_.ok(); }
    /// @return 存在值时返回 true。
    bool hasValue() const { return value_.has_value(); }
    /// @return 以引用形式返回存储的状态。
    const Status& status() const { return status_; }
    /// @return 不可变的存储值。
    /// @throws std::logic_error 不存在值时抛出。
    const T& value() const {
        if (!value_.has_value()) {
            throw std::logic_error(status_.message().empty() ? "Result has no value." : status_.message());
        }
        return *value_;
    }
    /// @return 可变的存储值。
    /// @throws std::logic_error 不存在值时抛出。
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

} // 命名空间 manumesh
