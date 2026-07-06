#include "line_quadrics_qem/algorithms/simplification/Metrics.h"
#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"
#include "line_quadrics_qem/core/MeshGenerators.h"

#include <iostream>
#include <stdexcept>

int main() {
  lq::Mesh input = lq::generateCylinderGrid(48, 12, 1.0, 2.0);

  lq::simplification::SimplifyOptions options;
  options.targetRatio = 0.35;
  options.useLineQuadrics = true;
  options.lineWeight = 1e-3;
  options.boundaryWeight = 1.0;

  lq::simplification::SimplifyReport report;
  lq::simplification::QEMSimplifier simplifier(options);
  lq::Mesh simplified = simplifier.simplify(input, &report);
  if (simplified.empty()) {
    throw std::runtime_error("simplifier returned an empty mesh");
  }

  const lq::simplification::MeshStats stats =
      lq::simplification::computeMeshStats(simplified);
  std::cout << "input_faces=" << input.faces.size()
            << " simplified_faces=" << stats.faces
            << " collapsed_edges=" << report.collapsedEdges << "\n";
  return stats.faces < static_cast<int>(input.faces.size()) ? 0 : 1;
}
