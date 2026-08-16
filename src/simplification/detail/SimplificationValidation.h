/**
 * @file src/simplification/detail/SimplificationValidation.h
 * @brief 声明简化输入网格和选项的入口校验。
 * @ingroup manumesh_simplification
 *
 * @details 校验在分配可变状态前完成，失败直接报告原始配置问题。
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "core/Mesh.h"

namespace manumesh {
namespace simplification {

/**
 * @throws 任一选项或跨字段范围无效时抛出 std::invalid_argument。
 */
void validateSimplifyOptions(const SimplifyOptions& options);
/**
 * @throws 输入无法安全处理时抛出 std::invalid_argument。
 */
void validateSimplifierInput(const Mesh& input);

} // namespace simplification
} // namespace manumesh
