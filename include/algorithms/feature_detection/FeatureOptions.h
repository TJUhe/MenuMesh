/**
 * @file include/algorithms/feature_detection/FeatureOptions.h
 * @brief 声明 ManuMesh 特征检测模块的轻量配置类型。
 * @ingroup manumesh_feature_detection
 *
 * @details 此头文件只包含特征检测配置契约，不依赖网格、Eigen 或检测结果类型。
 */

#pragma once

namespace manumesh {
namespace feature {

/// validateFeatureOptions 接受的迭代/尺度参数上限。实现不会计算超过这些上限的环数、尺度或迭代次数，
/// 因此越界请求会立即拒绝，而不是静默截断。
constexpr int kMaxNormalTensorSmoothingIterations = 8;
constexpr int kMaxNormalTensorScaleCount = 8;
constexpr int kMaxSmoothCurvatureBaseNeighborhoodRings = 4;
constexpr int kMaxSmoothCurvatureScaleCount = 6;
constexpr int kMaxSmoothCurvatureRobustFitIterations = 4;
constexpr int kMaxFeatureNormalFilterIterations = 16;

/// 面向含噪三角网格的可选法向域预处理。
///
/// 过滤器保持输入拓扑和顶点位置不变。它交替计算边指标并按面积加权松弛面法向，
/// 使特征证据可以使用稳定后的法向，而不会静默替换调用方的网格。
struct FeatureNormalFilterOptions {
    bool enabled = false;           ///< 在完整检测器中启用预处理。
    int iterations = 4;             ///< 松弛次数，范围为 [0, kMaxFeatureNormalFilterIterations]。
    double angleSigmaDeg = 20.0;    ///< 双边角度带宽，单位为度。
    double preserveAngleDeg = 50.0; ///< 大于等于此角度的不连续处会被冻结。
    double relaxation = 0.8;        ///< 向兼容邻域均值混合的比例，范围为 [0,1]。
};

/// 局部特征图清理后的组件级恢复。
struct FeatureGraphConsolidationOptions {
    bool enabled = false;           ///< 启用清理后的组件级恢复。
    double maxGapLengthRatio = 3.0; ///< 局部边长单位下允许的最大端点间隙。
    double minAlignment = 0.75;     ///< 延续切线点积绝对值的最小值。
};

/// 由活动特征图边诱导的可选面分区。
struct SurfacePatchOptions {
    bool enabled = false;            ///< 在图恢复后启用面分割。
    bool includeWeakEvidence = true; ///< 将仅由张量/曲率产生的边视为分区边界。
};

/// 折痕、边界和特征环检测参数。
///
/// 检测器首先针对 CAD/STL 风格网格调校：显式边界和二面角证据构成特征图，张量证据作为弱折痕的次要信号。
/// 因此应结合网格尺度和数据来源选择阈值，不应在扫描/含噪输入与干净 CAD 输入之间盲目复用。
struct FeatureOptions {
    /// 硬特征边的二面角阈值，单位为度。
    double featureAngleDeg = 40.0;
    /// 将特征边追踪为环归属时使用的二面角。
    /// 负值表示“复用 featureAngleDeg”。
    double loopTraceAngleDeg = -1.0;
    /// 校验圆环时使用的相对径向容差。
    double circleFitRelativeThreshold = 0.05;
    /// 校验椭圆环时使用的相对残差容差。
    double ellipseFitRelativeThreshold = 0.05;
    /// 低于此轴比容差的椭圆视为近圆。
    double nearCircleAxisRatioTolerance = 0.08;
    /// 恢复环接受和基本体拟合所需的最少顶点数。
    /// 直接追踪的开放链和闭合追踪结果即使低于此阈值仍会报告，只是不会通过该路径获得基本体拟合。
    int minFeatureLoopVertices = 8;
    /// 除图边外，启用由张量推导的弱特征候选。
    bool useNormalTensorFeatures = true;
    /// 弱特征分类所需的最小张量显著性分数。
    double normalTensorFeatureThreshold = 0.16;
    /// 接受张量推导边证据所需的最小边/切线对齐度。
    double normalTensorMinEdgeAlignment = 0.45;
    /// 多尺度评分前的逐顶点张量一环平滑次数。
    /// 有效范围：[0, kMaxNormalTensorSmoothingIterations]。
    int normalTensorSmoothingIterations = 0;
    /// 弱特征评分采样的张量尺度数量。
    /// 有效范围：[1, kMaxNormalTensorScaleCount]。
    int normalTensorScaleCount = 1;
    /// 支持张量特征权重和折痕边候选的最小尺度数量。
    /// 角点只贡献顶点权重；折痕边仍要求两端均为折痕主导且通过切向对齐检查。
    /// 有效范围：[1, normalTensorScaleCount]。
    int normalTensorMinPersistentScales = 1;
    /// 启用由局部 quadric 拟合得到的确定性平滑脊/谷证据。
    /// 保持为可选功能，因为 CAD/STL 硬特征与扫描/自由曲面场景需要不同的阈值和校验数据。
    bool useSmoothCurvatureFeatures = false;
    /// 最小尺度归一化平滑特征分数。
    double smoothCurvatureFeatureThreshold = 0.015;
    /// 网格边与恢复曲线切线之间的最小对齐度。
    double smoothCurvatureMinEdgeAlignment = 0.55;
    /// 最小跨尺度及端点切线一致性。
    double smoothCurvatureMinTangentConsistency = 0.65;
    /// 局部 quadric 拟合使用的基础拓扑半径。
    /// 有效范围：[1, kMaxSmoothCurvatureBaseNeighborhoodRings]。
    int smoothCurvatureBaseNeighborhoodRings = 2;
    /// 逐步增大的 quadric 拟合邻域数量。
    /// 有效范围：[1, kMaxSmoothCurvatureScaleCount]。
    int smoothCurvatureScaleCount = 3;
    /// 支持平滑特征候选的最小尺度数量。
    /// 有效范围：[1, smoothCurvatureScaleCount]。
    int smoothCurvatureMinPersistentScales = 2;
    /// 局部 quadric 拟合的确定性稳健重加权次数。
    /// 有效范围：[0, kMaxSmoothCurvatureRobustFitIterations]。
    int smoothCurvatureRobustFitIterations = 2;
    /// 根据跨尺度稳定性而不是单独的原始峰值分数选择参考拟合尺度。
    bool smoothCurvatureUseStableScaleSelection = false;
    /// 选定平滑曲率尺度可接受的最小稳定性。
    double smoothCurvatureMinScaleStability = 0.0;
    /// 在环恢复前启用局部特征图清理。
    bool cleanupFeatureGraph = true;
    /// 清理阶段可桥接的最大端点间隙，单位为局部平均边长。
    double featureGraphGapLengthRatio = 1.25;
    /// 旧版按边数清理规则会移除的最大弱证据（法向张量或平滑曲率）spur 长度。
    int featureGraphMaxWeakSpurEdges = 2;
    /// 报告高置信度组件时使用的置信度阈值。
    double featureComponentMinConfidence = 0.35;
    /// 用于移除弱 spur 的无量纲 Yoshizawa 风格强度阈值。
    ///
    /// 当该值为正时，仅当悬挂弱证据链的曲线强度 T = (积分 ds) * (积分 strength ds) 低于该值时才移除。
    /// 其中 ds 以局部平均边长为单位，每条边的强度为持久分数除以对应通道阈值。这样长但微弱的链会保留，
    /// 短但强烈的噪声尖峰会被裁剪，且长度超过 featureGraphMaxWeakSpurEdges 的链也可被裁剪。
    /// 默认值 0 保留旧行为：移除边数不超过 featureGraphMaxWeakSpurEdges 的所有弱 spur。
    double featureGraphMinWeakSpurStrength = 0.0;

    /// 可选的含噪输入预处理、组件恢复和面分割阶段。
    /// 将它们分组可以保持主要选项界面可读，同时保留值语义。
    FeatureNormalFilterOptions normalFilter;
    FeatureGraphConsolidationOptions graphConsolidation;
    SurfacePatchOptions surfacePatches;
};

/// Tsuchie-Higashi 风格法向张量特征评分参数。
struct NormalTensorOptions {
    int smoothingIterations = 0; ///< 多尺度评分前的逐顶点张量一环平滑次数。
    int scaleCount = 1;          ///< 逐步增大的拓扑尺度数量。
    /// 独立 Normal Tensor 调用的可选面法向预处理。
    /// 完整特征管线会在共享缓存上应用 `FeatureOptions::normalFilter`。
    FeatureNormalFilterOptions normalFilter;
};

/// 稳健尺度归一化局部 quadric 拟合参数。
struct SmoothCurvatureOptions {
    int baseNeighborhoodRings = 2;        ///< 最细拟合的拓扑半径。
    int scaleCount = 3;                   ///< 逐步增大的拟合数量。
    int robustFitIterations = 2;          ///< 确定性的残差重加权次数。
    double minTangentConsistency = 0.65;  ///< 跨尺度切线点积绝对值门限。
    bool useStableScaleSelection = false; ///< 相比原始峰值分数优先选择稳定尺度支持。
    double minScaleStability = 0.0;       ///< 可接受的最小尺度稳定性分数。
};

} // namespace feature
} // namespace manumesh
