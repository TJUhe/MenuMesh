/**
 * @file tests/unit/core/core_tests.cpp
 * @brief Verifies core tests behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
 */

#include "algorithms/analysis/MeshAnalysis.h"
#include "common/detail/MeshQueries.h"
#include "common/detail/SpatialIndex.h"
#include "core/Mesh.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"
#include "core/Status.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct NoDefaultValue {
    explicit NoDefaultValue(int v)
        : value(v) {}
    int value = 0;
};

bool containsItem(const std::vector<int>& items, int itemId) {
    return std::find(items.begin(), items.end(), itemId) != items.end();
}

std::vector<int> sorted(std::vector<int> items) {
    std::sort(items.begin(), items.end());
    return items;
}

TEST(ManuMesh, RemoveUnusedVerticesDropsInvalidFacesWithTheirExclusiveVertices) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(2.0, 2.0, 0.0),
    };
    // Face 1 references an out-of-range index and must be dropped as a whole;
    // vertex 3 is only used by that invalid face and must be removed too.
    mesh.faces = {manumesh::Face{{0, 1, 2}}, manumesh::Face{{2, 3, 9}}};

    mesh.removeUnusedVertices();

    ASSERT_EQ(1u, mesh.faces.size());
    ASSERT_EQ(3u, mesh.vertices.size());
    EXPECT_EQ(0, mesh.faces[0].v[0]);
    EXPECT_EQ(1, mesh.faces[0].v[1]);
    EXPECT_EQ(2, mesh.faces[0].v[2]);
    EXPECT_DOUBLE_EQ(0.0, mesh.vertices[2].x());
    EXPECT_DOUBLE_EQ(1.0, mesh.vertices[2].y());
    EXPECT_DOUBLE_EQ(0.0, mesh.vertices[2].z());
}

TEST(ManuMesh, TopologyEdgeKeyIsOrderIndependentAndSharedWithDetail) {
    const std::uint64_t forward = manumesh::topologyEdgeKey(3, 11);
    const std::uint64_t backward = manumesh::topologyEdgeKey(11, 3);
    EXPECT_EQ(forward, backward);
    EXPECT_EQ(forward, manumesh::common::meshEdgeKey(3, 11));
    EXPECT_EQ(forward, manumesh::common::meshEdgeKey(11, 3));

    const auto [a, b] = manumesh::common::unpackMeshEdgeKey(forward);
    EXPECT_EQ(3, a);
    EXPECT_EQ(11, b);
}

TEST(ManuMesh, GenerateClosedCubeGridIsClosedManifold) {
    const manumesh::Mesh mesh = manumesh::generateClosedCubeGrid(2, 2.0);
    EXPECT_EQ(26u, mesh.vertices.size());

    const manumesh::Result<manumesh::MeshTopology> topologyResult = manumesh::MeshTopology::build(mesh);
    ASSERT_TRUE(topologyResult.ok());
    const manumesh::MeshTopology& topology = topologyResult.value();
    EXPECT_EQ(0, topology.boundaryEdgeCount());
    EXPECT_EQ(0, topology.nonManifoldEdgeCount());
    const int eulerCharacteristic = topology.vertexCount() - topology.edgeCount() + topology.faceCount();
    EXPECT_EQ(2, eulerCharacteristic);
}

TEST(ManuMesh, BuildVertexNeighborsReturnsAscendingLists) {
    const manumesh::Mesh mesh = manumesh::generateClosedCubeGrid(3, 2.0);
    const std::vector<std::vector<int>> neighbors = manumesh::common::buildVertexNeighbors(mesh);

    ASSERT_EQ(mesh.vertices.size(), neighbors.size());
    for (const std::vector<int>& list : neighbors) {
        EXPECT_FALSE(list.empty());
        for (std::size_t i = 1; i < list.size(); ++i) {
            EXPECT_LT(list[i - 1], list[i]);
        }
    }
}

TEST(ManuMesh, ResultDoesNotRequireDefaultConstructibleValues) {
    const manumesh::Result<NoDefaultValue> success(NoDefaultValue(7));
    EXPECT_TRUE(success.ok());
    EXPECT_TRUE(success.hasValue());
    EXPECT_EQ(7, success.value().value);

    const manumesh::Result<NoDefaultValue> failure(manumesh::Status::invalidArgument("nope"));
    EXPECT_FALSE(failure.ok());
    EXPECT_FALSE(failure.hasValue());
    EXPECT_EQ(manumesh::StatusCode::InvalidArgument, failure.status().code());
    EXPECT_THROW(failure.value(), std::logic_error);
}

TEST(ManuMesh, UniformAabbCandidateGridInsertIsIdempotent) {
    manumesh::common::UniformAabbCandidateGrid grid;
    grid.reset(manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(10.0, 10.0, 10.0), 1000);
    ASSERT_TRUE(grid.enabled());

    grid.insert(1, manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(1.0, 1.0, 1.0));
    // Re-inserting the same item at a new location must drop the stale cells.
    grid.insert(1, manumesh::Vec3(5.0, 5.0, 5.0), manumesh::Vec3(6.0, 6.0, 6.0));

    const std::vector<int> oldLocation =
        grid.queryCandidates(manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(1.5, 1.5, 1.5));
    EXPECT_FALSE(containsItem(oldLocation, 1));

    const std::vector<int> newLocation =
        grid.queryCandidates(manumesh::Vec3(4.5, 4.5, 4.5), manumesh::Vec3(6.5, 6.5, 6.5));
    EXPECT_TRUE(containsItem(newLocation, 1));
}

TEST(ManuMesh, UniformAabbCandidateGridBufferedQueryMatchesByValueQuery) {
    manumesh::common::UniformAabbCandidateGrid grid;
    grid.reset(manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(10.0, 10.0, 10.0), 1000);
    ASSERT_TRUE(grid.enabled());

    grid.insert(1, manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(1.0, 1.0, 1.0));
    grid.insert(2, manumesh::Vec3(0.5, 0.5, 0.5), manumesh::Vec3(1.5, 1.5, 1.5));
    grid.insert(3, manumesh::Vec3(8.0, 8.0, 8.0), manumesh::Vec3(9.0, 9.0, 9.0));
    grid.insert(7, manumesh::Vec3(-1000.0, -1000.0, -1000.0), manumesh::Vec3(1000.0, 1000.0, 1000.0));

    const manumesh::Vec3 queries[][2] = {
        {manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(2.0, 2.0, 2.0)},
        {manumesh::Vec3(7.5, 7.5, 7.5), manumesh::Vec3(9.5, 9.5, 9.5)},
        {manumesh::Vec3(4.0, 4.0, 4.0), manumesh::Vec3(4.5, 4.5, 4.5)},
    };
    std::vector<int> buffered;
    for (const auto& query : queries) {
        const std::vector<int> byValue = grid.queryCandidates(query[0], query[1]);
        grid.queryCandidates(query[0], query[1], buffered);
        EXPECT_EQ(sorted(byValue), sorted(buffered));
        // Buffered results must be duplicate-free as well.
        const std::vector<int> unique = sorted(buffered);
        EXPECT_TRUE(std::adjacent_find(unique.begin(), unique.end()) == unique.end());
    }
}

TEST(ManuMesh, MeshQueriesComputeLocalVertexEdgeScale) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 2.0, 0.0),
        manumesh::Vec3(10.0, 10.0, 10.0),
    };
    mesh.faces = {{{0, 1, 2}}};

    const std::vector<double> scale = manumesh::common::computeVertexAverageEdgeLength(mesh);

    ASSERT_EQ(4u, scale.size());
    const double hyp = std::sqrt(5.0);
    EXPECT_NEAR((1.0 + 2.0) / 2.0, scale[0], 1e-12);
    EXPECT_NEAR((1.0 + hyp) / 2.0, scale[1], 1e-12);
    EXPECT_NEAR((2.0 + hyp) / 2.0, scale[2], 1e-12);
    EXPECT_NEAR((1.0 + 2.0 + hyp) / 3.0, scale[3], 1e-12);
}

TEST(ManuMesh, BuiltInGeneratorsCoverDemoAndIndustrialModels) {
    const std::vector<std::string> names = {
        "plane",
        "clustered-plane",
        "hole-plane",
        "ridge",
        "noisy-plane",
        "sine-terrain",
        "terrace",
        "bump",
        "cylinder",
        "torus",
        "cube",
        "thin-fin",
        "stepped-shaft",
        "pipe-coupling",
        "pulley",
    };

    for (const std::string& name : names) {
        manumesh::Mesh mesh;
        std::string error;
        EXPECT_TRUE(manumesh::generateMeshByName(name, 24, mesh, &error)) << name << ": " << error;
        EXPECT_FALSE(mesh.empty()) << name;

        const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(mesh);
        EXPECT_EQ(stats.vertices, static_cast<int>(mesh.vertices.size())) << name;
        EXPECT_EQ(stats.faces, static_cast<int>(mesh.faces.size())) << name;
        EXPECT_GT(stats.edges, 0) << name;
        EXPECT_GT(stats.area, 0.0) << name;
        EXPECT_GE(stats.meanTriangleQuality, 0.0) << name;
    }

    manumesh::Mesh mesh;
    std::string error;
    EXPECT_FALSE(manumesh::generateMeshByName("not-a-generator", 16, mesh, &error));
    EXPECT_FALSE(error.empty());

    error.clear();
    EXPECT_FALSE(manumesh::generateMeshByName("flange", 24, mesh, &error));
    EXPECT_FALSE(error.empty());
}

TEST(ManuMesh, ComputesMeshStatsForGeneratedCube) {
    const manumesh::Mesh input = manumesh::generateCubeGrid(4, 1.0);
    const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(input);

    EXPECT_EQ(stats.vertices, static_cast<int>(input.vertices.size()));
    EXPECT_EQ(stats.faces, static_cast<int>(input.faces.size()));
    EXPECT_GT(stats.edges, 0);
    EXPECT_GT(stats.area, 0.0);
    EXPECT_GT(stats.meanTriangleQuality, 0.0);
}

TEST(ManuMesh, MeshAnalysisSkipsMalformedFacesAndKeepsRawContainerCounts) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(2.0, 0.0, 0.0),
        manumesh::Vec3(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0),
    };
    mesh.faces = {
        {{0, 1, 2}}, // usable
        {{0, 1, 9}}, // out of range
        {{0, 1, -1}},
        {{0, 1, 4}}, // non-finite coordinate
        {{0, 0, 2}}, // repeated index
        {{0, 1, 3}}, // zero area
    };

    manumesh::analysis::MeshStats stats;
    EXPECT_NO_THROW(stats = manumesh::analysis::computeMeshStats(mesh));
    EXPECT_EQ(5, stats.vertices);
    EXPECT_EQ(6, stats.faces);
    EXPECT_EQ(3, stats.edges);
    EXPECT_EQ(3, stats.boundaryEdges);
    EXPECT_EQ(0, stats.nonManifoldEdges);
    EXPECT_DOUBLE_EQ(0.5, stats.area);
    EXPECT_GT(stats.meanTriangleQuality, 0.0);
    EXPECT_GT(stats.minTriangleQuality, 0.0);
    EXPECT_TRUE(std::isfinite(stats.meanEdgeLength));
    EXPECT_TRUE(std::isfinite(stats.edgeLengthCv));
}

TEST(ManuMesh, MeshAnalysisReturnsZeroMeasurementsWhenNoUsableFaceRemains) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(std::numeric_limits<double>::infinity(), 1.0, 0.0),
    };
    mesh.faces = {{{0, 0, 1}}, {{0, 1, 2}}, {{0, 1, 8}}};

    const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(mesh);
    EXPECT_EQ(3, stats.vertices);
    EXPECT_EQ(3, stats.faces);
    EXPECT_EQ(0, stats.edges);
    EXPECT_EQ(0, stats.boundaryEdges);
    EXPECT_EQ(0, stats.nonManifoldEdges);
    EXPECT_DOUBLE_EQ(0.0, stats.area);
    EXPECT_DOUBLE_EQ(0.0, stats.meanTriangleQuality);
    EXPECT_DOUBLE_EQ(0.0, stats.minTriangleQuality);
    EXPECT_DOUBLE_EQ(0.0, stats.meanEdgeLength);
    EXPECT_DOUBLE_EQ(0.0, stats.edgeLengthCv);
}

TEST(ManuMesh, SampledDistanceSkipsMalformedFacesOnBothSides) {
    manumesh::Mesh clean;
    clean.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    clean.faces = {{{0, 1, 2}}};

    manumesh::Mesh malformed = clean;
    malformed.vertices.push_back(manumesh::Vec3(2.0, 0.0, 0.0));
    malformed.vertices.push_back(manumesh::Vec3(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0));
    malformed.faces.push_back({{0, 1, 7}});
    malformed.faces.push_back({{0, 0, 2}});
    malformed.faces.push_back({{0, 1, 3}}); // zero area with distinct indices
    malformed.faces.push_back({{0, 1, 4}});
    malformed.faces.push_back({{0, 1, 1}});

    manumesh::analysis::DistanceStats stats;
    EXPECT_NO_THROW(stats = manumesh::analysis::compareMeshesBySampledDistance(malformed, clean, 32));
    EXPECT_NEAR(0.0, stats.meanOriginalToSimplified, 1e-15);
    EXPECT_NEAR(0.0, stats.maxOriginalToSimplified, 1e-15);
    EXPECT_NEAR(0.0, stats.meanSimplifiedToOriginal, 1e-15);
    EXPECT_NEAR(0.0, stats.maxSimplifiedToOriginal, 1e-15);
}

TEST(ManuMesh, SampledDistanceReturnsZerosForUnusableSurface) {
    manumesh::Mesh valid;
    valid.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    valid.faces = {{{0, 1, 2}}};

    manumesh::Mesh unusable = valid;
    unusable.faces = {{{0, 1, 9}}, {{0, 0, 2}}, {{0, 1, 1}}};

    const manumesh::analysis::DistanceStats stats =
        manumesh::analysis::compareMeshesBySampledDistance(valid, unusable, 32);
    EXPECT_DOUBLE_EQ(0.0, stats.meanOriginalToSimplified);
    EXPECT_DOUBLE_EQ(0.0, stats.maxOriginalToSimplified);
    EXPECT_DOUBLE_EQ(0.0, stats.meanSimplifiedToOriginal);
    EXPECT_DOUBLE_EQ(0.0, stats.maxSimplifiedToOriginal);
}

TEST(ManuMesh, MeshTopologyCachesBoundaryAndNonManifoldEdges) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(0.0, 0.0, 1.0),
        manumesh::Vec3(0.0, 0.0, -1.0),
    };
    mesh.faces = {
        {{0, 1, 2}},
        {{1, 0, 3}},
        {{0, 1, 4}},
    };

    const manumesh::Result<manumesh::MeshTopology> topologyResult = manumesh::MeshTopology::build(mesh);
    ASSERT_TRUE(topologyResult.ok()) << topologyResult.status().message();
    const manumesh::MeshTopology& topology = topologyResult.value();

    EXPECT_EQ(topology.vertexCount(), 5);
    EXPECT_EQ(topology.faceCount(), 3);
    EXPECT_EQ(topology.edgeCount(), 7);
    EXPECT_EQ(topology.boundaryEdgeCount(), 6);
    EXPECT_EQ(topology.nonManifoldEdgeCount(), 1);

    const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(mesh);
    EXPECT_EQ(stats.edges, topology.edgeCount());
    EXPECT_EQ(stats.boundaryEdges, topology.boundaryEdgeCount());
    EXPECT_EQ(stats.nonManifoldEdges, topology.nonManifoldEdgeCount());
}

TEST(ManuMesh, MeshTopologyRejectsOutOfRangeHandles) {
    const manumesh::Mesh mesh = manumesh::generatePlaneGrid(4, 1.0, false);
    const manumesh::Result<manumesh::MeshTopology> topologyResult = manumesh::MeshTopology::build(mesh);
    ASSERT_TRUE(topologyResult.ok()) << topologyResult.status().message();
    const manumesh::MeshTopology& topology = topologyResult.value();

    EXPECT_TRUE(topology.hasEdge(manumesh::EdgeId{0}));
    EXPECT_TRUE(topology.hasVertex(manumesh::VertexId{0}));
    EXPECT_FALSE(topology.hasEdge(manumesh::EdgeId{topology.edgeCount()}));
    EXPECT_FALSE(topology.hasVertex(manumesh::VertexId{topology.vertexCount()}));
    EXPECT_THROW(topology.edge(manumesh::EdgeId{topology.edgeCount()}), std::out_of_range);
    EXPECT_THROW(topology.vertex(manumesh::VertexId{topology.vertexCount()}), std::out_of_range);
}

TEST(ManuMesh, MeshTopologyCopiesAndMovesPimplCache) {
    const manumesh::Mesh mesh = manumesh::generatePlaneGrid(4, 1.0, false);
    const manumesh::Result<manumesh::MeshTopology> topologyResult = manumesh::MeshTopology::build(mesh);
    ASSERT_TRUE(topologyResult.ok()) << topologyResult.status().message();

    manumesh::MeshTopology copied = topologyResult.value();
    EXPECT_EQ(topologyResult.value().vertexCount(), copied.vertexCount());
    EXPECT_EQ(topologyResult.value().edgeCount(), copied.edgeCount());
    EXPECT_EQ(topologyResult.value().boundaryEdgeCount(), copied.boundaryEdgeCount());

    manumesh::MeshTopology moved = std::move(copied);
    EXPECT_EQ(static_cast<int>(mesh.vertices.size()), moved.vertexCount());
    EXPECT_GT(moved.edgeCount(), 0);
    EXPECT_GT(moved.boundaryEdgeCount(), 0);
    EXPECT_EQ(0, copied.vertexCount());
}

TEST(ManuMesh, MeshTopologyRejectsInvalidFaces) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 5}}};

    const manumesh::Result<manumesh::MeshTopology> topologyResult = manumesh::MeshTopology::build(mesh);
    EXPECT_FALSE(topologyResult.ok());
    EXPECT_EQ(topologyResult.status().code(), manumesh::StatusCode::InvalidArgument);
    EXPECT_FALSE(topologyResult.hasValue());
    EXPECT_THROW(topologyResult.value(), std::logic_error);
}

} // namespace
