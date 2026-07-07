# 算法现状复核与路线图

本文按 ManuMesh 当前源码复核，而不是按早期设想描述。核对对象包括 `include/manumesh/algorithms/feature_detection/FeatureDetector.h`、`include/manumesh/algorithms/simplification/`、`src/feature_detection/`、`src/simplification/`、`apps/manumesh/main.cpp` 和当前 76 个非性能 CTest。

## 当前已经实现

| 方向 | 当前状态 |
| --- | --- |
| 标准 QEM | 已实现面 plane quadric 累加、边折叠候选、placement 求解和 fallback。 |
| line quadrics | 已实现 `--method line` 和 `lineWeight`，作为候选排序成本的附加项。 |
| 权重模式 | `uniform`、`dihedral`、`normal-tensor`、`height`、`xband`。 |
| 特征检测 | 已作为 `feature_detection` 平级算法模块实现，支持边界、非流形边、二面角边、normal-tensor 弱特征、feature graph、loop tracing。 |
| primitive 拟合 | 支持圆、近圆、椭圆、折线 loop 的报告和保护策略。 |
| 特征保护 | `none`、`circular-only`、`primitive-curves`、`all-feature-edges`。默认是 `primitive-curves`。 |
| 合法性过滤 | 边界、拓扑、法线偏转、三角形质量、局部误差和局部自交过滤。 |
| 诊断报告 | `SimplifyReport` / `ManuMeshSimplifyReport` 输出终止原因、拒绝计数、特征计数和权重范围。 |
| CLI | `generate`、`simplify`、`compare`、`feature-report`、`feature-compare`、`sweep`、`ratio-sweep`、`face-sweep`、`demo`、`summarize-metrics`、`validate-features`、`validate-external`。 |
| SDK | Eigen-backed C++ API、Eigen-free `PlainMesh` C++ 入口和 C ABI 均可用，示例位于 `examples/`。 |

## 当前没有实现

- 论文中的完整 edge dihedral plane quadrics。
- 通用扫描去噪、曲率重建或法线重估计流水线。
- 通用 CAD/B-Rep 特征识别。
- 布尔运算、offset/thickening、孔洞修复和全自动 manifold repair。
- 学习式显著特征评分、时间序列一致性简化或神经 QEM 表示。

## 主要技术风险

1. line quadrics 是软约束，权重过高会牺牲几何保真。
2. 特征检测目前以 CAD/STL 风格网格为主，噪声扫描件需要更稳健的预处理。
3. `all-feature-edges` 会锁住太多 generic crease，可能导致 `rejection-limit`，默认 `primitive-curves` 更平衡。
4. 局部自交和局部误差过滤改善安全性，但不是全局几何证明。
5. C ABI 结构体依赖 `struct_size` 和 `abi_version`，外部调用必须初始化；同一 ABI 版本内允许旧尾部尺寸，缺失字段使用默认值。

## 短期路线

- 保持 `build: mingw+ninja release all`、`test: mingw+ninja release` 和 `test: mingw+ninja release full` 稳定可跑。
- 扩展特征报告 CSV，继续区分 circular、near-circle、ellipse、polygonal loop。
- 用更多工业件验证 `primitive-curves` 默认策略，避免 generic crease 过度硬锁。
- 补充 CLI 子命令级帮助或明确保持顶层帮助模式。
- 对 C API 增加更多端到端示例和 ABI 兼容测试。

## 中期路线

- 实现独立 edge dihedral plane quadrics，和现有 feature graph 保护策略对比。
- 增加属性传播策略，为法线、颜色、UV、source face id 做准备。
- 继续沿 `algorithms/<domain>` 组织新增模块，建立更清晰的 `repair`、`remesh` 和 `boolean` 边界。
- 将本地自交过滤扩展成更完整的空间查询和 envelope 检查。

## 长期路线

ManuMesh 的长期目标是一个可嵌入的增材制造多边形网格 SDK。QEM/line quadrics 是其中的 decimation 模块，不应承担所有修复、识别和布尔任务。未来模块应保持公共 API、实现、测试和文档边界清楚。
