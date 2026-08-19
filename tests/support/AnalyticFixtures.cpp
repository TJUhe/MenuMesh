/**
 * @file tests/support/AnalyticFixtures.cpp
 * @brief 实现带解析真值的球体、圆柱、环面、倒角盒和高斯脊测试网格。
 * @ingroup manumesh_tests
 */

#include "AnalyticFixtures.h"
#include "core/MathUtils.h"

#include "core/MathConstants.h"

#include <algorithm>
#include <cmath>

namespace manumesh {
namespace test {
namespace analytic {
namespace {

std::pair<int, int> sortedEdge(int a, int b) { return a < b ? std::make_pair(a, b) : std::make_pair(b, a); }

} // 匿名命名空间

// ---------------------------------------------------------------------------
// 球体
// ---------------------------------------------------------------------------

bool SphereFixture::isPole(int vertex) const {
    return vertex == 0 || vertex == static_cast<int>(mesh.vertices.size()) - 1;
}

int SphereFixture::ringIndex(int vertex) const {
    if (vertex == 0) {
        return 0;
    }
    if (vertex == static_cast<int>(mesh.vertices.size()) - 1) {
        return rings;
    }
    return 1 + (vertex - 1) / segments;
}

bool SphereFixture::isInterior(int vertex, int margin) const {
    const int row = ringIndex(vertex);
    return row >= margin && row <= rings - margin;
}

PrincipalCurvatures SphereFixture::analyticPrincipalCurvatures(int vertex) const {
    PrincipalCurvatures result;
    result.kappaMax = 1.0 / radius;
    result.kappaMin = 1.0 / radius;
    result.normal = mesh.vertices[vertex].normalized();
    // 脐点处任意切向都可以作为主方向，因此构造一组任意的正交归一切向基。
    const Vec3 seed = std::abs(result.normal.z()) < 0.9 ? Vec3(0.0, 0.0, 1.0) : Vec3(1.0, 0.0, 0.0);
    result.directionMax = result.normal.cross(seed).normalized();
    result.directionMin = result.normal.cross(result.directionMax).normalized();
    return result;
}

SphereFixture makeUvSphere(int rings, int segments, double radius) {
    SphereFixture fixture;
    fixture.rings = std::max(3, rings);
    fixture.segments = std::max(3, segments);
    fixture.radius = radius;
    Mesh& mesh = fixture.mesh;

    mesh.vertices.emplace_back(0.0, 0.0, radius);
    for (int row = 1; row < fixture.rings; ++row) {
        const double phi = kPi * static_cast<double>(row) / fixture.rings;
        for (int i = 0; i < fixture.segments; ++i) {
            const double theta = 2.0 * kPi * static_cast<double>(i) / fixture.segments;
            mesh.vertices.emplace_back(
                radius * std::sin(phi) * std::cos(theta),
                radius * std::sin(phi) * std::sin(theta),
                radius * std::cos(phi)
            );
        }
    }
    mesh.vertices.emplace_back(0.0, 0.0, -radius);
    const int southPole = static_cast<int>(mesh.vertices.size()) - 1;

    auto rowVertex = [&](int row, int i) {
        return 1 + (row - 1) * fixture.segments + i % fixture.segments;
    };
    for (int i = 0; i < fixture.segments; ++i) {
        mesh.faces.push_back({{0, rowVertex(1, i), rowVertex(1, i + 1)}});
    }
    for (int row = 1; row + 1 < fixture.rings; ++row) {
        for (int i = 0; i < fixture.segments; ++i) {
            const int v00 = rowVertex(row, i);
            const int v01 = rowVertex(row, i + 1);
            const int v10 = rowVertex(row + 1, i);
            const int v11 = rowVertex(row + 1, i + 1);
            mesh.faces.push_back({{v00, v10, v11}});
            mesh.faces.push_back({{v00, v11, v01}});
        }
    }
    for (int i = 0; i < fixture.segments; ++i) {
        mesh.faces.push_back({{southPole, rowVertex(fixture.rings - 1, i + 1), rowVertex(fixture.rings - 1, i)}});
    }
    return fixture;
}

// ---------------------------------------------------------------------------
// 圆柱
// ---------------------------------------------------------------------------

bool CylinderFixture::isRimVertex(int vertex) const {
    const int sideCount = (rings + 1) * segments;
    if (vertex >= sideCount) {
        return false;
    }
    const int row = vertex / segments;
    return row == 0 || row == rings;
}

bool CylinderFixture::isSideInterior(int vertex) const {
    const int sideCount = (rings + 1) * segments;
    return vertex < sideCount && !isRimVertex(vertex);
}

PrincipalCurvatures CylinderFixture::analyticPrincipalCurvatures(int vertex) const {
    PrincipalCurvatures result;
    const Vec3& p = mesh.vertices[vertex];
    const Vec3 radial = Vec3(p.x(), p.y(), 0.0).normalized();
    result.normal = radial;
    result.kappaMax = 1.0 / radius;
    result.kappaMin = 0.0;
    result.directionMax = Vec3(-radial.y(), radial.x(), 0.0); // 周向
    result.directionMin = Vec3(0.0, 0.0, 1.0);                // 轴向
    return result;
}

std::vector<GroundTruthCircle> CylinderFixture::groundTruthCircles() const {
    return {
        {Vec3(0.0, 0.0, -0.5 * height), Vec3(0.0, 0.0, 1.0), radius},
        {Vec3(0.0, 0.0, 0.5 * height), Vec3(0.0, 0.0, 1.0), radius},
    };
}

std::vector<std::pair<int, int>> CylinderFixture::groundTruthFeatureEdges() const {
    std::vector<std::pair<int, int>> edges;
    edges.reserve(2 * static_cast<std::size_t>(segments));
    for (int i = 0; i < segments; ++i) {
        const int next = (i + 1) % segments;
        edges.push_back(sortedEdge(i, next));
        edges.push_back(sortedEdge(rings * segments + i, rings * segments + next));
    }
    return edges;
}

CylinderFixture makeCylinder(int segments, int rings, double radius, double height, bool capped) {
    CylinderFixture fixture;
    fixture.segments = std::max(3, segments);
    fixture.rings = std::max(1, rings);
    fixture.radius = radius;
    fixture.height = height;
    fixture.capped = capped;
    Mesh& mesh = fixture.mesh;

    for (int row = 0; row <= fixture.rings; ++row) {
        const double z = (static_cast<double>(row) / fixture.rings - 0.5) * height;
        for (int i = 0; i < fixture.segments; ++i) {
            const double theta = 2.0 * kPi * static_cast<double>(i) / fixture.segments;
            mesh.vertices.emplace_back(radius * std::cos(theta), radius * std::sin(theta), z);
        }
    }
    for (int row = 0; row < fixture.rings; ++row) {
        for (int i = 0; i < fixture.segments; ++i) {
            const int j = (i + 1) % fixture.segments;
            const int v00 = row * fixture.segments + i;
            const int v10 = row * fixture.segments + j;
            const int v01 = (row + 1) * fixture.segments + i;
            const int v11 = (row + 1) * fixture.segments + j;
            mesh.faces.push_back({{v00, v10, v11}});
            mesh.faces.push_back({{v00, v11, v01}});
        }
    }
    if (capped) {
        const int bottomCenter = static_cast<int>(mesh.vertices.size());
        mesh.vertices.emplace_back(0.0, 0.0, -0.5 * height);
        const int topCenter = static_cast<int>(mesh.vertices.size());
        mesh.vertices.emplace_back(0.0, 0.0, 0.5 * height);
        for (int i = 0; i < fixture.segments; ++i) {
            const int j = (i + 1) % fixture.segments;
            mesh.faces.push_back({{bottomCenter, j, i}});
            mesh.faces.push_back(
                {{topCenter, fixture.rings * fixture.segments + i, fixture.rings * fixture.segments + j}}
            );
        }
    }
    return fixture;
}

// ---------------------------------------------------------------------------
// 环面
// ---------------------------------------------------------------------------

double TorusFixture::minorAngle(int vertex) const {
    const int j = vertex % minorSegments;
    return 2.0 * kPi * static_cast<double>(j) / minorSegments;
}

bool TorusFixture::isInnerSide(int vertex) const { return std::cos(minorAngle(vertex)) < 0.0; }

PrincipalCurvatures TorusFixture::analyticPrincipalCurvatures(int vertex) const {
    PrincipalCurvatures result;
    const int i = vertex / minorSegments;
    const double u = 2.0 * kPi * static_cast<double>(i) / majorSegments;
    const double v = minorAngle(vertex);
    const Vec3 axisRadial(std::cos(u), std::sin(u), 0.0);
    result.normal = std::cos(v) * axisRadial + Vec3(0.0, 0.0, std::sin(v));
    const double minorCurvature = 1.0 / minorRadius;
    const double majorCurvature = std::cos(v) / (majorRadius + minorRadius * std::cos(v));
    const Vec3 minorDirection = -std::sin(v) * axisRadial + Vec3(0.0, 0.0, std::cos(v));
    const Vec3 majorDirection(-std::sin(u), std::cos(u), 0.0);
    if (minorCurvature >= majorCurvature) {
        result.kappaMax = minorCurvature;
        result.directionMax = minorDirection;
        result.kappaMin = majorCurvature;
        result.directionMin = majorDirection;
    } else {
        result.kappaMax = majorCurvature;
        result.directionMax = majorDirection;
        result.kappaMin = minorCurvature;
        result.directionMin = minorDirection;
    }
    return result;
}

TorusFixture makeTorus(int majorSegments, int minorSegments, double majorRadius, double minorRadius) {
    TorusFixture fixture;
    fixture.majorSegments = std::max(3, majorSegments);
    fixture.minorSegments = std::max(3, minorSegments);
    fixture.majorRadius = majorRadius;
    fixture.minorRadius = minorRadius;
    Mesh& mesh = fixture.mesh;

    for (int i = 0; i < fixture.majorSegments; ++i) {
        const double u = 2.0 * kPi * static_cast<double>(i) / fixture.majorSegments;
        for (int j = 0; j < fixture.minorSegments; ++j) {
            const double v = 2.0 * kPi * static_cast<double>(j) / fixture.minorSegments;
            const double ringRadius = majorRadius + minorRadius * std::cos(v);
            mesh.vertices.emplace_back(ringRadius * std::cos(u), ringRadius * std::sin(u), minorRadius * std::sin(v));
        }
    }
    for (int i = 0; i < fixture.majorSegments; ++i) {
        const int in = (i + 1) % fixture.majorSegments;
        for (int j = 0; j < fixture.minorSegments; ++j) {
            const int jn = (j + 1) % fixture.minorSegments;
            const int v00 = i * fixture.minorSegments + j;
            const int v10 = in * fixture.minorSegments + j;
            const int v01 = i * fixture.minorSegments + jn;
            const int v11 = in * fixture.minorSegments + jn;
            mesh.faces.push_back({{v00, v10, v11}});
            mesh.faces.push_back({{v00, v11, v01}});
        }
    }
    return fixture;
}

// ---------------------------------------------------------------------------
// 倒角盒（八棱柱）
// ---------------------------------------------------------------------------

std::vector<std::pair<int, int>> ChamferBoxFixture::groundTruthHardEdges() const {
    std::vector<std::pair<int, int>> edges;
    // 竖直折痕线：每个八边形角点（共 8 个）对应一列 divisions 条边，
    // 另加上下两条八边形边界（每条 8 条边）。
    for (int corner = 0; corner < 8; ++corner) {
        for (int row = 0; row < divisions; ++row) {
            edges.push_back(sortedEdge(row * 8 + corner, (row + 1) * 8 + corner));
        }
    }
    for (int corner = 0; corner < 8; ++corner) {
        const int next = (corner + 1) % 8;
        edges.push_back(sortedEdge(corner, next));
        edges.push_back(sortedEdge(divisions * 8 + corner, divisions * 8 + next));
    }
    return edges;
}

ChamferBoxFixture makeChamferBox(double size, double chamfer, int divisions) {
    ChamferBoxFixture fixture;
    fixture.size = size;
    fixture.chamfer = manumesh::clampValue(chamfer, 1e-6 * size, 0.49 * size);
    fixture.divisions = std::max(1, divisions);
    Mesh& mesh = fixture.mesh;

    const double h = 0.5 * size;
    const double c = fixture.chamfer;
    // 八边形角点按逆时针顺序排列（外法向背离轴线）：以 [-h, h]^2 为基础，
    // 沿每条相邻边以 45 度方向向内切去长度为 c 的角。
    const double corners[8][2] = {
        {h, -h + c},
        {h, h - c},
        {h - c, h},
        {-h + c, h},
        {-h, h - c},
        {-h, -h + c},
        {-h + c, -h},
        {h - c, -h},
    };

    for (int row = 0; row <= fixture.divisions; ++row) {
        const double z = (static_cast<double>(row) / fixture.divisions - 0.5) * size;
        for (int corner = 0; corner < 8; ++corner) {
            mesh.vertices.emplace_back(corners[corner][0], corners[corner][1], z);
        }
    }
    for (int row = 0; row < fixture.divisions; ++row) {
        for (int corner = 0; corner < 8; ++corner) {
            const int next = (corner + 1) % 8;
            const int v00 = row * 8 + corner;
            const int v10 = row * 8 + next;
            const int v01 = (row + 1) * 8 + corner;
            const int v11 = (row + 1) * 8 + next;
            mesh.faces.push_back({{v00, v10, v11}});
            mesh.faces.push_back({{v00, v11, v01}});
        }
    }
    const int bottomCenter = static_cast<int>(mesh.vertices.size());
    mesh.vertices.emplace_back(0.0, 0.0, -h);
    const int topCenter = static_cast<int>(mesh.vertices.size());
    mesh.vertices.emplace_back(0.0, 0.0, h);
    for (int corner = 0; corner < 8; ++corner) {
        const int next = (corner + 1) % 8;
        mesh.faces.push_back({{bottomCenter, next, corner}});
        mesh.faces.push_back({{topCenter, fixture.divisions * 8 + corner, fixture.divisions * 8 + next}});
    }
    return fixture;
}

// ---------------------------------------------------------------------------
// 高斯脊面
// ---------------------------------------------------------------------------

std::vector<int> GaussianRidgeSheetFixture::interiorCrestVertices() const {
    std::vector<int> crest;
    const double spacing = size / subdivisions;
    const double halfSize = 0.5 * size;
    for (int vertex = 0; vertex < static_cast<int>(mesh.vertices.size()); ++vertex) {
        const Vec3& p = mesh.vertices[vertex];
        if (std::abs(p.x()) < 1e-9 * size && std::abs(p.y()) < halfSize - 0.5 * spacing) {
            crest.push_back(vertex);
        }
    }
    return crest;
}

double GaussianRidgeSheetFixture::analyticCrestCurvature() const { return 2.0 * height * sharpness; }

GaussianRidgeSheetFixture makeGaussianRidgeSheet(int subdivisions, double size, double height, double sharpness) {
    GaussianRidgeSheetFixture fixture;
    // 使用偶数细分数，使某一列顶点恰好位于脊线 x = 0 上。
    fixture.subdivisions = std::max(4, subdivisions + (subdivisions % 2));
    fixture.size = size;
    fixture.height = height;
    fixture.sharpness = sharpness;
    Mesh& mesh = fixture.mesh;

    const int n = fixture.subdivisions;
    for (int row = 0; row <= n; ++row) {
        const double y = (static_cast<double>(row) / n - 0.5) * size;
        for (int col = 0; col <= n; ++col) {
            const double x = (static_cast<double>(col) / n - 0.5) * size;
            mesh.vertices.emplace_back(x, y, height * std::exp(-sharpness * x * x));
        }
    }
    for (int row = 0; row < n; ++row) {
        for (int col = 0; col < n; ++col) {
            const int v00 = row * (n + 1) + col;
            const int v10 = v00 + 1;
            const int v01 = v00 + (n + 1);
            const int v11 = v01 + 1;
            mesh.faces.push_back({{v00, v10, v11}});
            mesh.faces.push_back({{v00, v11, v01}});
        }
    }
    return fixture;
}

// ---------------------------------------------------------------------------
// 分级密度高斯脊面
// ---------------------------------------------------------------------------

GradedGaussianRidgeSheetFixture
makeGradedGaussianRidgeSheet(int fineColumns, double size, double height, double sharpness, int densityRatio) {
    GradedGaussianRidgeSheetFixture fixture;
    const int ratio = std::max(1, densityRatio);
    // 将细网格半区向上取整为 densityRatio 的整数倍，从而使粗网格半区的区间数为整数，
    // 并确保某一列恰好落在 x = 0 上。
    fixture.fineColumns = std::max(ratio, ((std::max(2, fineColumns) + ratio - 1) / ratio) * ratio);
    fixture.coarseColumns = fixture.fineColumns / ratio;
    fixture.size = size;
    fixture.height = height;
    fixture.sharpness = sharpness;
    Mesh& mesh = fixture.mesh;

    const double fineSpacing = fixture.fineSpacing();
    const double coarseSpacing = fineSpacing * ratio;
    // 行间距取两种列间距的几何平均值，使两侧三角形的长宽比均受 sqrt(ratio) 约束。
    const double rowSpacing = fineSpacing * std::sqrt(static_cast<double>(ratio));
    fixture.rows = std::max(4, static_cast<int>(std::lround(size / rowSpacing)));

    const int columns = fixture.fineColumns + fixture.coarseColumns + 1;
    for (int row = 0; row <= fixture.rows; ++row) {
        const double y = (static_cast<double>(row) / fixture.rows - 0.5) * size;
        for (int col = 0; col < columns; ++col) {
            const double x = col <= fixture.fineColumns ? -0.5 * size + col * fineSpacing
                                                        : (col - fixture.fineColumns) * coarseSpacing;
            mesh.vertices.emplace_back(x, y, height * std::exp(-sharpness * x * x));
        }
    }
    for (int row = 0; row < fixture.rows; ++row) {
        for (int col = 0; col + 1 < columns; ++col) {
            const int v00 = row * columns + col;
            const int v10 = v00 + 1;
            const int v01 = v00 + columns;
            const int v11 = v01 + 1;
            mesh.faces.push_back({{v00, v10, v11}});
            mesh.faces.push_back({{v00, v11, v01}});
        }
    }
    return fixture;
}

// ---------------------------------------------------------------------------
// 工具函数
// ---------------------------------------------------------------------------

Mesh withDeterministicNoise(const Mesh& mesh, double amplitude, std::uint64_t seed) {
    // Knuth MMIX LCG：使用完整的 64 位状态，并将最高 53 位映射到 [-1, 1]。
    std::uint64_t state = seed * 6364136223846793005ull + 1442695040888963407ull;
    auto nextSymmetric = [&state]() {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        const double unit = static_cast<double>(state >> 11) * (1.0 / 9007199254740992.0);
        return 2.0 * unit - 1.0;
    };
    Mesh noisy = mesh;
    for (Vec3& vertex : noisy.vertices) {
        vertex.x() += amplitude * nextSymmetric();
        vertex.y() += amplitude * nextSymmetric();
        vertex.z() += amplitude * nextSymmetric();
    }
    return noisy;
}

Mesh uniformlyScaled(const Mesh& mesh, double factor) {
    Mesh scaled = mesh;
    for (Vec3& vertex : scaled.vertices) {
        vertex *= factor;
    }
    return scaled;
}

double meanEdgeLength(const Mesh& mesh) {
    const std::vector<std::pair<int, int>> edges = uniqueEdges(mesh);
    if (edges.empty()) {
        return 0.0;
    }
    double total = 0.0;
    for (const auto& pairEntry : edges) {
        const int a = pairEntry.first;
        const int b = pairEntry.second;
        total += (mesh.vertices[a] - mesh.vertices[b]).norm();
    }
    return total / static_cast<double>(edges.size());
}

} // namespace analytic
} // namespace test
} // namespace manumesh
