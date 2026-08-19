/**
 * @file include/algorithms/feature_detection/FeatureDetector.h
 * @brief 声明特征曲线检测器及其无状态便捷入口。
 * @ingroup manumesh_feature_detection
 *
 * @details 检测器拥有配置，分析时按固定顺序收集证据、清理图、恢复曲线并汇总组件。
 */

#pragma once

#include "Export.h"
#include "algorithms/feature_detection/FeatureTypes.h"
#include "core/ExecutionOptions.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace manumesh {
namespace feature {

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/// 有状态的特征检测门面。
///
/// 此模块与 QEM 简化并列为一级算法，只依赖核心网格类型。因此简化、校验、修复、重网格或未来
/// 面向 CAD 的算法都可以消费同一个 FeatureAnalysis，而不会产生反向依赖。
class MANUMESH_API FeatureDetector {
public:
    /// @param[in] options 复制到检测器中的已校验选项。
    /// @throws std::invalid_argument 当选项范围不一致时抛出。
    explicit FeatureDetector(FeatureOptions options = {});
    ~FeatureDetector();

    FeatureDetector(const FeatureDetector& other);
    FeatureDetector& operator=(const FeatureDetector& other);
    FeatureDetector(FeatureDetector&& other) noexcept;
    FeatureDetector& operator=(FeatureDetector&& other) noexcept;

    /// 返回后续分析使用的选项。
    const FeatureOptions& options() const;
    /// 替换后续分析使用的选项。
    /// @param[in] options 新的已校验选项集。
    /// @throws std::invalid_argument 当选项范围不一致时抛出。
    void setOptions(FeatureOptions options);

    /// 检测硬证据、可选的张量/曲率证据以及拟合曲线。
    /// @param[in] mesh 三角表面网格；允许零面积面，并在结果中报告。
    /// @return 完整的特征图、曲线、组件、诊断信息和可选分区。
    /// @throws std::invalid_argument 当索引无效、坐标非有限或面顶点重复时抛出。
    FeatureAnalysis analyze(const Mesh& mesh) const;
    /// 使用显式执行约束运行检测；只有独立的只读范围会并行执行。
    FeatureAnalysis analyze(const Mesh& mesh, const ExecutionOptions& executionOptions) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

/// 在保留网格拓扑和顶点位置的同时，为含噪输入的特征检测稳定面法向。
/// @param[in] mesh 输入三角网格。
/// @param[in] options 过滤迭代次数、角度带宽、保留角度和松弛参数。
/// @return 每个面的一个过滤后法向以及定量诊断信息。
/// @algorithm 迭代计算角度边指标，冻结强不连续处，并按面积加权松弛其余面法向。
MANUMESH_API FeatureNormalFilterResult
filterFeatureNormals(const Mesh& mesh, const FeatureNormalFilterOptions& options = {});

/// 使用显式执行约束稳定面法向。
MANUMESH_API FeatureNormalFilterResult filterFeatureNormals(
    const Mesh& mesh, const FeatureNormalFilterOptions& options, const ExecutionOptions& executionOptions
);

/// 根据多尺度面法向投票计算局部法向张量分数。
/// @param[in] mesh 输入三角表面。
/// @param[in] options 张量平滑、尺度计划和可选法向预处理。
/// @return 每个顶点一个张量分解和持久性记录。
/// @algorithm 累积按面积/空间加权的法向外积，对对称张量进行特征分解，
/// 再根据有序特征值差异推导曲面、折痕和角点显著性。
/// @failuremodes 孤立顶点以及只包含不可用面的邻域返回零证据，而不是伪造方向。
MANUMESH_API std::vector<NormalTensorVertex>
computeNormalTensorFeatures(const Mesh& mesh, const NormalTensorOptions& options = {});

/// 使用显式执行约束计算法向张量证据。
MANUMESH_API std::vector<NormalTensorVertex> computeNormalTensorFeatures(
    const Mesh& mesh, const NormalTensorOptions& options, const ExecutionOptions& executionOptions
);

/// 仅当尺度显著性达到给定阈值时，才将该尺度计为持久证据。
/// @param[in] mesh 输入三角表面。
/// @param[in] options 张量平滑、尺度计划和可选法向预处理。
/// @param[in] persistenceThreshold 每个尺度的最小归一化支持度。
/// @return 每个输入顶点一个张量记录。
/// @throws std::invalid_argument 当尺度、平滑、法向过滤或持久性阈值参数无效时抛出。
MANUMESH_API std::vector<NormalTensorVertex>
computeNormalTensorFeatures(const Mesh& mesh, const NormalTensorOptions& options, double persistenceThreshold);

/// 使用显式执行约束和持久性阈值计算法向张量证据。
MANUMESH_API std::vector<NormalTensorVertex> computeNormalTensorFeatures(
    const Mesh& mesh,
    const NormalTensorOptions& options,
    double persistenceThreshold,
    const ExecutionOptions& executionOptions
);

/// 根据稳健局部 quadric 拟合、主曲率、方向极值和尺度持久性计算确定性的平滑脊/谷证据。
/// 不使用学习模型或训练数据。
/// @param[in] mesh 输入三角表面。
/// @param[in] options 邻域半径、稳健迭代次数和稳定性策略。
/// @return 每个顶点一个带符号曲率证据记录。
/// @algorithm 收集确定性的 k-ring 邻域，按局部采样尺度归一化，拟合稳健 Monge quadric，
/// 恢复主曲率和方向，测试双侧方向极值，然后要求跨尺度的符号和切向持久性。
/// @complexity O(V * S * N)，其中 S 为尺度数量，N 为每次局部最小二乘拟合使用的有界邻域大小。
/// @failuremodes 秩亏或单侧邻域、不稳定主方向框架以及不一致极值均报告为零证据。
MANUMESH_API std::vector<SmoothCurvatureVertex>
computeSmoothCurvatureFeatures(const Mesh& mesh, const SmoothCurvatureOptions& options = {});

/// 使用显式执行约束计算稳健多尺度平滑曲率证据。
MANUMESH_API std::vector<SmoothCurvatureVertex> computeSmoothCurvatureFeatures(
    const Mesh& mesh, const SmoothCurvatureOptions& options, const ExecutionOptions& executionOptions
);

/// 仅当尺度归一化分数达到给定阈值时，才将该尺度计为持久证据。
/// @param[in] mesh 输入三角表面。
/// @param[in] options 邻域和稳健拟合计划。
/// @param[in] persistenceThreshold 支持尺度上的最小归一化分数。
/// @return 每个顶点一个带符号曲率证据记录。
MANUMESH_API std::vector<SmoothCurvatureVertex>
computeSmoothCurvatureFeatures(const Mesh& mesh, const SmoothCurvatureOptions& options, double persistenceThreshold);

/// 使用显式执行约束和持久性阈值计算平滑曲率证据。
MANUMESH_API std::vector<SmoothCurvatureVertex> computeSmoothCurvatureFeatures(
    const Mesh& mesh,
    const SmoothCurvatureOptions& options,
    double persistenceThreshold,
    const ExecutionOptions& executionOptions
);

/// 校验每个特征检测选项及跨字段范围。
/// @param[in] options 待校验的选项。
/// @throws std::invalid_argument 当值非有限、范围无效或持久尺度数量大于尺度总数时抛出。
MANUMESH_API void validateFeatureOptions(const FeatureOptions& options);

/// 检测边界、非流形、二面角、张量、可选平滑曲率以及拟合的几何基元曲线。
///
/// 实现首先追踪图支持的环，然后为稀疏圆环应用有界的 CAD 修复回退。它不是针对含噪扫描的一般曲率脊提取器；
/// 对此类输入应启用张量特征并调节尺度/阈值参数。
/// @param[in] mesh 三角表面网格。
/// @param[in] options 检测、恢复、清理和分区策略。
/// @return 完整的确定性特征分析。
/// @algorithm 收集强弱证据，构建显式追踪图，清理并整合兼容组件，追踪链和环，
/// 恢复有界回退环，拟合解析几何基元，计算组件置信度，并可选地将面划分为分区。
/// @invariants 证据计数不包含合成桥接边；图边端点始终是有效网格顶点；每个简化环拥有稳定 ID。
MANUMESH_API FeatureAnalysis detectFeatureCurves(const Mesh& mesh, const FeatureOptions& options);

/// 使用显式执行约束检测特征；图构建、环恢复和分区阶段保持确定性顺序。
MANUMESH_API FeatureAnalysis detectFeatureCurves(
    const Mesh& mesh, const FeatureOptions& options, const ExecutionOptions& executionOptions
);

/// 计算并返回存储在 `FeatureAnalysis` 中的确定性来源身份。
/// @param[in] mesh 有效的索引几何；忽略 UV 数据。
/// @return 计数以及稳定的拓扑/几何指纹。
MANUMESH_API FeatureAnalysisSource featureAnalysisSource(const Mesh& mesh);

/// 验证分析结果是否属于 `mesh`，以及所有公开索引记录是否内部一致。
/// @param[in] mesh 预期生成 `analysis` 的网格。
/// @param[in] analysis 复用前需要验证的特征结果。
/// @throws std::invalid_argument 当来源身份或存储索引不一致时抛出。
MANUMESH_API void validateFeatureAnalysis(const Mesh& mesh, const FeatureAnalysis& analysis);

/// 构建由活动特征图边分隔的面分区，并写入 analysis.facePatchIds / patches / patchAdjacencies。
/// @param[in] mesh 生成 `analysis` 的网格。
/// @param[in,out] analysis 现有特征图以及作为目标的分区数组。
/// @param[in] options 分区启用状态和弱边界边策略。
MANUMESH_API void
segmentFeaturePatches(const Mesh& mesh, FeatureAnalysis& analysis, const SurfacePatchOptions& options = {});

/// 将一个检测到的环与给定圆进行度量比较。
/// @param[in] mesh 包含环顶点的网格。
/// @param[in] loop 待测量的环。
/// @param[in] center 模型坐标中的圆心。
/// @param[in] normal 圆平面法向；函数内部会归一化。
/// @param[in] radius 正的圆半径。
/// @return 方向性径向/平面偏差和样本计数。
MANUMESH_API DirectionalCurveError measureLoopAgainstCircle(
    const Mesh& mesh, const FeatureLoop& loop, const Vec3& center, const Vec3& normal, double radius
);

/// 特征环报告的 CSV 表头。
MANUMESH_API std::string featureReportHeaderCsv();
/// 一个特征环的 CSV 行。
MANUMESH_API std::string featureLoopRowCsv(const FeatureLoop& loop);
/// 拟合特征几何基元的稳定字符串名称。
MANUMESH_API std::string toString(FeaturePrimitiveType primitive);
/// 将检测到的图边与顶点索引形式的真实标签进行比较。
/// @param[in] analysis 待评分的检测结果。
/// @param[in] groundTruthEdges 带标签的无向边。
/// @param[in] groundTruthJunctionVertices 可选的带标签连接点顶点。
/// @return Precision/recall/F1 以及连接点诊断信息。
MANUMESH_API FeatureEdgeBenchmark benchmarkFeatureEdges(
    const FeatureAnalysis& analysis,
    const std::vector<std::pair<int, int>>& groundTruthEdges,
    const std::vector<int>& groundTruthJunctionVertices = {}
);

/// 针对分支延续和面分区标签的扩展基准测试。
/// @param[in] mesh 生成 `analysis` 的网格。
/// @param[in] analysis 待评分的检测结果。
/// @param[in] labels 边、连接点、延续关系和分区的真实标签。
/// @return 每个给定标签族的聚合基准指标。
MANUMESH_API FeatureEdgeBenchmark
benchmarkFeatureAnalysis(const Mesh& mesh, const FeatureAnalysis& analysis, const FeatureBenchmarkLabels& labels);

} // namespace feature
} // namespace manumesh
