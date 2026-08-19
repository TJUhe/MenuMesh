#include "SlimCli.h"

#include "io/MeshIo.h"
#include "slim/analysis/SlimMeshAnalysis.h"
#include "slim/features/SlimFeatureDetection.h"
#include "slim/simplification/SlimQem.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace manumesh {
namespace slim {
namespace cli {
namespace {

void printUsage() {
    std::cout << "Mesh slim command line\n\n"
              << "  mesh-slim version\n"
              << "  mesh-slim stats input.obj\n"
              << "  mesh-slim feature-report input.obj [--angle 45]\n"
              << "  mesh-slim simplify input.obj output.stl [--ratio 0.5] [--target-faces 5000]\n"
              << "      [--feature-angle 45] [--no-preserve-boundary] [--no-preserve-features]\n";
}

double parseDouble(const std::string& text, const char* option) {
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    if (consumed != text.size() || !std::isfinite(value)) {
        throw std::invalid_argument(std::string(option) + " must be a finite number");
    }
    return value;
}

int parseInteger(const std::string& text, const char* option) {
    std::size_t consumed = 0;
    const int value = std::stoi(text, &consumed);
    if (consumed != text.size()) {
        throw std::invalid_argument(std::string(option) + " must be an integer");
    }
    return value;
}

Mesh loadInput(const std::string& path) {
    Mesh mesh;
    std::string error;
    if (!loadMesh(path, mesh, &error)) {
        throw std::runtime_error(error);
    }
    return mesh;
}

std::string lowerExtension(const std::string& path) {
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return {};
    }
    std::string extension = path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return extension;
}

void saveOutput(const std::string& path, const Mesh& mesh) {
    std::string error;
    const std::string extension = lowerExtension(path);
    const bool saved = extension == ".obj" ? saveObj(path, mesh, &error)
                       : extension == ".stl" ? saveBinaryStl(path, mesh, &error)
                                             : false;
    if (!saved) {
        if (extension != ".obj" && extension != ".stl") {
            throw std::invalid_argument("output must use .obj or .stl");
        }
        throw std::runtime_error(error);
    }
}

void printStats(const Mesh& mesh) {
    const MeshStatistics stats = analyzeMesh(mesh);
    std::cout << std::setprecision(12) << "vertices=" << stats.vertices << " faces=" << stats.faces
              << " area=" << stats.surfaceArea << " bbox_min=" << stats.minimum.transpose()
              << " bbox_max=" << stats.maximum.transpose() << "\n";
}

void requireExactArguments(const std::vector<std::string>& values, std::size_t expected, const char* usage) {
    if (values.size() != expected) {
        throw std::invalid_argument(usage);
    }
}

int runStats(const std::vector<std::string>& values) {
    requireExactArguments(values, 1U, "stats requires one input mesh");
    printStats(loadInput(values[0]));
    return 0;
}

int runFeatureReport(const std::vector<std::string>& values) {
    if (values.empty()) {
        throw std::invalid_argument("feature-report requires an input mesh");
    }
    const Mesh mesh = loadInput(values[0]);
    FeatureOptions options;
    for (std::size_t index = 1; index < values.size(); ++index) {
        if (values[index] != "--angle" || ++index == values.size()) {
            throw std::invalid_argument("feature-report accepts only --angle degrees");
        }
        options.dihedralAngleDegrees = parseDouble(values[index], "--angle");
    }
    const FeatureReport report = detectFeatures(mesh, options);
    std::cout << "boundary_edges=" << report.boundaryEdges << " sharp_edges=" << report.sharpEdges
              << " non_manifold_edges=" << report.nonManifoldEdges << " angle_degrees="
              << options.dihedralAngleDegrees << "\n";
    return 0;
}

int runSimplify(const std::vector<std::string>& values) {
    if (values.size() < 2U) {
        throw std::invalid_argument("simplify requires input and output meshes");
    }
    const std::string inputPath = values[0];
    const std::string outputPath = values[1];
    SimplifyOptions options;
    for (std::size_t index = 2; index < values.size(); ++index) {
        const std::string& option = values[index];
        if (option == "--no-preserve-boundary") {
            options.preserveBoundary = false;
        } else if (option == "--no-preserve-features") {
            options.preserveFeatures = false;
        } else {
            if (++index == values.size()) {
                throw std::invalid_argument(option + " requires a value");
            }
            if (option == "--ratio") {
                options.ratio = parseDouble(values[index], "--ratio");
            } else if (option == "--target-faces") {
                options.targetFaces = parseInteger(values[index], "--target-faces");
                if (options.targetFaces <= 0) {
                    throw std::invalid_argument("--target-faces must be positive");
                }
            } else if (option == "--feature-angle") {
                options.featureAngleDegrees = parseDouble(values[index], "--feature-angle");
            } else {
                throw std::invalid_argument("unknown simplify option: " + option);
            }
        }
    }

    const Mesh input = loadInput(inputPath);
    SimplifyReport report;
    const Mesh output = simplifyQem(input, options, &report);
    saveOutput(outputPath, output);
    std::cout << "initial_faces=" << report.initialFaces << " target_faces=" << report.targetFaces
              << " final_faces=" << report.finalFaces << " collapses=" << report.collapses
              << " rejected=" << report.rejectedCandidates << " stop=" << report.stopReason << "\n";
    return 0;
}

} // namespace

int run(int argc, const char* const* argv) {
    try {
        if (argc < 2) {
            printUsage();
            return 0;
        }
        const std::string command = argv[1];
        std::vector<std::string> values;
        for (int index = 2; index < argc; ++index) {
            values.emplace_back(argv[index]);
        }
        if (command == "--help" || command == "-h" || command == "help") {
            printUsage();
            return 0;
        }
        if (command == "--version" || command == "version") {
            std::cout << "Mesh slim " << MANUMESH_VERSION << "\n";
            return 0;
        }
        if (command == "stats") {
            return runStats(values);
        }
        if (command == "feature-report") {
            return runFeatureReport(values);
        }
        if (command == "simplify") {
            return runSimplify(values);
        }
        throw std::invalid_argument("unknown command: " + command);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return 1;
    }
}

} // namespace cli
} // namespace slim
} // namespace manumesh
