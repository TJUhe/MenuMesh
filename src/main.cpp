#include "Mesh.h"
#include "MeshGenerators.h"
#include "Metrics.h"
#include "QEMSimplifier.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Args {
  std::vector<std::string> values;
};

bool hasFlag(const Args& args, const std::string& name) {
  for (const std::string& value : args.values) {
    if (value == name) return true;
  }
  return false;
}

std::string getArg(const Args& args, const std::string& name,
                   const std::string& defaultValue = "") {
  for (std::size_t i = 0; i + 1 < args.values.size(); ++i) {
    if (args.values[i] == name) {
      return args.values[i + 1];
    }
  }
  return defaultValue;
}

int getIntArg(const Args& args, const std::string& name, int defaultValue) {
  const std::string value = getArg(args, name);
  return value.empty() ? defaultValue : std::stoi(value);
}

double getDoubleArg(const Args& args, const std::string& name,
                    double defaultValue) {
  const std::string value = getArg(args, name);
  return value.empty() ? defaultValue : std::stod(value);
}

std::vector<std::string> positionalArgs(const Args& args) {
  std::vector<std::string> result;
  for (std::size_t i = 0; i < args.values.size(); ++i) {
    const std::string& value = args.values[i];
    if (!value.empty() && value[0] == '-') {
      if (i + 1 < args.values.size() && !args.values[i + 1].empty() &&
          args.values[i + 1][0] != '-') {
        ++i;
      }
      continue;
    }
    result.push_back(value);
  }
  return result;
}

std::vector<double> parseWeights(const std::string& text) {
  std::vector<double> weights;
  std::stringstream ss(text);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (!item.empty()) {
      weights.push_back(std::stod(item));
    }
  }
  return weights;
}

std::vector<int> parseFaceCounts(const std::string& text) {
  std::vector<int> counts;
  std::stringstream ss(text);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (!item.empty()) {
      counts.push_back(std::stoi(item));
    }
  }
  return counts;
}

std::string sanitizeWeight(double value) {
  std::ostringstream out;
  out << std::scientific << std::setprecision(0) << value;
  std::string text = out.str();
  for (char& ch : text) {
    if (ch == '+' || ch == '-' || ch == '.') ch = '_';
  }
  return text;
}

std::string sanitizeRatio(double value) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(2) << value;
  std::string text = out.str();
  while (text.size() > 1 && text.back() == '0') {
    text.pop_back();
  }
  if (!text.empty() && text.back() == '.') {
    text.pop_back();
  }
  for (char& ch : text) {
    if (ch == '.') ch = '_';
  }
  return text;
}

lq::SimplifyOptions parseSimplifyOptions(const Args& args) {
  lq::SimplifyOptions options;
  options.targetRatio = getDoubleArg(args, "--ratio", options.targetRatio);
  options.targetFaces = getIntArg(args, "--target-faces", options.targetFaces);
  options.lineWeight = getDoubleArg(args, "--line-weight", options.lineWeight);
  options.featureBoost =
      getDoubleArg(args, "--feature-boost", options.featureBoost);
  options.featureAngleDeg =
      getDoubleArg(args, "--feature-angle-deg", options.featureAngleDeg);
  options.boundaryWeight =
      getDoubleArg(args, "--boundary-weight", options.boundaryWeight);
  options.adaptiveBaseLineWeight =
      getDoubleArg(args, "--adaptive-base-line-weight",
                   options.adaptiveBaseLineWeight);
  options.verbose = hasFlag(args, "--verbose");
  options.adaptiveScale = hasFlag(args, "--adaptive-scale");

  const std::string mode = getArg(args, "--weight-mode", "uniform");
  options.weightMode = lq::parseWeightMode(mode);

  const std::string method = getArg(args, "--method", "line");
  if (method == "standard" || method == "qem") {
    options.useLineQuadrics = false;
    options.lineWeight = 0.0;
  } else if (method == "line") {
    options.useLineQuadrics = true;
  } else {
    throw std::invalid_argument("Unknown --method. Use standard or line.");
  }
  return options;
}

void printUsage() {
  std::cout
      << "Line Quadrics QEM reproduction\n\n"
      << "Commands:\n"
      << "  linequadrics generate --type clustered-plane --n 50 --out input.stl\n"
      << "  linequadrics simplify input.stl output.stl [options]\n"
      << "  linequadrics compare original.stl simplified.stl [--samples 3000]\n"
      << "  linequadrics sweep input.stl out_dir [options]\n\n"
      << "  linequadrics ratio-sweep input.stl out_dir [options]\n\n"
      << "  linequadrics face-sweep input.stl out_dir [options]\n\n"
      << "Simplify options:\n"
      << "  --method standard|line          Standard QEM or line-quadric QEM\n"
      << "  --ratio 0.25                    Target face ratio\n"
      << "  --target-faces N                Overrides --ratio\n"
      << "  --line-weight W                 Paper default is around 1e-3\n"
      << "  --weight-mode uniform|dihedral|height|xband\n"
      << "  --feature-boost W               Added line weight for feature modes\n"
      << "  --feature-angle-deg A           Dihedral threshold for feature mode\n"
      << "  --adaptive-scale                Add small line quadrics then scale Q\n"
      << "  --boundary-weight W             Optional boundary plane quadrics\n"
      << "  --metrics-csv path              Write one-row CSV metrics\n"
      << "  --samples N                     Distance sample count\n"
      << "  --ratios list                   For ratio-sweep, e.g. 0.8,0.5,0.25,0.1\n"
      << "  --faces list                    For face-sweep, e.g. 1000,900,800\n";
  std::cout
      << "\nGenerator types:\n"
      << "  plane, clustered-plane, hole-plane, ridge, noisy-plane,\n"
      << "  sine-terrain, terrace, bump, cylinder, torus, cube, thin-fin,\n"
      << "  flange\n";
}

void printStats(const std::string& label, const lq::MeshStats& stats) {
  std::cout << label << ": vertices=" << stats.vertices
            << " faces=" << stats.faces
            << " mean_quality=" << stats.meanTriangleQuality
            << " min_quality=" << stats.minTriangleQuality
            << " edge_cv=" << stats.edgeLengthCv << "\n";
}

int commandGenerate(const Args& args) {
  const std::string type = getArg(args, "--type", "clustered-plane");
  const std::string outPath = getArg(args, "--out");
  const int n = getIntArg(args, "--n", 50);
  if (outPath.empty()) {
    throw std::invalid_argument("generate requires --out path.");
  }

  lq::Mesh mesh;
  std::string error;
  if (!lq::generateMeshByName(type, n, mesh, &error)) {
    throw std::runtime_error(error);
  }
  if (!lq::saveAsciiStl(outPath, mesh, type, &error)) {
    throw std::runtime_error(error);
  }

  printStats(type, lq::computeMeshStats(mesh));
  std::cout << "Wrote " << outPath << "\n";
  return 0;
}

int commandCompare(const Args& args) {
  const auto positional = positionalArgs(args);
  if (positional.size() < 2) {
    throw std::invalid_argument("compare requires original.stl simplified.stl.");
  }
  const int samples = getIntArg(args, "--samples", 3000);

  lq::Mesh original;
  lq::Mesh simplified;
  std::string error;
  if (!lq::loadStl(positional[0], original, &error)) throw std::runtime_error(error);
  if (!lq::loadStl(positional[1], simplified, &error)) {
    throw std::runtime_error(error);
  }

  const lq::MeshStats stats = lq::computeMeshStats(simplified);
  const lq::DistanceStats distance =
      lq::compareMeshesBySampledDistance(original, simplified, samples);
  printStats("simplified", stats);
  std::cout << "distance mean original->simplified="
            << distance.meanOriginalToSimplified
            << " max=" << distance.maxOriginalToSimplified << "\n";
  std::cout << "distance mean simplified->original="
            << distance.meanSimplifiedToOriginal
            << " max=" << distance.maxSimplifiedToOriginal << "\n";
  return 0;
}

int commandSimplify(const Args& args) {
  const auto positional = positionalArgs(args);
  if (positional.size() < 2) {
    throw std::invalid_argument("simplify requires input.stl output.stl.");
  }

  const int samples = getIntArg(args, "--samples", 3000);
  const std::string metricsCsv = getArg(args, "--metrics-csv");
  lq::SimplifyOptions options = parseSimplifyOptions(args);

  lq::Mesh input;
  std::string error;
  if (!lq::loadStl(positional[0], input, &error)) {
    throw std::runtime_error(error);
  }

  lq::SimplifyReport report;
  lq::Mesh output = lq::simplifyMesh(input, options, &report);
  if (!lq::saveAsciiStl(positional[1], output, "linequadrics", &error)) {
    throw std::runtime_error(error);
  }

  const lq::MeshStats inStats = lq::computeMeshStats(input);
  const lq::MeshStats outStats = lq::computeMeshStats(output);
  const lq::DistanceStats distance =
      lq::compareMeshesBySampledDistance(input, output, samples);

  printStats("input", inStats);
  printStats("output", outStats);
  std::cout << "collapsed=" << report.collapsedEdges
            << " rejected=" << report.rejectedCollapses
            << " solver_fallbacks=" << report.solverFallbacks
            << " line_weight_range=[" << report.minAppliedLineWeight << ", "
            << report.maxAppliedLineWeight << "]\n";
  std::cout << "distance mean original->simplified="
            << distance.meanOriginalToSimplified
            << " max=" << distance.maxOriginalToSimplified << "\n";

  if (!metricsCsv.empty()) {
    const fs::path metricsPath(metricsCsv);
    if (metricsPath.has_parent_path()) {
      fs::create_directories(metricsPath.parent_path());
    }
    std::ofstream csv(metricsCsv);
    csv << lq::statsHeaderCsv()
        << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
           "min_line_weight,max_line_weight\n";
    csv << lq::statsRowCsv("output", outStats, &distance) << ","
        << report.collapsedEdges << "," << report.rejectedCollapses << ","
        << report.solverFallbacks << "," << report.minAppliedLineWeight << ","
        << report.maxAppliedLineWeight << "\n";
  }

  std::cout << "Wrote " << positional[1] << "\n";
  return 0;
}

int commandSweep(const Args& args) {
  const auto positional = positionalArgs(args);
  if (positional.size() < 2) {
    throw std::invalid_argument("sweep requires input.stl out_dir.");
  }

  lq::Mesh input;
  std::string error;
  if (!lq::loadStl(positional[0], input, &error)) {
    throw std::runtime_error(error);
  }

  const fs::path outDir = positional[1];
  fs::create_directories(outDir);
  const int samples = getIntArg(args, "--samples", 3000);
  const std::vector<double> weights =
      parseWeights(getArg(args, "--weights", "0,1e-5,1e-4,1e-3,1e-2,1e-1"));
  lq::SimplifyOptions base = parseSimplifyOptions(args);

  std::ofstream csv(outDir / "metrics.csv");
  csv << "method,line_weight,weight_mode,"
      << lq::statsHeaderCsv()
      << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
         "min_line_weight,max_line_weight\n";

  for (double weight : weights) {
    lq::SimplifyOptions options = base;
    options.lineWeight = weight;
    options.useLineQuadrics = weight > 0.0 || options.weightMode != lq::WeightMode::Uniform;
    if (weight <= 0.0 && options.weightMode == lq::WeightMode::Uniform) {
      options.useLineQuadrics = false;
    }

    lq::SimplifyReport report;
    lq::Mesh output = lq::simplifyMesh(input, options, &report);
    const std::string method = options.useLineQuadrics ? "line" : "standard";
    const std::string label = method + "_w_" + sanitizeWeight(weight);
    const fs::path outStl = outDir / (label + ".stl");
    if (!lq::saveAsciiStl(outStl.string(), output, label, &error)) {
      throw std::runtime_error(error);
    }

    const lq::MeshStats stats = lq::computeMeshStats(output);
    const lq::DistanceStats distance =
        lq::compareMeshesBySampledDistance(input, output, samples);
    csv << method << "," << weight << "," << lq::toString(options.weightMode)
        << "," << lq::statsRowCsv(label, stats, &distance) << ","
        << report.collapsedEdges << "," << report.rejectedCollapses << ","
        << report.solverFallbacks << "," << report.minAppliedLineWeight << ","
        << report.maxAppliedLineWeight << "\n";
    printStats(label, stats);
  }

  std::cout << "Wrote sweep outputs to " << outDir << "\n";
  return 0;
}

int commandRatioSweep(const Args& args) {
  const auto positional = positionalArgs(args);
  if (positional.size() < 2) {
    throw std::invalid_argument("ratio-sweep requires input.stl out_dir.");
  }

  lq::Mesh input;
  std::string error;
  if (!lq::loadStl(positional[0], input, &error)) {
    throw std::runtime_error(error);
  }

  const fs::path outDir = positional[1];
  fs::create_directories(outDir);
  const int samples = getIntArg(args, "--samples", 3000);
  const std::vector<double> ratios =
      parseWeights(getArg(args, "--ratios", "0.8,0.5,0.25,0.1,0.05"));
  lq::SimplifyOptions base = parseSimplifyOptions(args);

  std::ofstream csv(outDir / "metrics.csv");
  csv << "method,line_weight,weight_mode,ratio,"
      << lq::statsHeaderCsv()
      << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
         "min_line_weight,max_line_weight\n";

  for (double ratio : ratios) {
    if (ratio <= 0.0 || ratio >= 1.0) {
      std::cerr << "skip invalid ratio " << ratio << "\n";
      continue;
    }
    lq::SimplifyOptions options = base;
    options.targetFaces = -1;
    options.targetRatio = ratio;

    lq::SimplifyReport report;
    lq::Mesh output = lq::simplifyMesh(input, options, &report);
    const std::string method = options.useLineQuadrics ? "line" : "standard";
    const std::string label =
        method + "_r_" + sanitizeRatio(ratio) + "_w_" +
        sanitizeWeight(options.lineWeight);
    const fs::path outStl = outDir / (label + ".stl");
    if (!lq::saveAsciiStl(outStl.string(), output, label, &error)) {
      throw std::runtime_error(error);
    }

    const lq::MeshStats stats = lq::computeMeshStats(output);
    const lq::DistanceStats distance =
        lq::compareMeshesBySampledDistance(input, output, samples);
    csv << method << "," << options.lineWeight << ","
        << lq::toString(options.weightMode) << "," << ratio << ","
        << lq::statsRowCsv(label, stats, &distance) << ","
        << report.collapsedEdges << "," << report.rejectedCollapses << ","
        << report.solverFallbacks << "," << report.minAppliedLineWeight << ","
        << report.maxAppliedLineWeight << "\n";
    printStats(label, stats);
  }

  std::cout << "Wrote ratio-sweep outputs to " << outDir << "\n";
  return 0;
}

int commandFaceSweep(const Args& args) {
  const auto positional = positionalArgs(args);
  if (positional.size() < 2) {
    throw std::invalid_argument("face-sweep requires input.stl out_dir.");
  }

  lq::Mesh input;
  std::string error;
  if (!lq::loadStl(positional[0], input, &error)) {
    throw std::runtime_error(error);
  }

  const fs::path outDir = positional[1];
  fs::create_directories(outDir);
  const int samples = getIntArg(args, "--samples", 3000);
  const std::vector<int> faceCounts =
      parseFaceCounts(getArg(args, "--faces",
                             "1000,900,800,700,600,500,400,300,200,100"));
  lq::SimplifyOptions base = parseSimplifyOptions(args);

  std::ofstream csv(outDir / "metrics.csv");
  csv << "method,line_weight,weight_mode,target_faces,"
      << lq::statsHeaderCsv()
      << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
         "min_line_weight,max_line_weight\n";

  for (int targetFaces : faceCounts) {
    if (targetFaces <= 0) {
      std::cerr << "skip invalid target face count " << targetFaces << "\n";
      continue;
    }
    lq::SimplifyOptions options = base;
    options.targetFaces = targetFaces;

    lq::SimplifyReport report;
    lq::Mesh output = lq::simplifyMesh(input, options, &report);
    const std::string method = options.useLineQuadrics ? "line" : "standard";
    const std::string label = method + "_f_" + std::to_string(targetFaces) +
                              "_w_" + sanitizeWeight(options.lineWeight);
    const fs::path outStl = outDir / (label + ".stl");
    if (!lq::saveAsciiStl(outStl.string(), output, label, &error)) {
      throw std::runtime_error(error);
    }

    const lq::MeshStats stats = lq::computeMeshStats(output);
    const lq::DistanceStats distance =
        lq::compareMeshesBySampledDistance(input, output, samples);
    csv << method << "," << options.lineWeight << ","
        << lq::toString(options.weightMode) << "," << targetFaces << ","
        << lq::statsRowCsv(label, stats, &distance) << ","
        << report.collapsedEdges << "," << report.rejectedCollapses << ","
        << report.solverFallbacks << "," << report.minAppliedLineWeight << ","
        << report.maxAppliedLineWeight << "\n";
    printStats(label, stats);
  }

  std::cout << "Wrote face-sweep outputs to " << outDir << "\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      printUsage();
      return 0;
    }

    const std::string command = argv[1];
    Args args;
    for (int i = 2; i < argc; ++i) {
      args.values.emplace_back(argv[i]);
    }

    if (command == "generate") return commandGenerate(args);
    if (command == "simplify") return commandSimplify(args);
    if (command == "compare") return commandCompare(args);
    if (command == "sweep") return commandSweep(args);
    if (command == "ratio-sweep") return commandRatioSweep(args);
    if (command == "face-sweep") return commandFaceSweep(args);
    if (command == "--help" || command == "-h" || command == "help") {
      printUsage();
      return 0;
    }

    throw std::invalid_argument("Unknown command: " + command);
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n\n";
    printUsage();
    return 1;
  }
}
