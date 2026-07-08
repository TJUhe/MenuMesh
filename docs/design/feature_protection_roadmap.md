# 特征保护路线图

ManuMesh 当前特征保护已经能处理边界、二面角硬边、normal-tensor 弱特征、圆/近圆/椭圆 loop 和 primitive-based 硬保护。下一步重点不是“锁住更多边”，而是更准确地区分哪些特征必须硬保护，哪些只需要软成本。

## 当前能力

- `detectFeatureCurves()` 输出 `FeatureAnalysis`，包含 feature graph、loop、vertex ownership 和边来源计数。
- `FeatureOptions::loopTraceAngleDeg` 可独立控制 loop tracing 阈值；默认 `-1` 复用 `featureAngleDeg`。
- `FeaturePrimitiveType` 支持 `Circle`、`NearCircle`、`Ellipse`、`PolygonalLoop`。
- `SimplifyOptions::featureProtectionMode` 支持四种硬保护策略。
- `FeatureAnalysis` / `SimplifyReport` 区分 traced/untraced feature edges、primitive/generic feature rejections、curve budget rejections、projected placements，以及 normal-tensor local scale / persistence 诊断。

## 近期改进

1. 已移除 loop tracing 的 40 度硬下限，浅二面角特征可按用户阈值进入 loop ownership。
2. 已修正 primitive recovery 和 circular fallback 的 loop id 分配，避免无效 primitive 造成非连续 id。
3. 已将 normal-tensor 对 small cycle basis / circular fallback 的影响改为 component-level，而不是全局关闭。
4. 已增加 traced/untraced feature edge 诊断，并同步到 CLI、CSV、C ABI 和 VS Code 调试入口。
5. 已把 common 局部边长尺度接入 normal tensor，弱特征接受和 QEM normal-tensor 权重共用 `persistentFeatureScore` 与最小 persistence 门槛。
6. 继续让 feature report CSV 更容易比较多次运行，并对每个 loop 输出更清晰的 primitive 类型、半径、轴比、平面误差和径向误差。

## 中期改进

- 实现 edge dihedral plane quadrics，并和现有 feature curve quadric 对比。
- 在已落地局部尺度归一化和多尺度 persistence 的基础上，引入 component-level confidence，提升 weak ridge / shallow feature 的可解释保护。
- 增加带 ground-truth labels 的 precision/recall、loop closure rate、junction correctness、弱特征保留率和 feature drift benchmark。
- 增加属性或 source region 信息，让用户可从外部标注必须保护的边/环。
- 引入更严格的误差 envelope，而不是只看局部 drift。
- 为 open boundary、多孔相邻、共享顶点 loop 增加更细的 ownership 策略。

## 暂不承诺

- 从任意 STL 自动恢复完整 CAD feature tree。
- 保证输出可直接用于制造公差检查。
- 对高噪声扫描输入自动去噪并恢复平滑曲面。

当前路线是先把三角网格层的特征保护做稳，再考虑更高层 CAD 语义。
