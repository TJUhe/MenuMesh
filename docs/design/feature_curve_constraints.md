# 特征曲线约束说明

ManuMesh 当前特征曲线保护由独立 `FeatureDetector`/`detectFeatureCurves()`、`FeatureConstraints.cpp` 和 `SimplifyOptions` 中的 feature 相关参数共同实现。它不是完整 CAD 约束求解器，而是在三角网格上识别特征 loop，并在 QEM collapse 中加入软成本、投影和硬拒绝策略。

## 检测输入

`FeatureOptions` 当前使用：

- 边界边。
- 非流形边。
- 二面角超过 `featureAngleDeg` 的硬边。
- normal-tensor 弱特征证据。
- loop tracing 后的圆、近圆、椭圆和折线 primitive 拟合。

## 简化中的约束层

| 层 | 当前实现 | 作用 |
| --- | --- | --- |
| 软 line weight | `weightMode`、`featureBoost` | 让特征附近候选成本更高。 |
| 特征曲线 quadric | `featureCurveWeight` | 沿检测到的 loop 加 tangent-line 约束。 |
| placement 投影 | 圆、近圆、椭圆、polyline | 把候选位置拉回拟合 primitive 或原始折线。 |
| 曲线预算 | `maxFeatureCurveDeviationRatio` | 原始 placement 偏离曲线太远时拒绝。 |
| 最小 loop 顶点数 | `minFeatureLoopVertices`、`minCircularFeatureLoopVertices` | 防止重要 loop 被压到过少顶点。 |
| 硬保护策略 | `featureProtectionMode` | 决定哪些 loop/边可以触发硬拒绝。 |

## 保护模式

| 模式 | 含义 |
| --- | --- |
| `none` | 不启用硬特征保护，只保留软成本。 |
| `circular-only` | 只硬保护圆和近圆 loop。 |
| `primitive-curves` | 默认模式，硬保护圆、近圆和椭圆，generic crease 保持较软。 |
| `all-feature-edges` | 旧式严格模式，所有检测到的特征边都硬保护。 |

`--protect-all-feature-edges` 只是 `all-feature-edges` 的兼容别名。

## 适用场景

适合：干净 CAD/STL 三角网格、孔洞边界、圆孔、近圆孔、椭圆孔、明显硬边和规则工业件。

不适合直接承诺：高噪扫描件、缺失拓扑的点云重建、B-Rep 语义特征、严格尺寸公差证明和全局几何约束。

## 调参建议

- 普通圆孔：`--preserve-feature-curves --feature-protection-mode primitive-curves --min-circular-feature-loop-vertices 12`。
- 泛硬边太多导致停滞：避免 `all-feature-edges`，使用默认 `primitive-curves`。
- 弱特征不明显：尝试 `--weight-mode normal-tensor --normal-tensor-threshold 0.06 --normal-tensor-edge-alignment 0.2`。
- 输出偏离曲线：增大 `featureCurveWeight` 或减小 `maxFeatureCurveDeviationRatio`，同时检查拒绝计数是否过高。
