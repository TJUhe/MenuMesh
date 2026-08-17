/**
 * @file src/core/detail/MeshValidation.h
 * @brief 核心拓扑构建使用的已索引校验辅助函数。
 * @ingroup manumesh_core
 */

#pragma once

#include "core/Mesh.h"

namespace manumesh {
namespace detail {

/**
 * @brief 校验几何、UV 和面退化状态，但跳过已完成的索引扫描。
 *
 * 仅供先调用 validateMeshIndices 的内部构建路径使用，避免同一网格重复
 * 扫描全部面索引；对外校验 API 仍保持原有完整语义。
 */
bool validateMeshGeometryLenientAfterIndices(const Mesh& mesh, std::string* error = nullptr);

} // namespace detail
} // namespace manumesh
