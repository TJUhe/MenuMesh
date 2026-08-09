/**
 * @file include/core/PlainMesh.h
 * @brief 声明 ManuMesh 核心网格模块的纯数据网格设施。
 * @ingroup manumesh_core
 *
 * @details 核心类型建立所有算法模块共同使用的存储、校验、容差、拓扑和状态契约。
 */

#pragma once

#include "Export.h"

#include <array>
#include <vector>

namespace manumesh {

struct Mesh;

/// 不暴露 Eigen 的 SDK 边界所使用的无 Eigen 顶点类型。
struct PlainVec3 {
    double x = 0.0; ///< 模型单位下的 X 坐标。
    double y = 0.0; ///< 模型单位下的 Y 坐标。
    double z = 0.0; ///< 模型单位下的 Z 坐标。
};

/// SDK 边界使用的无 Eigen 纹理坐标。
struct PlainVec2 {
    double u = 0.0; ///< U 纹理坐标。
    double v = 0.0; ///< V 纹理坐标。
};

/// 存储三个从零开始顶点索引的无 Eigen 三角形面。
struct PlainFace {
    std::array<int, 3> v{};
};

/// 一个三角形的无 Eigen 逐角纹理坐标。
struct PlainFaceTexCoords {
    std::array<PlainVec2, 3> uv{};
    bool valid = false;
};

/// 无 Eigen 三角网格交换容器。
///
/// 算法内部仍使用 `Mesh`。当宿主应用不希望在自己的公共 API 中暴露 Eigen 时，
/// 此类型提供纯 C++ 数据边界。
struct PlainMesh {
    std::vector<PlainVec3> vertices;
    std::vector<PlainFace> faces;
    std::vector<PlainFaceTexCoords> faceTexCoords;
};

/// 将纯 SDK 网格转换为内部基于 Eigen 的网格类型。
/// @param[in] plain 源数据；索引和数值按原样复制。
/// @return 独立的基于 Eigen 的网格副本。
MANUMESH_API Mesh toMesh(const PlainMesh& plain);
/// 将内部基于 Eigen 的网格类型转换为纯 SDK 网格。
/// @param[in] mesh 源网格。
/// @return 独立的无 Eigen 网格副本。
MANUMESH_API PlainMesh toPlainMesh(const Mesh& mesh);

} // 命名空间 manumesh
