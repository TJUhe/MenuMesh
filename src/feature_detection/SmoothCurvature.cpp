#include "algorithms/feature_detection/FeatureDetector.h"

#include "common/detail/MeshQueries.h"

#include <Eigen/Eigenvalues>
#include <Eigen/QR>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

namespace manumesh::feature {
namespace {

struct NeighborhoodVertex {
    int id = -1;
    int depth = 0;
};

struct ScaleEstimate {
    bool valid = false;
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    std::array<Vec3, 2> directions = {Vec3(1.0, 0.0, 0.0), Vec3(0.0, 1.0, 0.0)};
    std::array<double, 2> curvatures = {0.0, 0.0};
    double fitResidual = 0.0;
    double localScale = 0.0;
};

struct ScaleCandidate {
    bool valid = false;
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    Vec3 curveTangent = Vec3(1.0, 0.0, 0.0);
    Vec3 extremumDirection = Vec3(0.0, 1.0, 0.0);
    double principalCurvature = 0.0;
    double secondaryCurvature = 0.0;
    double anisotropy = 0.0;
    double extremumStrength = 0.0;
    double score = 0.0;
    double fitResidual = 0.0;
    double localScale = 0.0;
    int signedKind = 0;
};

std::vector<Vec3> computeAreaWeightedVertexNormals(const Mesh& mesh) {
    std::vector<Vec3> normals(mesh.vertices.size(), Vec3::Zero());
    for (const Face& face : mesh.faces) {
        const Vec3 cross = (mesh.vertices[face.v[1]] - mesh.vertices[face.v[0]])
                               .cross(mesh.vertices[face.v[2]] - mesh.vertices[face.v[0]]);
        if (cross.squaredNorm() <= 1e-30) {
            continue;
        }
        for (int id : face.v) {
            normals[id] += cross;
        }
    }
    for (Vec3& normal : normals) {
        if (normal.norm() > 1e-20) {
            normal.normalize();
        } else {
            normal = Vec3(0.0, 0.0, 1.0);
        }
    }
    return normals;
}

std::vector<NeighborhoodVertex> gatherNeighborhood(
    const std::vector<std::vector<int>>& neighbors, int seed, int maxDepth, std::vector<int>& visitStamp, int stamp
) {
    std::vector<NeighborhoodVertex> result;
    std::queue<NeighborhoodVertex> queue;
    queue.push({seed, 0});
    visitStamp[seed] = stamp;
    while (!queue.empty()) {
        const NeighborhoodVertex current = queue.front();
        queue.pop();
        if (current.depth > 0) {
            result.push_back(current);
        }
        if (current.depth >= maxDepth) {
            continue;
        }
        for (int nb : neighbors[current.id]) {
            if (visitStamp[nb] == stamp) {
                continue;
            }
            visitStamp[nb] = stamp;
            queue.push({nb, current.depth + 1});
        }
    }
    return result;
}

double medianAbsolute(std::vector<double> values) {
    if (values.empty()) {
        return 0.0;
    }
    for (double& value : values) {
        value = std::abs(value);
    }
    const std::size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle), values.end());
    double median = values[middle];
    if (values.size() % 2 == 0 && middle > 0) {
        const auto lower = std::max_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle));
        median = 0.5 * (median + *lower);
    }
    return median;
}

ScaleEstimate fitScale(
    const Mesh& mesh,
    const std::vector<Vec3>& vertexNormals,
    const std::vector<NeighborhoodVertex>& neighborhood,
    int seed,
    int ringCount,
    double averageEdgeLength,
    int robustIterations
) {
    ScaleEstimate result;
    if (averageEdgeLength <= 1e-20) {
        return result;
    }

    const double radius = std::max(averageEdgeLength * static_cast<double>(ringCount), 1e-12);
    Vec3 normal = vertexNormals[seed];
    Vec3 normalSum = normal;
    for (const NeighborhoodVertex& item : neighborhood) {
        if (item.depth > ringCount) {
            continue;
        }
        Vec3 candidate = vertexNormals[item.id];
        if (candidate.dot(normal) < 0.0) {
            candidate = -candidate;
        }
        const double distance = (mesh.vertices[item.id] - mesh.vertices[seed]).norm();
        const double normalizedDistance = distance / radius;
        normalSum += std::exp(-0.5 * normalizedDistance * normalizedDistance) * candidate;
    }
    if (normalSum.norm() > 1e-20) {
        normal = normalSum.normalized();
    }

    Vec3 axisU = Vec3::Zero();
    double longestProjection = 0.0;
    for (const NeighborhoodVertex& item : neighborhood) {
        if (item.depth > ringCount) {
            continue;
        }
        Vec3 projected = mesh.vertices[item.id] - mesh.vertices[seed];
        projected -= normal * projected.dot(normal);
        if (projected.squaredNorm() > longestProjection) {
            longestProjection = projected.squaredNorm();
            axisU = projected;
        }
    }
    if (axisU.norm() <= 1e-20) {
        axisU = std::abs(normal.x()) < 0.8 ? Vec3(1.0, 0.0, 0.0) : Vec3(0.0, 1.0, 0.0);
        axisU -= normal * axisU.dot(normal);
    }
    if (axisU.norm() <= 1e-20) {
        return result;
    }
    axisU.normalize();
    const Vec3 axisV = normal.cross(axisU).normalized();

    std::vector<Eigen::Matrix<double, 1, 5>> rows;
    std::vector<double> targets;
    std::vector<double> spatialWeights;
    rows.reserve(neighborhood.size());
    targets.reserve(neighborhood.size());
    spatialWeights.reserve(neighborhood.size());
    for (const NeighborhoodVertex& item : neighborhood) {
        if (item.depth > ringCount) {
            continue;
        }
        const Vec3 delta = mesh.vertices[item.id] - mesh.vertices[seed];
        const double u = delta.dot(axisU) / radius;
        const double v = delta.dot(axisV) / radius;
        const double w = delta.dot(normal) / radius;
        const double r2 = u * u + v * v;
        if (!std::isfinite(r2) || r2 <= 1e-20) {
            continue;
        }
        Eigen::Matrix<double, 1, 5> row;
        row << u * u, u * v, v * v, u, v;
        rows.push_back(row);
        targets.push_back(w);
        spatialWeights.push_back(std::exp(-0.5 * r2));
    }
    if (rows.size() < 5) {
        return result;
    }

    Eigen::VectorXd coefficients(5);
    std::vector<double> robustWeights(rows.size(), 1.0);
    std::vector<double> residuals(rows.size(), 0.0);
    const int iterations = std::clamp(robustIterations, 0, 4);
    for (int iteration = 0; iteration <= iterations; ++iteration) {
        Eigen::MatrixXd weightedRows(rows.size(), 5);
        Eigen::VectorXd weightedTargets(rows.size());
        for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
            const double weight = std::sqrt(std::max(1e-16, spatialWeights[i] * robustWeights[i]));
            weightedRows.row(i) = weight * rows[i];
            weightedTargets(i) = weight * targets[i];
        }
        Eigen::ColPivHouseholderQR<Eigen::MatrixXd> solver(weightedRows);
        if (solver.rank() < 5) {
            return result;
        }
        coefficients = solver.solve(weightedTargets);
        if (!coefficients.allFinite()) {
            return result;
        }
        for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
            residuals[i] = rows[i].dot(coefficients) - targets[i];
        }
        if (iteration == iterations) {
            break;
        }
        const double sigma = std::max(1e-8, 1.4826 * medianAbsolute(residuals));
        const double cutoff = 1.5 * sigma;
        for (int i = 0; i < static_cast<int>(residuals.size()); ++i) {
            const double magnitude = std::abs(residuals[i]);
            robustWeights[i] = magnitude <= cutoff ? 1.0 : cutoff / magnitude;
        }
    }

    const double a = coefficients(0);
    const double b = coefficients(1);
    const double c = coefficients(2);
    const double d = coefficients(3);
    const double e = coefficients(4);
    Eigen::Matrix2d firstForm;
    firstForm << 1.0 + d * d, d * e, d * e, 1.0 + e * e;
    const double normalZ = 1.0 / std::sqrt(1.0 + d * d + e * e);
    Eigen::Matrix2d secondForm;
    secondForm << 2.0 * a * normalZ, b * normalZ, b * normalZ, 2.0 * c * normalZ;
    Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::Matrix2d> eig(secondForm, firstForm);
    if (eig.info() != Eigen::Success || !eig.eigenvalues().allFinite() || !eig.eigenvectors().allFinite()) {
        return result;
    }

    result.normal = (-d * axisU - e * axisV + normal).normalized();
    for (int i = 0; i < 2; ++i) {
        const Eigen::Vector2d direction2 = eig.eigenvectors().col(i);
        Vec3 direction =
            direction2.x() * axisU + direction2.y() * axisV + (d * direction2.x() + e * direction2.y()) * normal;
        direction -= result.normal * direction.dot(result.normal);
        if (direction.norm() <= 1e-20) {
            return ScaleEstimate{};
        }
        result.directions[i] = direction.normalized();
        result.curvatures[i] = eig.eigenvalues()(i);
    }

    double weightedError = 0.0;
    double weightSum = 0.0;
    for (int i = 0; i < static_cast<int>(residuals.size()); ++i) {
        weightedError += spatialWeights[i] * residuals[i] * residuals[i];
        weightSum += spatialWeights[i];
    }
    result.fitResidual = weightSum > 0.0 ? std::sqrt(weightedError / weightSum) : 0.0;
    result.localScale = radius;
    result.valid = true;
    return result;
}

double normalCurvature(const ScaleEstimate& estimate, const Vec3& direction) {
    Vec3 tangent = direction - estimate.normal * direction.dot(estimate.normal);
    if (tangent.norm() <= 1e-20) {
        return 0.0;
    }
    tangent.normalize();
    const double w0 = tangent.dot(estimate.directions[0]);
    const double w1 = tangent.dot(estimate.directions[1]);
    return estimate.curvatures[0] * w0 * w0 + estimate.curvatures[1] * w1 * w1;
}

ScaleCandidate classifyScaleCandidate(
    const Mesh& mesh,
    const std::vector<std::vector<int>>& neighbors,
    const std::vector<ScaleEstimate>& estimates,
    int vertex
) {
    ScaleCandidate best;
    const ScaleEstimate& center = estimates[vertex];
    if (!center.valid) {
        return best;
    }

    for (int principal = 0; principal < 2; ++principal) {
        const double curvature = center.curvatures[principal];
        const double otherCurvature = center.curvatures[1 - principal];
        if (std::abs(curvature) <= 1e-10) {
            continue;
        }
        const Vec3 extremumDirection = center.directions[principal];
        double positiveSum = 0.0;
        double negativeSum = 0.0;
        double positiveWeight = 0.0;
        double negativeWeight = 0.0;
        for (int nb : neighbors[vertex]) {
            if (!estimates[nb].valid) {
                continue;
            }
            Vec3 edge = mesh.vertices[nb] - mesh.vertices[vertex];
            edge -= center.normal * edge.dot(center.normal);
            if (edge.norm() <= 1e-20) {
                continue;
            }
            edge.normalize();
            const double alignment = edge.dot(extremumDirection);
            if (std::abs(alignment) < 0.2) {
                continue;
            }
            const double weight = alignment * alignment;
            const double neighborCurvature = normalCurvature(estimates[nb], extremumDirection);
            if (alignment > 0.0) {
                positiveSum += weight * neighborCurvature;
                positiveWeight += weight;
            } else {
                negativeSum += weight * neighborCurvature;
                negativeWeight += weight;
            }
        }
        if (positiveWeight <= 1e-12 || negativeWeight <= 1e-12) {
            continue;
        }

        const double neighborAverage = 0.5 * (positiveSum / positiveWeight + negativeSum / negativeWeight);
        const int signedKind = curvature >= 0.0 ? 1 : -1;
        const double extremum = static_cast<double>(signedKind) * (curvature - neighborAverage);
        if (extremum <= 0.0) {
            continue;
        }
        const double anisotropy = std::clamp(
            std::abs(curvature - otherCurvature) / (std::abs(curvature) + std::abs(otherCurvature) + 0.025), 0.0, 1.0
        );
        const double extremumRatio = std::clamp(extremum / (std::abs(curvature) + 0.025), 0.0, 1.0);
        const double fitQuality = 1.0 / (1.0 + 25.0 * center.fitResidual);
        const double score = std::min(std::abs(curvature), 4.0) * std::sqrt(anisotropy * extremumRatio) * fitQuality;
        if (!std::isfinite(score) || score <= best.score) {
            continue;
        }

        best.valid = true;
        best.normal = center.normal;
        best.extremumDirection = extremumDirection;
        best.curveTangent = center.directions[1 - principal];
        best.principalCurvature = curvature;
        best.secondaryCurvature = otherCurvature;
        best.anisotropy = anisotropy;
        best.extremumStrength = extremum;
        best.score = score;
        best.fitResidual = center.fitResidual;
        best.localScale = center.localScale;
        best.signedKind = signedKind;
    }
    return best;
}

} // namespace

std::vector<SmoothCurvatureVertex> computeSmoothCurvatureFeatures(
    const Mesh& mesh, const SmoothCurvatureOptions& options, double requestedPersistenceThreshold
) {
    std::vector<SmoothCurvatureVertex> result(mesh.vertices.size());
    if (mesh.empty()) {
        return result;
    }

    const int baseRings = std::clamp(options.baseNeighborhoodRings, 1, 4);
    const int scaleCount = std::clamp(options.scaleCount, 1, 6);
    const int maxRings = baseRings + scaleCount - 1;
    const int robustIterations = std::clamp(options.robustFitIterations, 0, 4);
    const double tangentConsistency = std::clamp(options.minTangentConsistency, 0.0, 1.0);
    const double persistenceThreshold =
        std::isfinite(requestedPersistenceThreshold) ? std::max(1e-12, requestedPersistenceThreshold) : 1e-12;

    const std::vector<std::vector<int>> neighbors = manumesh::detail::buildVertexNeighbors(mesh);
    const std::vector<double> averageEdgeLength = manumesh::detail::computeVertexAverageEdgeLength(mesh);
    const std::vector<Vec3> vertexNormals = computeAreaWeightedVertexNormals(mesh);
    std::vector<std::vector<ScaleEstimate>> estimates(scaleCount, std::vector<ScaleEstimate>(mesh.vertices.size()));
    std::vector<int> visitStamp(mesh.vertices.size(), 0);
    int stamp = 0;
    for (int vertex = 0; vertex < static_cast<int>(mesh.vertices.size()); ++vertex) {
        if (++stamp == std::numeric_limits<int>::max()) {
            std::fill(visitStamp.begin(), visitStamp.end(), 0);
            stamp = 1;
        }
        const std::vector<NeighborhoodVertex> neighborhood =
            gatherNeighborhood(neighbors, vertex, maxRings, visitStamp, stamp);
        for (int scale = 0; scale < scaleCount; ++scale) {
            const int ringCount = baseRings + scale;
            estimates[scale][vertex] = fitScale(
                mesh, vertexNormals, neighborhood, vertex, ringCount, averageEdgeLength[vertex], robustIterations
            );
        }
    }

    std::vector<std::vector<ScaleCandidate>> candidates(scaleCount, std::vector<ScaleCandidate>(mesh.vertices.size()));
    for (int scale = 0; scale < scaleCount; ++scale) {
        for (int vertex = 0; vertex < static_cast<int>(mesh.vertices.size()); ++vertex) {
            candidates[scale][vertex] = classifyScaleCandidate(mesh, neighbors, estimates[scale], vertex);
        }
    }

    for (int vertex = 0; vertex < static_cast<int>(mesh.vertices.size()); ++vertex) {
        int bestScale = -1;
        for (int scale = 0; scale < scaleCount; ++scale) {
            if (candidates[scale][vertex].valid &&
                (bestScale < 0 || candidates[scale][vertex].score > candidates[bestScale][vertex].score)) {
                bestScale = scale;
            }
        }
        SmoothCurvatureVertex& output = result[vertex];
        output.normal = vertexNormals[vertex];
        if (bestScale < 0) {
            continue;
        }

        const ScaleCandidate& best = candidates[bestScale][vertex];
        const double relativePersistenceThreshold = 0.30 * best.score;
        double supportedScoreSum = 0.0;
        double supportedAlignmentSum = 0.0;
        bool coarsestScaleSupported = false;
        for (int scale = 0; scale < scaleCount; ++scale) {
            const ScaleCandidate& candidate = candidates[scale][vertex];
            if (!candidate.valid || candidate.score < std::max(persistenceThreshold, relativePersistenceThreshold) ||
                candidate.signedKind != best.signedKind) {
                continue;
            }
            const double alignment = std::abs(candidate.curveTangent.dot(best.curveTangent));
            if (alignment < tangentConsistency) {
                continue;
            }
            ++output.persistentScales;
            supportedScoreSum += candidate.score;
            supportedAlignmentSum += alignment;
            coarsestScaleSupported = coarsestScaleSupported || scale + 1 == scaleCount;
        }

        output.normal = best.normal;
        output.curveTangent = best.curveTangent;
        output.extremumDirection = best.extremumDirection;
        output.principalCurvature = best.principalCurvature;
        output.secondaryCurvature = best.secondaryCurvature;
        output.anisotropy = best.anisotropy;
        output.extremumStrength = best.extremumStrength;
        output.featureScore = best.score;
        output.averageFeatureScore = supportedScoreSum / static_cast<double>(scaleCount);
        output.fitResidual = best.fitResidual;
        output.localScale = best.localScale;
        output.signedKind = best.signedKind;
        if (!coarsestScaleSupported) {
            output.persistentScales = 0;
            output.averageFeatureScore = 0.0;
        } else if (output.persistentScales > 0) {
            const double persistenceRatio =
                static_cast<double>(output.persistentScales) / static_cast<double>(scaleCount);
            const double meanAlignment = supportedAlignmentSum / static_cast<double>(output.persistentScales);
            const double meanSupportedScore = supportedScoreSum / static_cast<double>(output.persistentScales);
            output.persistentFeatureScore =
                (0.65 * output.featureScore + 0.35 * meanSupportedScore) * persistenceRatio * meanAlignment;
        }
    }
    return result;
}

std::vector<SmoothCurvatureVertex>
computeSmoothCurvatureFeatures(const Mesh& mesh, const SmoothCurvatureOptions& options) {
    return computeSmoothCurvatureFeatures(mesh, options, 0.0);
}

} // namespace manumesh::feature
