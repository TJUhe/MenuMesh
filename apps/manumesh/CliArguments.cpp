/**
 * @file apps/manumesh/CliArguments.cpp
 * @brief Implements cli arguments facilities for the ManuMesh command-line application.
 * @ingroup manumesh_cli
 *
 * @details CLI parsing, validation, dispatch, and reporting are kept outside the geometry library so SDK behavior is independent of process-global command-line state.
 */

#include "CliArguments.h"

#include <cmath>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace manumesh::cli {
namespace {

/// Single source of truth for CLI options. Each spec drives help text, the
/// global known-flag union, and per-command accepted-option sets. A non-empty
/// `arg` marks a value flag; an empty `arg` marks a switch flag.
struct OptionSpec {
    const char* flag;
    const char* arg;
    const char* help;
};

struct OptionGroup {
    const char* title;
    std::vector<OptionSpec> options;
};

/// Options shared by simplify, sweep, ratio-sweep, and face-sweep.
const std::vector<OptionSpec>& simplifyOptionSpecs() {
    static const std::vector<OptionSpec> specs = {
        {"--method", "standard|line", "Standard QEM or line-quadric QEM"},
        {"--ratio", "0.25", "Target face ratio"},
        {"--target-faces", "N", "Overrides --ratio"},
        {"--line-weight", "W", "Paper default is around 1e-3"},
        {"--weight-mode", "uniform|dihedral|normal-tensor|height|xband", "Line-quadric weighting mode"},
        {"--feature-boost", "W", "Added line weight for feature modes"},
        {"--feature-angle-deg", "A", "Dihedral threshold for feature mode"},
        {"--loop-trace-angle-deg", "A", "Dihedral threshold for loop tracing; negative reuses feature angle"},
        {"--adaptive-base-line-weight", "W", "Base line-quadric weight added before adaptive Q scaling"},
        {"--boundary-weight", "W", "Optional boundary plane quadrics"},
        {"--feature-curve-weight", "W", "Tangent-line quadric weight for loops"},
        {"--max-feature-curve-deviation-ratio",
         "R",
         "Reject polygonal feature collapses whose raw placement drifts beyond R*bbox_diag"},
        {"--circle-fit-threshold", "R", "Relative fit threshold for circular loops"},
        {"--ellipse-fit-threshold", "R", "Relative fit threshold for ellipse reports"},
        {"--near-circle-axis-ratio-tolerance", "R", "Axis-ratio tolerance for near-circles"},
        {"--min-feature-loop-vertices", "N", "Stop collapsing a loop below N vertices"},
        {"--min-circular-feature-loop-vertices", "N", "Stop circular loops below N"},
        {"--feature-protection-mode",
         "none|circular-only|primitive-curves|all-feature-edges",
         "Hard policy for detected features; default primitive-curves"},
        {"--normal-tensor-threshold", "S", "Feature score threshold for tensor edges"},
        {"--normal-tensor-edge-alignment", "A", "Minimum edge/tangent alignment"},
        {"--normal-tensor-smoothing", "N", "Optional tensor smoothing iterations"},
        {"--normal-tensor-scales", "N", "Number of tensor smoothing scales"},
        {"--normal-tensor-min-persistent-scales", "N", "Required supporting tensor scales"},
        {"--smooth-curvature-threshold", "S", "Scale-normalized quadric feature threshold"},
        {"--smooth-curvature-edge-alignment", "A", "Minimum edge/curve-tangent alignment"},
        {"--smooth-curvature-tangent-consistency", "A", "Cross-scale tangent agreement"},
        {"--smooth-curvature-base-rings", "N", "Smallest quadric-fit neighborhood"},
        {"--smooth-curvature-scales", "N", "Number of quadric-fit scales"},
        {"--smooth-curvature-min-persistent-scales", "N", "Required supporting smooth scales"},
        {"--smooth-curvature-robust-iterations", "N", "Robust fit reweighting passes"},
        {"--feature-graph-gap-ratio", "R", "Bridge endpoint gaps up to R local edges"},
        {"--feature-graph-max-weak-spur-edges", "N", "Remove weak-evidence spurs up to N edges"},
        {"--feature-graph-min-weak-spur-strength", "S", "Keep weak spurs whose integrated strength reaches S"},
        {"--feature-component-min-confidence", "C", "Component confidence report threshold"},
        {"--min-triangle-quality", "Q", "Reject collapses below quality Q in [0,1]"},
        {"--max-normal-deviation-deg", "A", "Reject local face normal changes above A"},
        {"--max-local-error", "D", "Reject local collapse drift above D"},
        {"--max-local-error-ratio", "R", "Reject local drift above R*bbox diagonal"},
        {"--quality-refinement-iterations", "N", "Run N fixed-topology quality passes"},
        {"--adaptive-scale", "", "Scale queue priority by local curvature (placement unchanged)"},
        {"--preserve-boundary", "", "Preserve open boundary topology"},
        {"--preserve-feature-curves", "", "Protect detected crease/boundary loops"},
        {"--prevent-local-intersections", "", "Reject local triangle intersections"},
        {"--industrial-safe", "", "Enable conservative boundary/quality guards"},
        {"--no-normal-tensor-features", "", "Disable tensor candidates in feature detection"},
        {"--smooth-curvature-features", "", "Enable deterministic smooth ridge/valley preservation"},
        {"--smooth-curvature-stable-scale", "", "Select smooth-feature scale by cross-scale stability"},
        {"--smooth-curvature-min-scale-stability", "S", "Reject smooth evidence below scale stability S"},
        {"--feature-normal-filter", "", "Stabilize noisy face normals before feature detection"},
        {"--feature-normal-filter-iterations", "N", "Normal-filter iterations"},
        {"--feature-normal-filter-angle-sigma-deg", "A", "Normal-filter angular bandwidth"},
        {"--feature-normal-filter-preserve-angle-deg", "A", "Do not smooth across edges sharper than A"},
        {"--feature-normal-filter-relaxation", "R", "Normal-filter relaxation in [0,1]"},
        {"--feature-graph-consolidation", "", "Recover compatible gaps between weak components"},
        {"--feature-graph-consolidation-gap-ratio", "R", "Component recovery gap in local-edge units"},
        {"--feature-graph-consolidation-alignment", "A", "Minimum continuation alignment for recovery"},
        {"--no-feature-graph-cleanup", "", "Disable weak spur/gap graph cleanup"},
    };
    return specs;
}

/// Options shared by feature-report, feature-benchmark, and feature-compare.
const std::vector<OptionSpec>& featureOptionSpecs() {
    static const std::vector<OptionSpec> specs = {
        {"--feature-angle-deg", "A", "Dihedral threshold for feature edges"},
        {"--loop-trace-angle-deg", "A", "Dihedral threshold for loop tracing; negative reuses feature angle"},
        {"--circle-fit-threshold", "R", "Relative fit threshold for circular loops"},
        {"--ellipse-fit-threshold", "R", "Relative fit threshold for ellipse reports"},
        {"--near-circle-axis-ratio-tolerance", "R", "Axis-ratio tolerance for near-circles"},
        {"--min-feature-loop-vertices",
         "N",
         "Minimum recovered-cycle/primitive-fit vertices; traced chains are still reported"},
        {"--normal-tensor-threshold", "S", "Feature score threshold for tensor edges"},
        {"--normal-tensor-edge-alignment", "A", "Minimum edge/tangent alignment"},
        {"--normal-tensor-smoothing", "N", "Optional tensor smoothing iterations"},
        {"--normal-tensor-scales", "N", "Number of tensor smoothing scales"},
        {"--normal-tensor-min-persistent-scales", "N", "Required supporting tensor scales"},
        {"--smooth-curvature-threshold", "S", "Scale-normalized quadric feature threshold"},
        {"--smooth-curvature-edge-alignment", "A", "Minimum edge/curve-tangent alignment"},
        {"--smooth-curvature-tangent-consistency", "A", "Cross-scale tangent agreement"},
        {"--smooth-curvature-base-rings", "N", "Smallest quadric-fit neighborhood"},
        {"--smooth-curvature-scales", "N", "Number of quadric-fit scales"},
        {"--smooth-curvature-min-persistent-scales", "N", "Required supporting scales"},
        {"--smooth-curvature-robust-iterations", "N", "Robust fit reweighting passes"},
        {"--smooth-curvature-stable-scale", "", "Select reference scale by stability, not peak score"},
        {"--smooth-curvature-min-scale-stability", "S", "Minimum accepted reference-scale stability"},
        {"--feature-normal-filter", "", "Stabilize noisy face normals before evidence extraction"},
        {"--feature-normal-filter-iterations", "N", "Normal-filter iterations"},
        {"--feature-normal-filter-angle-sigma-deg", "A", "Normal-filter angular bandwidth"},
        {"--feature-normal-filter-preserve-angle-deg", "A", "Preserved crease angle"},
        {"--feature-normal-filter-relaxation", "R", "Normal-filter relaxation in [0,1]"},
        {"--feature-graph-gap-ratio", "R", "Bridge endpoint gaps up to R local edges"},
        {"--feature-graph-max-weak-spur-edges", "N", "Remove weak-evidence spurs up to N edges"},
        {"--feature-graph-min-weak-spur-strength", "S", "Keep weak spurs whose integrated strength reaches S"},
        {"--feature-component-min-confidence", "C", "Component confidence report threshold"},
        {"--feature-graph-consolidation", "", "Recover compatible gaps between weak components"},
        {"--feature-graph-consolidation-gap-ratio", "R", "Recovery gap in local-edge units"},
        {"--feature-graph-consolidation-alignment", "A", "Minimum continuation alignment"},
        {"--surface-patches", "", "Partition faces using active feature edges"},
        {"--surface-patches-strong-only", "", "Exclude tensor/curvature-only barriers from patches"},
        {"--csv", "path", "Write the feature report/compare CSV"},
        {"--smooth-curvature-features", "", "Enable deterministic smooth ridge/valley detection"},
        {"--no-normal-tensor-features", "", "Disable tensor candidates in feature detection"},
        {"--no-feature-graph-cleanup", "", "Disable weak spur/gap graph cleanup"},
    };
    return specs;
}

const std::vector<OptionSpec>& generateOptionSpecs() {
    static const std::vector<OptionSpec> specs = {
        {"--type", "TYPE", "Generator type (see generator list)"},
        {"--n", "N", "Generator resolution parameter"},
        {"--out", "path", "Output STL path"},
    };
    return specs;
}

const OptionSpec kSamplesSpec = {"--samples", "N", "Distance sample count"};
const OptionSpec kMetricsCsvSpec = {"--metrics-csv", "path", "Write one-row CSV metrics"};
const OptionSpec kWeightsSpec = {"--weights", "list", "For sweep, e.g. 0,1e-5,1e-4,1e-3,1e-2"};
const OptionSpec kRatiosSpec = {"--ratios", "list", "For ratio-sweep, e.g. 0.8,0.5,0.25,0.1"};
const OptionSpec kFacesSpec = {"--faces", "list", "For face-sweep, e.g. 1000,900,800"};
const OptionSpec kInputDirSpec = {"--input-dir", "dir", "Workflow input directory"};
const OptionSpec kOutputDirSpec = {"--output-dir", "dir", "Workflow output directory"};
const OptionSpec kRatioSpec = {"--ratio", "R", "Target face ratio for workflow runs"};
const OptionSpec kQuickSpec = {"--quick", "", "Run a reduced/faster workflow"};
const OptionSpec kSpindleInputSpec = {"--spindle-input", "path", "External spindle/shaft STL"};
const OptionSpec kRingInputSpec = {"--ring-input", "path", "External ring/track STL"};
const OptionSpec kPulleyInputSpec = {"--pulley-input", "path", "External pulley STL"};
const OptionSpec kFlangeInputSpec = {"--flange-input", "path", "External finished flange STL"};
const OptionSpec kVerboseSpec = {"--verbose", "", "Verbose diagnostic logging"};

/// Help layout grouped by command family and derived from validation specs.
const std::vector<OptionGroup>& helpGroups() {
    static const std::vector<OptionGroup> groups = [] {
        std::vector<OptionGroup> g;
        g.push_back({"Generate options (generate):", generateOptionSpecs()});
        g.push_back({"Simplify options (simplify, sweep, ratio-sweep, face-sweep):", simplifyOptionSpecs()});
        g.push_back(
            {"Sweep / distance options:", {kSamplesSpec, kMetricsCsvSpec, kWeightsSpec, kRatiosSpec, kFacesSpec}}
        );
        g.push_back(
            {"Feature-analysis options (feature-report, feature-benchmark, feature-compare):", featureOptionSpecs()}
        );
        g.push_back(
            {"Workflow options (demo, validate-features, validate-external):",
             {kInputDirSpec,
              kOutputDirSpec,
              kRatioSpec,
              kSamplesSpec,
              kSpindleInputSpec,
              kRingInputSpec,
              kPulleyInputSpec,
              kFlangeInputSpec,
              kQuickSpec}}
        );
        g.push_back({"Common options (all commands):", {kVerboseSpec}});
        return g;
    }();
    return groups;
}

struct CommandOptionSet {
    std::unordered_set<std::string> valueFlags;
    std::unordered_set<std::string> switchFlags;
};

void addSpec(CommandOptionSet& set, const OptionSpec& spec) {
    if (spec.arg && spec.arg[0] != '\0') {
        set.valueFlags.insert(spec.flag);
    } else {
        set.switchFlags.insert(spec.flag);
    }
}

void addSpecs(CommandOptionSet& set, const std::vector<OptionSpec>& specs) {
    for (const OptionSpec& spec : specs) {
        addSpec(set, spec);
    }
}

const std::map<std::string, CommandOptionSet>& commandOptionSets() {
    static const std::map<std::string, CommandOptionSet> sets = [] {
        std::map<std::string, CommandOptionSet> m;

        auto makeSimplifyFamily = [](const OptionSpec& listSpec) {
            CommandOptionSet set;
            addSpecs(set, simplifyOptionSpecs());
            addSpec(set, kSamplesSpec);
            addSpec(set, listSpec);
            addSpec(set, kVerboseSpec);
            return set;
        };

        CommandOptionSet generate;
        addSpecs(generate, generateOptionSpecs());
        addSpec(generate, kVerboseSpec);
        m["generate"] = generate;

        CommandOptionSet simplify;
        addSpecs(simplify, simplifyOptionSpecs());
        addSpec(simplify, kSamplesSpec);
        addSpec(simplify, kMetricsCsvSpec);
        addSpec(simplify, kVerboseSpec);
        m["simplify"] = simplify;

        m["sweep"] = makeSimplifyFamily(kWeightsSpec);
        m["ratio-sweep"] = makeSimplifyFamily(kRatiosSpec);
        m["face-sweep"] = makeSimplifyFamily(kFacesSpec);

        CommandOptionSet compare;
        addSpec(compare, kSamplesSpec);
        addSpec(compare, kVerboseSpec);
        m["compare"] = compare;

        CommandOptionSet features;
        addSpecs(features, featureOptionSpecs());
        addSpec(features, kVerboseSpec);
        m["feature-report"] = features;
        m["feature-benchmark"] = features;
        m["feature-compare"] = features;

        CommandOptionSet demo;
        addSpec(demo, kInputDirSpec);
        addSpec(demo, kOutputDirSpec);
        addSpec(demo, kFlangeInputSpec);
        addSpec(demo, kSamplesSpec);
        addSpec(demo, kQuickSpec);
        addSpec(demo, kVerboseSpec);
        m["demo"] = demo;

        CommandOptionSet summarize;
        addSpec(summarize, kVerboseSpec);
        m["summarize-metrics"] = summarize;

        CommandOptionSet validateFeatures;
        addSpec(validateFeatures, kInputDirSpec);
        addSpec(validateFeatures, kOutputDirSpec);
        addSpec(validateFeatures, kSpindleInputSpec);
        addSpec(validateFeatures, kRingInputSpec);
        addSpec(validateFeatures, kPulleyInputSpec);
        addSpec(validateFeatures, kFlangeInputSpec);
        addSpec(validateFeatures, kRatioSpec);
        addSpec(validateFeatures, kSamplesSpec);
        addSpec(validateFeatures, kVerboseSpec);
        m["validate-features"] = validateFeatures;

        CommandOptionSet validateExternal;
        addSpec(validateExternal, kInputDirSpec);
        addSpec(validateExternal, kOutputDirSpec);
        addSpec(validateExternal, kRatioSpec);
        addSpec(validateExternal, kSamplesSpec);
        addSpec(validateExternal, kVerboseSpec);
        m["validate-external"] = validateExternal;

        return m;
    }();
    return sets;
}

/// Global union used when a per-command option context is unavailable.
const std::unordered_set<std::string>& valueFlags() {
    static const std::unordered_set<std::string> flags = [] {
        std::unordered_set<std::string> f;
        for (const auto& [command, set] : commandOptionSets()) {
            (void)command;
            f.insert(set.valueFlags.begin(), set.valueFlags.end());
        }
        return f;
    }();
    return flags;
}

const std::unordered_set<std::string>& switchFlags() {
    static const std::unordered_set<std::string> flags = [] {
        std::unordered_set<std::string> f;
        for (const auto& [command, set] : commandOptionSets()) {
            (void)command;
            f.insert(set.switchFlags.begin(), set.switchFlags.end());
        }
        return f;
    }();
    return flags;
}

bool looksLikeMissingValue(const std::string& next) { return next.rfind("--", 0) == 0; }

std::string commandsAcceptingFlag(const std::string& flag) {
    std::string owners;
    for (const auto& [command, set] : commandOptionSets()) {
        if (set.valueFlags.count(flag) || set.switchFlags.count(flag)) {
            if (!owners.empty()) {
                owners += ", ";
            }
            owners += command;
        }
    }
    return owners;
}

} // namespace

bool hasFlag(const Args& args, const std::string& name) {
    for (const std::string& value : args.values) {
        if (value == name)
            return true;
    }
    return false;
}

bool isKnownFlag(const std::string& value) {
    return valueFlags().find(value) != valueFlags().end() || switchFlags().find(value) != switchFlags().end();
}

bool takesValue(const std::string& value) { return valueFlags().find(value) != valueFlags().end(); }

void validateArgsForCommand(const std::string& command, const Args& args) {
    const auto it = commandOptionSets().find(command);
    if (it == commandOptionSets().end()) {
        // Unknown commands are reported by the dispatcher, not here.
        return;
    }
    const CommandOptionSet& set = it->second;

    for (std::size_t i = 0; i < args.values.size(); ++i) {
        const std::string& value = args.values[i];
        if (value.empty() || value[0] != '-') {
            continue;
        }
        if (set.valueFlags.count(value)) {
            if (i + 1 >= args.values.size() || looksLikeMissingValue(args.values[i + 1])) {
                throw std::invalid_argument(value + " requires a value.");
            }
            ++i;
            continue;
        }
        if (set.switchFlags.count(value)) {
            continue;
        }
        if (isKnownFlag(value)) {
            const std::string owners = commandsAcceptingFlag(value);
            throw std::invalid_argument(
                "Option " + value + " is not valid for the '" + command + "' command." +
                (owners.empty() ? "" : " It belongs to: " + owners + ".")
            );
        }
        throw std::invalid_argument("Unknown option: " + value);
    }
}

std::string optionsHelpText() {
    std::ostringstream out;
    constexpr std::size_t kColumn = 40;
    for (const OptionGroup& group : helpGroups()) {
        out << "\n" << group.title << "\n";
        for (const OptionSpec& spec : group.options) {
            std::string left = std::string("  ") + spec.flag;
            if (spec.arg && spec.arg[0] != '\0') {
                left += std::string(" ") + spec.arg;
            }
            if (left.size() + 2 <= kColumn) {
                left.append(kColumn - left.size(), ' ');
                out << left << spec.help << "\n";
            } else {
                out << left << "\n" << std::string(kColumn, ' ') << spec.help << "\n";
            }
        }
    }
    return out.str();
}

std::string getArg(const Args& args, const std::string& name, const std::string& defaultValue) {
    for (std::size_t i = 0; i + 1 < args.values.size(); ++i) {
        if (args.values[i] == name) {
            if (looksLikeMissingValue(args.values[i + 1])) {
                throw std::invalid_argument(name + " requires a value.");
            }
            return args.values[i + 1];
        }
    }
    if (!args.values.empty() && args.values.back() == name) {
        throw std::invalid_argument(name + " requires a value.");
    }
    return defaultValue;
}

int parseIntStrict(const std::string& value, const std::string& name) {
    try {
        std::size_t parsed = 0;
        const int result = std::stoi(value, &parsed);
        if (parsed != value.size()) {
            throw std::invalid_argument("");
        }
        return result;
    } catch (const std::exception&) {
        throw std::invalid_argument(name + " must be an integer.");
    }
}

double parseDoubleStrict(const std::string& value, const std::string& name) {
    try {
        std::size_t parsed = 0;
        const double result = std::stod(value, &parsed);
        if (parsed != value.size() || !std::isfinite(result)) {
            throw std::invalid_argument("");
        }
        return result;
    } catch (const std::exception&) {
        throw std::invalid_argument(name + " must be a finite number.");
    }
}

int getIntArg(const Args& args, const std::string& name, int defaultValue) {
    const std::string value = getArg(args, name);
    return value.empty() ? defaultValue : parseIntStrict(value, name);
}

double getDoubleArg(const Args& args, const std::string& name, double defaultValue) {
    const std::string value = getArg(args, name);
    return value.empty() ? defaultValue : parseDoubleStrict(value, name);
}

std::vector<std::string> positionalArgs(const Args& args) {
    std::vector<std::string> result;
    for (std::size_t i = 0; i < args.values.size(); ++i) {
        const std::string& value = args.values[i];
        if (!value.empty() && value[0] == '-') {
            if (takesValue(value) && i + 1 < args.values.size()) {
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
            weights.push_back(parseDoubleStrict(item, "--weights"));
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
            counts.push_back(parseIntStrict(item, "--faces"));
        }
    }
    return counts;
}

} // namespace manumesh::cli
