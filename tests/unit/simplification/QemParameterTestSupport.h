/**
 * @file tests/unit/simplification/QemParameterTestSupport.h
 * @brief 提供 QEM 参数测试使用的椭圆特征筛选工具。
 * @ingroup manumesh_tests
 */

#pragma once

#include "algorithms/feature_detection/FeatureDetector.h"

#include <vector>

namespace manumesh {
namespace test {
namespace qem_parameters {

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

} // namespace qem_parameters
} // namespace test
} // namespace manumesh
