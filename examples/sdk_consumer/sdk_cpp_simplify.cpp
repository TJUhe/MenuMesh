#include "line_quadrics_qem/algorithms/simplification/Metrics.h"
#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"
#include "line_quadrics_qem/core/MeshGenerators.h"

int main() {
  lq::Mesh input = lq::generateCylinderGrid(32, 8, 1.0, 2.0);

  lq::simplification::SimplifyOptions options;
  options.targetRatio = 0.35;
  options.useLineQuadrics = true;
  options.weightMode = lq::simplification::WeightMode::Dihedral;

  lq::simplification::QEMSimplifier simplifier(options);
  lq::simplification::SimplifyReport report;
  lq::Mesh output = simplifier.simplify(input, &report);

  if (output.empty()) {
    return 1;
  }
  if (report.finalFaces >= report.initialFaces) {
    return 2;
  }
  const lq::simplification::MeshStats stats =
      lq::simplification::computeMeshStats(output);
  return stats.faces == static_cast<int>(output.faces.size()) ? 0 : 3;
}
