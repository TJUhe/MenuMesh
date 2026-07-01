#include "line_quadrics_qem/core/MeshGenerators.h"
#include "line_quadrics_qem/simplification/Metrics.h"
#include "line_quadrics_qem/simplification/QEMSimplifier.h"

#include <iostream>
#include <stdexcept>

int main() {
  lq::Mesh input = lq::generateCylinderGrid(48, 12, 1.0, 2.0);

  lq::SimplifyOptions options;
  options.targetRatio = 0.35;
  options.useLineQuadrics = true;
  options.lineWeight = 1e-3;
  options.boundaryWeight = 1.0;

  lq::SimplifyReport report;
  lq::Mesh simplified = lq::simplifyMesh(input, options, &report);
  if (simplified.empty()) {
    throw std::runtime_error("simplifier returned an empty mesh");
  }

  const lq::MeshStats stats = lq::computeMeshStats(simplified);
  std::cout << "input_faces=" << input.faces.size()
            << " simplified_faces=" << stats.faces
            << " collapsed_edges=" << report.collapsedEdges << "\n";
  return stats.faces < static_cast<int>(input.faces.size()) ? 0 : 1;
}
