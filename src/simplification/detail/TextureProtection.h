#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "core/Mesh.h"
#include "simplification/detail/CollapseTopology.h"

#include <vector>

namespace manumesh::simplification {

struct TextureCollapseEvaluation {
    TextureCollapseRejectReason rejectReason = TextureCollapseRejectReason::None;
    double cost = 0.0;

    bool allowed() const { return rejectReason == TextureCollapseRejectReason::None; }
};

/// Local texture policy layered on top of the unchanged 4x4 geometry QEM.
class TextureProtection {
public:
    TextureProtection(const Mesh& input, const SimplifyOptions& options);

    bool active() const;
    int countProtectedEdges(
        const std::vector<FaceState>& faces,
        const std::vector<VertexState>& vertices,
        const DynamicTopology& topology,
        const std::vector<FaceTexCoords>& faceTexCoords
    ) const;
    TextureCollapseEvaluation evaluate(
        CollapseEdge edge,
        const Vec3& position,
        const std::vector<FaceState>& faces,
        const std::vector<VertexState>& vertices,
        const DynamicTopology& topology,
        const std::vector<FaceTexCoords>& faceTexCoords
    ) const;
    bool apply(
        CollapseEdge edge,
        const Vec3& position,
        const std::vector<FaceState>& faces,
        const std::vector<VertexState>& vertices,
        const DynamicTopology& topology,
        std::vector<FaceTexCoords>& faceTexCoords
    ) const;

private:
    bool enabled_ = false;
    double weight_ = 0.0;
    double uvTolerance_ = 1e-12;
    double uvAreaEpsilon_ = 1e-24;
    double minAreaRatio_ = 0.0;
};

} // namespace manumesh::simplification
