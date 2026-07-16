/**
 * @file src/feature_detection/detail/FeatureDetectionCache.h
 * @brief Declares feature detection cache facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"
#include "common/detail/MeshQueries.h"

#include <utility>
#include <vector>

namespace manumesh::feature::detector_detail {

/**
 * @brief Lazily built, per-analysis cache of mesh-wide auxiliary structures.
 *
 * One analyze() call previously rebuilt vertex adjacency, per-vertex average
 * edge lengths, and face normals five to six times across the evidence,
 * smoothing, and cleanup stages. Each accessor below builds its structure on
 * first use and returns the cached copy afterwards, so every stage of one
 * pipeline run shares a single instance. The cache must only be used with the
 * mesh it was created for.
 */
class FeatureDetectionCache {
public:
    /**
     * @brief Binds a lazy cache to one immutable mesh and filter policy.
     * @param[in] mesh Mesh that must outlive the cache.
     * @param[in] normalFilterOptions Optional face-normal smoothing policy.
     */
    explicit FeatureDetectionCache(const Mesh& mesh, FeatureNormalFilterOptions normalFilterOptions = {})
        : mesh_(&mesh),
          normalFilterOptions_(normalFilterOptions) {}

    FeatureDetectionCache(const FeatureDetectionCache&) = delete;
    FeatureDetectionCache& operator=(const FeatureDetectionCache&) = delete;

    /** @brief Returns filtered face normals, computing them on first access. */
    const std::vector<Vec3>& faceNormals();

    /** @brief Returns diagnostics from the lazily executed normal filter. */
    const FeatureNormalFilterReport& normalFilterReport();

    /** @brief Returns cached undirected edge-to-face incidence. */
    const manumesh::common::MeshEdgeInfoMap& edgeInfo() {
        if (!hasEdgeInfo_) {
            edgeInfo_ = manumesh::common::buildMeshEdgeInfo(*mesh_);
            hasEdgeInfo_ = true;
        }
        return edgeInfo_;
    }

    /** @brief Returns cached deterministic one-ring vertex adjacency. */
    const std::vector<std::vector<int>>& vertexNeighbors() {
        if (!hasVertexNeighbors_) {
            vertexNeighbors_ = manumesh::common::buildVertexNeighbors(*mesh_);
            hasVertexNeighbors_ = true;
        }
        return vertexNeighbors_;
    }

    /** @brief Returns cached per-vertex sampling-density estimates. */
    const std::vector<double>& vertexAverageEdgeLength() {
        if (!hasVertexAverageEdgeLength_) {
            vertexAverageEdgeLength_ = manumesh::common::computeVertexAverageEdgeLength(*mesh_);
            hasVertexAverageEdgeLength_ = true;
        }
        return vertexAverageEdgeLength_;
    }

    /**
     * @brief Area-weighted vertex normals shared by the smooth-curvature stage.
     */
    const std::vector<Vec3>& areaWeightedVertexNormals();

private:
    const Mesh* mesh_ = nullptr;
    std::vector<Vec3> faceNormals_;
    manumesh::common::MeshEdgeInfoMap edgeInfo_;
    std::vector<std::vector<int>> vertexNeighbors_;
    std::vector<double> vertexAverageEdgeLength_;
    std::vector<Vec3> areaWeightedVertexNormals_;
    FeatureNormalFilterOptions normalFilterOptions_;
    FeatureNormalFilterReport normalFilterReport_;
    bool hasFaceNormals_ = false;
    bool hasEdgeInfo_ = false;
    bool hasVertexNeighbors_ = false;
    bool hasVertexAverageEdgeLength_ = false;
    bool hasAreaWeightedVertexNormals_ = false;
};

/**
 * @brief Cache-aware siblings of the public scoring entry points. The public
 * overloads build a private cache; the pipeline passes one shared instance.
 */
std::vector<NormalTensorVertex> computeNormalTensorFeaturesCached(
    const Mesh& mesh, FeatureDetectionCache& cache, const NormalTensorOptions& options, double persistenceThreshold
);

std::vector<SmoothCurvatureVertex> computeSmoothCurvatureFeaturesCached(
    const Mesh& mesh, FeatureDetectionCache& cache, const SmoothCurvatureOptions& options, double persistenceThreshold
);

} // namespace manumesh::feature::detector_detail
