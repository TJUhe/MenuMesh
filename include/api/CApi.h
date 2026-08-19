/**
 * @file include/api/CApi.h
 * @brief 声明 ManuMesh 稳定 C ABI 接口。
 * @ingroup manumesh_c_api
 *
 * @details C 边界检查指针和容量，将失败转换为状态码，并确保 C++ 异常不会越过 ABI。
 */

#pragma once

#include "Export.h"

#include <stddef.h>
#include <stdint.h>

#if defined(_MSC_VER)
#define MANUMESH_CDECL __cdecl
#else
#define MANUMESH_CDECL
#endif

/**
 * @addtogroup manumesh_c_api
 * @{
 *
 * @brief ManuMesh 稳定 C ABI 版本 1。
 *
 * @par 线程安全
 * ManuMeshContext 不是线程安全的。没有外部同步时不要跨线程共享同一个上下文；应为每个线程使用独立上下文。
 * 每个 ManuMeshMeshHandle 内部独立同步，未共享 context 时可以从多个线程并发读取和修改同一句柄。
 * 原地简化以及双句柄简化会在整个算法和提交期间保持相关句柄锁，避免并发更新被旧快照覆盖。
 * 涉及两个句柄的 copy/append 会以无死锁方式同时锁定二者。同一句柄自拷贝、自追加和原地简化均受支持。
 * destroy 不参与该同步：销毁 context 或 mesh handle 前，调用方必须保证不再有使用该对象的并发或未完成调用。
 *
 * @par 错误上下文
 * 除非函数另有说明，`context` 可以为 NULL；操作仍会执行并返回状态码，但错误文本会被丢弃。
 * `manumesh_context_last_error(NULL)` 返回固定诊断文本，`manumesh_context_clear_error(NULL)` 不执行任何操作。
 *
 * @par 纹理坐标
 * 初始 v1 几何入口只接受顶点位置和面索引，并会清除已有 UV；扩展的逐面 UV
 * 入口 `manumesh_mesh_get/set/copy_face_texcoords` 使用固定的面角布局访问 UV。
 * UV 坐标归属于面角而不是几何顶点。
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ManuMeshContext ManuMeshContext;       ///< 不透明的错误和操作上下文。
typedef struct ManuMeshMeshHandle ManuMeshMeshHandle; ///< 不透明的自有三角网格句柄。

#define MANUMESH_ABI_VERSION 1u

/** @brief 可失败 C ABI 函数返回的稳定状态。 */
typedef enum ManuMeshStatus {
    MANUMESH_STATUS_OK = 0,                ///< 操作成功完成。
    MANUMESH_STATUS_INVALID_ARGUMENT = 1,  ///< 指针、值、网格或 ABI 约定无效。
    MANUMESH_STATUS_BUFFER_TOO_SMALL = 2,  ///< 调用方拥有的输出存储容量不足。
    MANUMESH_STATUS_IO_ERROR = 3,          ///< 文件解析、创建、读取或写入失败。
    MANUMESH_STATUS_ALGORITHM_ERROR = 4,   ///< 几何算法无法完成。
    MANUMESH_STATUS_OUT_OF_MEMORY = 5,     ///< 分配失败。
    MANUMESH_STATUS_INVALID_MESH = 6,      ///< 网格索引、拓扑或几何不满足请求操作。
    MANUMESH_STATUS_UNSUPPORTED_FORMAT = 7 ///< 输入格式或当前 ABI 不支持。
} ManuMeshStatus;

#pragma pack(push, 8)

/** @brief 用于缩放 line-quadric 排序代价的空间策略。 */
typedef enum ManuMeshWeightMode {
    MANUMESH_WEIGHT_MODE_UNIFORM = 0,          /**< 常量 line-quadric 权重。 */
    MANUMESH_WEIGHT_MODE_DIHEDRAL = 1,         /**< 二面角特征敏感权重。 */
    MANUMESH_WEIGHT_MODE_HEIGHT = 2,           /**< 高度相关权重。 */
    MANUMESH_WEIGHT_MODE_X_BAND = 3,           /**< X 区间权重。 */
    MANUMESH_WEIGHT_MODE_NORMAL_TENSOR = 4,    /**< Normal Tensor 逐顶点持久性权重。 */
    MANUMESH_WEIGHT_MODE_SMOOTH_CURVATURE = 5  /**< SmoothCurvature 逐顶点持久性权重。 */
} ManuMeshWeightMode;

/** @brief 简化运行停止的原因。 */
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

/** @brief 算法调用使用的执行模式。 */
typedef enum ManuMeshExecutionMode {
    MANUMESH_EXECUTION_MODE_SERIAL = 0,
    MANUMESH_EXECUTION_MODE_PARALLEL = 1
} ManuMeshExecutionMode;

/**
 * @brief 存储在 ManuMeshContext 上的带大小版本执行约束。
 *
 * 公共 C ABI 不暴露 oneTBB 类型。并行后端不可用时，C++ 算法契约仍允许
 * 串行回退；调用方可用 `manumesh_parallel_execution_available` 查询能力。
 */
typedef struct ManuMeshExecutionOptions {
    size_t struct_size;
    unsigned int abi_version;
    ManuMeshExecutionMode mode;
    int max_concurrency;       ///< 0 表示由后端选择；正数限制并发度。
    size_t min_items_per_task; ///< 调度块的最小元素数，必须为正。
} ManuMeshExecutionOptions;

/** @brief 以模型单位表示的双精度三维坐标。 */
typedef struct ManuMeshVec3 {
    double x; ///< X 坐标。
    double y; ///< Y 坐标。
    double z; ///< Z 坐标。
} ManuMeshVec3;

/** @brief 以模型单位表示的双精度二维纹理坐标。 */
typedef struct ManuMeshVec2 {
    double u;
    double v;
} ManuMeshVec2;

/**
 * @brief 包含三个从零开始顶点索引的三角形面。
 *
 * 索引顺序决定三角面的法向方向，但 C ABI 不要求统一为逆时针，也不会自动翻转索引。
 * `manumesh_mesh_set_data` 与 `manumesh_mesh_copy_faces` 按原顺序传递索引；
 * 下游需要一致法向时，调用方应保持相邻三角面的绕序一致。
 */
typedef struct ManuMeshFace {
    int v[3]; ///< 指向顶点数组的从零开始索引，顺序按输入保留。
} ManuMeshFace;

/** @brief 一个三角形面的逐角纹理坐标；valid 为零时坐标被定义为全零。 */
typedef struct ManuMeshFaceTexCoords {
    ManuMeshVec2 uv[3];
    int valid;
} ManuMeshFaceTexCoords;

/** @brief 一个无向边及其入射面分类。端点总是满足 a < b。 */
typedef struct ManuMeshEdge {
    int a;
    int b;
    size_t face_count;
    int boundary;
    int non_manifold;
} ManuMeshEdge;

/** @brief 轴对齐包围盒；valid 为零时 min/max 均为零向量。 */
typedef struct ManuMeshBounds {
    ManuMeshVec3 min;
    ManuMeshVec3 max;
    int valid;
} ManuMeshBounds;

/** @brief 基于共享边计算的连通性、边分类与绕序摘要。 */
typedef struct ManuMeshTopologySummary {
    size_t connected_face_components;
    size_t unique_edges;
    size_t boundary_edges;
    size_t non_manifold_edges;
    int closed_manifold;
    int consistently_oriented;
} ManuMeshTopologySummary;

/**
 * @brief 独立特征检测的带大小版本 C ABI 选项。
 *
 * 调用 `manumesh_detect_feature_edges` 前必须使用
 * `manumesh_feature_options_init` 初始化。对同一 ABI 版本，库接受尾部字段
 * 更少的旧结构体大小；未出现的尾部字段使用库默认值。输入声明的
 * `struct_size` 不得大于当前公开结构体大小；若调用方使用了更新、更大的布局，
 * 应使用相应版本的库入口，以避免库在没有独立容量参数时读取未知尾部。
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
   * 输入声明的 `struct_size` 大于当前结构体大小会被拒绝；这保证没有独立容量参数时
   * 不会读取调用方未知的未来尾部。
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
    /** 0.x flat-API compatibility default is 16; a feature_options override uses its own detector default. */
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
    /** 纹理保护配置；仅在输入含逐角 UV 且显式启用时生效。尾部扩展字段。 */
    int preserve_texture;
    double texture_weight;
    double texture_seam_tolerance;
    double min_texture_area_ratio;
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
    /** 纹理保护硬过滤与更新诊断。尾部扩展字段。 */
    int texture_rejected_collapses;
    int texture_protected_edges;
    int texture_apply_failures;
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

#pragma pack(pop)

/** @return 静态的以空字符结尾的语义版本字符串。 */
MANUMESH_API const char* MANUMESH_CDECL manumesh_version(void);
/**
 * @param[in] status 稳定的状态值。
 * @return 静态的以空字符结尾的英文状态名称。
 */
MANUMESH_API const char* MANUMESH_CDECL manumesh_status_message(ManuMeshStatus status);

/** @return 新上下文；分配失败时为 NULL。 */
MANUMESH_API ManuMeshContext* MANUMESH_CDECL manumesh_context_create(void);
/**
 * @param[in] context 待销毁的上下文；接受 NULL。
 * @pre 不得与任何读取或写入此 context 的调用并发。
 */
MANUMESH_API void MANUMESH_CDECL manumesh_context_destroy(ManuMeshContext* context);
/**
 * 返回此上下文记录的最后一条错误消息，若无则返回空字符串。传入 NULL 时返回固定的诊断文本，
 * 不返回 NULL。
 *
 * 生命周期：返回指针指向上下文拥有的存储，仅保证在使用同一上下文进行下一次 ManuMesh API 调用之前
 *（任何调用都可能清除或替换消息）或上下文销毁之前有效。如需超出该窗口使用，请复制字符串。上下文不是
 * 线程安全的；参见头文件说明。
 */
MANUMESH_API const char* MANUMESH_CDECL manumesh_context_last_error(const ManuMeshContext* context);
/** @param[in,out] context 待清除错误字符串的上下文；可以为 NULL，此时函数不执行任何操作。 */
MANUMESH_API void MANUMESH_CDECL manumesh_context_clear_error(ManuMeshContext* context);

/** 初始化完整执行选项，默认使用串行模式。 */
MANUMESH_API void MANUMESH_CDECL manumesh_execution_options_init(ManuMeshExecutionOptions* options);
/** 按调用方容量初始化执行选项前缀。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_execution_options_init_with_size(ManuMeshExecutionOptions* options, size_t struct_capacity);
/** 将执行约束复制到上下文；后续特征检测和简化调用读取该设置。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_context_set_execution_options(
    ManuMeshContext* context, const ManuMeshExecutionOptions* options
);
/** @return 当前库是否包含可用的内部并行后端。 */
MANUMESH_API int MANUMESH_CDECL manumesh_parallel_execution_available(void);
/** @return 静态后端名称（"oneTBB" 或 "serial"）。 */
MANUMESH_API const char* MANUMESH_CDECL manumesh_parallel_execution_backend(void);

/**
 * @param[in,out] context 可选的分配错误上下文；可以为 NULL。
 * @return 新的空网格句柄；仅分配失败时为 NULL。
 */
MANUMESH_API ManuMeshMeshHandle* MANUMESH_CDECL manumesh_mesh_create(ManuMeshContext* context);
/**
 * @param[in] mesh 待销毁的网格句柄；接受 NULL。
 * @pre 不得与任何使用此句柄的调用并发；调用方必须先等待全部在途调用结束。
 */
MANUMESH_API void MANUMESH_CDECL manumesh_mesh_destroy(ManuMeshMeshHandle* mesh);
/**
 * @param[in,out] context 错误上下文。
 * @param[in,out] mesh 待清除几何的网格。
 * @retval MANUMESH_STATUS_OK 网格已清空。
 * @retval MANUMESH_STATUS_INVALID_ARGUMENT 必需指针为空。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_clear(ManuMeshContext* context, ManuMeshMeshHandle* mesh);
/**
 * @brief 使用调用方拥有的位置和三角形数组替换网格。
 * @param[in,out] context 错误上下文。
 * @param[in,out] mesh 目标网格句柄。
 * @param[in] vertices 包含 `vertex_count` 个顶点坐标的数组。
 * @param[in] vertex_count 顶点数量。
 * @param[in] faces 包含 `face_count` 个三角形的数组。
 * @param[in] face_count 三角形数量。
 * @return 状态。无效索引、非有限坐标和面内重复顶点索引会被拒绝；
 *         顶点索引不同但几何面积为零的面会被接受，并由分析/简化报告为退化面。
 * @note 输入数组会被复制，函数返回后可以释放。
 * @note 校验失败时目标网格保持不变。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_set_data(
    ManuMeshContext* context,
    ManuMeshMeshHandle* mesh,
    const ManuMeshVec3* vertices,
    size_t vertex_count,
    const ManuMeshFace* faces,
    size_t face_count
);
/**
 * @brief 使用调用方拥有的几何和可选逐角 UV 数组原子替换网格。
 *
 * `face_texcoords` 为 NULL 时，不存储 UV；否则必须指向 `face_count` 个条目。
 * 与 `manumesh_mesh_set_data` 相同，输入会被复制，校验失败时目标网格保持不变。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_set_data_with_texcoords(
    ManuMeshContext* context,
    ManuMeshMeshHandle* mesh,
    const ManuMeshVec3* vertices,
    size_t vertex_count,
    const ManuMeshFace* faces,
    size_t face_count,
    const ManuMeshFaceTexCoords* face_texcoords
);
/**
 * vertex_count 或 face_count 可以有一个为 null，以只请求一种计数；两者都为 null 属于无效参数。
 * @param[in,out] context 错误上下文。
 * @param[in] mesh 源网格。
 * @param[out] vertex_count 可选的顶点计数目标。
 * @param[out] face_count 可选的面计数目标。
 * @return 状态码。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_get_counts(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, size_t* vertex_count, size_t* face_count
);
/**
 * @brief 将顶点复制到调用方拥有的存储中。
 * @param[in,out] context 错误上下文。
 * @param[in] mesh 源网格。
 * @param[out] vertices 输出数组。查询调用（容量为零）可以为 NULL；源网格为空时也可以为 NULL。
 *                      容量足够且需要写入元素时必须提供非空数组。
 * @param[in] vertex_capacity 可用数组元素数量。
 * @param[out] vertices_written 必须非 NULL；成功时为已写入数量，容量不足时为所需数量。
 * @retval MANUMESH_STATUS_BUFFER_TOO_SMALL 容量小于所需数量。
 * @note 容量不足时不会部分写入 `vertices`。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_copy_vertices(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshVec3* vertices,
    size_t vertex_capacity,
    size_t* vertices_written
);
/** 与 manumesh_mesh_copy_vertices() 相同的约定，用于三角形索引；索引顺序按原样返回。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_copy_faces(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshFace* faces,
    size_t face_capacity,
    size_t* faces_written
);

/** @brief 深拷贝 source，并仅在成功时替换 destination；允许同句柄调用。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_mesh_copy(ManuMeshContext* context, const ManuMeshMeshHandle* source, ManuMeshMeshHandle* destination);
/** @brief 读取一个顶点；索引越界返回 INVALID_ARGUMENT 且不改写输出。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_get_vertex(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, size_t vertex_index, ManuMeshVec3* vertex
);
/** @brief 原子设置一个有限顶点；若会使网格无效则保持原网格不变。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_mesh_set_vertex(ManuMeshContext* context, ManuMeshMeshHandle* mesh, size_t vertex_index, ManuMeshVec3 vertex);
/** @brief 读取一个三角面；索引越界时不改写输出。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_mesh_get_face(ManuMeshContext* context, const ManuMeshMeshHandle* mesh, size_t face_index, ManuMeshFace* face);
/** @brief 原子设置一个三角面的三个顶点索引。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_mesh_set_face(ManuMeshContext* context, ManuMeshMeshHandle* mesh, size_t face_index, ManuMeshFace face);
/** @brief 返回轴对齐包围盒；空网格返回 valid=0 和全零边界。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_mesh_get_bounds(ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshBounds* bounds);
/** @brief 原子平移全部顶点；偏移及结果必须有限。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_mesh_translate(ManuMeshContext* context, ManuMeshMeshHandle* mesh, ManuMeshVec3 offset);
/**
 * @brief 使用行主序 4x4 齐次矩阵原子变换全部顶点。
 *
 * 每个顶点按 `(x,y,z,1)` 计算，并除以非零的齐次 w；矩阵和所有结果必须有限。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_mesh_transform(ManuMeshContext* context, ManuMeshMeshHandle* mesh, const double matrix_row_major[16]);
/** @brief 移除未使用顶点并重写索引；无效面会被删除。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_compact(ManuMeshContext* context, ManuMeshMeshHandle* mesh);
/**
 * @brief 校验网格。strict 非零时零面积面也会返回 INVALID_MESH。
 * @param context 可选错误上下文。
 * @param mesh 要校验的只读网格句柄。
 * @param strict 非零时将零面积面视为无效；零时仅报告其数量。
 * @param[out] degenerate_face_count 可选；索引可安全读取时即使严格校验失败也会写入计数。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_validate(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, int strict, size_t* degenerate_face_count
);

/** 与顶点复制接口使用相同的容量查询和“不部分写入”约定。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_copy_face_areas(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, double* areas, size_t area_capacity, size_t* areas_written
);
/** @brief 返回全部面的总表面积；退化面贡献零。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_mesh_get_surface_area(ManuMeshContext* context, const ManuMeshMeshHandle* mesh, double* surface_area);
/**
 * @brief 返回严格有效、封闭二流形且绕序一致网格的有符号体积。
 *
 * 外向绕序通常为正，整体反转绕序后符号也会反转。开放、非流形或绕序冲突网格返回
 * `MANUMESH_STATUS_INVALID_MESH`，结果无法表示为有限 double 时同样返回该状态。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_mesh_get_signed_volume(ManuMeshContext* context, const ManuMeshMeshHandle* mesh, double* signed_volume);
/** @brief 返回按面积加权的表面质心；没有非退化面时返回零向量。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_get_surface_centroid(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshVec3* surface_centroid
);
/** @brief 复制逐面质心；容量不足或校验失败时不会部分写入。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_copy_face_centroids(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshVec3* centroids,
    size_t centroid_capacity,
    size_t* centroids_written
);
/** @brief 复制单位面法向；退化面为零向量，失败时不会部分写入。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_copy_face_normals(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshVec3* normals,
    size_t normal_capacity,
    size_t* normals_written
);
/** @brief 复制面积加权单位顶点法向；孤立/相消顶点为零向量。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_copy_vertex_normals(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshVec3* normals,
    size_t normal_capacity,
    size_t* normals_written
);
/** @brief 复制按端点字典序排列的唯一无向边及边界/非流形分类。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_copy_unique_edges(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshEdge* edges,
    size_t edge_capacity,
    size_t* edges_written
);
/**
 * @brief 返回按共享完整边定义的面连通分量、边分类、闭合性和绕序摘要。
 *
 * 空网格的 `closed_manifold` 和 `consistently_oriented` 均为零；仅共享顶点的面属于不同组件。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_get_topology_summary(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshTopologySummary* summary
);

/** @brief 查询网格是否至少有一个有效逐面 UV。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_mesh_has_texture_coordinates(ManuMeshContext* context, const ManuMeshMeshHandle* mesh, int* has_texcoords);
/** @brief 读取逐面 UV；无 UV 存储时返回 valid=0 和全零坐标。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_get_face_texcoords(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, size_t face_index, ManuMeshFaceTexCoords* texcoords
);
/** @brief 原子设置逐面 UV；valid=0 会存储确定性的全零坐标。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_set_face_texcoords(
    ManuMeshContext* context, ManuMeshMeshHandle* mesh, size_t face_index, const ManuMeshFaceTexCoords* texcoords
);
/** @brief 复制与面数组等长的逐面 UV；无 UV 的面输出 invalid/zero。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_mesh_copy_face_texcoords(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    ManuMeshFaceTexCoords* texcoords,
    size_t texcoord_capacity,
    size_t* texcoords_written
);

/** @brief 反转全部面绕序并同步交换逐角 UV。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_mesh_reverse_winding(ManuMeshContext* context, ManuMeshMeshHandle* mesh);
/** @brief 删除重复索引或零面积面，并压缩未使用顶点。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_mesh_remove_degenerate_faces(ManuMeshContext* context, ManuMeshMeshHandle* mesh, size_t* removed_face_count);
/** @brief 将 source 深拷贝追加到 destination；支持同句柄自追加并保持 UV 对齐。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_mesh_append(ManuMeshContext* context, ManuMeshMeshHandle* destination, const ManuMeshMeshHandle* source);

/**
 * @brief 根据文件扩展名加载 STL 或 OBJ 几何。
 * @param[in,out] context 错误上下文。
 * @param[in] path 以空字符结尾的 UTF-8 输入路径。
 * @param[in,out] mesh 目标网格，成功时替换。
 * @param[in] merge_relative_epsilon STL 重合顶点合并的相对容差。
 * @return I/O、校验、分配或成功状态。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_load_mesh(ManuMeshContext* context, const char* path, ManuMeshMeshHandle* mesh, double merge_relative_epsilon);
/**
 * @brief 将严格有效的网格写为 ASCII STL。
 * @param[in,out] context 错误上下文。
 * @param[in] path 以空字符结尾的 UTF-8 目标路径。
 * @param[in] mesh 源网格。
 * @param[in] solid_name 可选 STL 实体标签；NULL 使用默认值。
 * @return I/O、校验、分配或成功状态。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_save_ascii_stl(
    ManuMeshContext* context, const char* path, const ManuMeshMeshHandle* mesh, const char* solid_name
);
/**
 * @brief 将严格有效的网格写为标准小端二进制 STL。
 * @param[in,out] context 错误上下文。
 * @param[in] path 以空字符结尾的 UTF-8 目标路径。
 * @param[in] mesh 源网格。
 * @return I/O、校验、分配或成功状态。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_save_binary_stl(ManuMeshContext* context, const char* path, const ManuMeshMeshHandle* mesh);
/**
 * @brief 将严格有效网格写为 OBJ，并保留逐角 UV。
 *
 * 输出路径是 UTF-8；写入采用同目录临时文件替换，因此失败不会截断已有目标文件。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_save_obj(ManuMeshContext* context, const char* path, const ManuMeshMeshHandle* mesh);

/**
 * @brief 根据稳定名称生成一个内置解析/测试网格。
 * @param[in,out] context 错误上下文。
 * @param[in] name 以空字符结尾的生成器名称。
 * @param[in] n 由所选生成器解释的分辨率参数。
 * @param[in,out] mesh 目标网格，成功时替换。
 * @return 无效参数、分配或成功状态。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_generate_mesh(ManuMeshContext* context, const char* name, int n, ManuMeshMeshHandle* mesh);

/**
 * 按给定缓冲区字节数初始化独立特征检测选项。
 * `options` 不能为空，`struct_capacity` 至少要覆盖 `struct_size` 和 `abi_version` 字段。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_feature_options_init_with_size(ManuMeshFeatureOptions* options, size_t struct_capacity);
/**
 * 为已构建调用方保留的旧 ABI-v1 无容量符号。它只初始化该符号首次发布时的
 * `ManuMeshFeatureOptions` 布局；当前源码通过下方 inline alias 传入当前公共结构体大小。
 */
MANUMESH_API void MANUMESH_CDECL manumesh_feature_options_init(ManuMeshFeatureOptions* options);

/**
 * @brief 检测活动特征图边并复制到调用方拥有的缓冲区。
 * @param[in,out] context 错误上下文。
 * @param[in] mesh 输入三角网格。
 * @param[in] options 可选的已初始化检测选项；NULL 使用库默认值。
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
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_detect_feature_edges(
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
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_detect_feature_edges_v2(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* mesh,
    const ManuMeshFeatureOptions* options,
    ManuMeshFeatureEdgeV2* edges,
    size_t edge_capacity,
    size_t* edges_written
);

/**
 * 带大小信息的初始化器。库最多写入 struct_capacity 字节，在 struct_size 中记录初始化大小；
 * 当 struct_capacity 大于库当前类型时，忽略未知的未来尾部字节。输出指针不能为空，容量至少要覆盖
 * `struct_size` 和 `abi_version` 字段。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_simplify_options_init_with_size(ManuMeshSimplifyOptions* options, size_t struct_capacity);
/** @copydoc manumesh_simplify_options_init_with_size */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_simplify_report_init_with_size(ManuMeshSimplifyReport* report, size_t struct_capacity);
/** @copydoc manumesh_simplify_options_init_with_size */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_mesh_stats_init_with_size(ManuMeshMeshStats* stats, size_t struct_capacity);

/**
 * 为已构建调用方保留的旧 ABI v1 符号。它们只初始化随这些符号发布的原始 v1 布局。
 * 新源码包含下面的兼容宏，并透明地以当前公共结构体大小调用带大小信息的入口。
 */
MANUMESH_API void MANUMESH_CDECL manumesh_simplify_options_init(ManuMeshSimplifyOptions* options);
/** 初始化简化报告的原始 ABI-v1 前缀。 */
MANUMESH_API void MANUMESH_CDECL manumesh_simplify_report_init(ManuMeshSimplifyReport* report);
/** 初始化网格统计结构的原始 ABI-v1 前缀。 */
MANUMESH_API void MANUMESH_CDECL manumesh_mesh_stats_init(ManuMeshMeshStats* stats);

/**
 * 带容量信息的输出入口。非空输出缓冲区的容量必须至少覆盖完整的 `abi_version` 字段；
 * 库最多写入给定容量和当前结构体大小中的较小值，更大的未知尾部保持不变。
 * `report` 可以为 NULL，因为简化诊断是可选的；`manumesh_compute_mesh_stats_with_size` 要求 `stats` 非空。
 */
/**
 * @brief 使用容量受限的诊断输出简化网格。
 * @param[in,out] context 错误上下文。
 * @param[in] input 源网格；允许与 `output` 为同一句柄，此时在句柄锁保护下原子替换原网格。
 * @param[in] options 可选的已初始化且 ABI 兼容的选项；NULL 使用库默认简化选项。
 * @param[in,out] output 目标网格，仅成功时替换。
 * @param[out] report 可选的前缀兼容报告缓冲区。
 * @param[in] report_capacity `report` 处可写字节数；report 为 NULL 时忽略。
 * @return 校验、算法、分配或成功状态。
 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_simplify_mesh_with_report_size(
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
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_compute_mesh_stats_with_size(
    ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshMeshStats* stats, size_t stats_capacity
);

/**
 * 为已构建调用方保留的旧 ABI v1 输出符号。它们不接收容量参数，只按最初发布的 v1 报告/统计前缀写入，
 * 并使用与 v1 布局匹配的固定容量执行必要校验。当前源码调用会重定向到带容量信息的入口。
 */
/** @brief 旧 ABI-v1 简化符号；新源码通过宏路由到带大小信息的入口。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL manumesh_simplify_mesh(
    ManuMeshContext* context,
    const ManuMeshMeshHandle* input,
    const ManuMeshSimplifyOptions* options,
    ManuMeshMeshHandle* output,
    ManuMeshSimplifyReport* report
);
/** @brief 旧 ABI-v1 统计符号；新源码通过宏路由到带大小信息的入口。 */
MANUMESH_API ManuMeshStatus MANUMESH_CDECL
manumesh_compute_mesh_stats(ManuMeshContext* context, const ManuMeshMeshHandle* mesh, ManuMeshMeshStats* stats);

#if !defined(MANUMESH_C_API_IMPLEMENTATION) && !defined(MANUMESH_DISABLE_SIZE_AWARE_ALIASES) &&                        \
    !defined(MANUMESH_DISABLE_SIZE_AWARE_INIT_MACROS)
#if defined(_MSC_VER) && !defined(__cplusplus)
#define MANUMESH_C_API_INLINE __inline
#else
#define MANUMESH_C_API_INLINE inline
#endif

static MANUMESH_C_API_INLINE void
    MANUMESH_CDECL manumesh_detail_simplify_options_init_current(ManuMeshSimplifyOptions* options) {
    (void)manumesh_simplify_options_init_with_size(options, sizeof(ManuMeshSimplifyOptions));
}

static MANUMESH_C_API_INLINE void MANUMESH_CDECL
manumesh_detail_feature_options_init_current(ManuMeshFeatureOptions* options) {
    (void)manumesh_feature_options_init_with_size(options, sizeof(ManuMeshFeatureOptions));
}

static MANUMESH_C_API_INLINE void MANUMESH_CDECL
manumesh_detail_simplify_report_init_current(ManuMeshSimplifyReport* report) {
    (void)manumesh_simplify_report_init_with_size(report, sizeof(ManuMeshSimplifyReport));
}

static MANUMESH_C_API_INLINE void MANUMESH_CDECL manumesh_detail_mesh_stats_init_current(ManuMeshMeshStats* stats) {
    (void)manumesh_mesh_stats_init_with_size(stats, sizeof(ManuMeshMeshStats));
}

static MANUMESH_C_API_INLINE ManuMeshStatus MANUMESH_CDECL manumesh_detail_simplify_mesh_current(
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

static MANUMESH_C_API_INLINE ManuMeshStatus MANUMESH_CDECL manumesh_detail_compute_mesh_stats_current(
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
