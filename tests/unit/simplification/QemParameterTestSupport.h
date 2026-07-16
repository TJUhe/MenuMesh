/**
 * @file tests/unit/simplification/QemParameterTestSupport.h
 * @brief Verifies qem parameter test support behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
 */

#pragma once

#include "algorithms/feature_detection/FeatureDetector.h"

#include <vector>

namespace manumesh::test::qem_parameters {

inline std::vector<feature::FeatureLoop> innerEllipseLoops(const feature::FeatureAnalysis& analysis) {
    std::vector<feature::FeatureLoop> loops;
    for (const feature::FeatureLoop& loop : analysis.loops) {
        if (loop.primitive == feature::FeaturePrimitiveType::Ellipse && loop.majorRadius < 1.0 &&
            loop.minorRadius > 0.0) {
            loops.push_back(loop);
        }
    }
    return loops;
}

} // namespace manumesh::test::qem_parameters
