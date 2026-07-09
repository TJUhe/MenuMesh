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
  QEMSimplifier();
  explicit QEMSimplifier(SimplifyOptions options);
  ~QEMSimplifier();

  QEMSimplifier(const QEMSimplifier& other);
  QEMSimplifier& operator=(const QEMSimplifier& other);
  QEMSimplifier(QEMSimplifier&& other) noexcept;
  QEMSimplifier& operator=(QEMSimplifier&& other) noexcept;

  /// Returns the options used by subsequent simplification runs.
  const SimplifyOptions& options() const;
  /// Replaces the options used by subsequent simplification runs.
  void setOptions(SimplifyOptions options);
  /// Returns diagnostics from the most recent simplification run.
  const SimplifyReport& report() const;

  /// Simplifies a mesh and stores diagnostics on this object.
  Mesh simplify(const Mesh& input);
  /// Simplifies a mesh, stores diagnostics, and optionally copies them out.
  Mesh simplify(const Mesh& input, SimplifyReport* report);
  /// Simplifies a mesh using precomputed feature analysis when feature
  /// preservation is enabled.
  Mesh simplify(const Mesh& input, const feature::FeatureAnalysis& features);
  /// Simplifies a mesh using precomputed feature analysis and optionally copies
  /// diagnostics out.
  Mesh simplify(const Mesh& input, const feature::FeatureAnalysis& features,
                SimplifyReport* report);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

/// Simplifies a mesh with standard QEM or line-quadrics-augmented QEM.
/// Prefer QEMSimplifier for new code that needs an object-oriented API.
MANUMESH_API Mesh simplifyMesh(const Mesh& input, const SimplifyOptions& options,
                               SimplifyReport* report = nullptr);
MANUMESH_API Mesh simplifyMesh(const Mesh& input, const SimplifyOptions& options,
                               const feature::FeatureAnalysis& features,
                               SimplifyReport* report = nullptr);

} // namespace manumesh::simplification
