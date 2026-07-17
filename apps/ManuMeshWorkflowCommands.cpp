/**
 * @file apps/ManuMeshWorkflowCommands.cpp
 * @brief Implements manu mesh workflow commands facilities for the ManuMesh command-line application.
 * @ingroup manumesh_cli
 *
 * @details CLI parsing, validation, dispatch, and reporting are kept outside the geometry library so SDK behavior is independent of process-global command-line state.
 */

#include "ManuMeshWorkflowCommands.h"

#include "CliCommands.h"
#include "CliCsv.h"
#include "CliPath.h"
#include "core/Mesh.h"
#include "io/MeshIo.h"

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace manumesh::cli::workflow_commands {
namespace {

void runRegisteredCommand(const std::string& name, const Args& args) {
    const auto handler = commandRegistry().find(name);
    if (handler == commandRegistry().end())
        throw std::logic_error("CLI workflow references unregistered command: " + name);
    // Internally constructed Args must pass the same per-command option
    // validation as user input so option typos in workflows are not silently
    // ignored.
    validateArgsForCommand(name, args);
    const int code = handler->second(args);
    if (code != 0) {
        throw std::runtime_error(
            "CLI workflow subcommand '" + name + "' failed with exit code " + std::to_string(code)
        );
    }
}

Args makeArgs(std::initializer_list<std::string> values) {
    Args args;
    args.values.assign(values.begin(), values.end());
    return args;
}

std::string pathString(const fs::path& path) { return pathToUtf8(path); }

void runGenerate(const fs::path& inputDir, const std::string& name, const std::string& type, int n) {
    runRegisteredCommand(
        "generate",
        makeArgs({"--type", type, "--n", std::to_string(n), "--out", pathString(inputDir / (name + ".stl"))})
    );
}

fs::path copyExternalInput(const fs::path& inputDir, const std::string& name, const fs::path& source) {
    if (!fs::exists(source)) {
        throw std::runtime_error("External input does not exist: " + pathToUtf8(source));
    }
    fs::create_directories(inputDir);
    const fs::path destination = inputDir / (name + ".stl");
    std::error_code ec;
    fs::remove(destination, ec);
    ec.clear();
    fs::copy_file(source, destination, fs::copy_options::none, ec);
    if (ec) {
        throw std::runtime_error(
            "Failed to copy external input from " + pathToUtf8(source) + " to " + pathToUtf8(destination) + ": " +
            ec.message()
        );
    }
    return destination;
}

void runSweep(const fs::path& input, const fs::path& outDir, std::initializer_list<std::string> options) {
    Args args;
    args.values.push_back(pathString(input));
    args.values.push_back(pathString(outDir));
    args.values.insert(args.values.end(), options.begin(), options.end());
    runRegisteredCommand("sweep", args);
}

void runRatioSweep(const fs::path& input, const fs::path& outDir, std::initializer_list<std::string> options) {
    Args args;
    args.values.push_back(pathString(input));
    args.values.push_back(pathString(outDir));
    args.values.insert(args.values.end(), options.begin(), options.end());
    runRegisteredCommand("ratio-sweep", args);
}

void runFaceSweep(const fs::path& input, const fs::path& outDir, std::initializer_list<std::string> options) {
    Args args;
    args.values.push_back(pathString(input));
    args.values.push_back(pathString(outDir));
    args.values.insert(args.values.end(), options.begin(), options.end());
    runRegisteredCommand("face-sweep", args);
}

void runSimplify(const fs::path& input, const fs::path& output, std::initializer_list<std::string> options) {
    Args args;
    args.values.push_back(pathString(input));
    args.values.push_back(pathString(output));
    args.values.insert(args.values.end(), options.begin(), options.end());
    runRegisteredCommand("simplify", args);
}

void runFeatureReport(const fs::path& input, std::initializer_list<std::string> options) {
    Args args;
    args.values.push_back(pathString(input));
    args.values.insert(args.values.end(), options.begin(), options.end());
    runRegisteredCommand("feature-report", args);
}

void runFeatureCompare(
    const fs::path& original, const fs::path& simplified, std::initializer_list<std::string> options
) {
    Args args;
    args.values.push_back(pathString(original));
    args.values.push_back(pathString(simplified));
    args.values.insert(args.values.end(), options.begin(), options.end());
    runRegisteredCommand("feature-compare", args);
}

void runSummarizeMetrics(const fs::path& outputRoot, const fs::path& summaryPath) {
    Args args;
    args.values = {pathString(outputRoot), pathString(summaryPath)};
    runRegisteredCommand("summarize-metrics", args);
}

} // namespace

int demo(const Args& args) {
    const fs::path inputDir = pathFromUtf8(getArg(args, "--input-dir", "output/demo_input"));
    const fs::path outputDir = pathFromUtf8(getArg(args, "--output-dir", "output/demo"));
    const fs::path flangeSource =
        pathFromUtf8(getArg(args, "--flange-input", "tests/data/external/openfoam_flange.stl"));
    const bool quick = hasFlag(args, "--quick");
    const std::string samples = getArg(args, "--samples", quick ? "500" : "1000");
    fs::create_directories(inputDir);
    fs::create_directories(outputDir);

    if (quick) {
        const fs::path flange = copyExternalInput(inputDir, "external_flange", flangeSource);
        runSweep(
            flange,
            outputDir / "external_flange_standard_budget",
            {"--method", "standard", "--ratio", "0.15", "--weights", "0", "--samples", samples}
        );
        runSweep(
            flange,
            outputDir / "external_flange_line_budget",
            {"--method",
             "line",
             "--ratio",
             "0.15",
             "--weight-mode",
             "dihedral",
             "--feature-boost",
             "0.08",
             "--feature-angle-deg",
             "25",
             "--weights",
             "1e-4,1e-3,1e-2",
             "--samples",
             samples}
        );
        runRatioSweep(
            flange,
            outputDir / "external_flange_ratio_dihedral",
            {"--method",
             "line",
             "--line-weight",
             "1e-3",
             "--weight-mode",
             "dihedral",
             "--feature-boost",
             "0.08",
             "--feature-angle-deg",
             "25",
             "--ratios",
             "0.8,0.5,0.25,0.15,0.08,0.05",
             "--samples",
             samples}
        );
        runFaceSweep(
            flange,
            outputDir / "external_flange_face_ladder",
            {"--method",
             "line",
             "--line-weight",
             "1e-3",
             "--weight-mode",
             "dihedral",
             "--feature-boost",
             "0.08",
             "--feature-angle-deg",
             "25",
             "--faces",
             "1000,900,800,700,600,500,400,300,200,100",
             "--samples",
             samples}
        );
        runSummarizeMetrics(outputDir, outputDir / "demo_summary.csv");
        return 0;
    }

    runGenerate(inputDir, "clustered_plane", "clustered-plane", 60);
    runGenerate(inputDir, "hole_plane", "hole-plane", 60);
    runGenerate(inputDir, "ridge", "ridge", 60);
    runGenerate(inputDir, "noisy_plane", "noisy-plane", 60);
    runGenerate(inputDir, "sine_terrain", "sine-terrain", 56);
    runGenerate(inputDir, "terrace", "terrace", 56);
    runGenerate(inputDir, "bump", "bump", 56);
    runGenerate(inputDir, "cylinder", "cylinder", 48);
    runGenerate(inputDir, "torus", "torus", 48);
    runGenerate(inputDir, "cube", "cube", 45);
    runGenerate(inputDir, "thin_fin", "thin-fin", 48);
    const fs::path flange = copyExternalInput(inputDir, "external_flange", flangeSource);

    runSweep(
        inputDir / "clustered_plane.stl",
        outputDir / "clustered_plane",
        {"--ratio", "0.12", "--weights", "0,1e-5,1e-4,1e-3,1e-2", "--samples", samples}
    );
    runSweep(
        inputDir / "clustered_plane.stl",
        outputDir / "clustered_plane_boundary",
        {"--ratio", "0.12", "--boundary-weight", "5", "--weights", "0,1e-5,1e-4,1e-3,1e-2", "--samples", samples}
    );
    runSweep(
        inputDir / "hole_plane.stl",
        outputDir / "hole_plane_boundary",
        {"--ratio", "0.15", "--boundary-weight", "5", "--weights", "0,1e-4,1e-3,1e-2", "--samples", samples}
    );
    runSweep(
        inputDir / "ridge.stl",
        outputDir / "ridge_uniform",
        {"--ratio", "0.12", "--weights", "0,1e-4,1e-3,1e-2", "--samples", samples}
    );
    runSweep(
        inputDir / "ridge.stl",
        outputDir / "ridge_dihedral",
        {"--ratio",
         "0.12",
         "--weight-mode",
         "dihedral",
         "--feature-boost",
         "0.08",
         "--feature-angle-deg",
         "25",
         "--weights",
         "1e-3",
         "--samples",
         samples}
    );
    runSweep(
        inputDir / "noisy_plane.stl",
        outputDir / "noisy_plane",
        {"--ratio", "0.12", "--weights", "0,1e-3,1e-2,1e-1", "--samples", samples}
    );
    runSweep(
        inputDir / "sine_terrain.stl",
        outputDir / "sine_terrain",
        {"--ratio", "0.15", "--weights", "0,1e-4,1e-3,1e-2,1e-1", "--samples", samples}
    );
    runSweep(
        inputDir / "terrace.stl",
        outputDir / "terrace_uniform",
        {"--ratio", "0.15", "--weights", "0,1e-4,1e-3,1e-2", "--samples", samples}
    );
    runSweep(
        inputDir / "terrace.stl",
        outputDir / "terrace_dihedral",
        {"--ratio",
         "0.15",
         "--weight-mode",
         "dihedral",
         "--feature-boost",
         "0.08",
         "--feature-angle-deg",
         "20",
         "--weights",
         "1e-3",
         "--samples",
         samples}
    );
    runSweep(
        inputDir / "bump.stl",
        outputDir / "bump_height",
        {"--ratio",
         "0.15",
         "--weight-mode",
         "height",
         "--feature-boost",
         "0.05",
         "--weights",
         "1e-4,1e-3,1e-2",
         "--samples",
         samples}
    );
    runSweep(
        inputDir / "cylinder.stl",
        outputDir / "cylinder",
        {"--ratio", "0.18", "--boundary-weight", "2", "--weights", "0,1e-4,1e-3,1e-2", "--samples", samples}
    );
    runSweep(
        inputDir / "torus.stl",
        outputDir / "torus",
        {"--ratio", "0.18", "--weights", "0,1e-4,1e-3,1e-2", "--samples", samples}
    );
    runSweep(
        inputDir / "cube.stl",
        outputDir / "cube_dihedral",
        {"--ratio",
         "0.18",
         "--weight-mode",
         "dihedral",
         "--feature-boost",
         "0.08",
         "--feature-angle-deg",
         "25",
         "--weights",
         "1e-3",
         "--samples",
         samples}
    );
    runSweep(
        inputDir / "thin_fin.stl",
        outputDir / "thin_fin_uniform",
        {"--ratio", "0.18", "--boundary-weight", "2", "--weights", "0,1e-4,1e-3,1e-2", "--samples", samples}
    );
    runSweep(
        inputDir / "thin_fin.stl",
        outputDir / "thin_fin_dihedral",
        {"--ratio",
         "0.18",
         "--boundary-weight",
         "2",
         "--weight-mode",
         "dihedral",
         "--feature-boost",
         "0.1",
         "--feature-angle-deg",
         "20",
         "--weights",
         "1e-3",
         "--samples",
         samples}
    );

    runSweep(
        flange,
        outputDir / "external_flange_standard_budget",
        {"--method", "standard", "--ratio", "0.15", "--weights", "0", "--samples", samples}
    );
    runSweep(
        flange,
        outputDir / "external_flange_line_budget",
        {"--method",
         "line",
         "--ratio",
         "0.15",
         "--weight-mode",
         "dihedral",
         "--feature-boost",
         "0.08",
         "--feature-angle-deg",
         "25",
         "--weights",
         "1e-4,1e-3,1e-2",
         "--samples",
         samples}
    );
    runRatioSweep(
        inputDir / "sine_terrain.stl",
        outputDir / "sine_terrain_ratio_line",
        {"--method", "line", "--line-weight", "1e-3", "--ratios", "0.8,0.5,0.25,0.15,0.08,0.05", "--samples", samples}
    );
    runRatioSweep(
        inputDir / "ridge.stl",
        outputDir / "ridge_ratio_line",
        {"--method", "line", "--line-weight", "1e-3", "--ratios", "0.8,0.5,0.25,0.15,0.08,0.05", "--samples", samples}
    );
    runRatioSweep(
        inputDir / "cube.stl",
        outputDir / "cube_ratio_dihedral",
        {"--method",
         "line",
         "--line-weight",
         "1e-3",
         "--weight-mode",
         "dihedral",
         "--feature-boost",
         "0.08",
         "--feature-angle-deg",
         "25",
         "--ratios",
         "0.8,0.5,0.25,0.15,0.08,0.05",
         "--samples",
         samples}
    );
    runRatioSweep(
        flange,
        outputDir / "external_flange_ratio_dihedral",
        {"--method",
         "line",
         "--line-weight",
         "1e-3",
         "--weight-mode",
         "dihedral",
         "--feature-boost",
         "0.08",
         "--feature-angle-deg",
         "25",
         "--ratios",
         "0.8,0.5,0.25,0.15,0.08,0.05",
         "--samples",
         samples}
    );
    runFaceSweep(
        flange,
        outputDir / "external_flange_face_ladder",
        {"--method",
         "line",
         "--line-weight",
         "1e-3",
         "--weight-mode",
         "dihedral",
         "--feature-boost",
         "0.08",
         "--feature-angle-deg",
         "25",
         "--faces",
         "1000,900,800,700,600,500,400,300,200,100",
         "--samples",
         samples}
    );

    runSummarizeMetrics(outputDir, outputDir / "demo_summary.csv");
    return 0;
}

int validateFeatures(const Args& args) {
    struct CaseSpec {
        std::string name;
        fs::path externalInput;
    };
    const fs::path inputDir = pathFromUtf8(getArg(args, "--input-dir", "tests/output/generated_inputs"));
    const fs::path outDir = pathFromUtf8(getArg(args, "--output-dir", "tests/output/feature_curve_validation"));
    const fs::path spindleSource = pathFromUtf8(getArg(
        args,
        "--spindle-input",
        "tests/data/external/thingi10k/"
        "thingi10k_79361_zheng3_tinkeriffic_40mm_spool_spindle.stl"
    ));
    const fs::path ringSource =
        pathFromUtf8(getArg(args, "--ring-input", "tests/data/external/nasa_antenna_azimuth_track.stl"));
    const fs::path pulleySource = pathFromUtf8(
        getArg(args, "--pulley-input", "tests/data/external/thingi10k/thingi10k_318045_moko_mini_pulley.stl")
    );
    const fs::path flangeSource =
        pathFromUtf8(getArg(args, "--flange-input", "tests/data/external/openfoam_flange.stl"));
    const std::string ratio = getArg(args, "--ratio", "0.20");
    const std::string samples = getArg(args, "--samples", "1000");
    const std::vector<CaseSpec> cases = {
        {"external_spindle", spindleSource},
        {"external_ring_track", ringSource},
        {"external_pulley", pulleySource},
        {"external_flange", flangeSource},
    };
    fs::create_directories(inputDir);
    fs::create_directories(outDir);

    for (const CaseSpec& spec : cases) {
        const fs::path input = copyExternalInput(inputDir, spec.name, spec.externalInput);
        runFeatureReport(
            input,
            {"--feature-angle-deg",
             "25",
             "--circle-fit-threshold",
             "0.04",
             "--min-feature-loop-vertices",
             "8",
             "--csv",
             pathString(outDir / (spec.name + "_features.csv"))}
        );

        const fs::path lineOut = outDir / (spec.name + "_line.stl");
        const fs::path curveOut = outDir / (spec.name + "_curve.stl");
        runSimplify(
            input,
            lineOut,
            {"--method",
             "line",
             "--ratio",
             ratio,
             "--line-weight",
             "1e-3",
             "--weight-mode",
             "dihedral",
             "--feature-boost",
             "0.08",
             "--feature-angle-deg",
             "25",
             "--samples",
             samples,
             "--metrics-csv",
             pathString(outDir / (spec.name + "_line_metrics.csv"))}
        );
        runSimplify(
            input,
            curveOut,
            {"--method",
             "line",
             "--ratio",
             ratio,
             "--line-weight",
             "1e-3",
             "--weight-mode",
             "dihedral",
             "--feature-boost",
             "0.08",
             "--feature-angle-deg",
             "25",
             "--preserve-feature-curves",
             "--feature-curve-weight",
             "0.08",
             "--circle-fit-threshold",
             "0.04",
             "--min-feature-loop-vertices",
             "16",
             "--samples",
             samples,
             "--metrics-csv",
             pathString(outDir / (spec.name + "_curve_metrics.csv"))}
        );
        runFeatureCompare(
            input,
            lineOut,
            {"--feature-angle-deg",
             "25",
             "--circle-fit-threshold",
             "0.04",
             "--min-feature-loop-vertices",
             "8",
             "--csv",
             pathString(outDir / (spec.name + "_line_feature_compare.csv"))}
        );
        runFeatureCompare(
            input,
            curveOut,
            {"--feature-angle-deg",
             "25",
             "--circle-fit-threshold",
             "0.04",
             "--min-feature-loop-vertices",
             "8",
             "--csv",
             pathString(outDir / (spec.name + "_curve_feature_compare.csv"))}
        );
    }

    std::cout << "Feature validation outputs written to " << pathToUtf8(outDir) << "\n";
    return 0;
}

int validateExternal(const Args& args) {
    struct ExternalSpec {
        std::string name;
        std::vector<std::string> candidates;
        std::string notes;
    };
    const fs::path inputDir = pathFromUtf8(getArg(args, "--input-dir", "tests/data/external/common_3d_test_models"));
    const fs::path outDir = pathFromUtf8(getArg(args, "--output-dir", "tests/output/external_model_validation"));
    const std::string ratio = getArg(args, "--ratio", "0.25");
    const std::string samples = getArg(args, "--samples", "800");
    const std::vector<ExternalSpec> models = {
        {"fandisk", {"fandisk.obj"}, "CAD-ish benchmark with hard non-circular features"},
        {"rocker_arm", {"rocker_arm.obj", "rocker-arm.obj"}, "mechanical scan with holes and irregular tessellation"},
        {"beetle", {"beetle.obj"}, "small mixed smooth/sharp organic-style model"},
        {"cow", {"cow.obj"}, "organic model; useful for checking false circular features"},
        {"suzanne", {"suzanne.obj"}, "low-poly hard-edged mesh without true circular CAD loops"},
    };
    fs::create_directories(outDir);
    const fs::path summaryPath = outDir / "external_summary.csv";
    std::ofstream summary(summaryPath);
    summary << "model,notes,input_path,line_output,curve_output,input_faces,line_faces,"
               "curve_faces,line_matched,line_missing,curve_matched,curve_missing,"
               "line_rejected_collapses,curve_rejected_collapses\n";

    int processed = 0;
    for (const ExternalSpec& model : models) {
        fs::path input;
        for (const std::string& candidate : model.candidates) {
            const fs::path path = inputDir / candidate;
            if (fs::exists(path)) {
                input = path;
                break;
            }
        }
        if (input.empty()) {
            std::cout << "Skipping " << model.name << ": OBJ not found in " << pathToUtf8(inputDir) << "\n";
            continue;
        }

        ++processed;
        runFeatureReport(
            input,
            {"--feature-angle-deg",
             "35",
             "--circle-fit-threshold",
             "0.05",
             "--min-feature-loop-vertices",
             "8",
             "--csv",
             pathString(outDir / (model.name + "_features.csv"))}
        );
        const fs::path lineOut = outDir / (model.name + "_line.stl");
        const fs::path curveOut = outDir / (model.name + "_curve.stl");
        const fs::path lineMetrics = outDir / (model.name + "_line_metrics.csv");
        const fs::path curveMetrics = outDir / (model.name + "_curve_metrics.csv");
        const fs::path lineCompare = outDir / (model.name + "_line_feature_compare.csv");
        const fs::path curveCompare = outDir / (model.name + "_curve_feature_compare.csv");

        runSimplify(
            input,
            lineOut,
            {"--method",
             "line",
             "--ratio",
             ratio,
             "--line-weight",
             "1e-3",
             "--weight-mode",
             "dihedral",
             "--feature-boost",
             "0.08",
             "--feature-angle-deg",
             "35",
             "--samples",
             samples,
             "--metrics-csv",
             pathString(lineMetrics)}
        );
        runSimplify(
            input,
            curveOut,
            {"--method",
             "line",
             "--ratio",
             ratio,
             "--line-weight",
             "1e-3",
             "--weight-mode",
             "dihedral",
             "--feature-boost",
             "0.08",
             "--feature-angle-deg",
             "35",
             "--preserve-feature-curves",
             "--feature-curve-weight",
             "0.05",
             "--circle-fit-threshold",
             "0.05",
             "--min-feature-loop-vertices",
             "12",
             "--samples",
             samples,
             "--metrics-csv",
             pathString(curveMetrics)}
        );
        runFeatureCompare(
            input,
            lineOut,
            {"--feature-angle-deg",
             "35",
             "--circle-fit-threshold",
             "0.05",
             "--min-feature-loop-vertices",
             "8",
             "--csv",
             pathString(lineCompare)}
        );
        runFeatureCompare(
            input,
            curveOut,
            {"--feature-angle-deg",
             "35",
             "--circle-fit-threshold",
             "0.05",
             "--min-feature-loop-vertices",
             "8",
             "--csv",
             pathString(curveCompare)}
        );

        manumesh::Mesh mesh;
        std::string error;
        if (!manumesh::loadMesh(pathToUtf8(input), mesh, &error)) {
            throw std::runtime_error(error);
        }
        const auto lineMetricRow = readFirstCsvRow(lineMetrics);
        const auto curveMetricRow = readFirstCsvRow(curveMetrics);
        const auto lineCompareRow = readFirstCsvRow(lineCompare);
        const auto curveCompareRow = readFirstCsvRow(curveCompare);
        const std::vector<std::string> fields = {
            model.name,
            model.notes,
            pathToUtf8(input),
            pathToUtf8(lineOut),
            pathToUtf8(curveOut),
            std::to_string(mesh.faces.size()),
            csvValue(lineMetricRow, "faces"),
            csvValue(curveMetricRow, "faces"),
            csvValue(lineCompareRow, "matched"),
            csvValue(lineCompareRow, "missing"),
            csvValue(curveCompareRow, "matched"),
            csvValue(curveCompareRow, "missing"),
            csvValue(lineMetricRow, "rejected_collapses"),
            csvValue(curveMetricRow, "rejected_collapses"),
        };
        for (std::size_t i = 0; i < fields.size(); ++i) {
            if (i > 0)
                summary << ",";
            summary << quoteCsv(fields[i]);
        }
        summary << "\n";
    }

    if (processed == 0) {
        throw std::runtime_error(
            "No external OBJ files found. Place common-3d-test-models OBJ files in " + pathToUtf8(inputDir) + " first."
        );
    }
    std::cout << "External validation outputs written to " << pathToUtf8(outDir) << "\n";
    return 0;
}

} // namespace manumesh::cli::workflow_commands
