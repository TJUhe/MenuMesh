/**
 * @file src/simplification/TextureProtection.cpp
 * @brief Implements texture protection facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details Adds local per-corner UV policy without increasing the 4x4 geometry quadric dimension.
 * @algorithm Incident corners are clustered into deterministic local charts.
 * A proposed placement interpolates surviving UV corners, rejects chart
 * incompatibility, fold-over, and excessive signed-area loss, then adds a
 * scalar distortion cost and returns concrete UV rewrites.
 * @invariants Geometry placement remains three-dimensional and an accepted
 * texture plan is applied verbatim with the collapse.
 */

#include "detail/TextureProtection.h"

#include "common/detail/MeshQueries.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace manumesh::simplification {
namespace {

struct CornerSample {
    int face = -1;
    int corner = -1;
    int cluster = -1;
};

struct EndpointCharts {
    std::vector<Vec2> representatives;
    std::vector<CornerSample> samples;
    bool sawTextured = false;
    bool sawUntextured = false;
};

double cross2(const Vec2& a, const Vec2& b) { return a.x() * b.y() - a.y() * b.x(); }

int faceCorner(const FaceState& face, int vertex) {
    for (int corner = 0; corner < 3; ++corner) {
        if (face.v[corner] == vertex) {
            return corner;
        }
    }
    return -1;
}

bool faceContainsBoth(const FaceState& face, CollapseEdge edge) {
    return faceCorner(face, edge.keep) >= 0 && faceCorner(face, edge.remove) >= 0;
}

EndpointCharts collectEndpointCharts(
    int vertex,
    const std::vector<FaceState>& faces,
    const DynamicTopology& topology,
    const std::vector<FaceTexCoords>& faceTexCoords,
    double tolerance
) {
    EndpointCharts charts;
    if (vertex < 0 || vertex >= static_cast<int>(topology.vertexFaces.size())) {
        return charts;
    }
    const double tolerance2 = tolerance * tolerance;
    // Visit corner samples in (faceId, corner) order so cluster ids do not
    // depend on the unordered_set iteration order of the incident-face lists.
    std::vector<int> incidentFaces(topology.vertexFaces[vertex].begin(), topology.vertexFaces[vertex].end());
    std::sort(incidentFaces.begin(), incidentFaces.end());
    for (int faceId : incidentFaces) {
        if (faceId < 0 || faceId >= static_cast<int>(faces.size()) || !faces[faceId].active) {
            continue;
        }
        const int corner = faceCorner(faces[faceId], vertex);
        if (corner < 0 || faceId >= static_cast<int>(faceTexCoords.size()) || !faceTexCoords[faceId].valid) {
            charts.sawUntextured = true;
            continue;
        }
        charts.sawTextured = true;
        const Vec2& uv = faceTexCoords[faceId].uv[corner];
        // One-ring chart counts are tiny, so a direct scan over the chart
        // representatives is cheaper than the previous hash-grid and does not
        // allocate. Ties deterministically join the lowest-index cluster.
        int cluster = -1;
        for (int rep = 0; rep < static_cast<int>(charts.representatives.size()); ++rep) {
            if ((charts.representatives[static_cast<std::size_t>(rep)] - uv).squaredNorm() <= tolerance2) {
                cluster = rep;
                break;
            }
        }
        if (cluster < 0) {
            cluster = static_cast<int>(charts.representatives.size());
            charts.representatives.push_back(uv);
        }
        charts.samples.push_back(CornerSample{faceId, corner, cluster});
    }
    return charts;
}

int sampleCluster(const EndpointCharts& charts, int faceId) {
    for (const CornerSample& sample : charts.samples) {
        if (sample.face == faceId) {
            return sample.cluster;
        }
    }
    return -1;
}

bool assignPair(std::vector<int>& mapping, int source, int target) {
    if (source < 0 || source >= static_cast<int>(mapping.size())) {
        return false;
    }
    if (mapping[source] >= 0 && mapping[source] != target) {
        return false;
    }
    mapping[source] = target;
    return true;
}

double triangleUvDoubleArea(const FaceTexCoords& texcoords) {
    return cross2(texcoords.uv[1] - texcoords.uv[0], texcoords.uv[2] - texcoords.uv[0]);
}

double triangleUvScaleSquared(const FaceTexCoords& texcoords) {
    double sum = 0.0;
    for (int corner = 0; corner < 3; ++corner) {
        sum += (texcoords.uv[(corner + 1) % 3] - texcoords.uv[corner]).squaredNorm();
    }
    return sum / 3.0;
}

TextureUpdatePlan buildUpdatePlan(
    CollapseEdge edge,
    const Vec3& position,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology,
    const std::vector<FaceTexCoords>& faceTexCoords,
    double weight,
    double uvTolerance,
    double uvAreaEpsilon,
    double minAreaRatio,
    bool collectUpdates
) {
    TextureUpdatePlan plan;
    const EndpointCharts keepCharts = collectEndpointCharts(edge.keep, faces, topology, faceTexCoords, uvTolerance);
    const EndpointCharts removeCharts = collectEndpointCharts(edge.remove, faces, topology, faceTexCoords, uvTolerance);
    if (!keepCharts.sawTextured && !removeCharts.sawTextured) {
        return plan;
    }
    if (keepCharts.sawTextured != removeCharts.sawTextured ||
        (keepCharts.sawTextured && (keepCharts.sawUntextured || removeCharts.sawUntextured))) {
        plan.evaluation.rejectReason = TextureCollapseRejectReason::ChartMismatch;
        return plan;
    }

    std::vector<int> keepToRemove(keepCharts.representatives.size(), -1);
    std::vector<int> removeToKeep(removeCharts.representatives.size(), -1);
    for (int faceId : topology.vertexFaces[edge.keep]) {
        if (faceId < 0 || faceId >= static_cast<int>(faces.size()) || !faces[faceId].active ||
            !faceContainsBoth(faces[faceId], edge)) {
            continue;
        }
        if (faceId >= static_cast<int>(faceTexCoords.size()) || !faceTexCoords[faceId].valid) {
            plan.evaluation.rejectReason = TextureCollapseRejectReason::ChartMismatch;
            return plan;
        }
        const int keepCluster = sampleCluster(keepCharts, faceId);
        const int removeCluster = sampleCluster(removeCharts, faceId);
        if (!assignPair(keepToRemove, keepCluster, removeCluster) ||
            !assignPair(removeToKeep, removeCluster, keepCluster)) {
            plan.evaluation.rejectReason = TextureCollapseRejectReason::ChartMismatch;
            return plan;
        }
    }
    if (std::any_of(
            keepToRemove.begin(),
            keepToRemove.end(),
            [](int cluster) {
                return cluster < 0;
            }
        ) ||
        std::any_of(removeToKeep.begin(), removeToKeep.end(), [](int cluster) {
            return cluster < 0;
        })) {
        plan.evaluation.rejectReason = TextureCollapseRejectReason::ChartMismatch;
        return plan;
    }

    const Vec3 edgeVector = vertices[edge.remove].p - vertices[edge.keep].p;
    const double edgeLength2 = edgeVector.squaredNorm();
    const double t = edgeLength2 > 1e-30
                         ? std::clamp((position - vertices[edge.keep].p).dot(edgeVector) / edgeLength2, 0.0, 1.0)
                         : 0.5;
    std::vector<Vec2> mergedUv(keepCharts.representatives.size(), Vec2::Zero());
    for (int cluster = 0; cluster < static_cast<int>(mergedUv.size()); ++cluster) {
        mergedUv[cluster] =
            (1.0 - t) * keepCharts.representatives[cluster] + t * removeCharts.representatives[keepToRemove[cluster]];
    }

    // Deterministic face order keeps the floating-point cost accumulation and
    // the update-plan order independent of unordered_set iteration order.
    std::vector<int> touchedFaces;
    touchedFaces.insert(
        touchedFaces.end(), topology.vertexFaces[edge.keep].begin(), topology.vertexFaces[edge.keep].end()
    );
    touchedFaces.insert(
        touchedFaces.end(), topology.vertexFaces[edge.remove].begin(), topology.vertexFaces[edge.remove].end()
    );
    std::sort(touchedFaces.begin(), touchedFaces.end());
    touchedFaces.erase(std::unique(touchedFaces.begin(), touchedFaces.end()), touchedFaces.end());
    double weightedDisplacement = 0.0;
    double weightedUvScale = 0.0;
    double localArea = 0.0;
    for (int faceId : touchedFaces) {
        if (faceId < 0 || faceId >= static_cast<int>(faces.size()) || !faces[faceId].active ||
            faceContainsBoth(faces[faceId], edge)) {
            continue;
        }
        if (faceId >= static_cast<int>(faceTexCoords.size()) || !faceTexCoords[faceId].valid) {
            plan.evaluation.rejectReason = TextureCollapseRejectReason::ChartMismatch;
            return plan;
        }
        FaceTexCoords updated = faceTexCoords[faceId];
        double displacement2 = 0.0;
        for (int corner = 0; corner < 3; ++corner) {
            int keepCluster = -1;
            if (faces[faceId].v[corner] == edge.keep) {
                keepCluster = sampleCluster(keepCharts, faceId);
            } else if (faces[faceId].v[corner] == edge.remove) {
                const int removeCluster = sampleCluster(removeCharts, faceId);
                keepCluster = removeCluster >= 0 ? removeToKeep[removeCluster] : -1;
            }
            if (keepCluster >= 0) {
                displacement2 += (mergedUv[keepCluster] - updated.uv[corner]).squaredNorm();
                updated.uv[corner] = mergedUv[keepCluster];
            }
        }

        const double oldUvArea = triangleUvDoubleArea(faceTexCoords[faceId]);
        const double newUvArea = triangleUvDoubleArea(updated);
        const bool oldUvDegenerate = std::abs(oldUvArea) <= uvAreaEpsilon;
        const bool orientationFlip = !oldUvDegenerate && (oldUvArea * newUvArea <= 0.0 ||
                                                          std::abs(newUvArea) < minAreaRatio * std::abs(oldUvArea));
        // A UV-degenerate source triangle carries no reliable orientation, but
        // a clearly negative new signed area still indicates a UV fold-over;
        // use a larger tolerance so numeric noise around zero is not rejected.
        const bool degenerateFoldover = oldUvDegenerate && newUvArea < -64.0 * uvAreaEpsilon;
        if (orientationFlip || degenerateFoldover) {
            plan.evaluation.rejectReason = TextureCollapseRejectReason::TriangleFlip;
            plan.updates.clear();
            return plan;
        }

        std::array<Vec3, 3> points;
        for (int corner = 0; corner < 3; ++corner) {
            const int vertex = faces[faceId].v[corner];
            points[corner] = vertex == edge.keep || vertex == edge.remove ? position : vertices[vertex].p;
        }
        const double area = triangleArea(points[0], points[1], points[2]);
        const double areaWeight = std::max(area, 1e-30);
        weightedDisplacement += areaWeight * displacement2;
        weightedUvScale += areaWeight * triangleUvScaleSquared(faceTexCoords[faceId]);
        localArea += areaWeight;
        if (collectUpdates) {
            plan.updates.push_back(TextureFaceUpdate{faceId, updated});
        }
    }

    if (weightedUvScale > 1e-30 && localArea > 0.0) {
        const double meanUvScale = weightedUvScale / localArea;
        plan.evaluation.cost = weight * edgeLength2 * weightedDisplacement / meanUvScale;
    }
    if (!std::isfinite(plan.evaluation.cost)) {
        plan.evaluation.rejectReason = TextureCollapseRejectReason::ChartMismatch;
        plan.evaluation.cost = 0.0;
        plan.updates.clear();
    }
    return plan;
}

} // namespace

TextureProtection::TextureProtection(const Mesh& input, const SimplifyOptions& options) {
    enabled_ = options.preserveTexture && input.hasTextureCoordinates();
    weight_ = options.textureWeight;
    minAreaRatio_ = options.minTextureAreaRatio;
    if (!enabled_) {
        return;
    }
    Vec2 lo = Vec2::Constant(std::numeric_limits<double>::infinity());
    Vec2 hi = Vec2::Constant(-std::numeric_limits<double>::infinity());
    for (const FaceTexCoords& texcoords : input.faceTexCoords) {
        if (!texcoords.valid) {
            continue;
        }
        for (const Vec2& uv : texcoords.uv) {
            lo = lo.cwiseMin(uv);
            hi = hi.cwiseMax(uv);
        }
    }
    const double uvDiagonal = (hi - lo).norm();
    uvTolerance_ = std::max(1e-14, options.textureSeamTolerance * std::max(uvDiagonal, 1e-12));
    uvAreaEpsilon_ = std::max(1e-28, uvDiagonal * uvDiagonal * 1e-14);
}

bool TextureProtection::active() const { return enabled_; }

TextureCollapseEvaluation TextureProtection::evaluate(
    CollapseEdge edge,
    const Vec3& position,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology,
    const std::vector<FaceTexCoords>& faceTexCoords
) const {
    if (!enabled_) {
        return {};
    }
    return buildUpdatePlan(
               edge,
               position,
               faces,
               vertices,
               topology,
               faceTexCoords,
               weight_,
               uvTolerance_,
               uvAreaEpsilon_,
               minAreaRatio_,
               false
    )
        .evaluation;
}

TextureUpdatePlan TextureProtection::buildPlan(
    CollapseEdge edge,
    const Vec3& position,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology,
    const std::vector<FaceTexCoords>& faceTexCoords
) const {
    if (!enabled_) {
        return {};
    }
    return buildUpdatePlan(
        edge,
        position,
        faces,
        vertices,
        topology,
        faceTexCoords,
        weight_,
        uvTolerance_,
        uvAreaEpsilon_,
        minAreaRatio_,
        true
    );
}

bool TextureProtection::apply(const TextureUpdatePlan& plan, std::vector<FaceTexCoords>& faceTexCoords) const {
    if (!enabled_) {
        return true;
    }
    if (!plan.evaluation.allowed()) {
        return false;
    }
    for (const TextureFaceUpdate& update : plan.updates) {
        if (update.face >= 0 && update.face < static_cast<int>(faceTexCoords.size())) {
            faceTexCoords[update.face] = update.texcoords;
        }
    }
    return true;
}

} // namespace manumesh::simplification
