/**
 * @file src/simplification/detail/FeatureConstraintGraph.h
 * @brief Declares the simplifier-owned canonical view of detected feature constraints.
 * @ingroup manumesh_simplification
 */

#pragma once

#include "core/Mesh.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace manumesh {
namespace feature {
struct FeatureAnalysis;
} // namespace feature

namespace simplification {

/** Source and ownership metadata retained for one canonical undirected feature edge. */
struct FeatureConstraintEdge {
    int a = -1;
    int b = -1;
    bool active = true;
    bool inputMeshEdge = false;
    bool pathBacked = false;
    bool protectedFeature = false;
    bool syntheticRecovery = false;
    bool removedByCleanup = false;
    bool boundary = false;
    bool dihedral = false;
    bool normalTensor = false;
    bool smoothCurvature = false;
    bool nonManifold = false;
    bool cleanupBridge = false;
    bool consolidationBridge = false;
    int signedKind = 0;
    double confidence = 0.0;
    std::vector<int> loopIds;
    std::vector<int> componentIds;
};

/** Multi-loop and graph-role metadata retained for one input/current vertex id. */
struct FeatureConstraintVertex {
    bool protectedFeature = false;
    bool junction = false;
    bool shared = false;
    bool endpoint = false;
    bool ambiguousJunction = false;
    bool sourceJunction = false;
    bool sourceShared = false;
    bool sourceEndpoint = false;
    bool sourceAmbiguousJunction = false;
    double confidence = 0.0;
    std::vector<int> loopIds;
    std::vector<int> componentIds;
    std::vector<int> sourceLoopIds;
    std::vector<int> sourceComponentIds;
};

/**
 * Canonical feature graph consumed by simplification.
 *
 * Initial protected edges must be both active detector evidence and real input-mesh
 * topology. Contracting such an edge may create a path-backed edge between the
 * surviving vertex and the next feature vertex; synthetic recovery edges never
 * become protected or geometric constraints.
 */
struct FeatureConstraintGraph {
    std::vector<FeatureConstraintVertex> vertices;
    std::vector<FeatureConstraintEdge> edges;

    const FeatureConstraintEdge* findEdge(int a, int b) const;
    FeatureConstraintEdge* findMutableEdge(int a, int b);
    bool isProtectedPathEdge(int a, int b) const;
    bool isSyntheticRecoveryEdge(int a, int b) const;
    bool isOnlyProtectedEdgeInComponent(int a, int b) const;
    int protectedComponentVertexCount(int a, int b) const;
    bool hasProtectedIncidentEdge(int vertex) const;
    std::vector<int> protectedNeighbors(int vertex) const;

    /** Rebuild the active canonical-key lookup after construction or bulk edits. */
    void rebuildIndex();
    /** Contract remove into keep; return false when remove has no active constraint adjacency. */
    bool contractVertex(int keep, int remove);

private:
    std::unordered_map<std::uint64_t, int> activeEdgeByKey_;
    std::vector<std::vector<int>> activeEdgeIdsByVertex_;
    std::vector<int> protectedComponentByVertex_;
    std::vector<int> protectedComponentVertexCounts_;

    void addEdgeToIndexes(int edgeId);
    void removeEdgeFromIndexes(int edgeId);
    void refreshVertex(int vertex);
    void rebuildProtectedComponents();
};

FeatureConstraintGraph buildFeatureConstraintGraph(const Mesh& mesh, const feature::FeatureAnalysis& analysis);

} // namespace simplification
} // namespace manumesh
