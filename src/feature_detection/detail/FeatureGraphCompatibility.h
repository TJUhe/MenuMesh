#pragma once

#include "FeatureDetectionTypes.h"
#include "core/Mesh.h"

namespace manumesh::feature::detector_detail {

struct ContinuationBranch {
    int neighbor = -1;
    const TraceEdgeAttrs* attrs = nullptr;
    double alignment = 0.0;
};

ContinuationBranch
bestContinuationBranch(const Mesh& mesh, const TraceGraph& trace, int vertex, int target, double minAlignment);

bool compatibleFeatureEvidence(const TraceEdgeAttrs* lhs, const TraceEdgeAttrs* rhs);

int compatibleSignedKind(const TraceEdgeAttrs* lhs, const TraceEdgeAttrs* rhs);

} // namespace manumesh::feature::detector_detail
