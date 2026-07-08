#include "ManuMeshCli.h"

#include "CliArguments.h"
#include "CliCommands.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace manumesh::cli {
namespace {

void printUsage() {
  std::cout
      << "ManuMesh mesh simplification CLI\n\n"
      << "Commands:\n"
      << "  manumesh generate --type clustered-plane --n 50 --out input.stl\n"
      << "  manumesh simplify input.stl output.stl [options]\n"
      << "  manumesh compare original.stl simplified.stl [--samples 3000]\n"
      << "  manumesh feature-report input.stl [--csv report.csv]\n"
      << "  manumesh feature-compare original.stl simplified.stl [--csv report.csv]\n"
      << "  manumesh sweep input.stl out_dir [options]\n\n"
      << "  manumesh ratio-sweep input.stl out_dir [options]\n\n"
      << "  manumesh face-sweep input.stl out_dir [options]\n\n"
      << "  manumesh demo [--quick] [--samples N]\n\n"
      << "  manumesh summarize-metrics [output_root] [summary.csv]\n\n"
      << "  manumesh validate-features [--ratio R] [--samples N] "
         "[--input-dir dir] [--output-dir dir]\n\n"
      << "  manumesh validate-external [--input-dir dir] [--ratio R] "
         "[--output-dir dir]\n\n"
      << "Simplify options:\n"
      << "  --method standard|line          Standard QEM or line-quadric QEM\n"
      << "  --ratio 0.25                    Target face ratio\n"
      << "  --target-faces N                Overrides --ratio\n"
      << "  --line-weight W                 Paper default is around 1e-3\n"
      << "  --weight-mode uniform|dihedral|normal-tensor|height|xband\n"
      << "  --feature-boost W               Added line weight for feature modes\n"
      << "  --feature-angle-deg A           Dihedral threshold for feature mode\n"
      << "  --loop-trace-angle-deg A        Dihedral threshold for loop tracing; "
         "negative reuses feature angle\n"
      << "  --adaptive-scale                Add small line quadrics then scale Q\n"
      << "  --boundary-weight W             Optional boundary plane quadrics\n"
      << "  --preserve-boundary             Preserve open boundary topology\n"
      << "  --preserve-feature-curves       Protect detected crease/boundary loops\n"
      << "  --feature-protection-mode none|circular-only|primitive-curves|"
         "all-feature-edges\n"
      << "                                  Hard policy for detected features; "
         "default primitive-curves\n"
      << "  --feature-curve-weight W        Tangent-line quadric weight for loops\n"
      << "  --max-feature-curve-deviation-ratio R  Reject polygonal feature "
         "collapses whose raw placement drifts beyond R*bbox_diag\n"
      << "  --circle-fit-threshold R        Relative fit threshold for circular loops\n"
      << "  --ellipse-fit-threshold R       Relative fit threshold for ellipse "
         "reports\n"
      << "  --near-circle-axis-ratio-tolerance R  Axis-ratio tolerance for "
         "near-circles\n"
      << "  --min-feature-loop-vertices N   Stop collapsing a loop below N vertices\n"
      << "  --min-circular-feature-loop-vertices N  Stop circular loops below N\n"
      << "  --normal-tensor-threshold S     Feature score threshold for tensor edges\n"
      << "  --normal-tensor-edge-alignment A Minimum edge/tangent alignment\n"
      << "  --normal-tensor-smoothing N     Optional tensor smoothing iterations\n"
      << "  --normal-tensor-scales N        Number of tensor smoothing scales\n"
      << "  --normal-tensor-min-persistent-scales N  Required supporting tensor "
         "scales\n"
      << "  --no-normal-tensor-features     Disable tensor candidates in feature "
         "detection\n"
      << "  --min-triangle-quality Q        Reject collapses below quality Q in [0,1]\n"
      << "  --max-normal-deviation-deg A    Reject local face normal changes above A\n"
      << "  --max-local-error D             Reject local collapse drift above D\n"
      << "  --max-local-error-ratio R       Reject local drift above R*bbox diagonal\n"
      << "  --prevent-local-intersections   Reject local triangle intersections\n"
      << "  --industrial-safe               Enable conservative boundary/quality "
         "guards\n"
      << "  --metrics-csv path              Write one-row CSV metrics\n"
      << "  --samples N                     Distance sample count\n"
      << "  --ratios list                   For ratio-sweep, e.g. 0.8,0.5,0.25,0.1\n"
      << "  --faces list                    For face-sweep, e.g. 1000,900,800\n"
      << "  --spindle-input path            External spindle/shaft STL for "
         "validate-features\n"
      << "  --ring-input path               External ring/track STL for "
         "validate-features\n"
      << "  --pulley-input path             External pulley STL for "
         "validate-features\n"
      << "  --flange-input path             External finished flange STL for demo/"
         "validate-features\n";
  std::cout << "\nGenerator types:\n"
            << "  plane, clustered-plane, hole-plane, ridge, noisy-plane,\n"
            << "  sine-terrain, terrace, bump, cylinder, torus, cube, thin-fin,\n"
            << "  stepped-shaft, pipe-coupling, pulley\n";
}

} // namespace

int run(int argc, char** argv) {
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
    validateArgs(args);

    if (command == "--help" || command == "-h" || command == "help") {
      printUsage();
      return 0;
    }

    const auto& commands = commandRegistry();
    const auto it = commands.find(command);
    if (it != commands.end()) {
      return it->second(args);
    }

    throw std::invalid_argument("Unknown command: " + command);
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n\n";
    printUsage();
    return 1;
  }
}

} // namespace manumesh::cli
