/**
 * @file include/core/Handles.h
 * @brief 定义顶点、边和面的强类型整数句柄。
 * @ingroup manumesh_core
 *
 * @details 不同拓扑域的句柄不可隐式互换，底层索引仍保持紧凑整数表示。
 */

#pragma once

#include <cstdint>

namespace manumesh {

/**
 * @brief 用于稠密拓扑存储的强类型整数句柄。
 *
 * 不同的标签类型可防止顶点、面、边和半边索引被意外混用，同时保留
 * `int` 的存储和比较成本。默认构造的句柄无效。
 *
 * @tparam Tag 用于定义句柄域的空类型。
 */
template <typename Tag> struct Handle {
    ///< 从零开始的稠密索引；无效时为 -1。
    int id = -1;

    /// 创建无效句柄。
    constexpr Handle() = default;
    /// 根据从零开始的稠密索引创建句柄。
    /// @param[in] value 索引值；负值仍表示无效。
    explicit constexpr Handle(int value)
        : id(value) {}

    /// @return 当句柄表示非负索引时返回 true。
    constexpr bool valid() const { return id >= 0; }
    /// @return 未经校验的已存储稠密索引。
    constexpr explicit operator int() const { return id; }

    friend constexpr bool operator==(Handle a, Handle b) { return a.id == b.id; }
    friend constexpr bool operator!=(Handle a, Handle b) { return a.id != b.id; }
    friend constexpr bool operator<(Handle a, Handle b) { return a.id < b.id; }
};

struct VertexTag {};
struct FaceTag {};
struct EdgeTag {};
struct HalfedgeTag {};

using VertexId = Handle<VertexTag>;     ///< 顶点句柄。
using FaceId = Handle<FaceTag>;         ///< 面句柄。
using EdgeId = Handle<EdgeTag>;         ///< 无向边句柄。
using HalfedgeId = Handle<HalfedgeTag>; ///< 有向半边句柄。

} // 命名空间 manumesh
