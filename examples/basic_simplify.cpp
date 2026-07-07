#include "manumesh/algorithms/simplification/Metrics.h"
#include "manumesh/algorithms/simplification/QEMSimplifier.h"
#include "manumesh/core/MeshGenerators.h"

#include <iostream>
#include <stdexcept>

int main() {
  manumesh::Mesh input = manumesh::generateCylinderGrid(48, 12, 1.0, 2.0);

  manumesh::simplification::SimplifyOptions options;
  options.targetRatio = 0.35;
  options.useLineQuadrics = true;
  options.lineWeight = 1e-3;
  options.boundaryWeight = 1.0;

  manumesh::simplification::SimplifyReport report;
  manumesh::simplification::QEMSimplifier simplifier(options);
  manumesh::Mesh simplified = simplifier.simplify(input, &report);
  if (simplified.empty()) {
    throw std::runtime_error("simplifier returned an empty mesh");
  }

  const manumesh::simplification::MeshStats stats =
      manumesh::simplification::computeMeshStats(simplified);
  std::cout << "input_faces=" << input.faces.size()
            << " simplified_faces=" << stats.faces
            << " collapsed_edges=" << report.collapsedEdges << "\n";
  return stats.faces < static_cast<int>(input.faces.size()) ? 0 : 1;
}
