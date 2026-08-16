/**
 * @file src/simplification/detail/TextureProtection.h
 * @brief 声明逐角 UV 图表的代价、拒绝规则和更新计划。
 * @ingroup manumesh_simplification
 *
 * @details 纹理项保持独立于 4x4 几何 QEM，并只在输入带有效逐角 UV 时启用。
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "core/Mesh.h"
#include "simplification/detail/CollapseTopology.h"

#include <vector>

namespace manumesh {
namespace simplification {

/**
 * @brief 一次放置的纹理约束结果和标量惩罚。
 */
struct TextureCollapseEvaluation {
    TextureCollapseRejectReason rejectReason = TextureCollapseRejectReason::None;
    double cost = 0.0;

    /** @brief 报告所有启用的纹理约束是否均已通过。*/
    bool allowed() const { return rejectReason == TextureCollapseRejectReason::None; }
};

/**
 * @brief 折叠应用后仍保留、但每角 UV 会发生变化的面。
 */
struct TextureFaceUpdate {
    int face = -1;
    FaceTexCoords texcoords;
};

/**
 * @brief 一次折叠放置的完整纹理结果：用于排序/拒绝的评估，以及应用所需的具体 UV 重写。为接受放置构建的计划可以直接应用，避免在 applyCollapse 内再次构建相同计划。
 */
struct TextureUpdatePlan {
    TextureCollapseEvaluation evaluation;
    std::vector<TextureFaceUpdate> updates;
};

/**
 * @brief 叠加在不变的 4x4 几何 QEM 之上的局部纹理策略。
 */
class TextureProtection {
public:
    /** @brief 为一个输入网格捕获纹理策略和尺度容差。*/
    TextureProtection(const Mesh& input, const SimplifyOptions& options);

    /** @brief 判断纹理约束或惩罚是否已启用。*/
    bool active() const;
    /**
     * @brief 评估一次折叠放置，但不生成 UV 重写。
     */
    TextureCollapseEvaluation evaluate(
        CollapseEdge edge,
        const Vec3& position,
        const std::vector<FaceState>& faces,
        const std::vector<VertexState>& vertices,
        const DynamicTopology& topology,
        const std::vector<FaceTexCoords>& faceTexCoords
    ) const;
    /**
     * @brief 评估一次折叠放置；允许时返回应用所需的 UV 重写。
     */
    TextureUpdatePlan buildPlan(
        CollapseEdge edge,
        const Vec3& position,
        const std::vector<FaceState>& faces,
        const std::vector<VertexState>& vertices,
        const DynamicTopology& topology,
        const std::vector<FaceTexCoords>& faceTexCoords
    ) const;
    /**
     * @brief 应用此前为接受放置构建的计划。
     */
    bool apply(const TextureUpdatePlan& plan, std::vector<FaceTexCoords>& faceTexCoords) const;

private:
    bool enabled_ = false;
    double weight_ = 0.0;
    double uvTolerance_ = 1e-12;
    double uvAreaEpsilon_ = 1e-24;
    double minAreaRatio_ = 0.0;
};

} // namespace simplification
} // namespace manumesh
