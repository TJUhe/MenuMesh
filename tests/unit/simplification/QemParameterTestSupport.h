/**
 * @file tests/unit/simplification/QemParameterTestSupport.h
 * @brief 验证 ManuMesh 测试中的QEM 参数 测试支持行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
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
