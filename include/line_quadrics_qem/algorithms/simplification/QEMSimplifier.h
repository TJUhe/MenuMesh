#pragma once

#include "line_quadrics_qem/Export.h"
#include "line_quadrics_qem/algorithms/simplification/SimplificationTypes.h"
#include "line_quadrics_qem/core/Mesh.h"

#include <memory>

namespace lq::simplification {

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/// Stateful object API for configuring and running mesh simplification.
class LQ_API QEMSimplifier {
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

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

/// Simplifies a mesh with standard QEM or line-quadrics-augmented QEM.
/// Prefer QEMSimplifier for new code that needs an object-oriented API.
LQ_API Mesh simplifyMesh(const Mesh& input, const SimplifyOptions& options,
                         SimplifyReport* report = nullptr);

} // namespace lq::simplification

namespace lq {

using simplification::QEMSimplifier;
using simplification::simplifyMesh;

} // namespace lq
