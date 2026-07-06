# 特征保护路线图

Tessellix 当前特征保护已经能处理边界、二面角硬边、normal-tensor 弱特征、圆/近圆/椭圆 loop 和 primitive-based 硬保护。下一步重点不是“锁住更多边”，而是更准确地区分哪些特征必须硬保护，哪些只需要软成本。

## 当前能力

- `detectFeatureCurves()` 输出 `FeatureAnalysis`，包含 feature graph、loop、vertex ownership 和边来源计数。
- `FeaturePrimitiveType` 支持 `Circle`、`NearCircle`、`Ellipse`、`PolygonalLoop`。
- `SimplifyOptions::featureProtectionMode` 支持四种硬保护策略。
- `SimplifyReport` 区分 primitive/generic feature rejections、curve budget rejections 和 projected placements。

## 近期改进

1. 让 feature report CSV 更容易比较多次运行。
2. 对每个 loop 输出更清晰的 primitive 类型、半径、轴比、平面误差和径向误差。
3. 为 `primitive-curves` 增加更多真实工业件测试。
4. 保持 generic crease 默认软保护，除非用户显式选择 strict。

## 中期改进

- 实现 edge dihedral plane quadrics，并和现有 feature curve quadric 对比。
- 增加属性或 source region 信息，让用户可从外部标注必须保护的边/环。
- 引入更严格的误差 envelope，而不是只看局部 drift。
- 为 open boundary、多孔相邻、共享顶点 loop 增加更细的 ownership 策略。

## 暂不承诺

- 从任意 STL 自动恢复完整 CAD feature tree。
- 保证输出可直接用于制造公差检查。
- 对高噪声扫描输入自动去噪并恢复平滑曲面。

当前路线是先把三角网格层的特征保护做稳，再考虑更高层 CAD 语义。
