/**
 * @file tests/unit/feature_detection/FeatureDetectionTestSupport.h
 * @brief Verifies feature detection test support behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
 */

#pragma once

#include "algorithms/feature_detection/FeatureDetector.h"
#include "core/Mesh.h"

#include <vector>

namespace manumesh::test::feature_detection {

namespace feature = manumesh::feature;

using FeatureAnalysis = feature::FeatureAnalysis;
using FeatureLoop = feature::FeatureLoop;
using FeatureOptions = feature::FeatureOptions;
using FeaturePrimitiveType = feature::FeaturePrimitiveType;
using Mesh = manumesh::Mesh;
using Vec3 = manumesh::Vec3;

struct PlaneCluster {
    Vec3 normal = Vec3::Zero();
    double offset = 0.0;
    double area = 0.0;
};

int countClosedLoops(const FeatureAnalysis& analysis);
int countLoopsOfType(const FeatureAnalysis& analysis, FeaturePrimitiveType primitive);

FeatureOptions discreteOnlyOptions();

std::vector<PlaneCluster> clusterCoplanarFaces(const Mesh& mesh, double normalTolerance, double offsetTolerance);
std::vector<FeatureLoop> circularLoopsNearRadius(const FeatureAnalysis& analysis, double radius, double tolerance);

double radialCenterOffsetBetweenLoops(const FeatureLoop& a, const FeatureLoop& b);
double parallelError(const Vec3& a, const Vec3& b);

bool hasPlaneCluster(const std::vector<PlaneCluster>& planes, const Vec3& normal, double offset, double minArea);

Mesh makeBranchedCircularBoundaryMesh();
Mesh makeMultiJunctionPolygonalBoundaryMesh();
Mesh makeMixedDiscreteEvidenceMesh();
Mesh makeShallowDihedralLoopMesh(double height);
Mesh makeFragmentedCircleWithTensorRidgeMesh();

bool hasClosedLoopWithVertices(const FeatureAnalysis& features, const std::vector<int>& expectedVertices);

} // namespace manumesh::test::feature_detection
