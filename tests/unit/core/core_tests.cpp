/**
 * @file tests/unit/core/core_tests.cpp
 * @brief 验证网格核心类型、拓扑查询、生成器和 C++14 辅助类型。
 * @ingroup manumesh_tests
 */

#include "algorithms/analysis/MeshAnalysis.h"
#include "common/detail/MeshQueries.h"
#include "common/detail/SpatialIndex.h"
#include "core/Mesh.h"
#include "core/MeshGenerators.h"
#include "core/MeshTopology.h"
#include "core/Optional.h"
#include "core/Status.h"
#include "core/Tolerances.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
namespace {

struct NoDefaultValue {
    explicit NoDefaultValue(int v)
        : value(v) {}
    int value = 0;
};

struct OptionalLifetimeProbe {
    explicit OptionalLifetimeProbe(int v)
        : value(v) {
        ++liveCount;
    }

    OptionalLifetimeProbe(const OptionalLifetimeProbe& other)
        : value(other.value) {
        ++liveCount;
        ++copyConstructionCount;
    }

    OptionalLifetimeProbe(OptionalLifetimeProbe&& other) noexcept
        : value(other.value) {
        other.value = -1;
        ++liveCount;
        ++moveConstructionCount;
    }

    OptionalLifetimeProbe& operator=(const OptionalLifetimeProbe& other) {
        value = other.value;
        ++copyAssignmentCount;
        return *this;
    }

    OptionalLifetimeProbe& operator=(OptionalLifetimeProbe&& other) noexcept {
        value = other.value;
        other.value = -1;
        ++moveAssignmentCount;
        return *this;
    }

    ~OptionalLifetimeProbe() { --liveCount; }

    static void resetCounts() {
        liveCount = 0;
        copyConstructionCount = 0;
        moveConstructionCount = 0;
        copyAssignmentCount = 0;
        moveAssignmentCount = 0;
    }

    int value = 0;
    static int liveCount;
    static int copyConstructionCount;
    static int moveConstructionCount;
    static int copyAssignmentCount;
    static int moveAssignmentCount;
};

int OptionalLifetimeProbe::liveCount = 0;
int OptionalLifetimeProbe::copyConstructionCount = 0;
int OptionalLifetimeProbe::moveConstructionCount = 0;
int OptionalLifetimeProbe::copyAssignmentCount = 0;
int OptionalLifetimeProbe::moveAssignmentCount = 0;

bool containsItem(const std::vector<int>& items, int itemId) {
    return std::find(items.begin(), items.end(), itemId) != items.end();
}

std::vector<int> sorted(std::vector<int> items) {
    std::sort(items.begin(), items.end());
    return items;
}

std::vector<int> copiedIndices(manumesh::TopologyIndexView view) {
    if (view.empty()) {
        return {};
    }
    return std::vector<int>(view.begin(), view.end());
}

TEST(ManuMesh, EmptyMeshBoundsAreZero) {
    const manumesh::Mesh mesh;
    EXPECT_TRUE(mesh.bboxMin().isZero());
    EXPECT_TRUE(mesh.bboxMax().isZero());
    EXPECT_DOUBLE_EQ(0.0, mesh.bboxDiag());
}

TEST(ManuMesh, SuccessfulMeshValidationClearsStaleError) {
    const manumesh::Mesh mesh = manumesh::generatePlaneGrid(2, 1.0, false);
    std::string error = "stale error";

    ASSERT_TRUE(manumesh::validateMeshIndices(mesh, &error));
    EXPECT_TRUE(error.empty());

    error = "stale error";
    ASSERT_TRUE(manumesh::validateMeshGeometry(mesh, &error));
    EXPECT_TRUE(error.empty());

    error = "stale error";
    ASSERT_TRUE(manumesh::validateMeshGeometryLenient(mesh, &error));
    EXPECT_TRUE(error.empty());
}

TEST(ManuMesh, RemoveUnusedVerticesDropsInvalidFacesWithTheirExclusiveVertices) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(2.0, 2.0, 0.0),
    };
    // 面 1 引用了越界索引，必须整体丢弃；顶点 3 只被这个无效面使用，
    // 因此也必须一并移除。
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

    const std::pair<int, int> edge = manumesh::common::unpackMeshEdgeKey(forward);
    const int a = edge.first;
    const int b = edge.second;
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

TEST(ManuMesh, TopologySummaryAndSignedVolumeDescribeClosedOrientedComponents) {
    manumesh::Mesh tetrahedron;
    tetrahedron.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(0.0, 0.0, 1.0),
    };
    tetrahedron.faces = {
        manumesh::Face{{0, 2, 1}},
        manumesh::Face{{0, 1, 3}},
        manumesh::Face{{0, 3, 2}},
        manumesh::Face{{1, 2, 3}},
    };

    const manumesh::Result<manumesh::MeshTopologySummary> summaryResult = manumesh::summarizeMeshTopology(tetrahedron);
    ASSERT_TRUE(summaryResult.ok()) << summaryResult.status().message();
    const manumesh::MeshTopologySummary& summary = summaryResult.value();
    EXPECT_EQ(1u, summary.connectedFaceComponents);
    EXPECT_EQ(6u, summary.uniqueEdges);
    EXPECT_EQ(0u, summary.boundaryEdges);
    EXPECT_EQ(0u, summary.nonManifoldEdges);
    EXPECT_TRUE(summary.closedManifold);
    EXPECT_TRUE(summary.consistentlyOriented);
    EXPECT_NEAR(1.0 / 6.0, manumesh::computeSignedVolume(tetrahedron), 1e-15);

    manumesh::Mesh translated = tetrahedron;
    for (manumesh::Vec3& vertex : translated.vertices) {
        vertex += manumesh::Vec3(1e12, -1e12, 1e12);
    }
    EXPECT_NEAR(1.0 / 6.0, manumesh::computeSignedVolume(translated), 1e-15);

    // Isolated vertices must not widen the normalization frame used by the
    // volume calculation.  Otherwise the active tetrahedron's normalized
    // triple products can underflow to zero.
    translated.vertices.emplace_back(1e150, -1e150, 1e150);
    EXPECT_NEAR(1.0 / 6.0, manumesh::computeSignedVolume(translated), 1e-15);

    manumesh::Mesh distant = tetrahedron;
    for (manumesh::Vec3& vertex : distant.vertices) {
        vertex += manumesh::Vec3(1e9, 1e9, 1e9);
    }
    manumesh::Mesh separated = tetrahedron;
    std::string appendError;
    ASSERT_TRUE(manumesh::appendMesh(separated, distant, &appendError)) << appendError;
    const manumesh::Result<manumesh::MeshTopologySummary> separatedSummary = manumesh::summarizeMeshTopology(separated);
    ASSERT_TRUE(separatedSummary.ok());
    EXPECT_EQ(2u, separatedSummary.value().connectedFaceComponents);
    EXPECT_NEAR(1.0 / 3.0, manumesh::computeSignedVolume(separated), 1e-15);

    manumesh::reverseFaceWindings(tetrahedron);
    const manumesh::Result<manumesh::MeshTopologySummary> reversedSummary =
        manumesh::summarizeMeshTopology(tetrahedron);
    ASSERT_TRUE(reversedSummary.ok());
    EXPECT_TRUE(reversedSummary.value().consistentlyOriented);
    EXPECT_NEAR(-1.0 / 6.0, manumesh::computeSignedVolume(tetrahedron), 1e-15);

    tetrahedron.faces[0] = manumesh::Face{{0, 2, 1}};
    const manumesh::Result<manumesh::MeshTopologySummary> inconsistentSummary =
        manumesh::summarizeMeshTopology(tetrahedron);
    ASSERT_TRUE(inconsistentSummary.ok());
    EXPECT_TRUE(inconsistentSummary.value().closedManifold);
    EXPECT_FALSE(inconsistentSummary.value().consistentlyOriented);
}

TEST(ManuMesh, TopologySummaryClassifiesEmptyOpenNonManifoldAndVertexTouchingMeshes) {
    const manumesh::Result<manumesh::MeshTopologySummary> emptySummary =
        manumesh::summarizeMeshTopology(manumesh::Mesh{});
    ASSERT_TRUE(emptySummary.ok());
    EXPECT_EQ(0u, emptySummary.value().connectedFaceComponents);
    EXPECT_FALSE(emptySummary.value().closedManifold);
    EXPECT_FALSE(emptySummary.value().consistentlyOriented);

    manumesh::Mesh open;
    open.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(1.0, 1.0, 0.0),
    };
    open.faces = {manumesh::Face{{0, 1, 2}}, manumesh::Face{{1, 3, 2}}};
    const manumesh::Result<manumesh::MeshTopologySummary> openSummary = manumesh::summarizeMeshTopology(open);
    ASSERT_TRUE(openSummary.ok());
    EXPECT_EQ(1u, openSummary.value().connectedFaceComponents);
    EXPECT_EQ(5u, openSummary.value().uniqueEdges);
    EXPECT_EQ(4u, openSummary.value().boundaryEdges);
    EXPECT_EQ(0u, openSummary.value().nonManifoldEdges);
    EXPECT_FALSE(openSummary.value().closedManifold);
    EXPECT_TRUE(openSummary.value().consistentlyOriented);

    manumesh::Mesh nonManifold;
    nonManifold.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(0.0, 0.0, 1.0),
        manumesh::Vec3(0.0, 0.0, -1.0),
    };
    nonManifold.faces = {
        manumesh::Face{{0, 1, 2}},
        manumesh::Face{{1, 0, 3}},
        manumesh::Face{{0, 1, 4}},
    };
    const manumesh::Result<manumesh::MeshTopologySummary> nonManifoldSummary =
        manumesh::summarizeMeshTopology(nonManifold);
    ASSERT_TRUE(nonManifoldSummary.ok());
    EXPECT_EQ(1u, nonManifoldSummary.value().connectedFaceComponents);
    EXPECT_EQ(1u, nonManifoldSummary.value().nonManifoldEdges);
    EXPECT_FALSE(nonManifoldSummary.value().closedManifold);
    EXPECT_FALSE(nonManifoldSummary.value().consistentlyOriented);

    manumesh::Mesh vertexTouching;
    vertexTouching.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(-1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, -1.0, 0.0),
    };
    vertexTouching.faces = {manumesh::Face{{0, 1, 2}}, manumesh::Face{{0, 3, 4}}};
    const manumesh::Result<manumesh::MeshTopologySummary> vertexTouchingSummary =
        manumesh::summarizeMeshTopology(vertexTouching);
    ASSERT_TRUE(vertexTouchingSummary.ok());
    EXPECT_EQ(2u, vertexTouchingSummary.value().connectedFaceComponents);
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

TEST(ManuMesh, UniqueEdgesReturnsLexicographicallyStableInputEdgeOrder) {
    manumesh::Mesh mesh;
    mesh.vertices.resize(5, manumesh::Vec3::Zero());
    mesh.faces = {
        manumesh::Face{{4, 2, 1}},
        manumesh::Face{{3, 1, 2}},
    };

    const std::vector<std::pair<int, int>> edges = manumesh::uniqueEdges(mesh);
    const std::vector<std::pair<int, int>> expected = {
        {1, 2},
        {1, 3},
        {1, 4},
        {2, 3},
        {2, 4},
    };
    EXPECT_EQ(expected, edges);
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

TEST(ManuMesh, BasicMeshGeometryQueriesReturnAlignedAreasCentroidAndNormals) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(1.0, 1.0, 0.0),
        manumesh::Vec3(5.0, 5.0, 5.0),
    };
    mesh.faces = {
        manumesh::Face{{0, 1, 2}},
        manumesh::Face{{1, 3, 2}},
        manumesh::Face{{0, 1, 99}},
    };
    const std::vector<double> areas = manumesh::computeFaceAreas(mesh);
    ASSERT_EQ(3u, areas.size());
    EXPECT_DOUBLE_EQ(0.5, areas[0]);
    EXPECT_DOUBLE_EQ(0.5, areas[1]);
    EXPECT_DOUBLE_EQ(0.0, areas[2]);
    EXPECT_DOUBLE_EQ(1.0, manumesh::computeSurfaceArea(mesh));
    EXPECT_TRUE(manumesh::computeSurfaceCentroid(mesh).isApprox(manumesh::Vec3(0.5, 0.5, 0.0), 1e-12));

    const std::vector<manumesh::Vec3> faceNormals = manumesh::computeFaceNormals(mesh);
    ASSERT_EQ(3u, faceNormals.size());
    EXPECT_TRUE(faceNormals[0].isApprox(manumesh::Vec3::UnitZ(), 1e-12));
    EXPECT_TRUE(faceNormals[2].isZero());
    const std::vector<manumesh::Vec3> vertexNormals = manumesh::computeVertexNormals(mesh);
    ASSERT_EQ(mesh.vertices.size(), vertexNormals.size());
    EXPECT_TRUE(vertexNormals[0].isApprox(manumesh::Vec3::UnitZ(), 1e-12));
    EXPECT_TRUE(vertexNormals[4].isZero());
}

TEST(ManuMesh, TriangleNormalUsesTheSharedAbsoluteDegeneracyThreshold) {
    const manumesh::Vec3 a(0.0, 0.0, 0.0);
    const manumesh::Vec3 b(1e-13, 0.0, 0.0);
    const manumesh::Vec3 c(0.0, 1e-13, 0.0);

    EXPECT_GT(manumesh::triangleArea(a, b, c), 0.0);
    EXPECT_LE(manumesh::triangleArea(a, b, c), manumesh::kMinTriangleArea);
    EXPECT_TRUE(manumesh::triangleNormal(a, b, c).isZero());
}

TEST(ManuMesh, TriangleAreaAndNormalHandleExtremeFiniteAspectRatios) {
    const manumesh::Vec3 a(0.0, 0.0, 0.0);
    const manumesh::Vec3 b(1e100, 0.0, 0.0);
    const manumesh::Vec3 c(0.0, 1e-100, 0.0);
    EXPECT_DOUBLE_EQ(0.5, manumesh::triangleArea(a, b, c));
    EXPECT_TRUE(manumesh::triangleNormal(a, b, c).isApprox(manumesh::Vec3::UnitZ(), 1e-12));

    manumesh::Mesh mesh;
    mesh.vertices = {a, b, c};
    mesh.faces = {manumesh::Face{{0, 1, 2}}};
    std::string error;
    EXPECT_TRUE(manumesh::validateMeshGeometry(mesh, &error)) << error;
    EXPECT_DOUBLE_EQ(0.5, manumesh::computeSurfaceArea(mesh));
}

TEST(ManuMesh, BasicMeshEditsPreserveTextureAlignmentAndSupportSelfAppend) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(9.0, 9.0, 9.0),
    };
    mesh.faces = {manumesh::Face{{0, 1, 2}}, manumesh::Face{{0, 0, 1}}};
    mesh.faceTexCoords.resize(2);
    mesh.faceTexCoords[0].valid = true;
    mesh.faceTexCoords[0].uv[1] = manumesh::Vec2(0.25, 0.75);
    EXPECT_EQ(1, manumesh::removeDegenerateFaces(mesh));
    ASSERT_EQ(1u, mesh.faces.size());
    ASSERT_EQ(3u, mesh.vertices.size());
    ASSERT_EQ(1u, mesh.faceTexCoords.size());

    manumesh::reverseFaceWindings(mesh);
    EXPECT_EQ((std::array<int, 3>{{0, 2, 1}}), mesh.faces[0].v);
    EXPECT_TRUE(mesh.faceTexCoords[0].uv[2].isApprox(manumesh::Vec2(0.25, 0.75)));

    std::string error;
    ASSERT_TRUE(manumesh::appendMesh(mesh, mesh, &error)) << error;
    ASSERT_EQ(6u, mesh.vertices.size());
    ASSERT_EQ(2u, mesh.faces.size());
    EXPECT_EQ((std::array<int, 3>{{3, 5, 4}}), mesh.faces[1].v);
    ASSERT_EQ(2u, mesh.faceTexCoords.size());
    EXPECT_TRUE(mesh.faceTexCoords[1].valid);
}

TEST(ManuMesh, InvalidTextureEntriesAreDeterministicallyZeroedByBasicEdits) {
    manumesh::FaceTexCoords defaults;
    for (const manumesh::Vec2& uv : defaults.uv) {
        EXPECT_TRUE(uv.isZero());
    }

    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
    };
    mesh.faces = {manumesh::Face{{0, 1, 2}}};
    mesh.faceTexCoords.resize(1);
    mesh.faceTexCoords[0].uv = {manumesh::Vec2(1.0, 2.0), manumesh::Vec2(3.0, 4.0), manumesh::Vec2(5.0, 6.0)};

    manumesh::reverseFaceWindings(mesh);
    ASSERT_FALSE(mesh.faceTexCoords[0].valid);
    for (const manumesh::Vec2& uv : mesh.faceTexCoords[0].uv) {
        EXPECT_TRUE(uv.isZero());
    }
}

TEST(ManuMesh, MeshValidationRejectsCoordinatesOutsideSupportedNumericRange) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1e308, 0.0, 0.0),
        manumesh::Vec3(0.0, 1e308, 0.0),
    };
    mesh.faces = {manumesh::Face{{0, 1, 2}}};
    std::string error;
    EXPECT_FALSE(manumesh::validateMeshGeometryLenient(mesh, &error));
    EXPECT_NE(std::string::npos, error.find("numeric coordinate range"));
}

TEST(ManuMesh, ResultRejectsSuccessWithoutAValue) {
    EXPECT_THROW((manumesh::Result<NoDefaultValue>(manumesh::Status::success())), std::invalid_argument);
}

TEST(ManuMesh, OptionalSupportsCxx14ValueAndLifetimeOperations) {
    OptionalLifetimeProbe::resetCounts();

    manumesh::Optional<OptionalLifetimeProbe> empty;
    EXPECT_FALSE(empty.has_value());
    EXPECT_FALSE(static_cast<bool>(empty));
    EXPECT_THROW(empty.value(), std::logic_error);

    OptionalLifetimeProbe& emplaced = empty.emplace(7);
    EXPECT_TRUE(empty.has_value());
    EXPECT_EQ(7, emplaced.value);
    EXPECT_EQ(&emplaced, empty.operator->());
    EXPECT_EQ(1, OptionalLifetimeProbe::liveCount);

    manumesh::Optional<OptionalLifetimeProbe> copied(empty);
    EXPECT_EQ(7, copied->value);
    EXPECT_EQ(2, OptionalLifetimeProbe::liveCount);
    EXPECT_EQ(1, OptionalLifetimeProbe::copyConstructionCount);

    manumesh::Optional<OptionalLifetimeProbe> moved(std::move(copied));
    EXPECT_EQ(7, moved->value);
    EXPECT_TRUE(copied.has_value());
    EXPECT_EQ(-1, copied->value);
    EXPECT_EQ(3, OptionalLifetimeProbe::liveCount);
    EXPECT_EQ(1, OptionalLifetimeProbe::moveConstructionCount);

    copied = empty;
    EXPECT_EQ(7, copied->value);
    EXPECT_EQ(1, OptionalLifetimeProbe::copyAssignmentCount);

    moved = OptionalLifetimeProbe(11);
    EXPECT_EQ(11, moved->value);
    EXPECT_EQ(1, OptionalLifetimeProbe::moveAssignmentCount);

    empty.reset();
    copied.reset();
    moved.reset();
    EXPECT_EQ(0, OptionalLifetimeProbe::liveCount);
}

TEST(ManuMesh, OptionalAssignmentConstructsAndClearsDisengagedStorage) {
    manumesh::Optional<NoDefaultValue> source(NoDefaultValue(5));
    manumesh::Optional<NoDefaultValue> destination;

    destination = source;
    ASSERT_TRUE(destination.has_value());
    EXPECT_EQ(5, destination->value);

    source.reset();
    destination = source;
    EXPECT_FALSE(destination.has_value());
}

TEST(ManuMesh, UniformAabbCandidateGridInsertIsIdempotent) {
    manumesh::common::UniformAabbCandidateGrid grid;
    grid.reset(manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(10.0, 10.0, 10.0), 1000);
    ASSERT_TRUE(grid.enabled());

    grid.insert(1, manumesh::Vec3(0.0, 0.0, 0.0), manumesh::Vec3(1.0, 1.0, 1.0));
    // 在新位置重新插入同一条目时，必须清除旧位置中的过期单元。
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
        // 缓冲查询的结果同样不能包含重复项。
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
        {{0, 1, 2}}, // 可用面
        {{0, 1, 9}}, // 索引越界
        {{0, 1, -1}},
        {{0, 1, 4}}, // 坐标包含非有限值
        {{0, 0, 2}}, // 索引重复
        {{0, 1, 3}}, // 零面积
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
    malformed.faces.push_back({{0, 1, 3}}); // 不同索引但面积为零
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

TEST(ManuMesh, SampledDistanceCoversSmallDisconnectedComponents) {
    manumesh::Mesh original;
    original.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(100.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 100.0, 0.0),
        manumesh::Vec3(1000.0, 0.0, 0.0),
        manumesh::Vec3(1000.01, 0.0, 0.0),
        manumesh::Vec3(1000.0, 0.01, 0.0),
    };
    original.faces = {{{0, 1, 2}}, {{3, 4, 5}}};

    manumesh::Mesh simplified;
    simplified.vertices.assign(original.vertices.begin(), original.vertices.begin() + 3);
    simplified.faces = {{{0, 1, 2}}};

    const manumesh::analysis::DistanceStats stats =
        manumesh::analysis::compareMeshesBySampledDistance(original, simplified, 2);
    EXPECT_GT(stats.maxOriginalToSimplified, 800.0);
    EXPECT_GT(stats.meanOriginalToSimplified, 400.0);
    EXPECT_NEAR(0.0, stats.maxSimplifiedToOriginal, 1e-12);
}

TEST(ManuMesh, MeshAnalysisHandlesFiniteSurfaceAreaOverflowWithoutDroppingSamples) {
    manumesh::Mesh mesh;
    const double scale = 5e152;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(scale, 0.0, 0.0),
        manumesh::Vec3(0.0, scale, 0.0),
    };
    mesh.faces.assign(4096, manumesh::Face{{0, 1, 2}});

    const manumesh::analysis::MeshStats meshStats = manumesh::analysis::computeMeshStats(mesh);
    EXPECT_TRUE(std::isinf(meshStats.area));

    const manumesh::analysis::DistanceStats distanceStats =
        manumesh::analysis::compareMeshesBySampledDistance(mesh, mesh, 64);
    EXPECT_NEAR(0.0, distanceStats.meanOriginalToSimplified, 1e-12);
    EXPECT_NEAR(0.0, distanceStats.maxOriginalToSimplified, 1e-12);
    EXPECT_NEAR(0.0, distanceStats.meanSimplifiedToOriginal, 1e-12);
    EXPECT_NEAR(0.0, distanceStats.maxSimplifiedToOriginal, 1e-12);
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

TEST(ManuMesh, MeshTopologyCompactViewsMatchLegacyTopologyExactly) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(0.0, 0.0, 1.0),
        manumesh::Vec3(0.0, 0.0, -1.0),
        manumesh::Vec3(3.0, 3.0, 3.0), // isolated vertex exercises an empty CSR row
    };
    mesh.faces = {
        {{0, 1, 2}},
        {{1, 0, 3}},
        {{4, 0, 1}},
    };

    const manumesh::Result<manumesh::MeshTopology> result = manumesh::MeshTopology::build(mesh);
    ASSERT_TRUE(result.ok()) << result.status().message();
    const manumesh::MeshTopology& topology = result.value();
    const std::vector<manumesh::TopologyEdge>& legacyEdges = topology.edges();
    ASSERT_EQ(static_cast<std::size_t>(topology.edgeCount()), legacyEdges.size());
    ASSERT_GT(topology.edgeCount(), 0);
    EXPECT_EQ(&legacyEdges[0], &topology.edge(manumesh::EdgeId{0}));

    for (int edgeId = 0; edgeId < topology.edgeCount(); ++edgeId) {
        const manumesh::TopologyEdgeView compact = topology.edgeView(manumesh::EdgeId{edgeId});
        const manumesh::TopologyEdge& legacy = legacyEdges[static_cast<std::size_t>(edgeId)];
        EXPECT_EQ(legacy.vertices, compact.vertices);
        EXPECT_EQ(legacy.faces, copiedIndices(compact.faces));
        ASSERT_EQ(legacy.faceCorners.size(), compact.faceCornerCount());
        for (std::size_t incidence = 0; incidence < legacy.faceCorners.size(); ++incidence) {
            EXPECT_EQ(legacy.faceCorners[incidence], compact.faceCorner(incidence));
        }
        EXPECT_EQ(legacy.boundary(), compact.boundary());
        EXPECT_EQ(legacy.manifoldInterior(), compact.manifoldInterior());
        EXPECT_EQ(legacy.nonManifold(), compact.nonManifold());
    }

    for (int vertexId = 0; vertexId < topology.vertexCount(); ++vertexId) {
        const manumesh::VertexTopologyView compact = topology.vertexView(manumesh::VertexId{vertexId});
        const manumesh::VertexTopology& legacy = topology.vertex(manumesh::VertexId{vertexId});
        EXPECT_EQ(legacy.edges, copiedIndices(compact.edges));
        EXPECT_EQ(legacy.faces, copiedIndices(compact.faces));
    }
    EXPECT_TRUE(topology.vertexView(manumesh::VertexId{5}).edges.empty());
    EXPECT_TRUE(topology.vertexView(manumesh::VertexId{5}).faces.empty());
}

TEST(ManuMesh, MeshTopologyCompactViewsPreserveNonManifoldIncidenceAndCorners) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(0.0, 1.0, 0.0),
        manumesh::Vec3(0.0, 0.0, 1.0),
        manumesh::Vec3(0.0, 0.0, -1.0),
    };
    mesh.faces = {{{2, 0, 1}}, {{3, 1, 0}}, {{0, 1, 4}}};

    const manumesh::Result<manumesh::MeshTopology> result = manumesh::MeshTopology::build(mesh);
    ASSERT_TRUE(result.ok()) << result.status().message();
    const manumesh::MeshTopology& topology = result.value();

    int sharedEdgeId = -1;
    for (int edgeId = 0; edgeId < topology.edgeCount(); ++edgeId) {
        const manumesh::TopologyEdgeView edge = topology.edgeView(manumesh::EdgeId{edgeId});
        if (edge.vertices == std::array<int, 2>{{0, 1}}) {
            sharedEdgeId = edgeId;
            break;
        }
    }
    ASSERT_GE(sharedEdgeId, 0);
    const manumesh::TopologyEdgeView shared = topology.edgeView(manumesh::EdgeId{sharedEdgeId});
    EXPECT_TRUE(shared.nonManifold());
    EXPECT_EQ((std::vector<int>{0, 1, 2}), copiedIndices(shared.faces));
    ASSERT_EQ(3u, shared.faceCornerCount());
    EXPECT_EQ(1, shared.faceCorner(0));
    EXPECT_EQ(1, shared.faceCorner(1));
    EXPECT_EQ(0, shared.faceCorner(2));
    EXPECT_EQ(6, topology.boundaryEdgeCount());
    EXPECT_EQ(1, topology.nonManifoldEdgeCount());
}

TEST(ManuMesh, MeshTopologyUsesContiguousIncidenceStorageForLargeGrid) {
    constexpr int resolution = 256;
    const manumesh::Mesh mesh = manumesh::generatePlaneGrid(resolution, 1.0, false);
    const manumesh::Result<manumesh::MeshTopology> result = manumesh::MeshTopology::build(mesh);
    ASSERT_TRUE(result.ok()) << result.status().message();
    const manumesh::MeshTopology& topology = result.value();

    const std::size_t expectedVertices = static_cast<std::size_t>(resolution + 1) * (resolution + 1);
    const std::size_t expectedFaces = 2u * resolution * resolution;
    const std::size_t expectedEdges = 3u * resolution * resolution + 2u * resolution;
    ASSERT_EQ(expectedVertices, static_cast<std::size_t>(topology.vertexCount()));
    ASSERT_EQ(expectedFaces, static_cast<std::size_t>(topology.faceCount()));
    ASSERT_EQ(expectedEdges, static_cast<std::size_t>(topology.edgeCount()));
    EXPECT_EQ(4 * resolution, topology.boundaryEdgeCount());

    std::size_t edgeFaceIncidences = 0;
    const int* previousEdgeEnd = nullptr;
    for (int edgeId = 0; edgeId < topology.edgeCount(); ++edgeId) {
        const manumesh::TopologyEdgeView edge = topology.edgeView(manumesh::EdgeId{edgeId});
        ASSERT_FALSE(edge.faces.empty());
        if (previousEdgeEnd) {
            EXPECT_EQ(previousEdgeEnd, edge.faces.data());
        }
        previousEdgeEnd = edge.faces.end();
        edgeFaceIncidences += edge.faces.size();
    }
    EXPECT_EQ(3u * expectedFaces, edgeFaceIncidences);

    std::size_t vertexFaceIncidences = 0;
    std::size_t vertexEdgeIncidences = 0;
    const int* previousVertexFaceEnd = nullptr;
    const int* previousVertexEdgeEnd = nullptr;
    for (int vertexId = 0; vertexId < topology.vertexCount(); ++vertexId) {
        const manumesh::VertexTopologyView vertex = topology.vertexView(manumesh::VertexId{vertexId});
        ASSERT_FALSE(vertex.faces.empty());
        ASSERT_FALSE(vertex.edges.empty());
        if (previousVertexFaceEnd) {
            EXPECT_EQ(previousVertexFaceEnd, vertex.faces.data());
            EXPECT_EQ(previousVertexEdgeEnd, vertex.edges.data());
        }
        previousVertexFaceEnd = vertex.faces.end();
        previousVertexEdgeEnd = vertex.edges.end();
        vertexFaceIncidences += vertex.faces.size();
        vertexEdgeIncidences += vertex.edges.size();
    }
    EXPECT_EQ(3u * expectedFaces, vertexFaceIncidences);
    EXPECT_EQ(2u * expectedEdges, vertexEdgeIncidences);

    // Regression guard for the structural payload: one CSR payload must remain
    // materially smaller than per-edge/per-vertex vector objects plus incidences.
    const std::size_t compactPayload =
        expectedEdges * sizeof(std::array<int, 2>) +
        (expectedEdges + 1) * sizeof(std::size_t) +
        3u * expectedFaces * (sizeof(int) + sizeof(std::uint8_t)) +
        2u * (expectedVertices + 1) * sizeof(std::size_t) +
        3u * expectedFaces * sizeof(int) +
        2u * expectedEdges * sizeof(int);
    const std::size_t vectorBackedPayload =
        expectedEdges * sizeof(manumesh::TopologyEdge) +
        expectedVertices * sizeof(manumesh::VertexTopology) +
        (9u * expectedFaces + 2u * expectedEdges) * sizeof(int);
    EXPECT_LT(compactPayload, vectorBackedPayload * 3u / 4u);
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
    EXPECT_THROW(topology.edgeView(manumesh::EdgeId{topology.edgeCount()}), std::out_of_range);
    EXPECT_THROW(topology.vertexView(manumesh::VertexId{topology.vertexCount()}), std::out_of_range);
}

TEST(ManuMesh, MeshTopologyCopiesAndMovesPimplCache) {
    static_assert(
        std::is_nothrow_move_constructible<manumesh::MeshTopology>::value,
        "MeshTopology move construction must remain noexcept"
    );
    static_assert(
        std::is_nothrow_move_assignable<manumesh::MeshTopology>::value,
        "MeshTopology move assignment must remain noexcept"
    );

    const manumesh::Mesh mesh = manumesh::generatePlaneGrid(4, 1.0, false);
    const manumesh::Result<manumesh::MeshTopology> topologyResult = manumesh::MeshTopology::build(mesh);
    ASSERT_TRUE(topologyResult.ok()) << topologyResult.status().message();

    const manumesh::TopologyEdge& sourceLegacyEdge = topologyResult.value().edge(manumesh::EdgeId{0});
    const manumesh::VertexTopology& sourceLegacyVertex = topologyResult.value().vertex(manumesh::VertexId{0});

    manumesh::MeshTopology copied = topologyResult.value();
    EXPECT_EQ(topologyResult.value().vertexCount(), copied.vertexCount());
    EXPECT_EQ(topologyResult.value().edgeCount(), copied.edgeCount());
    EXPECT_EQ(topologyResult.value().boundaryEdgeCount(), copied.boundaryEdgeCount());
    EXPECT_EQ(
        copiedIndices(topologyResult.value().edgeView(manumesh::EdgeId{0}).faces),
        copiedIndices(copied.edgeView(manumesh::EdgeId{0}).faces)
    );
    EXPECT_EQ(
        copiedIndices(topologyResult.value().vertexView(manumesh::VertexId{0}).edges),
        copiedIndices(copied.vertexView(manumesh::VertexId{0}).edges)
    );
    const manumesh::TopologyEdge* copiedLegacyEdge = &copied.edge(manumesh::EdgeId{0});
    const manumesh::VertexTopology* copiedLegacyVertex = &copied.vertex(manumesh::VertexId{0});
    EXPECT_EQ(sourceLegacyEdge.faces, copiedLegacyEdge->faces);
    EXPECT_EQ(sourceLegacyVertex.edges, copiedLegacyVertex->edges);
    EXPECT_NE(&sourceLegacyEdge, copiedLegacyEdge);
    EXPECT_NE(&sourceLegacyVertex, copiedLegacyVertex);

    manumesh::MeshTopology moved = std::move(copied);
    EXPECT_EQ(static_cast<int>(mesh.vertices.size()), moved.vertexCount());
    EXPECT_GT(moved.edgeCount(), 0);
    EXPECT_GT(moved.boundaryEdgeCount(), 0);
    EXPECT_FALSE(moved.edgeView(manumesh::EdgeId{0}).faces.empty());
    EXPECT_FALSE(moved.vertexView(manumesh::VertexId{0}).edges.empty());
    EXPECT_EQ(copiedLegacyEdge, &moved.edge(manumesh::EdgeId{0}));
    EXPECT_EQ(copiedLegacyVertex, &moved.vertex(manumesh::VertexId{0}));
    EXPECT_EQ(0, copied.vertexCount());
    EXPECT_EQ(0, copied.faceCount());
    EXPECT_TRUE(copied.edges().empty());
    EXPECT_FALSE(copied.hasEdge(manumesh::EdgeId{0}));
    EXPECT_THROW(copied.edge(manumesh::EdgeId{0}), std::out_of_range);
    copied = topologyResult.value();
    EXPECT_EQ(static_cast<int>(mesh.vertices.size()), copied.vertexCount());
    EXPECT_GT(copied.edgeCount(), 0);

    manumesh::MeshTopology moveAssigned;
    moveAssigned = std::move(moved);
    EXPECT_EQ(static_cast<int>(mesh.vertices.size()), moveAssigned.vertexCount());
    EXPECT_EQ(0, moved.vertexCount());
    EXPECT_TRUE(moved.edges().empty());
}

TEST(ManuMesh, MeshTopologyValidationUsesLenientGeometryContract) {
    manumesh::Mesh mesh;
    mesh.vertices = {
        manumesh::Vec3(0.0, 0.0, 0.0),
        manumesh::Vec3(1.0, 0.0, 0.0),
        manumesh::Vec3(2.0, 0.0, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}};

    const manumesh::Result<manumesh::MeshTopology> zeroArea = manumesh::MeshTopology::build(mesh);
    ASSERT_TRUE(zeroArea.ok()) << zeroArea.status().message();

    manumesh::Mesh nonFinite = mesh;
    nonFinite.vertices[0].x() = std::numeric_limits<double>::infinity();
    const manumesh::Result<manumesh::MeshTopology> rejectedNonFinite = manumesh::MeshTopology::build(nonFinite);
    EXPECT_FALSE(rejectedNonFinite.ok());
    EXPECT_EQ(manumesh::StatusCode::InvalidArgument, rejectedNonFinite.status().code());
    EXPECT_TRUE(manumesh::MeshTopology::build(nonFinite, false).ok());

    manumesh::Mesh misalignedTexture = mesh;
    misalignedTexture.faceTexCoords.resize(2);
    const manumesh::Result<manumesh::MeshTopology> rejectedTexture = manumesh::MeshTopology::build(misalignedTexture);
    EXPECT_FALSE(rejectedTexture.ok());
    EXPECT_EQ(manumesh::StatusCode::InvalidArgument, rejectedTexture.status().code());
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

} // 命名空间
