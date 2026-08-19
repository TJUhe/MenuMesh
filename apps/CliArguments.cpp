/**
 * @file apps/CliArguments.cpp
 * @brief 实现命令行参数解析、校验、帮助文本和严格取值逻辑。
 * @ingroup manumesh_cli
 *
 * @details 选项规格表同时驱动帮助文本、命令归属校验和数值解析，避免多处规则漂移。
 */

#include "CliArguments.h"

#include <cctype>
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
        {"--profile", "default|cad|scan|noisy-scan|smooth|smooth-surface", "场景档案；显式参数会覆盖档案值；括号内为别名"},
        {"--print-resolved-config", "", "执行前输出有效配置摘要"},
        {"--method", "standard|qem|line", "标准 QEM（qem 为别名）或线二次型 QEM"},
        {"--ratio", "0.25", "目标面数比例；ratio-sweep/face-sweep 请改用各自列表"},
        {"--target-faces", "N", "绝对目标面数；与 --ratio 同时给出时优先；不用于 ratio-sweep/face-sweep"},
        {"--line-weight", "W", "论文默认值约为 1e-3；0 会退化为标准 QEM；sweep 请改用 --weights"},
        {"--weight-mode", "uniform|dihedral|normal-tensor|smooth-curvature|height|xband", "线二次型权重模式"},
        {"--feature-boost", "W", "特征模式额外增加的线权重"},
        {"--feature-angle-deg", "A", "特征模式的二面角阈值"},
        {"--loop-trace-angle-deg", "A", "环追踪的二面角阈值；负值时复用特征角度"},
        {"--adaptive-base-line-weight", "W", "自适应 Q 缩放前加入的基础线二次型权重；sweep 请改用 --weights"},
        {"--boundary-weight", "W", "可选的边界平面二次型权重"},
        {"--feature-curve-weight", "W", "环的切线二次型权重"},
        {"--max-feature-curve-deviation-ratio", "R", "拒绝原始放置偏移超过 R*bbox_diag 的多边形特征折叠"},
        {"--circle-fit-threshold", "R", "圆环的相对拟合阈值"},
        {"--ellipse-fit-threshold", "R", "椭圆报告的相对拟合阈值"},
        {"--near-circle-axis-ratio-tolerance", "R", "近圆的轴比容差"},
        {"--min-feature-loop-vertices", "N", "环顶点少于 N 时停止折叠；N<5 的保护阶段使用安全下限 5"},
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
        {"--feature-component-min-confidence", "C", "仅影响高置信度分量的报告计数，不改变检测或保护"},
        {"--min-triangle-quality", "Q", "拒绝质量低于 Q（范围 [0,1]）的折叠"},
        {"--max-normal-deviation-deg", "A", "拒绝局部面法线变化超过 A 的折叠"},
        {"--max-local-error", "D", "拒绝局部折叠偏移超过 D 的折叠"},
        {"--max-local-error-ratio", "R", "拒绝局部偏移超过 R*bbox 对角线的折叠"},
        {"--quality-refinement-iterations", "N", "执行 N 次固定拓扑质量优化"},
        {"--adaptive-scale", "", "按局部面积尺度构造基础 line Q，并将特征增益仅用于队列优先级；放置使用基础 Q"},
        {"--preserve-boundary", "", "保留开放边界拓扑"},
        {"--preserve-feature-curves", "", "保护检测到的折痕/边界环"},
        {"--no-preserve-feature-curves", "", "显式关闭 profile 或自动启用的特征曲线保护"},
        {"--prevent-local-intersections", "", "拒绝局部三角形相交"},
        {"--industrial-safe", "", "启用保守的边界/质量保护"},
        {"--normal-tensor-features", "", "显式启用张量弱特征证据"},
        {"--no-normal-tensor-features", "", "在特征检测中禁用张量候选"},
        {"--smooth-curvature-features", "", "启用确定性的平滑脊线/谷线保护"},
        {"--no-smooth-curvature-features", "", "显式关闭平滑脊线/谷线证据"},
        {"--smooth-curvature-stable-scale", "", "按跨尺度稳定性选择平滑特征尺度"},
        {"--smooth-curvature-min-scale-stability", "S", "仅在 --smooth-curvature-stable-scale 下拒绝稳定性低于 S 的证据"},
        {"--feature-normal-filter", "", "在特征检测前稳定含噪面法线"},
        {"--no-feature-normal-filter", "", "显式关闭 profile 启用的特征法线滤波"},
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
        {"--profile", "default|cad|scan|noisy-scan|smooth|smooth-surface", "场景档案；显式参数会覆盖档案值；括号内为别名"},
        {"--print-resolved-config", "", "执行前输出有效配置摘要"},
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
        {"--smooth-curvature-min-scale-stability", "S", "仅在 --smooth-curvature-stable-scale 下使用的最小参考尺度稳定性"},
        {"--feature-normal-filter", "", "在证据提取前稳定含噪面法线"},
        {"--no-feature-normal-filter", "", "显式关闭 profile 启用的特征法线滤波"},
        {"--feature-normal-filter-iterations", "N", "法线滤波迭代次数"},
        {"--feature-normal-filter-angle-sigma-deg", "A", "法线滤波角带宽"},
        {"--feature-normal-filter-preserve-angle-deg", "A", "保留的折痕角度"},
        {"--feature-normal-filter-relaxation", "R", "法线滤波松弛系数（范围 [0,1]）"},
        {"--feature-graph-gap-ratio", "R", "桥接不超过 R 个局部边长的端点间隙"},
        {"--feature-graph-max-weak-spur-edges", "N", "移除不超过 N 条边的弱证据毛刺"},
        {"--feature-graph-min-weak-spur-strength", "S", "保留积分强度达到 S 的弱毛刺"},
        {"--feature-component-min-confidence", "C", "仅影响高置信度分量的报告计数，不改变检测结果"},
        {"--feature-graph-consolidation", "", "恢复弱分量之间的兼容间隙"},
        {"--feature-graph-consolidation-gap-ratio", "R", "以局部边长为单位的恢复间隙"},
        {"--feature-graph-consolidation-alignment", "A", "最小延续对齐度"},
        {"--surface-patches", "", "使用启用的特征边划分面片"},
        {"--surface-patches-strong-only", "", "从面片中排除仅由张量/曲率形成的屏障"},
        {"--csv", "path", "写入特征报告/比较 CSV"},
        {"--smooth-curvature-features", "", "启用确定性的平滑脊线/谷线检测"},
        {"--no-smooth-curvature-features", "", "显式关闭平滑脊线/谷线证据"},
        {"--normal-tensor-features", "", "显式启用张量弱特征证据"},
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

const OptionSpec kPartitionTrianglesSpec = {
    "--partition-triangles", "N", "large-import 每个分区的最大三角形数量（正整数）"
};
const OptionSpec kMemoryMiBSpec = {
    "--memory-mib", "N", "large-mesh 操作声明的常驻内存上限（MiB，正整数）"
};
const OptionSpec kIoBufferMiBSpec = {
    "--io-buffer-mib", "N", "顺序 I/O 缓冲区大小（MiB，且不超过 --memory-mib）"
};

const OptionSpec kSamplesSpec = {"--samples", "N", "距离采样数量（正整数）"};
const OptionSpec kMetricsCsvSpec = {"--metrics-csv", "path", "写入单行 CSV 指标"};
const OptionSpec kPerformanceCsvSpec = {
    "--performance-csv", "path", "写入一次调用的阶段墙钟性能 CSV，并在 stderr 输出摘要"
};
const OptionSpec kWeightsSpec = {"--weights", "list", "用于 line sweep，例如 0,1e-5,1e-4,1e-3,1e-2；标准 QEM 只接受 0"};
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
const OptionSpec kThreadsSpec = {
    "--threads", "N", "算法最大并发度；0 由 oneTBB 自动选择，省略时保持串行"
};

/// 按命令族分组的帮助布局，来源于同一套校验规格。
const std::vector<OptionGroup>& helpGroups() {
    static const std::vector<OptionGroup> groups = [] {
        std::vector<OptionGroup> g;
        g.push_back({"生成选项（generate）：", generateOptionSpecs()});
        g.push_back(
            {"超大网格选项（large-import、large-validate）：",
             {kPartitionTrianglesSpec, kMemoryMiBSpec, kIoBufferMiBSpec}}
        );
        g.push_back({"简化选项（simplify、sweep、ratio-sweep、face-sweep）：", simplifyOptionSpecs()});
        g.push_back({"扫描/距离选项：", {kSamplesSpec, kMetricsCsvSpec, kWeightsSpec, kRatiosSpec, kFacesSpec}});
        g.push_back({"特征分析选项（feature-report、feature-benchmark、feature-compare）：", featureOptionSpecs()});
        g.push_back({"并行执行选项（简化、扫描和特征分析）：", {kThreadsSpec}});
        g.push_back({"性能计时选项（simplify、特征分析、large-import、large-validate）：", {kPerformanceCsvSpec}});
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
            addSpec(set, kThreadsSpec);
            addSpec(set, kVerboseSpec);
            return set;
        };

        CommandOptionSet generate;
        addSpecs(generate, generateOptionSpecs());
        addSpec(generate, kVerboseSpec);
        m["generate"] = generate;

        CommandOptionSet largeImport;
        addSpec(largeImport, kPartitionTrianglesSpec);
        addSpec(largeImport, kMemoryMiBSpec);
        addSpec(largeImport, kIoBufferMiBSpec);
        addSpec(largeImport, kPerformanceCsvSpec);
        addSpec(largeImport, kVerboseSpec);
        m["large-import"] = largeImport;

        CommandOptionSet largeValidate;
        addSpec(largeValidate, kMemoryMiBSpec);
        addSpec(largeValidate, kIoBufferMiBSpec);
        addSpec(largeValidate, kPerformanceCsvSpec);
        addSpec(largeValidate, kVerboseSpec);
        m["large-validate"] = largeValidate;

        CommandOptionSet simplify;
        addSpecs(simplify, simplifyOptionSpecs());
        addSpec(simplify, kSamplesSpec);
        addSpec(simplify, kMetricsCsvSpec);
        addSpec(simplify, kPerformanceCsvSpec);
        addSpec(simplify, kThreadsSpec);
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
        addSpec(features, kPerformanceCsvSpec);
        addSpec(features, kThreadsSpec);
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
    std::unordered_set<std::string> seenValueFlags;

    for (std::size_t i = 0; i < args.values.size(); ++i) {
        const std::string& value = args.values[i];
        if (value.empty() || value[0] != '-') {
            continue;
        }
        if (set.valueFlags.count(value)) {
            if (i + 1 >= args.values.size() || args.values[i + 1].empty() ||
                looksLikeMissingValue(args.values[i + 1])) {
                throw std::invalid_argument(value + " requires a value.");
            }
            if (!seenValueFlags.insert(value).second) {
                throw std::invalid_argument(value + " may be specified only once.");
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
    std::string result = defaultValue;
    bool found = false;
    for (std::size_t i = 0; i < args.values.size(); ++i) {
        if (args.values[i] == name) {
            if (i + 1 >= args.values.size() || args.values[i + 1].empty() ||
                looksLikeMissingValue(args.values[i + 1])) {
                throw std::invalid_argument(name + " requires a value.");
            }
            if (found) {
                throw std::invalid_argument(name + " may be specified only once.");
            }
            result = args.values[i + 1];
            found = true;
            ++i;
        }
    }
    return result;
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
    const int result = value.empty() ? defaultValue : parseIntStrict(value, name);
    // Distance metrics are meaningless without at least one sample. Keep this
    // contract at the shared CLI boundary so all commands and workflows agree.
    if (name == "--samples" && result <= 0) {
        throw std::invalid_argument("--samples must be a positive integer.");
    }
    return result;
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

namespace {

std::string trimListToken(const std::string& value) {
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    return value.substr(first, last - first);
}

} // namespace

std::vector<double> parseWeights(const std::string& text, const std::string& name) {
    if (text.empty()) {
        throw std::invalid_argument(name + " requires a non-empty comma-separated list.");
    }
    if (text.back() == ',') {
        throw std::invalid_argument(name + " contains an empty value.");
    }
    std::vector<double> weights;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trimListToken(item);
        if (item.empty()) {
            throw std::invalid_argument(name + " contains an empty value.");
        }
        weights.push_back(parseDoubleStrict(item, name));
    }
    if (weights.empty()) {
        throw std::invalid_argument(name + " requires at least one value.");
    }
    return weights;
}

std::vector<int> parseFaceCounts(const std::string& text, const std::string& name) {
    if (text.empty()) {
        throw std::invalid_argument(name + " requires a non-empty comma-separated list.");
    }
    if (text.back() == ',') {
        throw std::invalid_argument(name + " contains an empty value.");
    }
    std::vector<int> counts;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trimListToken(item);
        if (item.empty()) {
            throw std::invalid_argument(name + " contains an empty value.");
        }
        const int count = parseIntStrict(item, name);
        if (count <= 0) {
            throw std::invalid_argument(name + " values must be positive; got " + item + ".");
        }
        counts.push_back(count);
    }
    if (counts.empty()) {
        throw std::invalid_argument(name + " requires at least one value.");
    }
    return counts;
}

} // namespace cli
} // namespace manumesh
