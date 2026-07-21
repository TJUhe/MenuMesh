# ManuMesh 生成笔记说明

`documentation/generated/notes/` 保存 ManuMesh 的 HTML/PDF/ZIP 导出笔记和报告。它们是历史参考产物，不是当前 C++ 库的源代码布局说明；当前产品名为 ManuMesh，当前 C++ 命名空间为 `manumesh`；CMake 目标、include 路径和 CLI 名称仍沿用 `manumesh`、`manumesh`。

## 当前维护规则

- 如果 CLI、`SimplifyOptions`、特征检测、测试数据、验证输出或产品命名发生变化，相关 HTML/PDF 应重新生成或在索引中标注为历史资料。
- 本目录中的 `*.html` 可用于浏览算法解释、代码阅读笔记和实验报告；本轮已同步标注 `include/io`、`src/io` 和 Debug-only `src/debugUtil` 的当前源码位置。
- 2026-07-12：`manumesh-feature-recognition-pipeline.html`、`manumesh-loop-construction.html`、`manumesh-theory-explained.html`、`circular-feature-practice-results.html` 的算法描述已同步到当日源码基线；2026-07-15 又把二面角正典位置更新为 `common::computeOrientedDihedralAngle`，并保留 `inconsistentWindingEdges` 诊断。Taubin 圆拟合与 Halíř-Flusser 椭圆拟合（Kåsa/二阶矩为回退）、smooth-curvature 三次 Monge patch 与 Ohtake 边零交叉极值、weak-spur 强度裁决、GH97 三级 placement 回退链、Lindstrom-Turk 边界 placement、Wang 2008 featureBoost 解耦和公共 `manumesh::feature::matchCircularLoops` 均已同步；`circular-feature-practice-results.html` 的历史实测数值保持原样。
- 2026-07-15：按当前源码把特征识别 HTML 更新为 9 阶段 pipeline，并补入 opt-in normal filter、smooth stable-scale、共享 graph compatibility、component consolidation、junction branch pairing、surface patches、扩展 benchmark 与 size-aware C ABI；历史实验数字保持原样。主入口为 `manumesh-feature-recognition-pipeline.html`。
- 2026-07-15：主入口进一步加入 MathJax 公式专题、逐阶段伪代码、源码行号、数值算例和五组实跑 CLI；明确 normal filter 当前进入有向二面角与 smooth-curvature，但 normal tensor 仍从原始三角形几何重算法向。
- 2026-07-15（当前补丁）：smooth-curvature 与 weak-spur 参数已贯通 `SimplifyOptions`、`simplify` CLI 和 size-aware C ABI；feature/simplify 报告补齐绕向冲突、cleanup cap、圆恢复截断诊断。公共绕向感知二面角下沉到 `common::computeOrientedDihedralAngle`，退化面 point quadric 改为代表面积加权，局部相交检查覆盖新一环内部并区分合法共享拓扑接触；OBJ 凹多边形改用投影 ear clipping。归档 HTML/PDF 仍是历史快照，以本目录当前 HTML 与 `documentation/design`/`documentation/guide` 为准。
- 2026-07-19：新增 `manumesh-feature-recognition-gtest-debug-learning-plan.html`，按 GTest 调试视角组织 10 个递进学习单元，并汇总 VS Code 过滤调试、九阶段观察点和 Debug-only `debugUtil` HTML 线框工具的实际使用方法。
- 2026-07-21：新增网格简化、core/IO/analysis、API/CLI/SDK 三份 GTest 调试学习计划；统一使用 VS2019 CMake preset，按最小机制测试、主调用链、断点观察量和最终验证门组织学习顺序。
- `*.pdf` 是阅读版导出文件；当前未在本次文档更新中重写 PDF 二进制内容。
- `eye-care.css` 是导出 HTML 共用样式。

当前最权威的可维护文档仍然是：

| 路径 | 说明 |
| --- | --- |
| `documentation/design/` | 架构、算法设计和验证记录；2026-07 新增 `error_handling_policy.md`、`algorithm_extension_protocol.md` 与架构蓝图 `architecture_v2_2026_07_12.md`。 |
| `documentation/guide/` | 使用和集成指南。 |
| `documentation/design/source_organization.md` | 当前源码布局和私有/公共边界。 |
| `documentation/papers/` | 论文归档与索引；2026-07 新增确定性特征检测综述 `recent_deterministic_feature_detection_2026-07-11.md` 与开源网格库对照 `open_source_mesh_libraries.md` 更新。 |
| `README.md` | 项目入口说明。 |

如果发现 HTML 中的任务名、命令、源码路径或结论与当前 `.vscode/tasks.json`、`apps/main.cpp`、`include/algorithms/` 或测试结果不一致，应以当前源码为准更新。
