/**
 * @file src/simplification/detail/SimplificationValidation.h
 * @brief 声明 ManuMesh 的简化模块的简化 校验功能。
 * @ingroup manumesh_simplification
 *
 * @details 本文件属于带特征感知的边折叠流水线。二次误差代价用于排序候选；拓扑、几何、特征、边界、误差及可选纹理策略共同决定一个放置是否可以修改网格。
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "core/Mesh.h"

namespace manumesh::simplification {

/**
 * @throws 任一选项或跨字段范围无效时抛出 std::invalid_argument。
 */
void validateSimplifyOptions(const SimplifyOptions& options);
/**
 * @throws 输入无法安全处理时抛出 std::invalid_argument。
 */
void validateSimplifierInput(const Mesh& input);

} // 结束 manumesh::simplification 命名空间
