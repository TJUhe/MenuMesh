#include "manumesh/algorithms/simplification/Metrics.h"
#include "manumesh/algorithms/simplification/QEMSimplifier.h"
#include "manumesh/core/MeshGenerators.h"

int main() {
  manumesh::Mesh input = manumesh::generateCylinderGrid(32, 8, 1.0, 2.0);

  manumesh::simplification::SimplifyOptions options;
  options.targetRatio = 0.35;
  options.useLineQuadrics = true;
  options.weightMode = manumesh::simplification::WeightMode::Dihedral;

  manumesh::simplification::QEMSimplifier simplifier(options);
  manumesh::simplification::SimplifyReport report;
  manumesh::Mesh output = simplifier.simplify(input, &report);

  if (output.empty()) {
    return 1;
  }
  if (report.finalFaces >= report.initialFaces) {
    return 2;
  }
  const manumesh::simplification::MeshStats stats =
      manumesh::simplification::computeMeshStats(output);
  return stats.faces == static_cast<int>(output.faces.size()) ? 0 : 3;
}
