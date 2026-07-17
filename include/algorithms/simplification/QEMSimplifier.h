/**
 * @file include/algorithms/simplification/QEMSimplifier.h
 * @brief Declares qemsimplifier facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "Export.h"
#include "algorithms/simplification/SimplificationTypes.h"
#include "core/Mesh.h"

#include <memory>

namespace manumesh::feature {
struct FeatureAnalysis;
}

namespace manumesh::simplification {

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/// Stateful object API for configuring and running mesh simplification.
class MANUMESH_API QEMSimplifier {
public:
    /// Constructs a simplifier with default options.
    QEMSimplifier();
    /// @param[in] options Validated options copied into this object.
    /// @throws std::invalid_argument when options are inconsistent.
    explicit QEMSimplifier(SimplifyOptions options);
    ~QEMSimplifier();

    QEMSimplifier(const QEMSimplifier& other);
    QEMSimplifier& operator=(const QEMSimplifier& other);
    QEMSimplifier(QEMSimplifier&& other) noexcept;
    QEMSimplifier& operator=(QEMSimplifier&& other) noexcept;

    /// Returns the options used by subsequent simplification runs.
    const SimplifyOptions& options() const;
    /// Replaces the options used by subsequent simplification runs.
    /// @param[in] options New validated policy.
    /// @throws std::invalid_argument when options are inconsistent.
    void setOptions(SimplifyOptions options);
    /// Returns diagnostics from the most recent simplification run.
    const SimplifyReport& report() const;

    /// Simplifies a mesh and stores diagnostics on this object.
    /// @param[in] input Triangle surface mesh; it is not modified.
    /// @return Simplified dense mesh.
    Mesh simplify(const Mesh& input);
    /// Simplifies a mesh, stores diagnostics, and optionally copies them out.
    /// @param[in] input Triangle surface mesh.
    /// @param[out] report Optional copy of report().
    /// @return Simplified dense mesh.
    Mesh simplify(const Mesh& input, SimplifyReport* report);
    /// Simplifies a mesh using precomputed feature analysis when feature
    /// preservation is enabled.
    /// @param[in] input Mesh from which `features` was computed.
    /// @param[in] features Precomputed graph and loop ownership.
    /// @return Simplified dense mesh without rerunning feature detection.
    Mesh simplify(const Mesh& input, const feature::FeatureAnalysis& features);
    /// Simplifies a mesh using precomputed feature analysis and optionally copies
    /// diagnostics out.
    /// @param[in] input Mesh from which `features` was computed.
    /// @param[in] features Precomputed feature analysis.
    /// @param[out] report Optional diagnostics copy.
    /// @return Simplified dense mesh.
    Mesh simplify(const Mesh& input, const feature::FeatureAnalysis& features, SimplifyReport* report);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

/// Simplifies a mesh with standard QEM or line-quadrics-augmented QEM.
/// Prefer QEMSimplifier for new code that needs an object-oriented API.
/// @param[in] input Source triangle mesh.
/// @param[in] options Target, ranking costs, and acceptance policies.
/// @param[out] report Optional diagnostics.
/// @return Simplified mesh with compacted vertices and faces.
/// @algorithm Accumulates plane and optional line/constraint quadrics, solves
/// ranked placements for each active edge, repeatedly pops the cheapest
/// current candidate, applies hard acceptance filters, updates local topology
/// and candidate versions, then optionally performs fixed-topology refinement.
/// @invariants QEM ranks but never overrides a hard topology, boundary,
/// feature, self-intersection, texture, or error rejection.
/// @failuremodes The run can terminate above target when no legal candidates
/// remain or when the bounded rejection limit is reached; inspect
/// SimplifyReport::terminationReason and rejection counters.
MANUMESH_API Mesh simplifyMesh(const Mesh& input, const SimplifyOptions& options, SimplifyReport* report = nullptr);
/// Simplifies with feature analysis already computed for `input`.
/// @param[in] input Source mesh.
/// @param[in] options Simplification policy.
/// @param[in] features Feature graph and primitive constraints for `input`.
/// @param[out] report Optional diagnostics.
/// @return Simplified mesh without a duplicate feature-analysis pass.
MANUMESH_API Mesh simplifyMesh(
    const Mesh& input,
    const SimplifyOptions& options,
    const feature::FeatureAnalysis& features,
    SimplifyReport* report = nullptr
);

} // namespace manumesh::simplification
