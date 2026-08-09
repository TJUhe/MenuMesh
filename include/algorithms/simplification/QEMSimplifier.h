/**
 * @file include/algorithms/simplification/QEMSimplifier.h
 * @brief 声明 ManuMesh 简化模块的 QEMSimplifier 设施。
 * @ingroup manumesh_simplification
 *
 * @details 此文件属于面向特征的边坍缩管线。二次误差代价负责候选排序；拓扑、几何、特征、边界、误差和可选纹理策略共同决定位置是否可以修改网格。
 */

#pragma once

#include "Export.h"
#include "algorithms/simplification/SimplificationTypes.h"
#include "core/Mesh.h"

#include <memory>

namespace manumesh::feature {
struct FeatureAnalysis;
}

namespace manumesh::simplification {

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/// 用于配置和运行网格简化的有状态对象 API。
class MANUMESH_API QEMSimplifier {
public:
    /// 使用默认选项构造简化器。
    QEMSimplifier();
    /// @param[in] options 复制到此对象中的已校验选项。
    /// @throws std::invalid_argument 当选项不一致时抛出。
    explicit QEMSimplifier(SimplifyOptions options);
    ~QEMSimplifier();

    QEMSimplifier(const QEMSimplifier& other);
    QEMSimplifier& operator=(const QEMSimplifier& other);
    QEMSimplifier(QEMSimplifier&& other) noexcept;
    QEMSimplifier& operator=(QEMSimplifier&& other) noexcept;

    /// 返回后续简化运行使用的选项。
    const SimplifyOptions& options() const;
    /// 替换后续简化运行使用的选项。
    /// @param[in] options 新的已校验策略。
    /// @throws std::invalid_argument 当选项不一致时抛出。
    void setOptions(SimplifyOptions options);
    /// 返回最近一次简化运行的诊断信息。
    const SimplifyReport& report() const;

    /// 简化网格并将诊断信息存储在此对象中。
    /// @param[in] input 三角表面网格；不会被修改。
    /// @return 简化后的稠密网格。
    Mesh simplify(const Mesh& input);
    /// 简化网格、存储诊断信息，并可选择将其复制到输出参数。
    /// @param[in] input 三角表面网格。
    /// @param[out] report 可选的 report() 副本。
    /// @return 简化后的稠密网格。
    Mesh simplify(const Mesh& input, SimplifyReport* report);
    /// 启用特征保护时，使用预先计算的特征分析简化网格。
    /// @param[in] input 用于计算 `features` 的网格。
    /// @param[in] features 预先计算的图和环归属信息。
    /// @return 简化后的稠密网格，不会重新运行特征检测。
    Mesh simplify(const Mesh& input, const feature::FeatureAnalysis& features);
    /// 使用预先计算的特征分析简化网格，并可选择复制诊断信息。
    /// @param[in] input 用于计算 `features` 的网格。
    /// @param[in] features 预先计算的特征分析。
    /// @param[out] report 可选的诊断信息副本。
    /// @return 简化后的稠密网格。
    Mesh simplify(const Mesh& input, const feature::FeatureAnalysis& features, SimplifyReport* report);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

/// 使用标准 QEM 或加入 line quadrics 的 QEM 简化网格。
/// 需要面向对象 API 的新代码应优先使用 QEMSimplifier。
/// @param[in] input 源三角网格。
/// @param[in] options 目标、排序代价和接受策略。
/// @param[out] report 可选的诊断信息。
/// @return 顶点和面已压缩的简化网格。
/// @algorithm 累积面平面以及可选的线/约束 quadrics，为每条活动边求解排序后的位置，
/// 反复取出当前代价最低的候选，应用硬性接受过滤器，更新局部拓扑和候选版本，
/// 最后可选地执行固定拓扑精修。
/// @invariants QEM 只负责排序，绝不会覆盖硬性拓扑、边界、特征、自交、纹理或误差拒绝条件。
/// @failuremodes 当没有合法候选或达到有界拒绝上限时，运行可能在高于目标面数时终止；
/// 请检查 SimplifyReport::terminationReason 和拒绝计数。
MANUMESH_API Mesh simplifyMesh(const Mesh& input, const SimplifyOptions& options, SimplifyReport* report = nullptr);
/// 使用已为 `input` 计算的特征分析进行简化。
/// @param[in] input 源网格。
/// @param[in] options 简化策略。
/// @param[in] features `input` 的特征图和基本体约束。
/// @param[out] report 可选的诊断信息。
/// @return 简化后的网格，不会重复执行特征分析。
MANUMESH_API Mesh simplifyMesh(
    const Mesh& input,
    const SimplifyOptions& options,
    const feature::FeatureAnalysis& features,
    SimplifyReport* report = nullptr
);

} // namespace manumesh::simplification
