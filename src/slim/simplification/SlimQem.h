#pragma once

#include "core/Mesh.h"

#include <cstddef>
#include <string>

namespace manumesh {
namespace slim {

struct SimplifyOptions {
    double ratio = 0.5;
    int targetFaces = 0;
    double featureAngleDegrees = 45.0;
    bool preserveBoundary = true;
    bool preserveFeatures = true;
};

struct SimplifyReport {
    std::size_t initialFaces = 0;
    std::size_t targetFaces = 0;
    std::size_t finalFaces = 0;
    std::size_t collapses = 0;
    std::size_t rejectedCandidates = 0;
    std::string stopReason;
};

Mesh simplifyQem(const Mesh& input, const SimplifyOptions& options, SimplifyReport* report = nullptr);

} // namespace slim
} // namespace manumesh
