# ManuMesh 工业化验证现状

本文记录 ManuMesh 在工业风格模型上的验证边界。结论基于当前源码和测试，而不是产品化承诺。当前 CLI 名称为 `manumesh.exe`。

这些验证指标应按 [`algorithm_essence.md`](algorithm_essence.md) 的分层理解：QEM/line quadrics 解释候选排序，feature graph 解释特征支撑，拒绝计数解释硬过滤器和目标面数之间的冲突。

## 当前验证入口

```powershell
$buildDir = "build/mingw-ninja-release"
$exe = "$buildDir/bin/manumesh.exe"
& $exe validate-features --ratio 0.20 --samples 1000
& $exe validate-external --ratio 0.25 --samples 800
cmake -E chdir $buildDir ctest -LE performance --output-on-failure
```

VS Code 中对应常用任务：

- `run: feature validation`
- `run: external validation`
- `test: mingw+ninja release`

## 当前工业相关测试点

- 圆孔、近圆孔、椭圆孔是否能检测并在 aggressive simplify 后保留。
- Fandisk 等硬边模型是否能达到目标面数并避免 generic crease 过度硬锁。
- Casting/NASA/OpenFOAM 类 STL 是否能保持基本拓扑稳定。
- `industrial-safe` 是否能通过质量、法线、局部误差和自交过滤减少危险 collapse。
- C API 是否能通过 opaque handle 处理真实 STL。

## 重要报告字段

| 字段 | 解读 |
| --- | --- |
| `termination_reason` | 是否达到目标，或因候选耗尽/拒绝上限停止。 |
| `solver_fallbacks` | placement 求解退化到端点/中点候选的次数，常提示局部 QEM 矩阵欠约束或病态。 |
| `min_line_weight` / `max_line_weight` | CLI metrics CSV 中的实际 line quadric 权重范围；C ABI 报告字段名为 `min_applied_line_weight` / `max_applied_line_weight`。 |
| `feature_rejected_collapses` | 特征策略拒绝总数。 |
| `primitive_feature_rejected_collapses` | primitive loop 保护拒绝。 |
| `generic_feature_rejected_collapses` | generic feature 拒绝，过高可能说明过度锁边。 |
| `feature_components` / `weak_feature_components` / `high_confidence_feature_components` | cleanup 后 feature graph component 的总数、弱证据 component 数和高置信 component 数，用来判断弱特征是否已经形成可保护支撑。 |
| `graph_cleanup_bridged_gaps` / `graph_cleanup_removed_spurs` / `graph_cleanup_merged_junctions` | cleanup 的 endpoint/close-junction bridge 都复核双端方向、evidence source 和 signed kind；计数异常升高仍要排查局部贪心误合并。 |
| `graph_consolidation_bridges` / `graph_consolidation_skipped_by_cap` | opt-in 跨 component endpoint recovery 的实际桥数与 cap 跳过次数；应结合 closure、branch-pair benchmark 和可视化判断收益。 |
| `feature_normal_filter_*` | 法线域预处理的迭代、变化面、保留边、mean/max angular change 和 edge indicator；只解释 detection evidence，不表示顶点几何已去噪。 |
| `junction_branch_pairs` / `ambiguous_junctions` | junction continuation 配对数量与仍有未配对分支的 junction 数。 |
| `surface_patches` / `closed_surface_patches` / `segmentation_ignored_recovery_edges` | feature-induced face partition 的规模、闭合 patch 数，以及因 bridge 不是原 mesh edge 而忽略的 barrier 数。 |
| `mean_feature_component_confidence` / `min_feature_component_confidence` | component-level confidence，综合强/弱证据比例、闭合率、junction、primitive residual 和 tensor persistence；QEM 的特征曲线软成本会消费该置信度。 |
| `mean_normal_tensor_local_scale` / `mean_normal_tensor_persistence` | normal-tensor 弱特征的局部尺度与多尺度 persistence 诊断，适合和 `feature-benchmark` 的 precision/recall 一起看。 |
| `mean_smooth_curvature_scale_stability` | stable-scale 模式下参考尺度的一致性；低值提示证据对邻域尺度敏感。 |
| `boundary_rejected_collapses` | 边界保护拒绝。 |
| `quality_rejected_collapses` | 三角形质量过滤拒绝。 |
| `normal_flip_rejected_collapses` | 法线偏转过滤拒绝。 |
| `self_intersection_rejected_collapses` | 局部自交过滤拒绝。 |
| `error_rejected_collapses` | 局部误差预算拒绝。 |

## 当前结论

ManuMesh 当前算法已经能作为工业三角网格简化模块的基础：它可构建、可测试、可通过 C/C++ 集成，并能对常见孔洞、硬边和边界场景给出诊断。

但 ManuMesh 仍不是完整工业几何内核。要进入更严格生产环境，还需要全局误差 envelope、属性传播、更多真实数据、装配级测试、崩溃/异常恢复、版本化 ABI 和更系统的性能基线。
