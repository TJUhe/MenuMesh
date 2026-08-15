/**
 * @file tests/support/AnalyticFixtures.h
 * @brief 验证 ManuMesh 测试中的解析夹具行为。
 * @ingroup manumesh_tests
 *
 * @details 测试夹具和断言记录可观察契约、数值容差、确定性要求以及已修复的回归问题。
 */

#pragma once

#include "core/Mesh.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace manumesh {
namespace test {
namespace analytic {

/// 确定性的三角网格夹具，并提供闭式真值。
///
/// 每个生成器都是纯函数且可复现（不使用随机设备或时钟）。
/// 解析曲率的符号约定：从外表面法向弯离的法截面取正曲率。因此，半径为 r 的球体
/// 报告 kappaMax = kappaMin = 1/r；圆柱侧壁报告 kappaMax = 1/r（周向）和
/// kappaMin = 0（轴向）；环面则从 {1/r, cos(theta)/(R + r cos(theta))} 中
/// 给出 kappaMax/kappaMin。

/// 单个顶点的解析主曲率与主方向标架。
struct PrincipalCurvatures {
    double kappaMax = 0.0;
    double kappaMin = 0.0;
    Vec3 directionMax = Vec3(1.0, 0.0, 0.0);
    Vec3 directionMin = Vec3(0.0, 1.0, 0.0);
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
};

/// 一个解析真值特征圆（例如圆柱的边缘圆）。
struct GroundTruthCircle {
    Vec3 center = Vec3::Zero();
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    double radius = 0.0;
};

/// 经度/纬度球体。顶点 0 是北极，顶点 vertexCount-1 是南极；纬度行 k（1..rings-1）
/// 连续存放 `segments` 个顶点。
struct SphereFixture {
    Mesh mesh;
    double radius = 1.0;
    int rings = 0;
    int segments = 0;

    bool isPole(int vertex) const;
    /// 返回顶点所在的纬度行索引，范围为 [0, rings]；两极分别映射到 0 和 rings。
    int ringIndex(int vertex) const;
    /// 对于距离任一极点至少 `margin` 个纬度行的顶点返回 true。
    bool isInterior(int vertex, int margin) const;
    PrincipalCurvatures analyticPrincipalCurvatures(int vertex) const;
};
SphereFixture makeUvSphere(int rings, int segments, double radius);

/// 绕 z 轴的正圆柱，z 范围为 [-height/2, height/2]。
/// 侧壁顶点按从底部（第 0 行）到顶部（第 `rings` 行）的行主序存储，每行有
/// `segments` 个顶点；若封盖，则随后追加两个盖面中心（先底部后顶部）。封盖版本的
/// 两条边缘圆具有精确的 90 度折痕。
struct CylinderFixture {
    Mesh mesh;
    double radius = 1.0;
    double height = 1.0;
    int segments = 0;
    int rings = 0;
    bool capped = false;

    bool isRimVertex(int vertex) const;
    /// 严格位于两条边缘圆之间的侧壁顶点。
    bool isSideInterior(int vertex) const;
    /// 适用于侧壁顶点（包括两条边缘圆）。
    PrincipalCurvatures analyticPrincipalCurvatures(int vertex) const;
    /// 两条边缘圆（先底部后顶部）。仅封盖版本具有有意义的真值特征曲线。
    std::vector<GroundTruthCircle> groundTruthCircles() const;
    /// 将封盖版本的折痕边表示为按顶点索引排序的边对。
    std::vector<std::pair<int, int>> groundTruthFeatureEdges() const;
};
CylinderFixture makeCylinder(int segments, int rings, double radius, double height, bool capped);

/// 绕 z 轴的环面：P(u, v) = ((R + r cos v) cos u, (R + r cos v) sin u, r sin v)。
/// 顶点按 majorSegments 行、每行 minorSegments 个顶点存储（主索引 i、次索引 j 对应
/// i * minorSegments + j）。
struct TorusFixture {
    Mesh mesh;
    double majorRadius = 1.0;
    double minorRadius = 0.25;
    int majorSegments = 0;
    int minorSegments = 0;

    /// 返回某个顶点的次角 v，范围为 [0, 2*pi)。
    double minorAngle(int vertex) const;
    /// 当 cos(v) < 0 时顶点位于管体内侧。
    bool isInnerSide(int vertex) const;
    PrincipalCurvatures analyticPrincipalCurvatures(int vertex) const;
};
TorusFixture makeTorus(int majorSegments, int minorSegments, double majorRadius, double minorRadius);

/// 边长为 `size` 的轴对齐盒；四条竖直棱以腿长 `chamfer` 做 45 度倒角，形成带平面
/// 八边形盖面的八棱柱。每条原始竖直棱都被一个倒角面替代，该倒角面由两条平行的竖直
/// 折痕线界定。因此，真值硬边集合包含 8 条竖直折痕线（二面角 45 度）以及上下两条
/// 八边形边界（二面角 90 度）。
struct ChamferBoxFixture {
    Mesh mesh;
    double size = 1.0;
    double chamfer = 0.1;
    int divisions = 1;

    /// 返回所有真值硬边，每条边表示为按顶点索引排序的边对。
    std::vector<std::pair<int, int>> groundTruthHardEdges() const;
};
ChamferBoxFixture makeChamferBox(double size, double chamfer, int divisions);

/// 在 [-size/2, size/2]^2 上构造开口曲面 z = height * exp(-sharpness * x^2)，
/// y 轴方向有一条平滑脊线。脊顶处的剖面曲率为 f''(0) = -2 * height * sharpness，
/// 因此穿过脊线的主曲率幅值由 analyticCrestCurvature() 给出，脊线切向为 y 轴。
struct GaussianRidgeSheetFixture {
    Mesh mesh;
    int subdivisions = 0;
    double size = 2.0;
    double height = 0.25;
    double sharpness = 8.0;

    /// 位于脊线 x == 0 上、排除两条边界行的顶点。
    std::vector<int> interiorCrestVertices() const;
    /// |f''(0)| = 2 * height * sharpness（f'(0) = 0，因此这是脊顶处横跨脊线的精确
    /// 主曲率幅值）。
    double analyticCrestCurvature() const;
    Vec3 crestTangent() const { return Vec3(0.0, 1.0, 0.0); }
};
GaussianRidgeSheetFixture makeGaussianRidgeSheet(int subdivisions, double size, double height, double sharpness);

/// 与 GaussianRidgeSheetFixture 具有相同解析剖面（z = h * exp(-s * x^2)，脊线沿 y 轴、
/// 位于 x = 0），但列密度分级：x < 0 一侧的列密度是 x > 0 一侧的 `densityRatio` 倍，
/// 且有一列恰好位于脊线 x = 0。这样脊线位于密度过渡带中：每个顶点的拟合半径（相邻
/// 边长平均值）在每条穿过脊线的边的两端最多相差 `densityRatio` 倍，正是跨顶点极值
/// 比较必须进行单位换算的配置。
struct GradedGaussianRidgeSheetFixture {
    Mesh mesh;
    int fineColumns = 0;   ///< 覆盖 x ∈ [-size/2, 0] 的列间隔数。
    int coarseColumns = 0; ///< 覆盖 x ∈ [0, size/2] 的列间隔数。
    int rows = 0;          ///< 覆盖 y ∈ [-size/2, size/2] 的行间隔数。
    double size = 2.0;
    double height = 0.25;
    double sharpness = 8.0;

    double fineSpacing() const { return 0.5 * size / fineColumns; }
    double coarseSpacing() const { return 0.5 * size / coarseColumns; }
    /// 返回行主序布局中 (row, col) 对应的顶点索引。
    int vertexAt(int row, int col) const { return row * (fineColumns + coarseColumns + 1) + col; }
    /// 返回 x = 0 的脊线列索引。
    int crestColumn() const { return fineColumns; }
    /// |f''(0)| = 2 * height * sharpness。
    double analyticCrestCurvature() const { return 2.0 * height * sharpness; }
    Vec3 crestTangent() const { return Vec3(0.0, 1.0, 0.0); }
};
GradedGaussianRidgeSheetFixture
makeGradedGaussianRidgeSheet(int fineColumns, double size, double height, double sharpness, int densityRatio);

/// 返回网格副本：每个顶点都由固定种子的 64 位 LCG 生成的可复现伪随机偏移
/// （范围 [-amplitude, amplitude]^3）扰动。不使用 std::random_device 或时间；相同的
/// (mesh, amplitude, seed) 三元组始终产生逐位相同的结果。
Mesh withDeterministicNoise(const Mesh& mesh, double amplitude, std::uint64_t seed);

/// 返回网格副本，其中每个顶点都乘以 `factor`。
Mesh uniformlyScaled(const Mesh& mesh, double factor);

/// 返回网格无向唯一边的平均长度。
double meanEdgeLength(const Mesh& mesh);

} // namespace analytic
} // namespace test
} // namespace manumesh
