# 调试与诊断

源码调试以精简的 `vs2019-debug` 为基线；它按功能目录生成 `Core`、`Common`、`Geometry`、
`MeshEdit`、`Analysis`、`IO`、`FeatureDetection`、`Simplification`、`CAPI` 和 `CLI`。
需要调试测试时使用
`vs2019-debug-full`。Release 结果用于性能和安装验证，不适合作为默认断点环境。VS Code
任务和 launch 配置位于 `.vscode/`，任务名称应以当前文件为准。

## 推荐入口

```powershell
cmake --preset vs2019-debug
cmake --build --preset vs2019-debug --parallel
```

CLI 调试从 `manumesh feature-report ...` 或 `manumesh simplify ...` 开始；单元测试调试用
CTest 过滤器选择具体测试。先运行 `manumesh --help`，再把实际参数复制到 launch 配置，避免
手写已经改名的选项。

## 观察顺序

特征问题按以下源码顺序观察：

1. `FeatureEvidence.cpp`：boundary、non-manifold、oriented dihedral 和 normal-tensor 证据；
2. `FeatureNormalFilter.cpp`：只预处理法向，不移动顶点；
3. `FeatureGraphCleanup.cpp`、`FeatureGraphCompatibility.cpp`、`FeatureGraphConsolidation.cpp`：
   spur、兼容桥和 component 整合；
4. `FeatureLoopRecovery.cpp`、`FeaturePrimitiveRecovery.cpp`：trace、cycle、circle/ellipse/polygon；
5. `FeatureGraph.cpp`、`FeatureSegmentation.cpp`：junction/branch 和 face patch。

简化问题按 `Quadrics.cpp` -> `Placement.cpp` -> `CandidateQueue.cpp` ->
`CollapseLegality.cpp`/`CollapseAttempt.cpp` -> `CollapseTopology.cpp` -> `QualityRefinement.cpp`
观察。报告中的拒绝计数表示候选第一次被归因的硬过滤器，不应简单相加当作唯一失败总数。

## Debug-only wireframe

`MANUMESH_ENABLE_DEBUG_UTIL=ON` 才会编译内部 HTML wireframe 辅助工具；它只用于算法排查，
不是 SDK、交付 viewer 或稳定输出接口。生成的 HTML/快照放在构建或 `output/` 目录，不提交到
`documentation/`。如果工具输出与报告不一致，先以结构化 `FeatureAnalysis`、
`SimplifyReport` 和测试断言为准。
