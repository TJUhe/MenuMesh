/**
 * @file src/feature_detection/SmoothCurvature.cpp
 * @brief 估计多尺度平滑曲率并提取稳定脊线、谷线证据。
 * @ingroup manumesh_feature_detection
 *
 * @details 实现稳健、按尺度归一化的光顺脊线/谷线证据。
 * @algorithm 将确定性的 k-ring 邻域投影到局部 Monge 坐标系，并按邻域半径归一化；
 *            使用空间权重和稳健残差权重求解五系数二次曲面，
 *            从其 Hessian 得到主曲率和方向；双侧符号极值及跨尺度符号/切线持久性
 *            共同决定是否接受特征。
 * @failuremodes 秩亏拟合、缺少双侧样本、不稳定特征方向或符号不一致时不产生证据。
 */

#include "algorithms/feature_detection/FeatureDetector.h"
#include "core/MathUtils.h"

#include "common/detail/MeshQueries.h"
#include "common/detail/ParallelExecution.h"
#include "detail/FeatureDetectionCache.h"
#include "detail/FeatureInputValidation.h"

#include <Eigen/Cholesky>
#include <Eigen/Eigenvalues>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace manumesh {
namespace feature {
namespace {

/**
 * @brief 交叉边的插值 cyclideness（零交叉边两端平均 |extremality|）
 *        与主曲率平方之比的最小阈值。两者均使用相同的半径归一化单位，
 *        因而该比值无量纲且对统一网格缩放不变。实测真实脊线（高斯脊/凸起）
 *        通常不低于 0.38，而环面这一 Dupin cyclide 上的伪谷带在 24-48 个小段时
 *        峰值约为 0.06；取 0.15 可为两类样本保留约 2.5 倍余量。
 */
constexpr double kMinCrossingCyclidenessRatio = 0.15;

/** @brief 围绕一个拟合种子收集的顶点 ID 及环层深度。 */
struct NeighborhoodVertex {
    int id = -1;
    int depth = 0;
};

/** @brief 单个邻域尺度的曲率拟合和置信度诊断。 */
struct ScaleEstimate {
    bool valid = false;
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    std::array<Vec3, 2> directions = {Vec3(1.0, 0.0, 0.0), Vec3(0.0, 1.0, 0.0)};
    std::array<double, 2> curvatures = {0.0, 0.0};
    /**
     * @brief 根据三次 Monge 项计算极值量 e_i = grad(kappa_i) · t_i，
     *        单位与曲率相同，均按邻域半径归一化（M021 Yoshizawa）。
     */
    std::array<double, 2> extremalities = {0.0, 0.0};
    double fitResidual = 0.0;
    double localScale = 0.0;
};

/** @brief 有效尺度估计及其持久性选择元数据。 */
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

/** @brief 为一个网格顶点选择的稳定多尺度估计。 */
struct ScaleSelection {
    int scale = -1;
    double stability = 0.0;
};

/**
 * @brief 为多尺度拟合热路径复用的临时缓冲区。
 * 旧实现为每个顶点和稳健迭代分配向量、队列及 Eigen 矩阵；
 * 每次分析复用一个工作区可消除这些分配，同时不改变数值结果。
 */
struct FitWorkspace {
    std::vector<NeighborhoodVertex> neighborhood;
    std::vector<Eigen::Matrix<double, 1, 9>> rows;
    std::vector<double> targets;
    std::vector<double> spatialWeights;
    std::vector<double> robustWeights;
    std::vector<double> residuals;
    std::vector<double> medianScratch;
};

std::vector<Vec3> computeAreaWeightedVertexNormals(
    const Mesh& mesh,
    const std::vector<Vec3>& faceNormals,
    const common::parallel::RangeExecutionOptions& executionOptions
) {
    std::vector<Vec3> normals(mesh.vertices.size(), Vec3::Zero());
    for (int faceId = 0; faceId < static_cast<int>(mesh.faces.size()); ++faceId) {
        const Face& face = mesh.faces[faceId];
        const Vec3 cross = (mesh.vertices[face.v[1]] - mesh.vertices[face.v[0]])
                               .cross(mesh.vertices[face.v[2]] - mesh.vertices[face.v[0]]);
        if (cross.squaredNorm() <= 1e-30) {
            continue;
        }
        const Vec3 direction = faceId < static_cast<int>(faceNormals.size()) ? faceNormals[faceId] : cross.normalized();
        const Vec3 weighted = cross.norm() * direction;
        for (int id : face.v) {
            normals[id] += weighted;
        }
    }
    common::parallel::forEachRange(
        0,
        normals.size(),
        executionOptions,
        [&](std::size_t begin, std::size_t end) {
            for (std::size_t vertex = begin; vertex < end; ++vertex) {
                Vec3& normal = normals[vertex];
                if (normal.norm() > 1e-20) {
                    normal.normalize();
                } else {
                    normal = Vec3(0.0, 0.0, 1.0);
                }
            }
        }
    );
    return normals;
}

/**
 * @brief 将邻域按层宽度优先收集到复用缓冲区。种子位于索引 0、深度为 0，
 *        调用方从索引 1 开始遍历。
 */
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

/**
 * @brief 并行任务使用的可复用稀疏访问集合。
 *
 * 访问标记只随最大 k-ring 邻域增长，避免每个并行任务分配 O(V) 的全局 stamp
 * 数组；epoch 复位也不会释放或重新分配当前容量。开放寻址避免
 * std::unordered_set 在每个种子顶点插入节点和 clear 时的频繁小块分配。
 */
class SparseVisitSet {
public:
    SparseVisitSet() { rehash(kInitialCapacity); }

    /** @brief 开始新的访问轮次而不清理已分配的槽位。 */
    void reset() noexcept {
        size_ = 0;
        ++epoch_;
        if (epoch_ == 0) {
            std::fill(epochs_.begin(), epochs_.end(), 0);
            epoch_ = 1;
        }
    }

    /** @return 顶点此前未在当前访问轮次中出现时为 true。 */
    bool insert(int vertex) {
        std::size_t slot = findSlot(vertex);
        if (epochs_[slot] == epoch_) {
            return false;
        }
        if (size_ + 1 > growthThreshold()) {
            rehash(slots_.size() * 2);
            slot = findSlot(vertex);
        }
        slots_[slot] = vertex;
        epochs_[slot] = epoch_;
        ++size_;
        return true;
    }

private:
    static constexpr std::size_t kInitialCapacity = 128;

    static std::size_t hashVertex(int vertex) noexcept {
        std::uint32_t bits = static_cast<std::uint32_t>(vertex);
        bits ^= bits >> 16U;
        bits *= 0x7feb352dU;
        bits ^= bits >> 15U;
        bits *= 0x846ca68bU;
        bits ^= bits >> 16U;
        return static_cast<std::size_t>(bits);
    }

    std::size_t findSlot(int vertex) const noexcept {
        const std::size_t mask = slots_.size() - 1;
        std::size_t slot = hashVertex(vertex) & mask;
        while (epochs_[slot] == epoch_ && slots_[slot] != vertex) {
            slot = (slot + 1) & mask;
        }
        return slot;
    }

    std::size_t growthThreshold() const noexcept {
        // 75% load keeps short, predictable probe chains while preserving a compact task-local table.
        return slots_.size() - slots_.size() / 4;
    }

    void rehash(std::size_t requestedCapacity) {
        std::size_t capacity = kInitialCapacity;
        while (capacity < requestedCapacity) {
            capacity *= 2;
        }
        std::vector<int> newSlots(capacity, 0);
        std::vector<std::uint32_t> newEpochs(capacity, 0);
        const std::size_t mask = capacity - 1;
        for (std::size_t oldSlot = 0; oldSlot < slots_.size(); ++oldSlot) {
            if (epochs_[oldSlot] != epoch_) {
                continue;
            }
            std::size_t newSlot = hashVertex(slots_[oldSlot]) & mask;
            while (newEpochs[newSlot] == epoch_) {
                newSlot = (newSlot + 1) & mask;
            }
            newSlots[newSlot] = slots_[oldSlot];
            newEpochs[newSlot] = epoch_;
        }
        slots_.swap(newSlots);
        epochs_.swap(newEpochs);
    }

    std::vector<int> slots_;
    std::vector<std::uint32_t> epochs_;
    std::size_t size_ = 0;
    std::uint32_t epoch_ = 1;
};

/**
 * @brief 将邻域按层宽度优先收集到复用缓冲区，并以稀疏集合去重。
 *
 * 访问集合不参与迭代，邻域顺序始终由确定性邻接表的 BFS 首次发现顺序决定。
 */
void gatherNeighborhoodSparse(
    const std::vector<std::vector<int>>& neighbors,
    int seed,
    int maxDepth,
    SparseVisitSet& visited,
    std::vector<NeighborhoodVertex>& result
) {
    result.clear();
    visited.reset();
    result.push_back({seed, 0});
    visited.insert(seed);
    std::size_t head = 0;
    while (head < result.size()) {
        const NeighborhoodVertex current = result[head++];
        if (current.depth >= maxDepth) {
            continue;
        }
        for (int nb : neighbors[current.id]) {
            if (!visited.insert(nb)) {
                continue;
            }
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

ScaleSelection selectReferenceScale(
    const std::vector<std::vector<ScaleCandidate>>& candidates,
    int vertex,
    double tangentConsistency,
    double persistenceThreshold,
    bool stableSelection
) {
    ScaleSelection selection;
    double bestObjective = -1.0;
    double bestRawScore = -1.0;
    const int scaleCount = static_cast<int>(candidates.size());
    for (int scale = 0; scale < scaleCount; ++scale) {
        const ScaleCandidate& reference = candidates[scale][vertex];
        if (!reference.valid) {
            continue;
        }

        int supportCount = 0;
        double alignmentSum = 0.0;
        const double threshold = std::max(persistenceThreshold, 0.30 * reference.score);
        for (int otherScale = 0; otherScale < scaleCount; ++otherScale) {
            const ScaleCandidate& other = candidates[otherScale][vertex];
            if (!other.valid || other.signedKind != reference.signedKind || other.score < threshold) {
                continue;
            }
            const double alignment = std::abs(other.curveTangent.dot(reference.curveTangent));
            if (alignment < tangentConsistency) {
                continue;
            }
            ++supportCount;
            alignmentSum += alignment;
        }

        const double supportRatio =
            scaleCount > 0 ? static_cast<double>(supportCount) / static_cast<double>(scaleCount) : 0.0;
        const double meanAlignment = supportCount > 0 ? alignmentSum / static_cast<double>(supportCount) : 0.0;
        const double fitQuality = 1.0 / (1.0 + std::max(0.0, reference.fitResidual));
        const double stability = supportRatio * meanAlignment * fitQuality;
        const double objective = reference.score * (stableSelection ? 0.5 + 0.5 * stability : 1.0);
        if (selection.scale < 0 || objective > bestObjective ||
            (objective == bestObjective &&
             (reference.score > bestRawScore || (reference.score == bestRawScore && scale < selection.scale)))) {
            selection.scale = scale;
            selection.stability = stability;
            bestObjective = objective;
            bestRawScore = reference.score;
        }
    }
    return selection;
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
        // 三次 Monge 基（Yoshizawa M021）：二次项携带曲率，三次项携带曲率导数，
        // 后者用于计算下方的极值量。
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
    // 欠定性保护：三次拟合包含九个未知系数。
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
    const int iterations = manumesh::clampValue(robustIterations, 0, kMaxSmoothCurvatureRobustFitIterations);
    for (int iteration = 0; iteration <= iterations; ++iteration) {
        // 使用标量循环累加加权正规方程的上三角部分。坐标已按邻域半径预归一化，
        // 使 9x9 系统保持良好条件，并避免每次稳健迭代执行一次稠密 QR。
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
        // 根据主元跨度检查退化（类似 rcond）；拒绝数值秩亏的正规矩阵。
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
        // 极值量是三阶方向形式 C(t,t,t)（Yoshizawa 公式）；这里保留统一因子 6，
        // 以获得一致的归一化幅值。
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

/**
 * @brief 脊线/谷线主曲率占优判定（Ohtake M011）：脊线要求
 *        kappa_max > |kappa_min|，谷线要求 kappa_min < -|kappa_max|。
 */
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

    // 零交叉极值判据（Ohtake M011 式 3-4、Yoshizawa M021）：
    // 当沿着近似主方向的入射边上极值量发生符号变化时，顶点支持脊线或谷线。
    // 这里以一阶极大值检验替代难以稳定计算的二阶导数检验。主方向是线场，
    // 因而先同步邻点切线与极值量的符号，再要求两端都满足曲率占优条件。
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
            // 单位换算（P1-3）：每个顶点都在除以自身半径 r 的坐标中拟合，
            // 因此 kappa_hat = r * kappa_phys，e_hat = r^2 * e_phys。
            // 邻点半径通常不同，比较幅值前需将邻点极值量换算到中心顶点单位：
            // e_n->c = e_hat_n * (r_c / r_n)^2。该比例对统一网格缩放不变，
            // 因而后续判据保持尺度不变；只看符号的检验不受正比例因子影响。
            const double unitRatio = center.localScale / neighbor.localScale;
            neighborExtremality *= unitRatio * unitRatio;
            if (!(centerExtremality * neighborExtremality < 0.0)) {
                continue;
            }
            // 子顶点归属（Ohtake 逆插值）：零交叉点更接近 |e| 较小的端点，
            // 因此只允许该顶点认领候选，将检测带限制为单顶点宽度而非两个顶点。
            const double centerMagnitude = std::abs(centerExtremality);
            const double neighborMagnitude = std::abs(neighborExtremality);
            const double ownershipTolerance =
                64.0 * std::numeric_limits<double>::epsilon() * std::max({1.0, centerMagnitude, neighborMagnitude});
            if (centerMagnitude > neighborMagnitude + ownershipTolerance ||
                (std::abs(centerMagnitude - neighborMagnitude) <= ownershipTolerance && vertex > nb)) {
                continue;
            }
            const Vec3 delta = mesh.vertices[nb] - mesh.vertices[vertex];
            // 两个端点的一阶极大值检验：对脊线而言，极值必须沿各自切线位于
            // 每个顶点的前方；e 与 t 同时翻转，因此该条件与方向符号无关。
            const double centerSide =
                static_cast<double>(signedKind) * centerExtremality * delta.dot(extremumDirection);
            const double neighborSide =
                static_cast<double>(signedKind) * neighborExtremality * (-delta).dot(neighborTangent);
            if (!(centerSide > 0.0) || !(neighborSide > 0.0)) {
                continue;
            }
            // Cyclideness 门限（Yoshizawa M021 式 5-6）：在 Dupin cyclide
            // （球面/柱面/锥面/环面）上极值场恒为零，因此离散化造成的符号变化
            // 只是噪声而非曲率极值；整个极值圆是 kappa 的驻集，不携带脊线。
            // 真正脊线附近 |e| 随离开脊线的距离增长，端点 |e| 平均值可衡量极值显著性，
            // 并在交叉点处线性插值 Yoshizawa 的 cyclideness C。除以 kappa^2 后无量纲：
            // 中心单位下 e_hat = r_c^2 e_phys、kappa_hat^2 = r_c^2 kappa_phys^2，
            // 因而比值等于物理 e/kappa^2，不依赖 r_c，也对统一缩放严格不变。
            const double crossingCyclideness = 0.5 * (std::abs(centerExtremality) + std::abs(neighborExtremality));
            if (crossingCyclideness < kMinCrossingCyclidenessRatio * curvature * curvature) {
                continue;
            }
            // 以半径归一化单位计算曲率差代理：平均 |e| 乘以切向边长，
            // 与此前中心减邻点幅值的量级相当，使评分尺度保持一致。
            const double tangentialExtent = std::abs(delta.dot(extremumDirection)) / center.localScale;
            const double crossingStrength =
                0.5 * (std::abs(centerExtremality) + std::abs(neighborExtremality)) * tangentialExtent;
            extremum = std::max(extremum, crossingStrength);
        }
        if (extremum <= 0.0) {
            continue;
        }

        const double anisotropy = manumesh::clampValue(
            std::abs(curvature - otherCurvature) / (std::abs(curvature) + std::abs(otherCurvature) + 0.025), 0.0, 1.0
        );
        const double extremumRatio = manumesh::clampValue(extremum / (std::abs(curvature) + 0.025), 0.0, 1.0);
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

} // 匿名命名空间

namespace detector_detail {

const std::vector<Vec3>& FeatureDetectionCache::areaWeightedVertexNormals() {
    if (!hasAreaWeightedVertexNormals_) {
        areaWeightedVertexNormals_ = computeAreaWeightedVertexNormals(
            *mesh_, faceNormals(), common::parallel::makeRangeExecutionOptions(executionOptions_)
        );
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

    const int baseRings =
        manumesh::clampValue(options.baseNeighborhoodRings, 1, kMaxSmoothCurvatureBaseNeighborhoodRings);
    const int scaleCount = manumesh::clampValue(options.scaleCount, 1, kMaxSmoothCurvatureScaleCount);
    const int maxRings = baseRings + scaleCount - 1;
    const int robustIterations =
        manumesh::clampValue(options.robustFitIterations, 0, kMaxSmoothCurvatureRobustFitIterations);
    const double tangentConsistency = manumesh::clampValue(options.minTangentConsistency, 0.0, 1.0);
    const double persistenceThreshold =
        std::isfinite(requestedPersistenceThreshold) ? std::max(1e-12, requestedPersistenceThreshold) : 1e-12;

    const std::vector<std::vector<int>>& neighbors = cache.vertexNeighbors();
    const std::vector<double>& averageEdgeLength = cache.vertexAverageEdgeLength();
    const std::vector<Vec3>& vertexNormals = cache.areaWeightedVertexNormals();
    const common::parallel::RangeExecutionOptions rangeOptions =
        common::parallel::makeRangeExecutionOptions(cache.executionOptions());
    std::vector<std::vector<ScaleEstimate>> estimates(scaleCount, std::vector<ScaleEstimate>(mesh.vertices.size()));
    if (!rangeOptions.enabled) {
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
    } else {
        common::parallel::forEachRange(
            0,
            mesh.vertices.size(),
            rangeOptions,
            [&](std::size_t begin, std::size_t end) {
                FitWorkspace workspace;
                SparseVisitSet visited;
                for (std::size_t vertex = begin; vertex < end; ++vertex) {
                    gatherNeighborhoodSparse(
                        neighbors, static_cast<int>(vertex), maxRings, visited, workspace.neighborhood
                    );
                    for (int scale = 0; scale < scaleCount; ++scale) {
                        const int ringCount = baseRings + scale;
                        estimates[scale][vertex] = fitScale(
                            mesh,
                            vertexNormals,
                            workspace,
                            static_cast<int>(vertex),
                            ringCount,
                            averageEdgeLength[vertex],
                            robustIterations
                        );
                    }
                }
            }
        );
    }

    std::vector<std::vector<ScaleCandidate>> candidates(scaleCount, std::vector<ScaleCandidate>(mesh.vertices.size()));
    for (int scale = 0; scale < scaleCount; ++scale) {
        common::parallel::forEachRange(
            0,
            mesh.vertices.size(),
            rangeOptions,
            [&](std::size_t begin, std::size_t end) {
                for (std::size_t vertex = begin; vertex < end; ++vertex) {
                    candidates[scale][vertex] =
                        classifyScaleCandidate(mesh, neighbors, estimates[scale], static_cast<int>(vertex));
                }
            }
        );
    }

    common::parallel::forEachRange(
        0,
        mesh.vertices.size(),
        rangeOptions,
        [&](std::size_t begin, std::size_t end) {
            for (std::size_t vertex = begin; vertex < end; ++vertex) {
                const ScaleSelection selection = selectReferenceScale(
                    candidates,
                    static_cast<int>(vertex),
                    tangentConsistency,
                    persistenceThreshold,
                    options.useStableScaleSelection
                );
                const int bestScale = selection.scale;
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
                    if (!candidate.valid ||
                        candidate.score < std::max(persistenceThreshold, relativePersistenceThreshold) ||
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
                output.selectedScale = bestScale;
                output.scaleStability = selection.stability;
                output.signedKind = best.signedKind;
        // 纯投票计数（Luo-Zha M009）：持久性等于支持该候选的尺度数量，
        // 下游再与 smoothCurvatureMinPersistentScales 比较。旧实现还要求最粗尺度
        // 必须支持候选，导致密集网格中空间尺度小于最粗邻域半径（baseRings +
        // scaleCount - 1 环）的真实小圆角和短脊线被静默抑制；多尺度通道正是为了发现它们。
                const bool passesScaleStability =
                    !options.useStableScaleSelection || output.scaleStability >= options.minScaleStability;
                if (output.persistentScales > 0 && passesScaleStability) {
                    const double persistenceRatio =
                        static_cast<double>(output.persistentScales) / static_cast<double>(scaleCount);
                    const double meanAlignment = supportedAlignmentSum / static_cast<double>(output.persistentScales);
                    const double meanSupportedScore = supportedScoreSum / static_cast<double>(output.persistentScales);
                    output.persistentFeatureScore =
                        (0.65 * output.featureScore + 0.35 * meanSupportedScore) * persistenceRatio * meanAlignment;
                }
            }
        }
    );
    return result;
}

} // 命名空间 manumesh::feature::detector_detail

std::vector<SmoothCurvatureVertex> computeSmoothCurvatureFeatures(
    const Mesh& mesh, const SmoothCurvatureOptions& options, double requestedPersistenceThreshold
) {
    return computeSmoothCurvatureFeatures(mesh, options, requestedPersistenceThreshold, ExecutionOptions{});
}

std::vector<SmoothCurvatureVertex> computeSmoothCurvatureFeatures(
    const Mesh& mesh,
    const SmoothCurvatureOptions& options,
    double requestedPersistenceThreshold,
    const ExecutionOptions& executionOptions
) {
    validateExecutionOptions(executionOptions);
    detector_detail::FeatureDetectionCache cache(mesh, FeatureNormalFilterOptions{}, executionOptions);
    return detector_detail::computeSmoothCurvatureFeaturesCached(mesh, cache, options, requestedPersistenceThreshold);
}

std::vector<SmoothCurvatureVertex>
computeSmoothCurvatureFeatures(const Mesh& mesh, const SmoothCurvatureOptions& options) {
    return computeSmoothCurvatureFeatures(mesh, options, 0.0);
}

std::vector<SmoothCurvatureVertex> computeSmoothCurvatureFeatures(
    const Mesh& mesh, const SmoothCurvatureOptions& options, const ExecutionOptions& executionOptions
) {
    return computeSmoothCurvatureFeatures(mesh, options, 0.0, executionOptions);
}

} // namespace feature
} // namespace manumesh
