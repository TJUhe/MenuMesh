/**
 * @file src/debugUtil/debugUtil.h
 * @brief 声明仅调试构建可用的 HTML 网格快照接口。
 * @ingroup manumesh_debug
 *
 * @details 发布构建会去除调试可视化，调试工具不得影响算法结果。
 */

#pragma once

#include <string>
#include <vector>

namespace manumesh {
struct Mesh;
}

namespace manumesh {
namespace debugUtil {

/**
 * @brief 用于选择颜色和线宽的调试绘制语义。
 *
 * 这些值描述语义而非具体算法，因此检查特征检测、折叠拒绝、拓扑警告或
 * 简化前后快照时可以复用同一套调色板。
 */
enum class UseCase {
    /**
     * @brief 普通网格线框。
     */
    Mesh,
    /**
     * @brief 边界边或开放边界证据。
     */
    Boundary,
    /**
     * @brief 强特征边，例如二面角证据。
     */
    Feature,
    /**
     * @brief 弱特征证据，例如仅由法线张量提供的支持。
     */
    WeakFeature,
    /**
     * @brief 恢复得到或面向用户显示的特征环。
     */
    FeatureLoop,
    /**
     * @brief 正在检查、尚未决定接受或拒绝的候选边。
     */
    Candidate,
    /**
     * @brief 已接受的结果边，通常用于显示简化输出。
     */
    Accepted,
    /**
     * @brief 被拒绝的边或折叠失败的候选边。
     */
    Rejected,
    /**
     * @brief 可能需要进一步检查的可疑边。
     */
    Warning,
    /**
     * @brief 明确的错误证据，例如非流形拓扑。
     */
    Error,
};

/**
 * @brief 叠加在基础线框上的可选彩色边覆盖层。
 *
 * 顶点索引属于当前网格。无效索引会被忽略，不会使调试运行失败，
 * 因为该工具经常在错误处理路径中调用。
 */
struct EdgeOverlay {
    /**
     * @brief 第一个顶点索引。
     */
    int a = -1;
    /**
     * @brief 第二个顶点索引。
     */
    int b = -1;
    /**
     * @brief 此覆盖层使用的语义颜色和线宽。
     */
    UseCase useCase = UseCase::Feature;
    /**
     * @brief 可选的边中点附近标签。
     */
    std::string label;
};

#if defined(MANUMESH_ENABLE_DEBUG_UTIL) && !defined(NDEBUG)

/**
 * @brief Returns whether runtime debug snapshots are enabled.
 *
 * Set `MANUMESH_DEBUG_UTIL_ENABLED=0` to suppress HTML generation while
 * keeping the instrumentation compiled into the Debug build.
 */
bool enabled();

/**
 * @brief 为网格写入本地交互式 HTML 线框页面。
 */
void showWireframe(const char* tag, const Mesh& mesh, UseCase useCase = UseCase::Mesh);
/**
 * @brief 写入突出显示一条边的本地 HTML 快照。
 */
void showEdge(const char* tag, const Mesh& mesh, int a, int b, UseCase useCase, const char* label = nullptr);
/**
 * @brief 写入突出显示多条边的本地 HTML 快照。
 */
void showEdges(
    const char* tag, const Mesh& mesh, const std::vector<EdgeOverlay>& overlays, UseCase baseUseCase = UseCase::Mesh
);
/**
 * @brief 写入并排显示简化前后网格的本地 HTML 快照。
 */
void showBeforeAfter(const char* tag, const Mesh& before, const Mesh& after);

#endif

} // namespace debugUtil
} // namespace manumesh

// Include this header explicitly at instrumentation sites.
// 只有启用 CMake 调试工具选项且不是 NDEBUG 构建时才会执行，否则为空操作。
#if defined(MANUMESH_ENABLE_DEBUG_UTIL) && !defined(NDEBUG)

#define MANUMESH_DEBUG_UTIL_WIREFRAME(tag, mesh) ::manumesh::debugUtil::showWireframe((tag), (mesh))

#define MANUMESH_DEBUG_UTIL_WIREFRAME_AS(tag, mesh, useCase)                                                           \
    ::manumesh::debugUtil::showWireframe((tag), (mesh), (useCase))

#define MANUMESH_DEBUG_UTIL_EDGE(tag, mesh, a, b, useCase)                                                             \
    ::manumesh::debugUtil::showEdge((tag), (mesh), (a), (b), (useCase))

#define MANUMESH_DEBUG_UTIL_EDGE_LABEL(tag, mesh, a, b, useCase, label)                                                \
    ::manumesh::debugUtil::showEdge((tag), (mesh), (a), (b), (useCase), (label))

#define MANUMESH_DEBUG_UTIL_EDGES(tag, mesh, overlays) ::manumesh::debugUtil::showEdges((tag), (mesh), (overlays))

#define MANUMESH_DEBUG_UTIL_BEFORE_AFTER(tag, beforeMesh, afterMesh)                                                   \
    ::manumesh::debugUtil::showBeforeAfter((tag), (beforeMesh), (afterMesh))

#else

#define MANUMESH_DEBUG_UTIL_WIREFRAME(tag, mesh) ((void)0)
#define MANUMESH_DEBUG_UTIL_WIREFRAME_AS(tag, mesh, useCase) ((void)0)
#define MANUMESH_DEBUG_UTIL_EDGE(tag, mesh, a, b, useCase) ((void)0)
#define MANUMESH_DEBUG_UTIL_EDGE_LABEL(tag, mesh, a, b, useCase, label) ((void)0)
#define MANUMESH_DEBUG_UTIL_EDGES(tag, mesh, overlays) ((void)0)
#define MANUMESH_DEBUG_UTIL_BEFORE_AFTER(tag, beforeMesh, afterMesh) ((void)0)

#endif
