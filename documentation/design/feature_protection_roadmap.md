# 特征保护路线图

ManuMesh 当前特征保护已经能处理边界、二面角硬边、normal-tensor 弱特征、法线域 evidence stabilization、component consolidation、junction branch pairing、圆/近圆/椭圆 loop 和 primitive-based 硬保护。下一步重点不是“锁住更多边”，而是更准确地区分哪些特征必须硬保护，哪些只需要软成本。

## 当前能力

- `detectFeatureCurves()` 输出 `FeatureAnalysis`，包含 feature graph、junction branches、loop、component、vertex ownership、可选 surface patches 和来源/恢复诊断。
- `FeatureOptions::loopTraceAngleDeg` 可独立控制 loop tracing 阈值；默认 `-1` 复用 `featureAngleDeg`。
- `FeaturePrimitiveType` 支持 `Circle`、`NearCircle`、`Ellipse`、`PolygonalLoop`。
- `SimplifyOptions::featureProtectionMode` 支持四种硬保护策略。
- `FeatureAnalysis` / `SimplifyReport` 区分 traced/untraced feature edges、primitive/generic feature rejections、curve budget rejections、projected placements，以及 normal-tensor local scale / persistence、绕向冲突、cleanup cap、圆恢复截断诊断；C ABI report 以尾字段镜像简化报告中的这些值。
- `FeatureOptions::normalFilter` 和 `graphConsolidation` 默认关闭；前者不移动顶点，后者只恢复方向/source/sign 兼容的不同 endpoint components。

## 近期改进

1. 已移除 loop tracing 的 40 度硬下限，浅二面角特征可按用户阈值进入 loop ownership。
2. 已修正 primitive recovery 和 circular fallback 的 loop id 分配，避免无效 primitive 造成非连续 id。
3. 已将 normal-tensor 对 small cycle basis / circular fallback 的影响改为 component-level，而不是全局关闭。
4. 已增加 traced/untraced feature edge 诊断，并同步到 CLI、CSV、C ABI 和 VS Code 调试入口。
5. 已把 common 局部边长尺度接入 normal tensor，弱特征接受和 QEM normal-tensor 权重共用 `persistentFeatureScore` 与最小 persistence 门槛。
6. 已升级弱毛刺裁决（2026-07-12）：`featureGraphMinWeakSpurStrength`（默认 0 = 旧按边数剪枝）为正时按 Yoshizawa 组件级无量纲强度 `T = (∫ds)·(∫strength ds)` 判定，长而弱的真实曲线存活、短而强的噪声刺被剪除；gap 桥接同步采用 Yoshizawa 角度规则。
7. 已把二面角证据升级为有向（绕向感知）角并新增 `inconsistentWindingEdges` 诊断；primitive 拟合升级为 Taubin 圆 + Halíř-Flusser 椭圆（2026-07-12）。
8. 继续让 feature report CSV 更容易比较多次运行，并对每个 loop 输出更清晰的 primitive 类型、半径、轴比、平面误差和径向误差。
9. 已加入 junction continuation pairs、feature-induced surface patches，以及 edge/junction/branch/patch 四类 benchmark 标签。

## 中期改进

- 实现 edge dihedral plane quadrics，并和现有 feature curve quadric 对比。
- component-level confidence 和局部跨 component endpoint consolidation 已落地。下一步是加入全局不确定性、弱特征重定位/优化和更严格的 topology/patch consistency，而不是只扩大 gap ratio。
- benchmark 已覆盖 edge/junction/branch-pair precision/recall/F1 和 face-patch adjacency accuracy；下一步补 weak feature group、loop id、简化前后 feature drift、扫描类标注 fixture 和全局 Hausdorff 指标。
- 增加属性或 source region 信息，让用户可从外部标注必须保护的边/环。
- 引入更严格的误差 envelope，而不是只看局部 drift。
- 为 open boundary、多孔相邻、共享顶点 loop 增加更细的 ownership 策略。

## 暂不承诺

- 从任意 STL 自动恢复完整 CAD feature tree。
- 保证输出可直接用于制造公差检查。
- 对高噪声扫描输入自动移动顶点、重建并恢复平滑曲面；当前 normal filter 只稳定检测法向。
- 从 surface patches 自动拟合/合并 analytic CAD surfaces。

当前路线是先把三角网格层的特征保护做稳，再考虑更高层 CAD 语义。
