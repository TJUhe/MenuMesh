/**
 * @file src/feature_detection/detail/FeatureDetectionCache.h
 * @brief 声明一次分析中按需构建的几何查询缓存。
 * @ingroup manumesh_feature_detection
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"
#include "common/detail/MeshQueries.h"
#include "core/ExecutionOptions.h"

#include <utility>
#include <vector>

namespace manumesh {
namespace feature {
namespace detector_detail {

/**
 * @brief 按需构建的单次分析缓存，保存网格范围的辅助结构。
 *
 * 一次 analyze() 过去会在证据、平滑和清理阶段重复构建五至六次顶点邻接、
 * 每顶点平均边长及面法向。下面的访问器只在首次访问时构建数据，之后返回缓存副本，
 * 因而一次流水线运行的所有阶段共享同一份结构。缓存只能与创建它时绑定的网格配合使用。
 */
class FeatureDetectionCache {
public:
    /**
     * @brief 将惰性缓存绑定到一份不可变网格和法向滤波策略。
     * @param[in] mesh 缓存所引用的网格；其生命周期必须长于缓存。
     * @param[in] normalFilterOptions 可选的面法向平滑策略。
     * @param[in] executionOptions 本次流水线使用的并发和任务粒度约束。
     */
    explicit FeatureDetectionCache(
        const Mesh& mesh,
        FeatureNormalFilterOptions normalFilterOptions = {},
        ExecutionOptions executionOptions = {}
    )
        : mesh_(&mesh),
          normalFilterOptions_(normalFilterOptions),
          executionOptions_(executionOptions) {
        validateExecutionOptions(executionOptions_);
    }

    FeatureDetectionCache(const FeatureDetectionCache&) = delete;
    FeatureDetectionCache& operator=(const FeatureDetectionCache&) = delete;

    /** @brief 返回经过滤波的面法向，首次访问时计算。 */
    const std::vector<Vec3>& faceNormals();

    /** @brief 返回惰性执行的法向滤波诊断信息。 */
    const FeatureNormalFilterReport& normalFilterReport();

    /** @brief 返回本次流水线统一使用的公共执行约束。 */
    const ExecutionOptions& executionOptions() const { return executionOptions_; }

    /** @brief 返回缓存的无向边到相邻面的关联关系。 */
    const manumesh::common::MeshEdgeInfoMap& edgeInfo() {
        if (!hasEdgeInfo_) {
            edgeInfo_ = manumesh::common::buildMeshEdgeInfo(*mesh_);
            hasEdgeInfo_ = true;
        }
        return edgeInfo_;
    }

    /** @brief 返回缓存的确定性顶点一环邻接表。 */
    const std::vector<std::vector<int>>& vertexNeighbors() {
        if (!hasVertexNeighbors_) {
            vertexNeighbors_ = manumesh::common::buildVertexNeighbors(*mesh_);
            hasVertexNeighbors_ = true;
        }
        return vertexNeighbors_;
    }

    /** @brief 返回缓存的每顶点采样密度估计（平均边长）。 */
    const std::vector<double>& vertexAverageEdgeLength() {
        if (!hasVertexAverageEdgeLength_) {
            // 复用已经缓存的 edgeInfo，避免再次构建和哈希遍历整张边表。
            vertexAverageEdgeLength_ = manumesh::common::computeVertexAverageEdgeLength(*mesh_, edgeInfo());
            hasVertexAverageEdgeLength_ = true;
        }
        return vertexAverageEdgeLength_;
    }

    /**
     * @brief 返回供光顺曲率阶段共享的面积加权顶点法向。
     */
    const std::vector<Vec3>& areaWeightedVertexNormals();

private:
    const Mesh* mesh_ = nullptr;
    std::vector<Vec3> faceNormals_;
    manumesh::common::MeshEdgeInfoMap edgeInfo_;
    std::vector<std::vector<int>> vertexNeighbors_;
    std::vector<double> vertexAverageEdgeLength_;
    std::vector<Vec3> areaWeightedVertexNormals_;
    FeatureNormalFilterOptions normalFilterOptions_;
    ExecutionOptions executionOptions_;
    FeatureNormalFilterReport normalFilterReport_;
    bool hasFaceNormals_ = false;
    bool hasEdgeInfo_ = false;
    bool hasVertexNeighbors_ = false;
    bool hasVertexAverageEdgeLength_ = false;
    bool hasAreaWeightedVertexNormals_ = false;
};

/**
 * @brief 使用已有缓存计算法向张量特征；公共重载会自行创建缓存，
 *        流水线则传入同一个共享实例。
 */
std::vector<NormalTensorVertex> computeNormalTensorFeaturesCached(
    const Mesh& mesh, FeatureDetectionCache& cache, const NormalTensorOptions& options, double persistenceThreshold
);

std::vector<SmoothCurvatureVertex> computeSmoothCurvatureFeaturesCached(
    const Mesh& mesh, FeatureDetectionCache& cache, const SmoothCurvatureOptions& options, double persistenceThreshold
);

} // namespace detector_detail
} // namespace feature
} // namespace manumesh
