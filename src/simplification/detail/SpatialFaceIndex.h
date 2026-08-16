/**
 * @file src/simplification/detail/SpatialFaceIndex.h
 * @brief 声明活动面局部自相交检查使用的动态宽相索引。
 * @ingroup manumesh_simplification
 *
 * @details 索引只提供保守候选面，最终相交结论仍由精确窄相谓词给出。
 */

#pragma once

#include "common/detail/SpatialIndex.h"
#include "core/Mesh.h"
#include "detail/SimplificationTypes.h"

#include <vector>

namespace manumesh {
namespace simplification {

/**
 * @brief 用于精确局部自相交检查的动态宽相位 AABB 网格。
 */
class SpatialFaceIndex {
public:
    /** @brief 根据所有活动面重建宽相位结构。*/
    void rebuild(const std::vector<FaceState>& faces, const std::vector<VertexState>& vertices);
    /** @brief 从宽相位结构中移除一个面。*/
    void removeFace(int faceId);
    /** @brief 替换一个面的当前 AABB 登记。*/
    void updateFace(int faceId, const FaceState& face, const std::vector<VertexState>& vertices);
    /** @brief 返回与给定 AABB 重叠的宽相位面候选。*/
    std::vector<int> query(const Vec3& lo, const Vec3& hi) const;
    /** @brief 判断底层网格是否处于活动状态。*/
    bool enabled() const { return grid_.enabled(); }

private:
    /** @brief 将一个活动且有效的面插入宽相位结构。*/
    void insertFace(int faceId, const FaceState& face, const std::vector<VertexState>& vertices);

    manumesh::common::UniformAabbCandidateGrid grid_;
};

} // namespace simplification
} // namespace manumesh
