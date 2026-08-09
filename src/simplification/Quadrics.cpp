/**
 * @file src/simplification/Quadrics.cpp
 * @brief 实现 ManuMesh 的简化模块的二次误差功能。
 * @ingroup manumesh_simplification
 *
 * @details 构建平面、直线、边界和特征曲线的二次误差项。
 * @algorithm 每个有效面贡献一个按面积加权的齐次平面外积。边界和特征约束增加垂直平面或点到直线的二次误差项；按顶点求和后，边收缩代价具有可加性。
 * @invariants 除浮点舍入误差外，每个二次误差矩阵都保持对称半正定。
 * @failuremodes 退化面使用有界的点回退项，不提供不稳定的平面项。
 */

#include "detail/Quadrics.h"

#include "common/detail/MeshQueries.h"

#include <Eigen/Eigenvalues>
#include <algorithm>
#include <cmath>
#include <limits>

namespace manumesh::simplification {

double evaluateQuadric(const Mat4& q, const Vec3& p) {
    Eigen::Vector4d h;
    h << p.x(), p.y(), p.z(), 1.0;
    return h.dot(q * h);
}

Mat4 planeQuadric(const Vec3& normal, const Vec3& point) {
    Eigen::Vector4d plane;
    plane << normal.x(), normal.y(), normal.z(), -normal.dot(point);
    return plane * plane.transpose();
}

Mat4 pointQuadric(const Vec3& point) {
    Mat4 q = Mat4::Zero();
    q.block<3, 3>(0, 0).setIdentity();
    q.block<3, 1>(0, 3) = -point;
    q.block<1, 3>(3, 0) = -point.transpose();
    q(3, 3) = point.squaredNorm();
    return q;
}

Mat4 lineQuadric(const Vec3& point, const Vec3& normal) {
    Vec3 n = normal;
    const double nlen = n.norm();
    if (nlen <= 1e-20) {
        return pointQuadric(point);
    }
    n /= nlen;

    Vec3 seed = std::abs(n.x()) < 0.9 ? Vec3(1.0, 0.0, 0.0) : Vec3(0.0, 1.0, 0.0);
    Vec3 x = seed - n * n.dot(seed);
    const double xlen = x.norm();
    if (xlen <= 1e-20) {
        seed = Vec3(0.0, 0.0, 1.0);
        x = seed - n * n.dot(seed);
    }
    x.normalize();
    Vec3 y = n.cross(x).normalized();

    return planeQuadric(x, point) + planeQuadric(y, point);
}

void addBoundaryQuadrics(const Mesh& mesh, double boundaryWeight, std::vector<Mat4>& quadrics) {
    if (boundaryWeight <= 0.0) {
        return;
    }
    const std::vector<Vec3> faceNormals = common::computeFaceNormals(mesh);

    const common::MeshEdgeInfoMap edgeInfo = common::buildMeshEdgeInfo(mesh);
    for (const auto& [key, info] : edgeInfo) {
        if (info.faces.size() != 1) {
            continue;
        }
        const auto [a, b] = common::unpackMeshEdgeKey(key);
        const Vec3 edge = mesh.vertices[b] - mesh.vertices[a];
        if (edge.norm() <= 1e-20) {
            continue;
        }
        Vec3 n = faceNormals[info.faces.front()].cross(edge.normalized());
        if (n.norm() <= 1e-20) {
            continue;
        }
        n.normalize();
        // edge.squaredNorm() 使边界二次误差项在长度维度上与按面积加权的面二次误差项同阶，因此网格统一缩放时软边界约束不会漂移。
        const Mat4 q = boundaryWeight * edge.squaredNorm() * planeQuadric(n, mesh.vertices[a]);
        quadrics[a] += q;
        quadrics[b] += q;
    }
}

void computeInitialQuadrics(
    const Mesh& mesh,
    const SimplifyOptions& options,
    const FeatureGuidance& featureGuidance,
    InitialQuadrics& initial,
    SimplifyReport& report
) {
    std::vector<Mat4>& quadrics = initial.quadrics;
    quadrics.assign(mesh.vertices.size(), Mat4::Zero());
    initial.priorityScales.clear();
    std::vector<double> vertexArea(mesh.vertices.size(), 0.0);
    std::vector<Vec3> normalSum(mesh.vertices.size(), Vec3::Zero());
    std::vector<int> degenerateFaceIncidence(mesh.vertices.size(), 0);

    for (const Face& face : mesh.faces) {
        const Vec3& a = mesh.vertices[face.v[0]];
        const Vec3& b = mesh.vertices[face.v[1]];
        const Vec3& c = mesh.vertices[face.v[2]];
        const Vec3 n = triangleNormal(a, b, c);
        const double area = triangleArea(a, b, c);
        if (area <= 1e-24 || n.norm() <= 1e-20) {
            for (int id : face.v) {
                ++degenerateFaceIncidence[id];
            }
            continue;
        }

        const Mat4 q = planeQuadric(n, a);
        for (int id : face.v) {
            const double baryArea = area / 3.0;
            vertexArea[id] += baryArea;
            normalSum[id] += area * n;
            quadrics[id] += baryArea * q;
        }
    }

    double positiveAreaSum = 0.0;
    int positiveAreaCount = 0;
    for (double area : vertexArea) {
        if (area > 1e-24) {
            positiveAreaSum += area;
            ++positiveAreaCount;
        }
    }
    const double diagonal = mesh.bboxDiag();
    const double representativeArea = positiveAreaCount > 0 ? positiveAreaSum / static_cast<double>(positiveAreaCount)
                                                            : std::max(1e-300, diagonal * diagonal * 1e-6);
    for (int i = 0; i < static_cast<int>(quadrics.size()); ++i) {
        if (degenerateFaceIncidence[i] > 0) {
            quadrics[i] += static_cast<double>(degenerateFaceIncidence[i]) * 1e-6 * representativeArea *
                           pointQuadric(mesh.vertices[i]);
        }
    }

    addBoundaryQuadrics(mesh, options.boundaryWeight, quadrics);

    report.minAppliedLineWeight = std::numeric_limits<double>::infinity();
    report.maxAppliedLineWeight = 0.0;
    const bool useNormalLineQuadrics = options.useLineQuadrics && options.lineWeight > 0.0;
    if (!useNormalLineQuadrics) {
        report.minAppliedLineWeight = 0.0;
    }

    const FeatureWeightScores featureScores =
        useNormalLineQuadrics ? computeFeatureWeightScores(mesh, options) : FeatureWeightScores{};
    if (featureScores.normalTensorScoredVertices > 0) {
        report.normalTensorScoredVertices =
            std::max(report.normalTensorScoredVertices, featureScores.normalTensorScoredVertices);
        report.maxNormalTensorPersistentScore =
            std::max(report.maxNormalTensorPersistentScore, featureScores.maxNormalTensorPersistentScore);
        report.meanNormalTensorLocalScale = featureScores.meanNormalTensorLocalScale;
        report.meanNormalTensorPersistence = featureScores.meanNormalTensorPersistence;
    }

    if (useNormalLineQuadrics) {
        if (options.adaptiveScale) {
            initial.priorityScales.assign(mesh.vertices.size(), 1.0);
        }
        for (int i = 0; i < static_cast<int>(mesh.vertices.size()); ++i) {
            Vec3 normal = normalSum[i];
            if (normal.norm() <= 1e-20 || vertexArea[i] <= 1e-24) {
                quadrics[i] += 1e-6 * representativeArea * pointQuadric(mesh.vertices[i]);
                continue;
            }
            normal.normalize();

            const Mat4 ql = lineQuadric(mesh.vertices[i], normal);
            double appliedWeight = options.lineWeight;
            if (options.weightMode != WeightMode::Uniform) {
                appliedWeight += options.featureBoost * featureScores.values[i];
            }

            if (options.adaptiveScale) {
                // Wang 2008 解耦：特征增益不再乘到整个二次误差矩阵上（否则会扭曲放置并放大边界项），而是变为每个顶点的队列优先级因子；二次误差矩阵只保留干净的基础直线项。
                quadrics[i] += options.adaptiveBaseLineWeight * vertexArea[i] * ql;
                initial.priorityScales[i] = 1.0 + std::max(0.0, options.featureBoost) * featureScores.values[i];
                appliedWeight = options.adaptiveBaseLineWeight;
            } else {
                quadrics[i] += appliedWeight * vertexArea[i] * ql;
            }
            report.minAppliedLineWeight = std::min(report.minAppliedLineWeight, appliedWeight);
            report.maxAppliedLineWeight = std::max(report.maxAppliedLineWeight, appliedWeight);
        }
    }

    if (options.preserveFeatureCurves && featureGuidance.enabled && options.featureCurveWeight > 0.0) {
        for (int i = 0; i < static_cast<int>(mesh.vertices.size()); ++i) {
            if (i >= static_cast<int>(featureGuidance.vertices.size())) {
                continue;
            }
            const FeatureVertexGuidance& vf = featureGuidance.vertices[i];
            if (!vf.isFeature || vf.tangent.norm() <= 1e-20) {
                continue;
            }
            const double areaScale = std::max(vertexArea[i], representativeArea);
            const Mat4 qCurve = lineQuadric(mesh.vertices[i], vf.tangent);
            const double confidenceScale = 0.35 + 0.65 * std::clamp(vf.confidence, 0.0, 1.0);
            quadrics[i] += options.featureCurveWeight * confidenceScale * areaScale * qCurve;
        }
    }

    if (!std::isfinite(report.minAppliedLineWeight)) {
        report.minAppliedLineWeight = 0.0;
    }
}

std::vector<SolveResult> solvePlacementCandidates(const Mat4& q, const Vec3& a, const Vec3& b) {
    std::vector<Vec3> candidates;
    candidates.reserve(4);
    candidates.push_back(a);
    candidates.push_back(b);
    candidates.push_back(0.5 * (a + b));

    const Eigen::Matrix3d A = q.block<3, 3>(0, 0);
    const Eigen::Vector3d rhs = -q.block<3, 1>(0, 3);
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eig(A);
    bool solved = false;
    if (eig.info() == Eigen::Success) {
        const double maxEval = eig.eigenvalues().cwiseAbs().maxCoeff();
        const double minEval = eig.eigenvalues().cwiseAbs().minCoeff();
        if (maxEval > 1e-20 && minEval / maxEval > 1e-12) {
            const Vec3 x = A.ldlt().solve(rhs);
            if (x.allFinite()) {
                candidates.push_back(x);
                solved = true;
            }
        }
        if (!solved && maxEval > 1e-20) {
            // GH97 回退级别 2：沿线段 ab 求一维最优解。令 h(t) = a + t (b - a)，则 f(t) = h^T Q h 是标量二次函数，其极小点 t* = (rhs.d - d^T A a) / (d^T A d)。A 半正定，所以分母 >= 0；与 maxEval * |d|^2 同维度的相对阈值使除法具有尺度不变性。对于秩为 2 的二次误差矩阵（直折痕、边界折叠），这是在上面的满秩求解被拒绝后仍然良态的情形。
            const Vec3 d = b - a;
            const double denom = d.dot(A * d);
            if (denom > 1e-12 * maxEval * d.squaredNorm()) {
                const double t = std::clamp((rhs.dot(d) - d.dot(A * a)) / denom, 0.0, 1.0);
                const Vec3 alongEdge = a + t * d;
                if (alongEdge.allFinite()) {
                    candidates.push_back(alongEdge);
                }
            }
        }
    }

    std::vector<SolveResult> results;
    results.reserve(candidates.size());
    for (const Vec3& p : candidates) {
        const double cost = evaluateQuadric(q, p);
        if (!std::isfinite(cost)) {
            continue;
        }
        bool duplicate = false;
        for (const SolveResult& existing : results) {
            if ((existing.position - p).squaredNorm() <= 1e-24) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            results.push_back(SolveResult{p, cost, !solved});
        }
    }
    if (results.empty()) {
        // 所有候选代价都不是有限值，说明合并后的二次误差矩阵已损坏。将该边排在最后，而不是让 0.0 代价把错误折叠推到队列前端。
        results.push_back(SolveResult{0.5 * (a + b), std::numeric_limits<double>::max(), true});
    }
    std::stable_sort(results.begin(), results.end(), [](const SolveResult& lhs, const SolveResult& rhs) {
        return lhs.cost < rhs.cost;
    });
    return results;
}

SolveResult solveOptimal(const Mat4& q, const Vec3& a, const Vec3& b) {
    return solvePlacementCandidates(q, a, b).front();
}

InitialQuadricBuilder::InitialQuadricBuilder(const SimplifyOptions& options)
    : options_(options) {}

InitialQuadrics
InitialQuadricBuilder::build(const Mesh& mesh, const FeatureGuidance& featureGuidance, SimplifyReport& report) const {
    InitialQuadrics initial;
    computeInitialQuadrics(mesh, options_, featureGuidance, initial, report);
    return initial;
}

} // 结束 manumesh::simplification 命名空间
