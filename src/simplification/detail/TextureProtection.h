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

/// One surviving face whose per-corner UVs change when a collapse is applied.
struct TextureFaceUpdate {
    int face = -1;
    FaceTexCoords texcoords;
};

/// Full texture outcome of one collapse placement: the evaluation used for
/// ranking/rejection plus the concrete UV rewrites needed to apply it. A plan
/// built for the accepted placement can be applied directly, which avoids
/// rebuilding the same plan a second time inside applyCollapse.
struct TextureUpdatePlan {
    TextureCollapseEvaluation evaluation;
    std::vector<TextureFaceUpdate> updates;
};

/// Local texture policy layered on top of the unchanged 4x4 geometry QEM.
class TextureProtection {
public:
    TextureProtection(const Mesh& input, const SimplifyOptions& options);

    bool active() const;
    /// Evaluates one collapse placement without materializing UV rewrites.
    TextureCollapseEvaluation evaluate(
        CollapseEdge edge,
        const Vec3& position,
        const std::vector<FaceState>& faces,
        const std::vector<VertexState>& vertices,
        const DynamicTopology& topology,
        const std::vector<FaceTexCoords>& faceTexCoords
    ) const;
    /// Evaluates one collapse placement and, when allowed, returns the UV
    /// rewrites needed to apply it.
    TextureUpdatePlan buildPlan(
        CollapseEdge edge,
        const Vec3& position,
        const std::vector<FaceState>& faces,
        const std::vector<VertexState>& vertices,
        const DynamicTopology& topology,
        const std::vector<FaceTexCoords>& faceTexCoords
    ) const;
    /// Applies a plan previously built for the accepted placement.
    bool apply(const TextureUpdatePlan& plan, std::vector<FaceTexCoords>& faceTexCoords) const;

private:
    bool enabled_ = false;
    double weight_ = 0.0;
    double uvTolerance_ = 1e-12;
    double uvAreaEpsilon_ = 1e-24;
    double minAreaRatio_ = 0.0;
};

} // namespace manumesh::simplification
