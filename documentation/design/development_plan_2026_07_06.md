# 2026-07-06 开发计划

本计划保留 2026-07-06 的阶段目标。当前构建基线已经统一为 Visual Studio 16 2019 / MSVC v142，旧工具链目标不再适用。2026-07-15 已增长到 267 个启用的非性能 CTest（快速套件 256 个、external 11 个，另有 1 个 disabled 手工性能测试），并完成九阶段特征识别、normal filter、stable scale、component consolidation、junction branches 和 surface patches。当前路线以 [`feature_recognition_system_upgrade_2026_07_15.md`](feature_recognition_system_upgrade_2026_07_15.md) 为准。

## 第一周：稳定性和文档一致性

- 保持 `vs2019-release`、`vs2019-release-static` 与 `vs2019-release-sdk` 构建和测试矩阵稳定。
- 确保 `.vscode/tasks.json`、`.vscode/launch.json`、`documentation/guide/vscode_setup.md` 和 CLI 帮助一致。
- 将文档正文统一为中文，路径、命令、API 名称保留英文标识。
- 对 `feature-protection-mode primitive-curves` 默认策略补充更多测试和说明。
- 明确“已实现”和“路线图”边界，避免把布尔、修复、offset 写成当前能力。
- 维护 [`algorithm_essence.md`](algorithm_essence.md)，每次新增算法能力都补上现象、数学本质、实现步骤和论文来源。

## 第二周：算法增强

- 继续验证 normal tensor 特征证据在弱硬边、boss/pocket、薄片上的效果。
- 对 `maxFeatureCurveDeviationRatio`、`minCircularFeatureLoopVertices` 等参数做更系统的网格族实验。
- 评估实现 edge dihedral plane quadrics 的代价，并和现有 feature graph 保护策略分开比较。
- 改进 feature report CSV，让 circular、near-circle、ellipse、polygonal loop 的诊断更好用。

## 第三周：SDK 和工业验证

- 扩展 C ABI 示例，覆盖加载、设置内存数据、简化、统计和保存。
- 为 SDK 安装布局增加更多消费端 smoke test。
- 在 `tests/data/external/large` 和 `thingi10k` 子集上保留性能/质量基线。
- 准备面向宿主应用的集成说明：运行时 DLL、ABI 初始化、错误处理、输出报告解释。

## 可借鉴的第三方策略

- CGAL：学习 constrained edge 和 stop predicate 的边界，但不直接引入模板依赖到公共 ABI。
- OpenMesh：学习 cost module 与 legality module 分离。
- libigl：学习紧凑 QEM 实现和研究原型表达。
- MeshLab/VCGLib：学习生产 decimation filter 的质量保护和大量模型验证习惯。

## 风险控制

- 每次算法改动至少跑 `cmake --build --preset vs2019-release --parallel` 和 `ctest --preset vs2019-release-full`。
- 涉及 performance fixture 时单独启用 `MANUMESH_BUILD_PERFORMANCE_TESTS=ON`。
- 不在公共头中暴露 `src/.../detail/...` 类型。
- C ABI 结构体增加字段时只向后追加；同一 ABI 版本内保持旧 `struct_size` 可用，缺失尾部字段使用默认值。

## 对外回复口径

ManuMesh 当前不是完整 CAD 内核，也不是通用网格修复器。它现在最可靠的定位是：一个带 C++/C SDK 的增材制造三角网格 QEM/line-quadrics 简化内核，支持特征检测、曲线保护和保守合法性过滤，适合继续向工业网格 decimation 模块演进。
