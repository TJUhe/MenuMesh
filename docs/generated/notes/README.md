# ManuMesh 生成笔记说明

`docs/generated/notes/` 保存 ManuMesh 的 HTML/PDF/ZIP 导出笔记和报告。它们是历史参考产物，不是当前 C++ 库的源代码布局说明；当前产品名为 ManuMesh，当前 C++ 命名空间为 `manumesh`；CMake 目标、include 路径和 CLI 名称仍沿用 `manumesh`、`manumesh`。

## 当前维护规则

- 如果 CLI、`SimplifyOptions`、特征检测、测试数据、验证输出或产品命名发生变化，相关 HTML/PDF 应重新生成或在索引中标注为历史资料。
- 本目录中的 `*.html` 可用于浏览算法解释、代码阅读笔记和实验报告；本轮已同步标注 `include/io`、`src/io` 和 Debug-only `src/debugUtil` 的当前源码位置。
- 2026-07-12：`manumesh-feature-recognition-pipeline.html`、`manumesh-loop-construction.html`、`manumesh-theory-explained.html`、`circular-feature-practice-results.html` 的算法描述已同步到当日源码基线——有向二面角证据（`orientedDihedralAngle`，新诊断 `inconsistentWindingEdges`）、Taubin 圆拟合与 Halíř-Flusser 椭圆拟合（Kåsa/二阶矩为回退）、smooth-curvature 分支的三次 Monge patch 与 Ohtake 边零交叉极值、graph cleanup 的可选 `featureGraphMinWeakSpurStrength` 强度裁决、GH97 三级 placement 回退链与 Lindstrom-Turk 边界 placement、Wang 2008 的 featureBoost 队列优先级解耦、公共库函数 `manumesh::feature::matchCircularLoops`。`circular-feature-practice-results.html` 的历史实测数值保持原样。
- 2026-07-15：再次按当前源码审计特征识别 HTML。重点修正 smooth-curvature persistence 为“对最佳尺度的纯支持票数”（不要求相邻尺度或最粗尺度），明确 7 段 pipeline、退化面降级、强证据顶点排除区、normal-tensor 单端方向对齐、weak-evidence cleanup、五段 loop recovery、`minFeatureLoopVertices` 的真实语义、component confidence 公式，以及 feature-analysis CLI 与 precomputed `FeatureAnalysis` 简化入口的边界。主入口为 `manumesh-feature-recognition-pipeline.html`。
- `*.pdf` 是阅读版导出文件；当前未在本次文档更新中重写 PDF 二进制内容。
- `eye-care.css` 是导出 HTML 共用样式。

当前最权威的可维护文档仍然是：

| 路径 | 说明 |
| --- | --- |
| `docs/design/` | 架构、算法设计和验证记录；2026-07 新增 `error_handling_policy.md`、`algorithm_extension_protocol.md` 与架构蓝图 `architecture_v2_2026_07_12.md`。 |
| `docs/guide/` | 使用和集成指南。 |
| `docs/design/source_organization.md` | 当前源码布局和私有/公共边界。 |
| `docs/papers/` | 论文归档与索引；2026-07 新增确定性特征检测综述 `recent_deterministic_feature_detection_2026-07-11.md` 与开源网格库对照 `open_source_mesh_libraries.md` 更新。 |
| `README.md` | 项目入口说明。 |

如果发现 HTML 中的任务名、命令、源码路径或结论与当前 `.vscode/tasks.json`、`apps/manumesh/main.cpp`、`include/algorithms/` 或测试结果不一致，应以当前源码为准更新。
