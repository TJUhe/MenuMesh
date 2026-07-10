#include "FeatureDetectionTestSupport.h"

#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {

namespace feature = manumesh::feature;

using FeatureAnalysis = feature::FeatureAnalysis;
using FeatureDetector = feature::FeatureDetector;
using FeatureOptions = feature::FeatureOptions;
using FeaturePrimitiveType = feature::FeaturePrimitiveType;
using Mesh = manumesh::Mesh;
using Vec3 = manumesh::Vec3;
using manumesh::test::feature_detection::discreteOnlyOptions;

} // namespace

TEST(FeatureDetection, FeatureNamespaceApiIsProjectScoped) {
    static_assert(std::is_same_v<FeatureAnalysis, manumesh::feature::FeatureAnalysis>);
    static_assert(std::is_same_v<FeatureDetector, manumesh::feature::FeatureDetector>);
    static_assert(std::is_same_v<FeaturePrimitiveType, manumesh::feature::FeaturePrimitiveType>);

    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}};

    FeatureOptions options = discreteOnlyOptions();
    options.minFeatureLoopVertices = 3;
    FeatureDetector detector(options);

    const FeatureAnalysis direct = feature::detectFeatureCurves(mesh, options);
    const FeatureAnalysis objectResult = detector.analyze(mesh);
    const FeatureAnalysis projectScoped = manumesh::feature::detectFeatureCurves(mesh, options);

    EXPECT_EQ(direct.featureEdges, objectResult.featureEdges);
    EXPECT_EQ(direct.loops.size(), projectScoped.loops.size());
    EXPECT_EQ("circle", feature::toString(FeaturePrimitiveType::Circle));
}

TEST(FeatureDetection, FeatureDetectorObjectStoresOptions) {
    FeatureOptions options = discreteOnlyOptions();
    options.minFeatureLoopVertices = 3;
    FeatureDetector detector(options);
    EXPECT_EQ(3, detector.options().minFeatureLoopVertices);

    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}};
    const FeatureAnalysis first = detector.analyze(mesh);
    EXPECT_EQ(3, first.featureEdges);

    options.minFeatureLoopVertices = 100;
    detector.setOptions(options);
    EXPECT_EQ(100, detector.options().minFeatureLoopVertices);
    const FeatureAnalysis second = detector.analyze(mesh);
    const FeatureAnalysis direct = feature::detectFeatureCurves(mesh, options);
    EXPECT_EQ(first.featureEdges, second.featureEdges);
    EXPECT_EQ(direct.loops.size(), second.loops.size());
    EXPECT_EQ(direct.featureEdges, second.featureEdges);
}

TEST(FeatureDetection, FeatureDetectorCopiesAndMovesPimplOptions) {
    FeatureOptions originalOptions = discreteOnlyOptions();
    originalOptions.featureAngleDeg = 25.0;
    originalOptions.minFeatureLoopVertices = 5;
    FeatureDetector original(originalOptions);

    FeatureDetector copied(original);
    FeatureOptions changedOptions = originalOptions;
    changedOptions.featureAngleDeg = 70.0;
    changedOptions.minFeatureLoopVertices = 100;
    original.setOptions(changedOptions);

    EXPECT_DOUBLE_EQ(25.0, copied.options().featureAngleDeg);
    EXPECT_EQ(5, copied.options().minFeatureLoopVertices);
    EXPECT_DOUBLE_EQ(70.0, original.options().featureAngleDeg);

    FeatureDetector assigned;
    assigned = copied;
    changedOptions.featureAngleDeg = 35.0;
    copied.setOptions(changedOptions);

    EXPECT_DOUBLE_EQ(25.0, assigned.options().featureAngleDeg);
    EXPECT_EQ(5, assigned.options().minFeatureLoopVertices);
    EXPECT_DOUBLE_EQ(35.0, copied.options().featureAngleDeg);

    FeatureDetector moved(std::move(assigned));
    EXPECT_DOUBLE_EQ(25.0, moved.options().featureAngleDeg);
    EXPECT_EQ(5, moved.options().minFeatureLoopVertices);

    FeatureDetector moveAssigned;
    moveAssigned = std::move(moved);
    EXPECT_DOUBLE_EQ(25.0, moveAssigned.options().featureAngleDeg);
    EXPECT_EQ(5, moveAssigned.options().minFeatureLoopVertices);
}

TEST(FeatureDetection, RejectsInvalidOptionsAndMeshes) {
    Mesh mesh;
    mesh.vertices = {
        Vec3(0.0, 0.0, 0.0),
        Vec3(1.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
    };
    mesh.faces = {{{0, 1, 2}}};

    FeatureOptions invalidOptions = discreteOnlyOptions();
    invalidOptions.featureAngleDeg = std::numeric_limits<double>::infinity();
    EXPECT_THROW(feature::detectFeatureCurves(mesh, invalidOptions), std::invalid_argument);

    invalidOptions = discreteOnlyOptions();
    invalidOptions.minFeatureLoopVertices = 2;
    FeatureDetector detector(invalidOptions);
    EXPECT_THROW(detector.analyze(mesh), std::invalid_argument);

    Mesh invalidMesh = mesh;
    invalidMesh.faces = {{{0, 1, 9}}};
    EXPECT_THROW(feature::detectFeatureCurves(invalidMesh, discreteOnlyOptions()), std::invalid_argument);

    Mesh emptyFaceSet;
    emptyFaceSet.vertices = {Vec3(0.0, 0.0, 0.0)};
    EXPECT_NO_THROW({
        const FeatureAnalysis analysis = feature::detectFeatureCurves(emptyFaceSet, discreteOnlyOptions());
        EXPECT_TRUE(analysis.loops.empty());
    });
}
