/**
 * @file include/core/MeshGenerators.h
 * @brief 声明示例、解析测试和压力测试使用的确定性网格生成器。
 * @ingroup manumesh_core
 *
 * @details 每个生成器产生参数可控的三角网格，用于复现边界、特征、噪声和采样密度场景。
 */

#pragma once

#include "Export.h"
#include "core/Mesh.h"

#include <string>

namespace manumesh {

/// 生成矩形平面网格；聚簇模式会有意使网格单元偏斜。
/// @param[in] n 每个轴向的网格细分数。
/// @param[in] size 模型单位下的边长。
/// @param[in] clustered 是否使用有意的非均匀采样。
MANUMESH_API Mesh generatePlaneGrid(int n, double size, bool clustered);
/// 生成带圆形孔洞的平面网格。
/// @param[in] n 近似网格/周向分辨率。
/// @param[in] size 外部边长。
/// @param[in] radius 孔洞半径。
MANUMESH_API Mesh generateHolePlaneGrid(int n, double size, double radius);
/// 生成带凸起脊线的平面网格。
/// @param[in] n 每个轴向的网格细分数。
/// @param[in] size 边长。
/// @param[in] height 脊线高度。
MANUMESH_API Mesh generateRidgeGrid(int n, double size, double height);
/// 生成适合压力测试法向敏感行为的噪声平面。
/// @param[in] n 每个轴向的网格细分数。
/// @param[in] size 边长。
/// @param[in] noiseAmplitude 确定性位移幅度。
MANUMESH_API Mesh generateNoisyPlaneGrid(int n, double size, double noiseAmplitude);
/// 生成平滑正弦波地形。
/// @param[in] n 每个轴向的网格细分数。
/// @param[in] size 边长。
MANUMESH_API Mesh generateSineTerrainGrid(int n, double size);
/// 生成带明显台阶特征的阶梯地形。
/// @param[in] n 每个轴向的网格细分数。
/// @param[in] size 边长。
MANUMESH_API Mesh generateTerraceGrid(int n, double size);
/// 生成平滑凸起表面。
/// @param[in] n 每个轴向的网格细分数。
/// @param[in] size 边长。
MANUMESH_API Mesh generateBumpGrid(int n, double size);
/// 生成带端盖的圆柱网格。
/// @param[in] radialSegments 轴向周围的采样数。
/// @param[in] heightSegments 侧壁细分数。
/// @param[in] radius 圆柱半径。
/// @param[in] height 轴向总高度。
MANUMESH_API Mesh generateCylinderGrid(int radialSegments, int heightSegments, double radius, double height);
/// 生成环面网格。
/// @param[in] majorSegments 大圆周围的采样数。
/// @param[in] minorSegments 管截面圆周围的采样数。
/// @param[in] majorRadius 原点到管中心线的距离。
/// @param[in] minorRadius 管半径。
MANUMESH_API Mesh generateTorusGrid(int majorSegments, int minorSegments, double majorRadius, double minorRadius);
/// 生成细分立方体壳。
///
/// 六个面作为独立块生成。相邻块上的重合位置使用不同顶点索引，因此每条
/// 立方体边表现为两条重合边界边。要生成闭合流形立方体，请使用
/// generateClosedCubeGrid。
/// @param[in] n 每个面轴向的细分数。
/// @param[in] size 立方体边长。
MANUMESH_API Mesh generateCubeGrid(int n, double size);
/// 生成与 generateCubeGrid 使用相同块布局的细分立方体，但通过量化位置键
/// 合并重合顶点，得到 boundaryEdgeCount() == 0 的闭合二流形壳。
/// @param[in] n 每个面轴向的细分数。
/// @param[in] size 立方体边长。
MANUMESH_API Mesh generateClosedCubeGrid(int n, double size);
/// 生成薄鳍压力测试样例。
/// @param[in] n 表面分辨率。
/// @param[in] size 样例范围。
MANUMESH_API Mesh generateThinFinGrid(int n, double size);
/// 生成阶梯轴工业测试样例。
/// @param[in] n 周向/细节分辨率。
MANUMESH_API Mesh generateSteppedShaftGrid(int n);
/// 生成管接头工业测试样例。
/// @param[in] n 周向/细节分辨率。
MANUMESH_API Mesh generatePipeCouplingGrid(int n);
/// 生成类似滑轮的工业测试样例。
/// @param[in] n 周向/细节分辨率。
MANUMESH_API Mesh generatePulleyGrid(int n);

/// 根据稳定字符串名称分派一个内置生成器。
/// @param[in] type 稳定的生成器名称。
/// @param[in] n 传给选定生成器的分辨率。
/// @param[out] mesh 成功时替换其内容。
/// @param[out] error 可选的未知名称/参数诊断信息。
/// @return 当名称已知且生成成功时返回 true。
MANUMESH_API bool generateMeshByName(const std::string& type, int n, Mesh& mesh, std::string* error = nullptr);

} // 命名空间 manumesh
