#include "algorithms/feature_detection/FeatureDetector.h"

#include "common/detail/MeshQueries.h"
#include "detail/FeatureDetectionCache.h"
#include "detail/FeatureInputValidation.h"

#include <Eigen/Cholesky>
#include <Eigen/Eigenvalues>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace manumesh::feature {
namespace {

/// Minimum ratio of the interpolated cyclideness (mean |extremality| at the
/// two ends of a zero-crossing edge) to the squared dominant curvature for a
/// crossing to count as a curvature extremum. Both quantities are estimated
/// in the same radius-normalized units, so the ratio is dimensionless and
/// invariant under uniform mesh scaling. Measured landmarks (2026-07): true
/// crests (Gaussian ridge/bump fixtures) sit at >= 0.38 with medians well
/// above 1; the spurious valley band on a torus (a Dupin cyclide, where the
/// extremality vanishes identically) peaks at 0.06 across 24-48 minor
/// segments. 0.15 keeps a 2.5x margin to both populations.
constexpr double kMinCrossingCyclidenessRatio = 0.15;

struct NeighborhoodVertex {
    int id = -1;
    int depth = 0;
};

struct ScaleEstimate {
    bool valid = false;
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    std::array<Vec3, 2> directions = {Vec3(1.0, 0.0, 0.0), Vec3(0.0, 1.0, 0.0)};
    std::array<double, 2> curvatures = {0.0, 0.0};
    /// Extremality e_i = grad(kappa_i) . t_i from the cubic Monge terms, in
    /// the same radius-normalized units as the curvatures (M021 Yoshizawa).
    std::array<double, 2> extremalities = {0.0, 0.0};
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

/// Reusable per-call scratch buffers for the multiscale fitting hot path.
/// Earlier revisions allocated fresh vectors, queues, and Eigen matrices for
/// every vertex and robust iteration; one workspace per analysis removes that
/// churn without changing any numerical result.
struct FitWorkspace {
    std::vector<NeighborhoodVertex> neighborhood;
    std::vector<Eigen::Matrix<double, 1, 9>> rows;
    std::vector<double> targets;
    std::vector<double> spatialWeights;
    std::vector<double> robustWeights;
    std::vector<double> residuals;
    std::vector<double> medianScratch;
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

/// Breadth-first ring gathering into a reused buffer. The seed is stored at
/// index 0 with depth 0; consumers iterate from index 1.
void gatherNeighborhood(
    const std::vector<std::vector<int>>& neighbors,
    int seed,
    int maxDepth,
    std::vector<int>& visitStamp,
    int stamp,
    std::vector<NeighborhoodVertex>& result
) {
    result.clear();
    result.push_back({seed, 0});
    visitStamp[seed] = stamp;
    std::size_t head = 0;
    while (head < result.size()) {
        const NeighborhoodVertex current = result[head++];
        if (current.depth >= maxDepth) {
            continue;
        }
        for (int nb : neighbors[current.id]) {
            if (visitStamp[nb] == stamp) {
                continue;
            }
            visitStamp[nb] = stamp;
            result.push_back({nb, current.depth + 1});
        }
    }
}

double medianAbsolute(const std::vector<double>& values, std::vector<double>& scratch) {
    if (values.empty()) {
        return 0.0;
    }
    scratch.resize(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
        scratch[i] = std::abs(values[i]);
    }
    const std::size_t middle = scratch.size() / 2;
    std::nth_element(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(middle), scratch.end());
    double median = scratch[middle];
    if (scratch.size() % 2 == 0 && middle > 0) {
        const auto lower = std::max_element(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(middle));
        median = 0.5 * (median + *lower);
    }
    return median;
}

ScaleEstimate fitScale(
    const Mesh& mesh,
    const std::vector<Vec3>& vertexNormals,
    FitWorkspace& workspace,
    int seed,
    int ringCount,
    double averageEdgeLength,
    int robustIterations
) {
    ScaleEstimate result;
    if (averageEdgeLength <= 1e-20) {
        return result;
    }
    const std::vector<NeighborhoodVertex>& neighborhood = workspace.neighborhood;

    const double radius = std::max(averageEdgeLength * static_cast<double>(ringCount), 1e-12);
    Vec3 normal = vertexNormals[seed];
    Vec3 normalSum = normal;
    for (std::size_t k = 1; k < neighborhood.size(); ++k) {
        const NeighborhoodVertex& item = neighborhood[k];
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
    for (std::size_t k = 1; k < neighborhood.size(); ++k) {
        const NeighborhoodVertex& item = neighborhood[k];
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

    std::vector<Eigen::Matrix<double, 1, 9>>& rows = workspace.rows;
    std::vector<double>& targets = workspace.targets;
    std::vector<double>& spatialWeights = workspace.spatialWeights;
    rows.clear();
    targets.clear();
    spatialWeights.clear();
    for (std::size_t k = 1; k < neighborhood.size(); ++k) {
        const NeighborhoodVertex& item = neighborhood[k];
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
        // Cubic Monge basis (Yoshizawa M021): the quadratic block carries the
        // curvatures, the cubic block carries the curvature derivatives that
        // yield the extremality below.
        Eigen::Matrix<double, 1, 9> row;
        double* rowData = row.data();
        rowData[0] = u * u;
        rowData[1] = u * v;
        rowData[2] = v * v;
        rowData[3] = u;
        rowData[4] = v;
        rowData[5] = u * u * u;
        rowData[6] = u * u * v;
        rowData[7] = u * v * v;
        rowData[8] = v * v * v;
        rows.push_back(row);
        targets.push_back(w);
        spatialWeights.push_back(std::exp(-0.5 * r2));
    }
    // Underdetermination guard: the cubic fit has nine unknowns.
    if (rows.size() < 9) {
        return result;
    }

    const int rowCount = static_cast<int>(rows.size());
    using Vector9d = Eigen::Matrix<double, 9, 1>;
    using Matrix9d = Eigen::Matrix<double, 9, 9>;
    Vector9d coefficients = Vector9d::Zero();
    std::vector<double>& robustWeights = workspace.robustWeights;
    std::vector<double>& residuals = workspace.residuals;
    robustWeights.assign(rows.size(), 1.0);
    residuals.assign(rows.size(), 0.0);
    const int iterations = std::clamp(robustIterations, 0, kMaxSmoothCurvatureRobustFitIterations);
    for (int iteration = 0; iteration <= iterations; ++iteration) {
        // Weighted normal equations accumulated with raw scalar loops (only
        // the upper triangle): the coordinates are pre-normalized by the
        // neighborhood radius so the 9x9 system stays well conditioned, and
        // this replaces one dense QR per robust iteration.
        double ataUpper[45] = {0.0};
        double atbRaw[9] = {0.0};
        for (int i = 0; i < rowCount; ++i) {
            const double weight = std::max(1e-16, spatialWeights[i] * robustWeights[i]);
            const double* row = rows[i].data();
            const double weightedTarget = weight * targets[i];
            int upperIndex = 0;
            for (int c = 0; c < 9; ++c) {
                const double weightedEntry = weight * row[c];
                atbRaw[c] += weightedTarget * row[c];
                for (int d = c; d < 9; ++d) {
                    ataUpper[upperIndex++] += weightedEntry * row[d];
                }
            }
        }
        Matrix9d ata;
        Vector9d atb;
        int upperIndex = 0;
        for (int c = 0; c < 9; ++c) {
            atb(c) = atbRaw[c];
            for (int d = c; d < 9; ++d) {
                ata(c, d) = ataUpper[upperIndex];
                ata(d, c) = ataUpper[upperIndex];
                ++upperIndex;
            }
        }
        const Eigen::LDLT<Matrix9d> solver(ata);
        if (solver.info() != Eigen::Success) {
            return result;
        }
        // Degeneracy guard on the pivot spread (rcond-style): reject fits
        // whose normal matrix is numerically rank deficient.
        const Vector9d pivots = solver.vectorD().cwiseAbs();
        if (!(pivots.minCoeff() > 1e-13 * std::max(1.0, pivots.maxCoeff()))) {
            return result;
        }
        coefficients = solver.solve(atb);
        if (!coefficients.allFinite()) {
            return result;
        }
        for (int i = 0; i < rowCount; ++i) {
            const double* row = rows[i].data();
            double prediction = 0.0;
            for (int c = 0; c < 9; ++c) {
                prediction += row[c] * coefficients(c);
            }
            residuals[i] = prediction - targets[i];
        }
        if (iteration == iterations) {
            break;
        }
        const double sigma = std::max(1e-8, 1.4826 * medianAbsolute(residuals, workspace.medianScratch));
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
    const double cubic0 = coefficients(5);
    const double cubic1 = coefficients(6);
    const double cubic2 = coefficients(7);
    const double cubic3 = coefficients(8);
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
        // Extremality as the third-order directional form C(t, t, t)
        // (Yoshizawa Eq. e ~ c0 t1^3 + 3 c1 t1^2 t2 + 3 c2 t1 t2^2 + c3 t2^3,
        // here with h_uuu = 6 cubic0 etc. so the common factor 6 is kept for a
        // consistent normalized magnitude).
        const Eigen::Vector2d tangent2 = direction2.normalized();
        const double t1 = tangent2.x();
        const double t2 = tangent2.y();
        result.extremalities[i] =
            6.0 * (cubic0 * t1 * t1 * t1 + cubic1 * t1 * t1 * t2 + cubic2 * t1 * t2 * t2 + cubic3 * t2 * t2 * t2);
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

/// Ridge/valley dominance test (Ohtake M011): the ridge branch requires
/// kappa_max > |kappa_min|, the valley branch kappa_min < -|kappa_max|.
bool principalDominant(const ScaleEstimate& estimate, int principal) {
    const double curvature = estimate.curvatures[principal];
    const double other = estimate.curvatures[1 - principal];
    return principal == 1 ? curvature > std::abs(other) : curvature < -std::abs(other);
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

    // Edge zero-crossing extremality criterion (Ohtake M011 Eq.3-4, Yoshizawa
    // M021): a vertex supports a ridge (valley) when the extremality e changes
    // sign along an incident edge roughly following the principal direction,
    // with the first-order maximum test replacing the impractical second-order
    // derivative test. Principal directions form a line field, so the
    // neighbor's tangent and extremality are sign-synchronized before the
    // tests; both endpoints must satisfy the curvature dominance condition.
    for (int principal = 0; principal < 2; ++principal) {
        const double curvature = center.curvatures[principal];
        const double otherCurvature = center.curvatures[1 - principal];
        if (std::abs(curvature) <= 1e-10 || center.localScale <= 1e-20 || !principalDominant(center, principal)) {
            continue;
        }
        const int signedKind = principal == 1 ? 1 : -1;
        const Vec3 extremumDirection = center.directions[principal];
        const double centerExtremality = center.extremalities[principal];

        double extremum = 0.0;
        for (int nb : neighbors[vertex]) {
            const ScaleEstimate& neighbor = estimates[nb];
            if (!neighbor.valid || neighbor.localScale <= 1e-20 || !principalDominant(neighbor, principal)) {
                continue;
            }
            Vec3 neighborTangent = neighbor.directions[principal];
            double neighborExtremality = neighbor.extremalities[principal];
            if (neighborTangent.dot(extremumDirection) < 0.0) {
                neighborTangent = -neighborTangent;
                neighborExtremality = -neighborExtremality;
            }
            // Unit conversion (P1-3): each vertex fits in coordinates divided
            // by its own radius r = localScale, so with w' = w/r, u' = u/r the
            // fitted quantities relate to the physical ones as
            //   kappa_hat = d^2 w'/du'^2 = r * kappa_phys   (kappa ~ 1/L)
            //   e_hat     = d^3 w'/du'^3 = r^2 * e_phys     (e ~ 1/L^2).
            // Neighbors generally have a different r, so before any magnitude
            // comparison the neighbor's extremality is rescaled into the
            // center vertex's units: e_n->c = e_hat_n * (r_c / r_n)^2. The
            // ratio r_c / r_n is invariant under uniform mesh scaling, so all
            // downstream criteria stay exactly scale-invariant. Sign-only
            // tests (zero crossing, first-order maximum) are unaffected by
            // the positive factor.
            const double unitRatio = center.localScale / neighbor.localScale;
            neighborExtremality *= unitRatio * unitRatio;
            if (!(centerExtremality * neighborExtremality < 0.0)) {
                continue;
            }
            // Sub-vertex ownership (Ohtake inverse interpolation): the
            // crossing point lies nearer to the endpoint with the smaller |e|,
            // so only that vertex claims the candidate. This keeps the
            // detected band one vertex wide instead of two. Both magnitudes
            // are in the center vertex's units after the conversion above.
            if (std::abs(centerExtremality) > std::abs(neighborExtremality)) {
                continue;
            }
            const Vec3 delta = mesh.vertices[nb] - mesh.vertices[vertex];
            // First-order maximum test at both endpoints: for a ridge the
            // extremum must lie ahead of each vertex along its own tangent
            // (sign-flip invariant because e and t flip together).
            const double centerSide =
                static_cast<double>(signedKind) * centerExtremality * delta.dot(extremumDirection);
            const double neighborSide =
                static_cast<double>(signedKind) * neighborExtremality * (-delta).dot(neighborTangent);
            if (!(centerSide > 0.0) || !(neighborSide > 0.0)) {
                continue;
            }
            // Cyclideness gate (Yoshizawa M021 Eq.5-6): on a Dupin cyclide
            // (sphere/cylinder/cone/torus) the extremality field vanishes
            // identically, so a detected sign change there is discretization
            // noise, not a curvature extremum -- the whole extremal circle is
            // a stationary set of kappa and carries no crest. Near a genuine
            // crest |e| grows like |de/dt| * distance off the line, so the
            // mean endpoint magnitude of e measures the extremal significance
            // (it linearly interpolates Yoshizawa's cyclideness C at the
            // crossing). Dividing by kappa^2 makes the gate dimensionless:
            // in the center vertex's units e_hat = r_c^2 e_phys and
            // kappa_hat^2 = r_c^2 kappa_phys^2, so the ratio equals the
            // physical e / kappa^2 with no residual dependence on r_c (the
            // neighbor extremality was converted into center units above),
            // and it is exactly invariant under uniform scaling: it asks
            // whether kappa varies by at least a fixed fraction of itself
            // over one curvature radius along its own line of curvature --
            // zero on cyclides, O(1) on true crests.
            const double crossingCyclideness =
                0.5 * (std::abs(centerExtremality) + std::abs(neighborExtremality));
            if (crossingCyclideness < kMinCrossingCyclidenessRatio * curvature * curvature) {
                continue;
            }
            // Curvature-difference proxy in radius-normalized units: mean |e|
            // times the tangential edge extent, comparable to the previous
            // center-minus-neighbors magnitude so score scales are preserved.
            const double tangentialExtent = std::abs(delta.dot(extremumDirection)) / center.localScale;
            const double crossingStrength =
                0.5 * (std::abs(centerExtremality) + std::abs(neighborExtremality)) * tangentialExtent;
            extremum = std::max(extremum, crossingStrength);
        }
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

namespace detector_detail {

const std::vector<Vec3>& FeatureDetectionCache::areaWeightedVertexNormals() {
    if (!hasAreaWeightedVertexNormals_) {
        areaWeightedVertexNormals_ = computeAreaWeightedVertexNormals(*mesh_);
        hasAreaWeightedVertexNormals_ = true;
    }
    return areaWeightedVertexNormals_;
}

std::vector<SmoothCurvatureVertex> computeSmoothCurvatureFeaturesCached(
    const Mesh& mesh,
    FeatureDetectionCache& cache,
    const SmoothCurvatureOptions& options,
    double requestedPersistenceThreshold
) {
    detector_detail::validateFeatureMeshInput(mesh);
    std::vector<SmoothCurvatureVertex> result(mesh.vertices.size());
    if (mesh.empty()) {
        return result;
    }

    const int baseRings = std::clamp(options.baseNeighborhoodRings, 1, kMaxSmoothCurvatureBaseNeighborhoodRings);
    const int scaleCount = std::clamp(options.scaleCount, 1, kMaxSmoothCurvatureScaleCount);
    const int maxRings = baseRings + scaleCount - 1;
    const int robustIterations = std::clamp(options.robustFitIterations, 0, kMaxSmoothCurvatureRobustFitIterations);
    const double tangentConsistency = std::clamp(options.minTangentConsistency, 0.0, 1.0);
    const double persistenceThreshold =
        std::isfinite(requestedPersistenceThreshold) ? std::max(1e-12, requestedPersistenceThreshold) : 1e-12;

    const std::vector<std::vector<int>>& neighbors = cache.vertexNeighbors();
    const std::vector<double>& averageEdgeLength = cache.vertexAverageEdgeLength();
    const std::vector<Vec3>& vertexNormals = cache.areaWeightedVertexNormals();
    std::vector<std::vector<ScaleEstimate>> estimates(scaleCount, std::vector<ScaleEstimate>(mesh.vertices.size()));
    std::vector<int> visitStamp(mesh.vertices.size(), 0);
    FitWorkspace workspace;
    int stamp = 0;
    for (int vertex = 0; vertex < static_cast<int>(mesh.vertices.size()); ++vertex) {
        if (++stamp == std::numeric_limits<int>::max()) {
            std::fill(visitStamp.begin(), visitStamp.end(), 0);
            stamp = 1;
        }
        gatherNeighborhood(neighbors, vertex, maxRings, visitStamp, stamp, workspace.neighborhood);
        for (int scale = 0; scale < scaleCount; ++scale) {
            const int ringCount = baseRings + scale;
            estimates[scale][vertex] = fitScale(
                mesh, vertexNormals, workspace, vertex, ringCount, averageEdgeLength[vertex], robustIterations
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
        // Pure vote counting (Luo-Zha M009): persistence is the number of
        // supporting scales, thresholded downstream against
        // smoothCurvatureMinPersistentScales. An earlier revision additionally
        // vetoed any candidate the *coarsest* scale did not support, which
        // silenced real features whose spatial extent is smaller than the
        // coarsest neighborhood radius (baseRings + scaleCount - 1 rings) on
        // dense meshes -- exactly the small fillets and short ridges the
        // multiscale channel exists to find.
        if (output.persistentScales > 0) {
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

} // namespace detector_detail

std::vector<SmoothCurvatureVertex> computeSmoothCurvatureFeatures(
    const Mesh& mesh, const SmoothCurvatureOptions& options, double requestedPersistenceThreshold
) {
    detector_detail::FeatureDetectionCache cache(mesh);
    return detector_detail::computeSmoothCurvatureFeaturesCached(mesh, cache, options, requestedPersistenceThreshold);
}

std::vector<SmoothCurvatureVertex>
computeSmoothCurvatureFeatures(const Mesh& mesh, const SmoothCurvatureOptions& options) {
    return computeSmoothCurvatureFeatures(mesh, options, 0.0);
}

} // namespace manumesh::feature
