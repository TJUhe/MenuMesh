#include "detail/TextureProtection.h"

#include "common/detail/MeshQueries.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
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

struct FaceUpdate {
    int face = -1;
    FaceTexCoords texcoords;
};

struct UpdatePlan {
    TextureCollapseEvaluation evaluation;
    std::vector<FaceUpdate> updates;
};

struct QuantizedUvKey {
    long long u = 0;
    long long v = 0;

    bool operator==(const QuantizedUvKey& other) const { return u == other.u && v == other.v; }
};

struct QuantizedUvKeyHash {
    std::size_t operator()(const QuantizedUvKey& key) const {
        const std::size_t hu = std::hash<long long>{}(key.u);
        const std::size_t hv = std::hash<long long>{}(key.v);
        return hu ^ (hv + 0x9e3779b97f4a7c15ull + (hu << 6) + (hu >> 2));
    }
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

QuantizedUvKey quantizeUv(const Vec2& uv, const Vec2& origin, double tolerance) {
    return {
        static_cast<long long>(std::llround((uv.x() - origin.x()) / tolerance)),
        static_cast<long long>(std::llround((uv.y() - origin.y()) / tolerance))
    };
}

EndpointCharts collectEndpointCharts(
    int vertex,
    const std::vector<FaceState>& faces,
    const DynamicTopology& topology,
    const std::vector<FaceTexCoords>& faceTexCoords,
    double tolerance
) {
    EndpointCharts charts;
    Vec2 origin = Vec2::Zero();
    bool hasOrigin = false;
    std::unordered_multimap<QuantizedUvKey, int, QuantizedUvKeyHash> clustersByCell;
    if (vertex < 0 || vertex >= static_cast<int>(topology.vertexFaces.size())) {
        return charts;
    }
    for (int faceId : topology.vertexFaces[vertex]) {
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
        if (!hasOrigin) {
            origin = uv;
            hasOrigin = true;
        }
        const QuantizedUvKey key = quantizeUv(uv, origin, tolerance);
        int cluster = -1;
        const double tolerance2 = tolerance * tolerance;
        for (long long du = -1; du <= 1 && cluster < 0; ++du) {
            for (long long dv = -1; dv <= 1; ++dv) {
                const auto range = clustersByCell.equal_range({key.u + du, key.v + dv});
                for (auto found = range.first; found != range.second; ++found) {
                    if ((charts.representatives[found->second] - uv).squaredNorm() <= tolerance2) {
                        cluster = found->second;
                        break;
                    }
                }
                if (cluster >= 0) {
                    break;
                }
            }
        }
        if (cluster < 0) {
            cluster = static_cast<int>(charts.representatives.size());
            charts.representatives.push_back(uv);
            clustersByCell.emplace(key, cluster);
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

UpdatePlan buildUpdatePlan(
    CollapseEdge edge,
    const Vec3& position,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology,
    const std::vector<FaceTexCoords>& faceTexCoords,
    double weight,
    double uvTolerance,
    double uvAreaEpsilon,
    double minAreaRatio
) {
    UpdatePlan plan;
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

    std::unordered_set<int> touchedFaces;
    touchedFaces.insert(topology.vertexFaces[edge.keep].begin(), topology.vertexFaces[edge.keep].end());
    touchedFaces.insert(topology.vertexFaces[edge.remove].begin(), topology.vertexFaces[edge.remove].end());
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
        if (std::abs(oldUvArea) > uvAreaEpsilon &&
            (oldUvArea * newUvArea <= 0.0 || std::abs(newUvArea) < minAreaRatio * std::abs(oldUvArea))) {
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
        plan.updates.push_back(FaceUpdate{faceId, updated});
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

int TextureProtection::countProtectedEdges(
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology,
    const std::vector<FaceTexCoords>& faceTexCoords
) const {
    if (!enabled_) {
        return 0;
    }
    int count = 0;
    for (const auto& [a, b] : collectActiveEdges(faces)) {
        const Vec3 midpoint = 0.5 * (vertices[a].p + vertices[b].p);
        if (!evaluate({a, b}, midpoint, faces, vertices, topology, faceTexCoords).allowed()) {
            ++count;
        }
    }
    return count;
}

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
               minAreaRatio_
    )
        .evaluation;
}

bool TextureProtection::apply(
    CollapseEdge edge,
    const Vec3& position,
    const std::vector<FaceState>& faces,
    const std::vector<VertexState>& vertices,
    const DynamicTopology& topology,
    std::vector<FaceTexCoords>& faceTexCoords
) const {
    if (!enabled_) {
        return true;
    }
    UpdatePlan plan = buildUpdatePlan(
        edge, position, faces, vertices, topology, faceTexCoords, weight_, uvTolerance_, uvAreaEpsilon_, minAreaRatio_
    );
    if (!plan.evaluation.allowed()) {
        return false;
    }
    for (const FaceUpdate& update : plan.updates) {
        faceTexCoords[update.face] = update.texcoords;
    }
    return true;
}

} // namespace manumesh::simplification
