#pragma once

#include "Mesh.h"

#include <string>

namespace lq {

enum class WeightMode {
  Uniform,
  Dihedral,
  Height,
  XBand,
};

struct SimplifyOptions {
  int targetFaces = -1;
  double targetRatio = 0.25;
  bool useLineQuadrics = true;
  double lineWeight = 1e-3;
  WeightMode weightMode = WeightMode::Uniform;
  double featureBoost = 0.05;
  double featureAngleDeg = 40.0;
  bool adaptiveScale = false;
  double adaptiveBaseLineWeight = 1e-2;
  double boundaryWeight = 0.0;
  bool verbose = false;
};

struct SimplifyReport {
  int initialVertices = 0;
  int initialFaces = 0;
  int finalVertices = 0;
  int finalFaces = 0;
  int collapsedEdges = 0;
  int rejectedCollapses = 0;
  int solverFallbacks = 0;
  int queueRebuilds = 0;
  double minAppliedLineWeight = 0.0;
  double maxAppliedLineWeight = 0.0;
};

WeightMode parseWeightMode(const std::string& value);
std::string toString(WeightMode mode);

Mesh simplifyMesh(const Mesh& input, const SimplifyOptions& options,
                  SimplifyReport* report = nullptr);

}  // namespace lq
