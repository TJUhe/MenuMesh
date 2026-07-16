/**
 * @file src/simplification/detail/TextureProtection.h
 * @brief Declares texture protection facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "core/Mesh.h"
#include "simplification/detail/CollapseTopology.h"

#include <vector>

namespace manumesh::simplification {

/**
 * @brief Texture constraint result and scalar penalty for one placement.
 */
struct TextureCollapseEvaluation {
    TextureCollapseRejectReason rejectReason = TextureCollapseRejectReason::None;
    double cost = 0.0;

    /** @brief Reports whether every enabled texture constraint passed. */
    bool allowed() const { return rejectReason == TextureCollapseRejectReason::None; }
};

/**
 * @brief One surviving face whose per-corner UVs change when a collapse is applied.
 */
struct TextureFaceUpdate {
    int face = -1;
    FaceTexCoords texcoords;
};

/**
 * @brief Full texture outcome of one collapse placement: the evaluation used for
 * ranking/rejection plus the concrete UV rewrites needed to apply it. A plan
 * built for the accepted placement can be applied directly, which avoids
 * rebuilding the same plan a second time inside applyCollapse.
 */
struct TextureUpdatePlan {
    TextureCollapseEvaluation evaluation;
    std::vector<TextureFaceUpdate> updates;
};

/**
 * @brief Local texture policy layered on top of the unchanged 4x4 geometry QEM.
 */
class TextureProtection {
public:
    /** @brief Captures texture policy and scale tolerances for one input mesh. */
    TextureProtection(const Mesh& input, const SimplifyOptions& options);

    /** @brief Reports whether texture constraints or penalties are enabled. */
    bool active() const;
    /**
     * @brief Evaluates one collapse placement without materializing UV rewrites.
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
     * @brief Evaluates one collapse placement and, when allowed, returns the UV
     * rewrites needed to apply it.
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
     * @brief Applies a plan previously built for the accepted placement.
     */
    bool apply(const TextureUpdatePlan& plan, std::vector<FaceTexCoords>& faceTexCoords) const;

private:
    bool enabled_ = false;
    double weight_ = 0.0;
    double uvTolerance_ = 1e-12;
    double uvAreaEpsilon_ = 1e-24;
    double minAreaRatio_ = 0.0;
};

} // namespace manumesh::simplification
