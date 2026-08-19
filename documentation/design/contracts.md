# 公共契约

## 输入校验

算法阶段可以在宽松入口中保留坐标共线造成的零面积面，但输入校验仍拒绝越界索引、重复面
顶点、非有限坐标、未对齐或非有限 UV。`validateMeshGeometryLenient` 用于分析/简化入口；
`validateMeshGeometry` 和 STL 导出使用严格面积校验。`MeshTopology::build` 的默认
`validate=true` 会拒绝越界索引和重复索引面，同时使用宽松面积校验，因此不会把所有零面积面
都拒绝；宽松分析入口会自行记录并降级处理。`countDegenerateFaces` 明确报告被宽松入口容忍的退化面。

带失败状态的修改型入口（例如 `appendMesh`、I/O 和 C ABI 的 simplify/load/save）先在候选副本上
校验，成功后一次提交；失败不替换原对象。明确声明就地修改的 Mesh 工具
（`removeDegenerateFaces`、`removeUnusedVertices`、`reverseFaceWindings`）不提供这一事务保证，
应按各自头文件契约使用。`MeshTopology::build` 返回 `Result<MeshTopology>`，`summarizeMeshTopology`
报告共享边连通分量、边界/非流形边、闭合性和绕序一致性。只共享顶点的面不属于同一完整边组件。

## 错误模型

数据、I/O 和可预期算法失败按现有入口返回：拓扑/分区等新值型 API 使用 `Status`/`Result<T>`，
STL/OBJ 兼容 I/O 仍使用 `bool` 加可选诊断字符串，C ABI 使用状态码。C++ 前置条件和无效
调用通常抛标准异常（例如 `std::invalid_argument`、`std::out_of_range`）；结果值缺失等程序员
误用也可能抛 `std::logic_error` 或其他标准异常。异常不能越过 C ABI。状态文本是诊断，
`StatusCode` 才是稳定分类；新增入口应选择一种一致的返回边界，不要在同一入口混用多套失败表示。

## C ABI

ABI 版本当前为 v1。输入 options、输出 report/stats 都带 `struct_size` 和 `abi_version`；新
字段只能追加到尾部。优先使用 `*_init_with_size`、`*_with_report_size` 等显式容量入口。变长
数组通过 `capacity == 0` 查询所需数量；数组容量不足返回 `BUFFER_TOO_SMALL` 且不部分写入。
size-aware 的 struct 输出允许调用方提供不小于 ABI 头的旧前缀，库只写入该前缀内存在的字段。
公共 C 结构体使用 pack-8，导出函数使用 `__cdecl`；调用方不得改变这些 ABI 布局约束。
旧无容量符号只为已构建调用方保留，由头文件别名转到当前实现。

## 所有权与生命周期

`Mesh`、`PlainMesh`、报告和 `FeatureAnalysis` 均为调用方拥有的值/容器。拓扑 view 借用其
`MeshTopology` 的连续存储，不能越过拓扑对象生命周期。`FeatureAnalysisViews.h` 中的
`FeatureEvidenceView`、`FeatureCurveView`、`FeatureSegmentationView` 和
`FeatureDiagnosticsView` 都是不拥有数据的只读 view；它们不复制 `FeatureAnalysis`，必须在
分析对象存活且不被修改时使用。C handle 由 context 管理，销毁前调用方负责等待在途操作完成。

## 确定性和线程

默认执行模式是串行。显式 `ExecutionMode::Parallel` 只并行不重叠的纯计算，结果必须与串行
模式的排序、归约和容差一致；拓扑写入和全局图协调仍串行。oneTBB 是私有后端，公共头不暴露
其类型。C handle 内部同步，context 不保证线程安全。

## 数据集边界

MMPD v1 的 checksum、目录和分区记录是存储完整性契约。它不提供全局拓扑语义、跨分区 halo、
owner/ghost 或 out-of-core 算法保证。调用方需要这些语义时必须另建公共模块和验收标准。
