/**
 * @file examples/basic_simplify.cpp
 * @brief 演示使用分组配置完成一次基础 QEM 网格简化。
 * @ingroup manumesh_examples
 *
 * @details 示例只使用受支持的公共入口，同时作为可执行的集成文档。
 */

#include "algorithms/analysis/MeshAnalysis.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"

#include <iostream>
#include <stdexcept>

int main() {
    manumesh::Mesh input = manumesh::generateCylinderGrid(48, 12, 1.0, 2.0);

    manumesh::simplification::SimplifyConfig config;
    config.target = manumesh::simplification::SimplifyTarget::ratio(0.35);
    config.cost.lineQuadrics = manumesh::simplification::LineQuadricConfig::uniform(1e-3);
    config.cost.boundaryWeight = 1.0;

    manumesh::simplification::SimplifyReport report;
    manumesh::simplification::QEMSimplifier simplifier;
    simplifier.setConfig(config);
    manumesh::Mesh simplified = simplifier.simplify(input, &report);
    const manumesh::simplification::SimplifySummary summary = report.summary();
    if (simplified.empty()) {
        throw std::runtime_error("simplifier returned an empty mesh");
    }

    const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(simplified);
    std::cout << "input_faces=" << input.faces.size() << " simplified_faces=" << stats.faces
              << " collapsed_edges=" << summary.collapsedEdges << "\n";
    return stats.faces < static_cast<int>(input.faces.size()) ? 0 : 1;
}
