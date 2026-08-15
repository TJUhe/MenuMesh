/**
 * @file include/api/CApi.h
 * @brief 声明 ManuMesh C ABI 模块的 C API 设施。
 * @ingroup manumesh_c_api
 *
 * @details C 边界校验指针和容量，将失败转换为状态码，并绝不允许 C++ 异常越过 ABI。
 */

#pragma once

#include "Export.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @addtogroup manumesh_c_api
 * @{
 *
 * @brief ManuMesh 稳定 C ABI 版本 1。
 *
 * @par 线程安全
 * ManuMeshContext 不是线程安全的。没有外部同步时不要跨线程共享同一个上下文；应为每个线程使用独立上下文。
 *
 * @par 纹理坐标
 * v1 ABI 不携带逐角纹理坐标。manumesh_mesh_set_data 只接受顶点位置和面索引，
 * manumesh_mesh_copy_* 也只返回位置和索引。C++ manumesh::Mesh / PlainMesh
 *（Mesh::faceTexCoords）中的 UV 不在此处暴露，网格跨越此边界时会被丢弃。
 * 必须保留纹理坐标时请直接使用 C++ API。
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ManuMeshContext ManuMeshContext;       ///< 不透明的错误和操作上下文。
typedef struct ManuMeshMeshHandle ManuMeshMeshHandle; ///< 不透明的自有三角网格句柄。

#define MANUMESH_ABI_VERSION 1u

/** @brief 可失败 C ABI 函数返回的稳定状态。 */
typedef enum ManuMeshStatus {
    MANUMESH_STATUS_OK = 0,               ///< 操作成功完成。
    MANUMESH_STATUS_INVALID_ARGUMENT = 1, ///< 指针、值、网格或 ABI 约定无效。
    MANUMESH_STATUS_BUFFER_TOO_SMALL = 2, ///< 调用方拥有的输出存储容量不足。
    MANUMESH_STATUS_IO_ERROR = 3,         ///< 文件解析、创建、读取或写入失败。
    MANUMESH_STATUS_ALGORITHM_ERROR = 4,  ///< 几何算法无法完成。
    MANUMESH_STATUS_OUT_OF_MEMORY = 5     ///< 分配失败。
} ManuMeshStatus;

/** @brief 用于缩放 line-quadric 排序代价的空间策略。 */
typedef enum ManuMeshWeightMode {
    MANUMESH_WEIGHT_MODE_UNIFORM = 0,
    MANUMESH_WEIGHT_MODE_DIHEDRAL = 1,
    MANUMESH_WEIGHT_MODE_HEIGHT = 2,
    MANUMESH_WEIGHT_MODE_X_BAND = 3,
    MANUMESH_WEIGHT_MODE_NORMAL_TENSOR = 4
} ManuMeshWeightMode;

/** @brief 原本成功的简化运行停止的原因。 */
typedef enum ManuMeshSimplifyTerminationReason {
    MANUMESH_SIMPLIFY_TERMINATION_NOT_STARTED = 0,
    MANUMESH_SIMPLIFY_TERMINATION_REACHED_TARGET = 1,
    MANUMESH_SIMPLIFY_TERMINATION_ALREADY_AT_OR_BELOW_TARGET = 2,
    MANUMESH_SIMPLIFY_TERMINATION_NO_CANDIDATES = 3,
    MANUMESH_SIMPLIFY_TERMINATION_REJECTION_LIMIT = 4
} ManuMeshSimplifyTerminationReason;

/** @brief QEM 候选排序后应用的硬特征策略。 */
typedef enum ManuMeshFeatureProtectionMode {
    /** 禁用硬特征曲线保护。 */
    MANUMESH_FEATURE_PROTECTION_NONE = 0,
    /** 仅硬保护圆环和近圆环。 */
    MANUMESH_FEATURE_PROTECTION_CIRCULAR_ONLY = 1,
    /** 硬保护拟合的圆、近圆和椭圆环。 */
    MANUMESH_FEATURE_PROTECTION_PRIMITIVE_CURVES = 2,
    /** 严格行为：硬保护所有检测到的特征边。 */
    MANUMESH_FEATURE_PROTECTION_ALL_FEATURE_EDGES = 3
} ManuMeshFeatureProtectionMode;

/** @brief 模型单位下的普通双精度位置。 */
typedef struct ManuMeshVec3 {
    double x; ///< X 坐标。
    double y; ///< Y 坐标。
    double z; ///< Z 坐标。
} ManuMeshVec3;

/** @brief 包含三个从零开始顶点索引的三角形。 */
typedef struct ManuMeshFace {
    int v[3]; ///< 指向所提供顶点数组的逆时针索引。
} ManuMeshFace;

/**
 * @brief 独立特征检测的带大小版本 C ABI 选项。
 *
 * 调用 `manumesh_detect_feature_edges` 前必须使用
 * `manumesh_feature_options_init` 初始化。对同一 ABI 版本，库接受尾部字段
 * 更少的旧结构体大小；未出现的尾部字段使用库默认值。
 */
typedef struct ManuMeshFeatureOptions {
    size_t struct_size;
    unsigned int abi_version;
    double feature_angle_deg;
    double loop_trace_angle_deg;
    double circle_fit_relative_threshold;
    double ellipse_fit_relative_threshold;
    double near_circle_axis_ratio_tolerance;
    int min_feature_loop_vertices;
    int use_normal_tensor_features;
    double normal_tensor_feature_threshold;
    double normal_tensor_min_edge_alignment;
    int normal_tensor_smoothing_iterations;
    int normal_tensor_scale_count;
    int normal_tensor_min_persistent_scales;
    int use_smooth_curvature_features;
    double smooth_curvature_feature_threshold;
    double smooth_curvature_min_edge_alignment;
    double smooth_curvature_min_tangent_consistency;
    int smooth_curvature_base_neighborhood_rings;
    int smooth_curvature_scale_count;
    int smooth_curvature_min_persistent_scales;
    int smooth_curvature_robust_fit_iterations;
    int smooth_curvature_use_stable_scale_selection;
    double smooth_curvature_min_scale_stability;
    int cleanup_feature_graph;
    double feature_graph_gap_length_ratio;
    int feature_graph_max_weak_spur_edges;
    double feature_graph_min_weak_spur_strength;
    double feature_component_min_confidence;
    int normal_filter_enabled;
    int normal_filter_iterations;
    double normal_filter_angle_sigma_deg;
    double normal_filter_preserve_angle_deg;
    double normal_filter_relaxation;
    int graph_consolidation_enabled;
    double graph_consolidation_gap_length_ratio;
    double graph_consolidation_min_alignment;
} ManuMeshFeatureOptions;

/**
 * @brief 一条活动特征图无向边及其证据来源。
 *
 * `a` 和 `b` 是输入网格的零基顶点索引；来源标志可以同时为真。
 * 端点顺序不保证升序；需要稳定键时由调用方规范化为 `(min(a,b), max(a,b))`。
 * `signed_kind` 大于零表示凸，小于零表示凹，零表示未知或无符号。
 * C ABI 输入没有独立的边数组或全局边编号，因此一条输入边由无向端点对唯一标识。
 * 此结构是固定的 ABI-v1 数组元素布局；后续扩展必须使用新的结构或入口，不能改变其步长。
 */
typedef struct ManuMeshFeatureEdge {
    int a;
    int b;
    int boundary;
    int dihedral;
    int normal_tensor;
    int smooth_curvature;
    int non_manifold;
    int cleanup_bridge;
    int consolidation_bridge;
    int removed_by_cleanup;
    int signed_kind;
} ManuMeshFeatureEdge;

/** `ManuMeshFeatureEdgeV2::input_edge_index` 没有对应输入网格边时使用的哨兵。 */
#define MANUMESH_INVALID_EDGE_INDEX ((uint64_t)~(uint64_t)0)

/**
 * @brief 带稳定序号和几何约束语义的特征边 ABI-v2 数组元素。
 *
 * 前 11 个字段与 `ManuMeshFeatureEdge` 完全相同，但这是独立的固定步长结构，
 * 调用方不得把两种数组相互转换。`feature_edge_index` 是 v2 结果按规范端点和
 * 来源排序后的零基序号；`input_edge_index` 是 `uniqueEdges` 语义下按 `(a,b)`
 * 字典序排列的输入网格边序号。
 *
 * `synthetic` 表示该图边由 cleanup/consolidation 恢复产生。只有端点对确实是
 * 输入网格边时 `geometric_constraint` 才为真并拥有有效 `input_edge_index`；
 * 非拓扑恢复桥仅表达图连续性，不能直接作为 QEM 投影线段。
 */
typedef struct ManuMeshFeatureEdgeV2 {
    int a;
    int b;
    int boundary;
    int dihedral;
    int normal_tensor;
    int smooth_curvature;
    int non_manifold;
    int cleanup_bridge;
    int consolidation_bridge;
    int removed_by_cleanup;
    int signed_kind;
    uint64_t feature_edge_index;
    uint64_t input_edge_index;
    int synthetic;
    int geometric_constraint;
} ManuMeshFeatureEdgeV2;

/** @brief 用于 ABI 兼容扩展的带大小版本简化选项。 */
typedef struct ManuMeshSimplifyOptions {
    /**
   * 调用 manumesh_simplify_mesh 前必须由 manumesh_simplify_options_init 设置。
   * 对同一 ABI 版本，库接受尾部字段更少的旧结构体大小；struct_size 中不存在的字段使用库默认值。
   */
    size_t struct_size;
    unsigned int abi_version;
    /** 目标选择。`target_faces > 0` 时覆盖 `target_ratio`。 */
    int target_faces;
    double target_ratio;
    /** QEM 和 line-quadric 候选排序代价。 */
    int use_line_quadrics;
    double line_weight;
    ManuMeshWeightMode weight_mode;
    double feature_boost;
    double feature_angle_deg;
    int adaptive_scale;
    double adaptive_base_line_weight;
    /** 边界和特征策略。硬过滤器在 QEM 排序后运行。 */
    double boundary_weight;
    int preserve_boundary;
    int preserve_feature_curves;
    double feature_curve_weight;
    double max_feature_curve_deviation_ratio;
    /** 简化前使用的特征检测阈值。 */
    double circle_fit_relative_threshold;
    double ellipse_fit_relative_threshold;
    double near_circle_axis_ratio_tolerance;
    int min_feature_loop_vertices;
    int min_circular_feature_loop_vertices;
    /** 法向张量弱特征证据。 */
    int use_normal_tensor_features;
    double normal_tensor_feature_threshold;
    double normal_tensor_min_edge_alignment;
    int normal_tensor_smoothing_iterations;
    int normal_tensor_scale_count;
    /** 硬合法性过滤器。零局部误差预算禁用这些测试。 */
    double min_triangle_quality;
    double max_normal_deviation_deg;
    double max_local_error;
    double max_local_error_ratio;
    int prevent_local_intersections;
    int verbose;
    ManuMeshFeatureProtectionMode feature_protection_mode;
    double loop_trace_angle_deg;
    int normal_tensor_min_persistent_scales;
    int cleanup_feature_graph;
    double feature_graph_gap_length_ratio;
    int feature_graph_max_weak_spur_edges;
    double feature_component_min_confidence;
    /** 简化后的可选固定拓扑质量改进轮次。 */
    int quality_refinement_iterations;
    /** 可选的确定性平滑脊/谷证据。尾部扩展字段。 */
    int use_smooth_curvature_features;
    double smooth_curvature_feature_threshold;
    double smooth_curvature_min_edge_alignment;
    double smooth_curvature_min_tangent_consistency;
    int smooth_curvature_base_neighborhood_rings;
    int smooth_curvature_scale_count;
    int smooth_curvature_min_persistent_scales;
    int smooth_curvature_robust_fit_iterations;
    /** 积分弱 spur 强度阈值；零值保持按边数清理。 */
    double feature_graph_min_weak_spur_strength;
    /** 可选的含噪输入预处理和图恢复。尾部扩展字段。 */
    int use_feature_normal_filter;
    int feature_normal_filter_iterations;
    double feature_normal_filter_angle_sigma_deg;
    double feature_normal_filter_preserve_angle_deg;
    double feature_normal_filter_relaxation;
    int smooth_curvature_use_stable_scale_selection;
    double smooth_curvature_min_scale_stability;
    int consolidate_feature_graph;
    double feature_graph_consolidation_gap_length_ratio;
    double feature_graph_consolidation_min_alignment;
    /**
     * 可选的借用式规范特征配置。非 NULL 时覆盖上面的全部扁平特征检测字段，
     * 仅在简化调用期间读取且库不会保留该指针；所指对象必须由
     * manumesh_feature_options_init 初始化。
     */
    const ManuMeshFeatureOptions* feature_options;
} ManuMeshSimplifyOptions;

/** @brief 一次简化运行的带大小版本诊断信息。 */
typedef struct ManuMeshSimplifyReport {
    /**
   * 简化诊断信息的输出存储。当前源码对 manumesh_simplify_mesh 的调用通过带大小信息的内联包装器
   * 传入 sizeof(ManuMeshSimplifyReport)，因此无需预先初始化此对象。在简化调用前需要一个已初始化的
   * 独立值时，请调用 manumesh_simplify_report_init。
   */
    size_t struct_size;
    unsigned int abi_version;
    int initial_vertices;
    int initial_faces;
    int final_vertices;
    int final_faces;
    int collapsed_edges;
    int rejected_collapses;
    /** 位置求解使用端点/中点回退的当前候选数。 */
    int solver_fallbacks;
    int queue_rebuilds;
    /** 输入网格的特征检测摘要。 */
    int feature_loops;
    int circular_feature_loops;
    int feature_vertices;
    int normal_tensor_feature_edges;
    /** 拒绝每个当前坍缩候选的首个硬过滤器。 */
    int feature_rejected_collapses;
    int primitive_feature_rejected_collapses;
    int generic_feature_rejected_collapses;
    int boundary_rejected_collapses;
    int topology_rejected_collapses;
    int normal_flip_rejected_collapses;
    int quality_rejected_collapses;
    int self_intersection_rejected_collapses;
    int curve_budget_rejected_collapses;
    int error_rejected_collapses;
    int projected_feature_placements;
    ManuMeshSimplifyTerminationReason termination_reason;
    double min_applied_line_weight;
    double max_applied_line_weight;
    /** 特征追踪诊断。追加在尾部以保持旧字段偏移。 */
    int traced_feature_edges;
    int untraced_feature_edges;
    /** 法向张量尺度诊断。追加在尾部以保持 ABI 兼容。 */
    int normal_tensor_scored_vertices;
    double max_normal_tensor_persistent_score;
    double mean_normal_tensor_local_scale;
    double mean_normal_tensor_persistence;
    /** 特征组件置信度和清理诊断。 */
    int feature_components;
    int weak_feature_components;
    int high_confidence_feature_components;
    int graph_cleanup_bridged_gaps;
    int graph_cleanup_removed_spurs;
    int graph_cleanup_merged_junctions;
    double mean_feature_component_confidence;
    double min_feature_component_confidence;
    /** 固定拓扑第二轮质量精修诊断。 */
    int quality_refinement_iterations_completed;
    int quality_refinement_attempted_moves;
    int quality_refinement_accepted_moves;
    /** 被容忍为退化的输入面（面积为零或顶点重复）。追加在尾部以保持旧字段偏移。 */
    int degenerate_input_faces;
    /** 平滑曲率和特征恢复诊断。尾部扩展字段。 */
    int smooth_curvature_feature_edges;
    int smooth_curvature_scored_vertices;
    double max_smooth_curvature_persistent_score;
    double mean_smooth_curvature_local_scale;
    double mean_smooth_curvature_persistence;
    int inconsistent_winding_edges;
    int graph_cleanup_skipped_by_cap;
    int circular_recovery_truncated;
    /** 含噪输入预处理、稳定尺度和图恢复诊断。 */
    int feature_normal_filter_iterations_completed;
    int feature_normal_filter_changed_faces;
    int feature_normal_filter_preserved_edges;
    double mean_feature_normal_filter_angular_change_deg;
    double max_feature_normal_filter_angular_change_deg;
    double mean_feature_normal_filter_edge_indicator;
    double mean_smooth_curvature_scale_stability;
    int graph_consolidation_bridges;
    int graph_consolidation_skipped_by_cap;
    int junction_branch_pairs;
    int ambiguous_feature_junctions;
    /** 请求了质量精修，但纹理保护要求保持逐角 UV 拓扑，因此本次精修被跳过。 */
    int quality_refinement_skipped_for_texture;
} ManuMeshSimplifyReport;

/** @brief 带大小版本的几何和拓扑网格统计信息。 */
typedef struct ManuMeshMeshStats {
    /**
   * 网格统计信息的输出存储。当前源码对 manumesh_compute_mesh_stats 的调用通过带大小信息的内联包装器
   * 传入 sizeof(ManuMeshMeshStats)，因此无需预先初始化此对象。在统计调用前需要一个已初始化的独立值时，
   * 请调用 manumesh_mesh_stats_init。
   */
    size_t struct_size;
    unsigned int abi_version;
    int vertices;
    int faces;
    int edges;
    int boundary_edges;
    int non_manifold_edges;
    double area;
    double mean_triangle_quality;
    double min_triangle_quality;
    double mean_edge_length;
    double edge_length_cv;
} ManuMeshMeshStats;

/** @return 静态的以空字符结尾的语义版本字符串。 */
MANUMESH_API const char* manumesh_version(void);
/**
 * @param[in] status 稳定的状态值。
 * @return 静态的以空字符结尾的英文状态名称。
 */
MANUMESH_API const char* manumesh_status_message(ManuMeshStatus status);

/** @return 新上下文；分配失败时为 NULL。 */
MANUMESH_API ManuMeshContext* manumesh_context_create(void);
/** @param[in] context 待销毁的上下文；接受 NULL。 */
MANUMESH_API void manumesh_context_destroy(ManuMeshContext* context);
/**
 * 返回此上下文记录的最后一条错误消息，若无则返回空字符串。
 *
 * 生命周期：返回指针指向上下文拥有的存储，仅保证在使用同一上下文进行下一次 ManuMesh API 调用之前
 *（任何调用都可能清除或替换消息）或上下文销毁之前有效。如需超出该窗口使用，请复制字符串。上下文不是
 * 线程安全的；参见头文件说明。
 */
MANUMESH_API const char* manumesh_context_last_error(const ManuMeshContext* context);
/** @param[in,out] context 待清除错误字符串的上下文；必须非空。 */
MANUMESH_API void manumesh_context_clear_error(ManuMeshContext* context);

/**
 * @param[in,out] context 记录分配错误的上下文。
 * @return 新的空网格句柄；上下文无效或分配失败时为 NULL。
 */
MANUMESH_API ManuMeshMeshHandle* manumesh_mesh_create(ManuMeshContext* context);
/** @param[in] mesh 待销毁的网格句柄；接受 NULL。 */
MANUMESH_API void manumesh_mesh_destroy(ManuMeshMeshHandle* mesh);
/**
 * @param[in,out] context 错误上下文。
 * @param[in,out] mesh 待清除几何的网格。
 * @retval MANUMESH_STATUS_OK 网格为空。
 * @retval MANUMESH_STATUS_INVALID_ARGUMENT 必需指针为 NULL。
 */
MANUMESH_API ManuMeshStatus manumesh_mesh_clear(ManuMeshContext* context, ManuMeshMeshHandle* mesh);
/**
 * @brief 使用调用方拥有的位置和三角形数组替换网格。
 * @param[in,out] context 错误上下文。
 * @param[in,out] mesh 目标网格句柄。
 * @param[in] vertices 包含 `vertex_count` 个位置的数组。
 * @param[in] vertex_count 位置数量。
 * @param[in] faces 包含 `face_count` 个三角形的数组。
 * @param[in] face_count 三角形数量。
 * @return 状态；无效索引和非有限值会被拒绝。
 * @note 输入数组会被复制，函数返回后可以释放。
 */
MANUMESH_API ManuMeshStatus manumesh_mesh_set_data(
    ManuMeshContext* context,
    ManuMeshMeshHandle* mesh,
    const ManuMeshVec3* vertices,
    size_t vertex_count,
    const ManuMeshFace* faces,
    size_t face_count
);
/**
 * vertex_count 或 face_count 可以有一个为 null，以只请求一种计数；两者都为 null 属于无效参数。
 * @param[in,out] context 错误上下文。
 * @param[in] mesh 源网格。
 * @param[out] vertex_count 可选的顶点计数目标。
 * @param[out] face_count 可选的面计数目标。
 * @return 状态码。
 */
MANUMESH_API ManuMeshStatus manumesh_mesh_get_counts(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, size_t* vertex_count, size_t* face_count
);
/**
 * @brief 将顶点复制到调用方拥有的存储中。
 * @param[in,out] context 错误上下文。
 * @param[in] mesh 源网格。
 * @param[out] vertices 输出数组；仅当容量为零时可以为 NULL。
 * @param[in] vertex_capacity 可用数组元素数量。
 * @param[out] vertices_written 所需/已写入元素数量。
 * @retval MANUMESH_STATUS_BUFFER_TOO_SMALL 容量小于所需数量。
 */
MANUMESH_API ManuMeshStatus manumesh_mesh_copy_vertices(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshVec3* vertices,
    size_t vertex_capacity,
    size_t* vertices_written
);
/** 与 manumesh_mesh_copy_vertices() 相同的约定，用于三角形索引。 */
MANUMESH_API ManuMeshStatus manumesh_mesh_copy_faces(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshFace* faces,
    size_t face_capacity,
    size_t* faces_written
);

/**
 * @brief 根据文件扩展名加载 STL 或 OBJ 几何。
 * @param[in,out] context 错误上下文。
 * @param[in] path 以空字符结尾的 UTF-8 输入路径。
 * @param[in,out] mesh 目标网格，成功时替换。
 * @param[in] merge_relative_epsilon STL 重合顶点合并的相对容差。
 * @return I/O、校验、分配或成功状态。
 */
MANUMESH_API ManuMeshStatus
manumesh_load_mesh(ManuMeshContext* context, const char* path, ManuMeshMeshHandle* mesh, double merge_relative_epsilon);
/**
 * @brief 将严格有效的网格写为 ASCII STL。
 * @param[in,out] context 错误上下文。
 * @param[in] path 以空字符结尾的 UTF-8 目标路径。
 * @param[in] mesh 源网格。
 * @param[in] solid_name 可选 STL 实体标签；NULL 使用默认值。
 * @return I/O、校验、分配或成功状态。
 */
MANUMESH_API ManuMeshStatus manumesh_save_ascii_stl(
    ManuMeshContext* context, const char* path, const ManuMeshMeshHandle* mesh, const char* solid_name
);
/**
 * @brief 将严格有效的网格写为标准小端二进制 STL。
 * @param[in,out] context 错误上下文。
 * @param[in] path 以空字符结尾的 UTF-8 目标路径。
 * @param[in] mesh 源网格。
 * @return I/O、校验、分配或成功状态。
 */
MANUMESH_API ManuMeshStatus
manumesh_save_binary_stl(ManuMeshContext* context, const char* path, const ManuMeshMeshHandle* mesh);

/**
 * @brief 根据稳定名称生成一个内置解析/测试网格。
 * @param[in,out] context 错误上下文。
 * @param[in] name 以空字符结尾的生成器名称。
 * @param[in] n 由所选生成器解释的分辨率参数。
 * @param[in,out] mesh 目标网格，成功时替换。
 * @return 无效参数、分配或成功状态。
 */
MANUMESH_API ManuMeshStatus
manumesh_generate_mesh(ManuMeshContext* context, const char* name, int n, ManuMeshMeshHandle* mesh);

/** 按给定缓冲区字节数初始化独立特征检测选项。 */
MANUMESH_API ManuMeshStatus
manumesh_feature_options_init_with_size(ManuMeshFeatureOptions* options, size_t struct_capacity);
/**
 * 为已构建调用方保留的旧 ABI-v1 无容量符号。它只初始化该符号首次发布时的
 * `ManuMeshFeatureOptions` 布局；当前源码通过下方 inline alias 传入当前公共结构体大小。
 */
MANUMESH_API void manumesh_feature_options_init(ManuMeshFeatureOptions* options);

/**
 * @brief 检测活动特征图边并复制到调用方拥有的缓冲区。
 * @param[in,out] context 错误上下文。
 * @param[in] mesh 输入三角网格。
 * @param[in] options 已初始化的检测选项；NULL 使用库默认值。
 * @param[out] edges 输出边数组；查询数量时可以为 NULL。
 * @param[in] edge_capacity `edges` 可容纳的元素数量；查询时传 0。
 * @param[out] edges_written 所需或已写入的元素数量。
 * @retval MANUMESH_STATUS_BUFFER_TOO_SMALL 容量小于所需数量。
 * @note 除成功和缓冲区不足外，若 `edges_written` 有效，函数将其置零。
 * @note 查询调用在结果非空时返回 `MANUMESH_STATUS_BUFFER_TOO_SMALL` 并给出所需容量；
 * 空结果返回 `MANUMESH_STATUS_OK` 且数量为零。缓冲区不足时不会部分写入 `edges`。
 * @note 被清理移除的边不输出；清理和整合产生的桥接边保留，并通过对应
 * 标志区分。函数不缓存结果，因此查询和复制两次调用会各执行一次检测。
 */
MANUMESH_API ManuMeshStatus manumesh_detect_feature_edges(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    const ManuMeshFeatureOptions* options,
    ManuMeshFeatureEdge* edges,
    size_t edge_capacity,
    size_t* edges_written
);

/**
 * @brief 检测特征边并输出稳定的特征边序号、输入边序号和恢复桥语义。
 *
 * 容量查询、错误恢复和不部分写入规则与 `manumesh_detect_feature_edges` 相同。
 * v2 结果按规范端点对及来源标志确定性排序，因此相同输入和选项的
 * `feature_edge_index` 可跨重复调用稳定比较。
 */
MANUMESH_API ManuMeshStatus manumesh_detect_feature_edges_v2(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    const ManuMeshFeatureOptions* options,
    ManuMeshFeatureEdgeV2* edges,
    size_t edge_capacity,
    size_t* edges_written
);

/**
 * 带大小信息的初始化器。库最多写入 struct_capacity 字节，在 struct_size 中记录初始化大小；
 * 当 struct_capacity 大于库当前类型时，忽略未知的未来尾部字节。
 */
MANUMESH_API ManuMeshStatus
manumesh_simplify_options_init_with_size(ManuMeshSimplifyOptions* options, size_t struct_capacity);
/** @copydoc manumesh_simplify_options_init_with_size */
MANUMESH_API ManuMeshStatus
manumesh_simplify_report_init_with_size(ManuMeshSimplifyReport* report, size_t struct_capacity);
/** @copydoc manumesh_simplify_options_init_with_size */
MANUMESH_API ManuMeshStatus manumesh_mesh_stats_init_with_size(ManuMeshMeshStats* stats, size_t struct_capacity);

/**
 * 为已构建调用方保留的旧 ABI v1 符号。它们只初始化随这些符号发布的原始 v1 布局。
 * 新源码包含下面的兼容宏，并透明地以当前公共结构体大小调用带大小信息的入口。
 */
MANUMESH_API void manumesh_simplify_options_init(ManuMeshSimplifyOptions* options);
/** 初始化简化报告的原始 ABI-v1 前缀。 */
MANUMESH_API void manumesh_simplify_report_init(ManuMeshSimplifyReport* report);
/** 初始化网格统计结构的原始 ABI-v1 前缀。 */
MANUMESH_API void manumesh_mesh_stats_init(ManuMeshMeshStats* stats);

/**
 * 带容量信息的输出入口。非空输出缓冲区必须包含完整的 abi_version 字段。
 * 库最多写入给定容量和当前结构体大小中的较小值；更大的未知尾部保持不变。
 * report 可以为 null，因为简化诊断是可选的。manumesh_compute_mesh_stats_with_size 要求 stats 非空。
 */
/**
 * @brief 使用容量受限的诊断输出简化网格。
 * @param[in,out] context 错误上下文。
 * @param[in] input 源网格；不能与 `output` 或已销毁存储别名。
 * @param[in] options 已初始化且 ABI 兼容的选项。
 * @param[in,out] output 目标网格，仅成功时替换。
 * @param[out] report 可选的前缀兼容报告缓冲区。
 * @param[in] report_capacity `report` 处可写字节数；report 为 NULL 时忽略。
 * @return 校验、算法、分配或成功状态。
 */
MANUMESH_API ManuMeshStatus manumesh_simplify_mesh_with_report_size(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* input,
    const ManuMeshSimplifyOptions* options,
    ManuMeshMeshHandle* output,
    ManuMeshSimplifyReport* report,
    size_t report_capacity
);
/**
 * @brief 将网格统计信息计算到容量受限的输出结构中。
 * @param[in,out] context 错误上下文。
 * @param[in] mesh 源网格。
 * @param[out] stats 必需的前缀兼容输出缓冲区。
 * @param[in] stats_capacity `stats` 处可写字节数。
 * @return 校验、容量、分配或成功状态。
 */
MANUMESH_API ManuMeshStatus manumesh_compute_mesh_stats_with_size(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshMeshStats* stats, size_t stats_capacity
);

/**
 * 为已构建调用方保留的旧 ABI v1 输出符号。它们不检查输出内存，只写入最初发布的 v1 报告/统计布局。
 * 当前源码调用会重定向到带容量信息的入口。
 */
/** @brief 旧 ABI-v1 简化符号；新源码通过宏路由到带大小信息的入口。 */
MANUMESH_API ManuMeshStatus manumesh_simplify_mesh(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* input,
    const ManuMeshSimplifyOptions* options,
    ManuMeshMeshHandle* output,
    ManuMeshSimplifyReport* report
);
/** @brief 旧 ABI-v1 统计符号；新源码通过宏路由到带大小信息的入口。 */
MANUMESH_API ManuMeshStatus
manumesh_compute_mesh_stats(ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshMeshStats* stats);

#if !defined(MANUMESH_C_API_IMPLEMENTATION) && !defined(MANUMESH_DISABLE_SIZE_AWARE_ALIASES) &&                        \
    !defined(MANUMESH_DISABLE_SIZE_AWARE_INIT_MACROS)
#if defined(_MSC_VER) && !defined(__cplusplus)
#define MANUMESH_C_API_INLINE __inline
#else
#define MANUMESH_C_API_INLINE inline
#endif

static MANUMESH_C_API_INLINE void manumesh_detail_simplify_options_init_current(ManuMeshSimplifyOptions* options) {
    (void)manumesh_simplify_options_init_with_size(options, sizeof(ManuMeshSimplifyOptions));
}

static MANUMESH_C_API_INLINE void manumesh_detail_feature_options_init_current(ManuMeshFeatureOptions* options) {
    (void)manumesh_feature_options_init_with_size(options, sizeof(ManuMeshFeatureOptions));
}

static MANUMESH_C_API_INLINE void manumesh_detail_simplify_report_init_current(ManuMeshSimplifyReport* report) {
    (void)manumesh_simplify_report_init_with_size(report, sizeof(ManuMeshSimplifyReport));
}

static MANUMESH_C_API_INLINE void manumesh_detail_mesh_stats_init_current(ManuMeshMeshStats* stats) {
    (void)manumesh_mesh_stats_init_with_size(stats, sizeof(ManuMeshMeshStats));
}

static MANUMESH_C_API_INLINE ManuMeshStatus manumesh_detail_simplify_mesh_current(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* input,
    const ManuMeshSimplifyOptions* options,
    ManuMeshMeshHandle* output,
    ManuMeshSimplifyReport* report
) {
    return manumesh_simplify_mesh_with_report_size(
        context, input, options, output, report, sizeof(ManuMeshSimplifyReport)
    );
}

static MANUMESH_C_API_INLINE ManuMeshStatus manumesh_detail_compute_mesh_stats_current(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshMeshStats* stats
) {
    return manumesh_compute_mesh_stats_with_size(context, mesh, stats, sizeof(ManuMeshMeshStats));
}

#undef MANUMESH_C_API_INLINE

#define manumesh_feature_options_init manumesh_detail_feature_options_init_current
#define manumesh_simplify_options_init manumesh_detail_simplify_options_init_current
#define manumesh_simplify_report_init manumesh_detail_simplify_report_init_current
#define manumesh_mesh_stats_init manumesh_detail_mesh_stats_init_current
#define manumesh_simplify_mesh manumesh_detail_simplify_mesh_current
#define manumesh_compute_mesh_stats manumesh_detail_compute_mesh_stats_current
#endif

#ifdef __cplusplus
}
#endif

/** @} */
