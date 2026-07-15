#include "algorithms/feature_detection/FeatureDetector.h"

#include "common/detail/MeshQueries.h"
#include "detail/FeatureInputValidation.h"
#include "detail/FeatureSegmentation.h"

#include <algorithm>
#include <map>
#include <queue>
#include <unordered_set>
#include <utility>
#include <vector>

namespace manumesh::feature::detector_detail {

void buildFeaturePatches(const Mesh& mesh, FeatureAnalysis& analysis, const SurfacePatchOptions& options) {
    analysis.facePatchIds.assign(mesh.faces.size(), -1);
    analysis.patches.clear();
    analysis.patchAdjacencies.clear();
    analysis.closedSurfacePatches = 0;
    analysis.segmentationIgnoredRecoveryEdges = 0;
    if (mesh.faces.empty()) {
        return;
    }

    const common::MeshEdgeInfoMap edgeInfo = common::buildMeshEdgeInfo(mesh);
    std::unordered_set<std::uint64_t> barriers;
    barriers.reserve(analysis.graph.edges.size());
    for (const FeatureGraphEdge& edge : analysis.graph.edges) {
        if (edge.removedByCleanup || edge.a < 0 || edge.b < 0 || edge.a == edge.b) {
            continue;
        }
        const bool strong = edge.boundary || edge.dihedral || edge.nonManifold;
        const bool weak = edge.normalTensor || edge.smoothCurvature;
        if (!strong && (!options.includeWeakEvidence || !weak)) {
            continue;
        }
        const std::uint64_t key = common::meshEdgeKey(edge.a, edge.b);
        if (edgeInfo.find(key) == edgeInfo.end()) {
            if (edge.cleanupBridge || edge.consolidationBridge) {
                ++analysis.segmentationIgnoredRecoveryEdges;
            }
            continue;
        }
        barriers.insert(key);
    }

    std::vector<std::vector<int>> faceNeighbors(mesh.faces.size());
    for (const auto& [key, info] : edgeInfo) {
        if (info.faces.size() != 2 || barriers.find(key) != barriers.end()) {
            continue;
        }
        const int first = info.faces[0];
        const int second = info.faces[1];
        if (first < 0 || second < 0 || first >= static_cast<int>(mesh.faces.size()) ||
            second >= static_cast<int>(mesh.faces.size()) || first == second) {
            continue;
        }
        faceNeighbors[first].push_back(second);
        faceNeighbors[second].push_back(first);
    }
    for (std::vector<int>& neighbors : faceNeighbors) {
        std::sort(neighbors.begin(), neighbors.end());
        neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());
    }

    for (int seed = 0; seed < static_cast<int>(mesh.faces.size()); ++seed) {
        if (analysis.facePatchIds[seed] >= 0) {
            continue;
        }
        FeaturePatch patch;
        patch.id = static_cast<int>(analysis.patches.size());
        Vec3 normalSum = Vec3::Zero();
        std::queue<int> queue;
        queue.push(seed);
        analysis.facePatchIds[seed] = patch.id;
        while (!queue.empty()) {
            const int faceId = queue.front();
            queue.pop();
            ++patch.faceCount;
            const Face& face = mesh.faces[faceId];
            const Vec3 cross = (mesh.vertices[face.v[1]] - mesh.vertices[face.v[0]])
                                   .cross(mesh.vertices[face.v[2]] - mesh.vertices[face.v[0]]);
            patch.area += 0.5 * cross.norm();
            normalSum += cross;
            for (int neighbor : faceNeighbors[faceId]) {
                if (analysis.facePatchIds[neighbor] < 0) {
                    analysis.facePatchIds[neighbor] = patch.id;
                    queue.push(neighbor);
                }
            }
        }
        if (normalSum.squaredNorm() > 1e-30) {
            patch.normal = normalSum.normalized();
        }
        analysis.patches.push_back(std::move(patch));
    }

    std::map<std::pair<int, int>, int> adjacencyCounts;
    for (const auto& [key, info] : edgeInfo) {
        if (info.faces.size() == 1) {
            const int patchId = analysis.facePatchIds[info.faces.front()];
            if (patchId >= 0) {
                ++analysis.patches[patchId].meshBoundaryEdges;
            }
            continue;
        }
        if (info.faces.size() > 2) {
            std::vector<int> patches;
            for (int faceId : info.faces) {
                if (faceId >= 0 && faceId < static_cast<int>(analysis.facePatchIds.size())) {
                    patches.push_back(analysis.facePatchIds[faceId]);
                }
            }
            std::sort(patches.begin(), patches.end());
            patches.erase(std::unique(patches.begin(), patches.end()), patches.end());
            for (int patchId : patches) {
                if (patchId >= 0) {
                    ++analysis.patches[patchId].nonManifoldBoundaryEdges;
                }
            }
            continue;
        }

        const int firstPatch = analysis.facePatchIds[info.faces[0]];
        const int secondPatch = analysis.facePatchIds[info.faces[1]];
        if (firstPatch < 0 || secondPatch < 0 || firstPatch == secondPatch) {
            continue;
        }
        ++analysis.patches[firstPatch].featureBoundaryEdges;
        ++analysis.patches[secondPatch].featureBoundaryEdges;
        const std::pair<int, int> pair = std::minmax(firstPatch, secondPatch);
        if (barriers.find(key) != barriers.end()) {
            ++adjacencyCounts[pair];
        }
    }

    for (FeaturePatch& patch : analysis.patches) {
        patch.closed = patch.meshBoundaryEdges == 0 && patch.nonManifoldBoundaryEdges == 0;
        if (patch.closed) {
            ++analysis.closedSurfacePatches;
        }
    }
    for (const auto& [pair, featureEdges] : adjacencyCounts) {
        analysis.patchAdjacencies.push_back({pair.first, pair.second, featureEdges});
        analysis.patches[pair.first].neighboringPatches.push_back(pair.second);
        analysis.patches[pair.second].neighboringPatches.push_back(pair.first);
    }
    for (FeaturePatch& patch : analysis.patches) {
        std::sort(patch.neighboringPatches.begin(), patch.neighboringPatches.end());
        patch.neighboringPatches.erase(
            std::unique(patch.neighboringPatches.begin(), patch.neighboringPatches.end()),
            patch.neighboringPatches.end()
        );
    }
}

} // namespace manumesh::feature::detector_detail

namespace manumesh::feature {

void segmentFeaturePatches(const Mesh& mesh, FeatureAnalysis& analysis, const SurfacePatchOptions& options) {
    detector_detail::validateFeatureMeshInput(mesh);
    detector_detail::buildFeaturePatches(mesh, analysis, options);
}

} // namespace manumesh::feature
