/**
 * @file apps/CliArguments.cpp
 * @brief 实现命令行参数解析、校验、帮助文本和严格取值逻辑。
 * @ingroup manumesh_cli
 *
 * @details 选项规格表同时驱动帮助文本、命令归属校验和数值解析，避免多处规则漂移。
 */

#include "CliArguments.h"

#include <cmath>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace manumesh {
namespace cli {
namespace {

/// CLI 选项的唯一规格来源；每条规格同时驱动帮助文本、全局已知选项集合
/// 和各命令的可用选项集合。非空 `arg` 表示值选项，空 `arg` 表示开关选项。
struct OptionSpec {
    const char* flag;
    const char* arg;
    const char* help;
};

struct OptionGroup {
    const char* title;
    std::vector<OptionSpec> options;
};

/// simplify、sweep、ratio-sweep 和 face-sweep 共用的选项。
const std::vector<OptionSpec>& simplifyOptionSpecs() {
    static const std::vector<OptionSpec> specs = {
        {"--method", "standard|line", "标准 QEM 或线二次型 QEM"},
        {"--ratio", "0.25", "目标面数比例"},
        {"--target-faces", "N", "覆盖 --ratio"},
        {"--line-weight", "W", "论文默认值约为 1e-3"},
        {"--weight-mode", "uniform|dihedral|normal-tensor|height|xband", "线二次型权重模式"},
        {"--feature-boost", "W", "特征模式额外增加的线权重"},
        {"--feature-angle-deg", "A", "特征模式的二面角阈值"},
        {"--loop-trace-angle-deg", "A", "环追踪的二面角阈值；负值时复用特征角度"},
        {"--adaptive-base-line-weight", "W", "自适应 Q 缩放前加入的基础线二次型权重"},
        {"--boundary-weight", "W", "可选的边界平面二次型权重"},
        {"--feature-curve-weight", "W", "环的切线二次型权重"},
        {"--max-feature-curve-deviation-ratio", "R", "拒绝原始放置偏移超过 R*bbox_diag 的多边形特征折叠"},
        {"--circle-fit-threshold", "R", "圆环的相对拟合阈值"},
        {"--ellipse-fit-threshold", "R", "椭圆报告的相对拟合阈值"},
        {"--near-circle-axis-ratio-tolerance", "R", "近圆的轴比容差"},
        {"--min-feature-loop-vertices", "N", "环顶点少于 N 时停止折叠"},
        {"--min-circular-feature-loop-vertices", "N", "圆环顶点少于 N 时停止折叠"},
        {"--feature-protection-mode",
         "none|circular-only|primitive-curves|all-feature-edges",
         "检测特征的强制保护策略；默认值为 primitive-curves"},
        {"--normal-tensor-threshold", "S", "张量边的特征得分阈值"},
        {"--normal-tensor-edge-alignment", "A", "边与切线的最小对齐度"},
        {"--normal-tensor-smoothing", "N", "可选的张量平滑迭代次数"},
        {"--normal-tensor-scales", "N", "张量平滑尺度数量"},
        {"--normal-tensor-min-persistent-scales", "N", "所需的支持张量尺度数量"},
        {"--smooth-curvature-threshold", "S", "尺度归一化二次型特征阈值"},
        {"--smooth-curvature-edge-alignment", "A", "边与曲线切线的最小对齐度"},
        {"--smooth-curvature-tangent-consistency", "A", "跨尺度切线一致性"},
        {"--smooth-curvature-base-rings", "N", "二次型拟合的最小邻域环数"},
        {"--smooth-curvature-scales", "N", "二次型拟合尺度数量"},
        {"--smooth-curvature-min-persistent-scales", "N", "所需的支持平滑尺度数量"},
        {"--smooth-curvature-robust-iterations", "N", "稳健拟合重新加权次数"},
        {"--feature-graph-gap-ratio", "R", "桥接不超过 R 个局部边长的端点间隙"},
        {"--feature-graph-max-weak-spur-edges", "N", "移除不超过 N 条边的弱证据毛刺"},
        {"--feature-graph-min-weak-spur-strength", "S", "保留积分强度达到 S 的弱毛刺"},
        {"--feature-component-min-confidence", "C", "特征分量置信度报告阈值"},
        {"--min-triangle-quality", "Q", "拒绝质量低于 Q（范围 [0,1]）的折叠"},
        {"--max-normal-deviation-deg", "A", "拒绝局部面法线变化超过 A 的折叠"},
        {"--max-local-error", "D", "拒绝局部折叠偏移超过 D 的折叠"},
        {"--max-local-error-ratio", "R", "拒绝局部偏移超过 R*bbox 对角线的折叠"},
        {"--quality-refinement-iterations", "N", "执行 N 次固定拓扑质量优化"},
        {"--adaptive-scale", "", "按局部曲率调整队列优先级（不改变放置位置）"},
        {"--preserve-boundary", "", "保留开放边界拓扑"},
        {"--preserve-feature-curves", "", "保护检测到的折痕/边界环"},
        {"--prevent-local-intersections", "", "拒绝局部三角形相交"},
        {"--industrial-safe", "", "启用保守的边界/质量保护"},
        {"--no-normal-tensor-features", "", "在特征检测中禁用张量候选"},
        {"--smooth-curvature-features", "", "启用确定性的平滑脊线/谷线保护"},
        {"--smooth-curvature-stable-scale", "", "按跨尺度稳定性选择平滑特征尺度"},
        {"--smooth-curvature-min-scale-stability", "S", "拒绝尺度稳定性低于 S 的平滑证据"},
        {"--feature-normal-filter", "", "在特征检测前稳定含噪面法线"},
        {"--feature-normal-filter-iterations", "N", "法线滤波迭代次数"},
        {"--feature-normal-filter-angle-sigma-deg", "A", "法线滤波角带宽"},
        {"--feature-normal-filter-preserve-angle-deg", "A", "不跨越大于 A 的锐边进行平滑"},
        {"--feature-normal-filter-relaxation", "R", "法线滤波松弛系数（范围 [0,1]）"},
        {"--feature-graph-consolidation", "", "恢复弱分量之间的兼容间隙"},
        {"--feature-graph-consolidation-gap-ratio", "R", "以局部边长为单位的分量恢复间隙"},
        {"--feature-graph-consolidation-alignment", "A", "恢复所需的最小延续对齐度"},
        {"--no-feature-graph-cleanup", "", "禁用弱毛刺/间隙图清理"},
    };
    return specs;
}

/// feature-report、feature-benchmark 和 feature-compare 共用的选项。
const std::vector<OptionSpec>& featureOptionSpecs() {
    static const std::vector<OptionSpec> specs = {
        {"--feature-angle-deg", "A", "特征边的二面角阈值"},
        {"--loop-trace-angle-deg", "A", "环追踪二面角阈值；负值时复用特征角度"},
        {"--circle-fit-threshold", "R", "圆环的相对拟合阈值"},
        {"--ellipse-fit-threshold", "R", "椭圆报告的相对拟合阈值"},
        {"--near-circle-axis-ratio-tolerance", "R", "近圆的轴比容差"},
        {"--min-feature-loop-vertices", "N", "恢复循环/几何基元拟合所需的最小顶点数；仍会报告追踪到的链"},
        {"--normal-tensor-threshold", "S", "张量边的特征得分阈值"},
        {"--normal-tensor-edge-alignment", "A", "边与切线的最小对齐度"},
        {"--normal-tensor-smoothing", "N", "可选的张量平滑迭代次数"},
        {"--normal-tensor-scales", "N", "张量平滑尺度数量"},
        {"--normal-tensor-min-persistent-scales", "N", "所需的支持张量尺度数量"},
        {"--smooth-curvature-threshold", "S", "尺度归一化二次型特征阈值"},
        {"--smooth-curvature-edge-alignment", "A", "边与曲线切线的最小对齐度"},
        {"--smooth-curvature-tangent-consistency", "A", "跨尺度切线一致性"},
        {"--smooth-curvature-base-rings", "N", "二次型拟合的最小邻域环数"},
        {"--smooth-curvature-scales", "N", "二次型拟合尺度数量"},
        {"--smooth-curvature-min-persistent-scales", "N", "所需的支持尺度数量"},
        {"--smooth-curvature-robust-iterations", "N", "稳健拟合重新加权次数"},
        {"--smooth-curvature-stable-scale", "", "按稳定性而非峰值分数选择参考尺度"},
        {"--smooth-curvature-min-scale-stability", "S", "可接受的最小参考尺度稳定性"},
        {"--feature-normal-filter", "", "在证据提取前稳定含噪面法线"},
        {"--feature-normal-filter-iterations", "N", "法线滤波迭代次数"},
        {"--feature-normal-filter-angle-sigma-deg", "A", "法线滤波角带宽"},
        {"--feature-normal-filter-preserve-angle-deg", "A", "保留的折痕角度"},
        {"--feature-normal-filter-relaxation", "R", "法线滤波松弛系数（范围 [0,1]）"},
        {"--feature-graph-gap-ratio", "R", "桥接不超过 R 个局部边长的端点间隙"},
        {"--feature-graph-max-weak-spur-edges", "N", "移除不超过 N 条边的弱证据毛刺"},
        {"--feature-graph-min-weak-spur-strength", "S", "保留积分强度达到 S 的弱毛刺"},
        {"--feature-component-min-confidence", "C", "特征分量置信度报告阈值"},
        {"--feature-graph-consolidation", "", "恢复弱分量之间的兼容间隙"},
        {"--feature-graph-consolidation-gap-ratio", "R", "以局部边长为单位的恢复间隙"},
        {"--feature-graph-consolidation-alignment", "A", "最小延续对齐度"},
        {"--surface-patches", "", "使用启用的特征边划分面片"},
        {"--surface-patches-strong-only", "", "从面片中排除仅由张量/曲率形成的屏障"},
        {"--csv", "path", "写入特征报告/比较 CSV"},
        {"--smooth-curvature-features", "", "启用确定性的平滑脊线/谷线检测"},
        {"--no-normal-tensor-features", "", "在特征检测中禁用张量候选"},
        {"--no-feature-graph-cleanup", "", "禁用弱毛刺/间隙图清理"},
    };
    return specs;
}

const std::vector<OptionSpec>& generateOptionSpecs() {
    static const std::vector<OptionSpec> specs = {
        {"--type", "TYPE", "生成器类型（参见生成器列表）"},
        {"--n", "N", "生成器分辨率参数"},
        {"--out", "path", "输出 STL 路径"},
    };
    return specs;
}

const OptionSpec kSamplesSpec = {"--samples", "N", "距离采样数量"};
const OptionSpec kMetricsCsvSpec = {"--metrics-csv", "path", "写入单行 CSV 指标"};
const OptionSpec kWeightsSpec = {"--weights", "list", "用于 sweep，例如 0,1e-5,1e-4,1e-3,1e-2"};
const OptionSpec kRatiosSpec = {"--ratios", "list", "用于 ratio-sweep，例如 0.8,0.5,0.25,0.1"};
const OptionSpec kFacesSpec = {"--faces", "list", "用于 face-sweep，例如 1000,900,800"};
const OptionSpec kInputDirSpec = {"--input-dir", "dir", "工作流输入目录"};
const OptionSpec kOutputDirSpec = {"--output-dir", "dir", "工作流输出目录"};
const OptionSpec kRatioSpec = {"--ratio", "R", "工作流运行的目标面数比例"};
const OptionSpec kQuickSpec = {"--quick", "", "运行精简的快速工作流"};
const OptionSpec kSpindleInputSpec = {"--spindle-input", "path", "外部主轴/轴 STL"};
const OptionSpec kRingInputSpec = {"--ring-input", "path", "外部环/轨道 STL"};
const OptionSpec kPulleyInputSpec = {"--pulley-input", "path", "外部滑轮 STL"};
const OptionSpec kFlangeInputSpec = {"--flange-input", "path", "外部成品法兰 STL"};
const OptionSpec kVerboseSpec = {"--verbose", "", "输出详细诊断日志"};

/// 按命令族分组的帮助布局，来源于同一套校验规格。
const std::vector<OptionGroup>& helpGroups() {
    static const std::vector<OptionGroup> groups = [] {
        std::vector<OptionGroup> g;
        g.push_back({"生成选项（generate）：", generateOptionSpecs()});
        g.push_back({"简化选项（simplify、sweep、ratio-sweep、face-sweep）：", simplifyOptionSpecs()});
        g.push_back({"扫描/距离选项：", {kSamplesSpec, kMetricsCsvSpec, kWeightsSpec, kRatiosSpec, kFacesSpec}});
        g.push_back({"特征分析选项（feature-report、feature-benchmark、feature-compare）：", featureOptionSpecs()});
        g.push_back(
            {"工作流选项（demo、validate-features、validate-external）：",
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
        g.push_back({"通用选项（所有命令）：", {kVerboseSpec}});
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

/// 当无法取得命令上下文时使用的全局值选项集合。
const std::unordered_set<std::string>& valueFlags() {
    static const std::unordered_set<std::string> flags = [] {
        std::unordered_set<std::string> f;
        for (const auto& pairEntry : commandOptionSets()) {
            const auto& command = pairEntry.first;
            const auto& set = pairEntry.second;
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
        for (const auto& pairEntry : commandOptionSets()) {
            const auto& command = pairEntry.first;
            const auto& set = pairEntry.second;
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
    for (const auto& pairEntry : commandOptionSets()) {
        const auto& command = pairEntry.first;
        const auto& set = pairEntry.second;
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
        // 未知命令由分发器报告，这里只校验已知命令的选项。
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

} // namespace cli
} // namespace manumesh
