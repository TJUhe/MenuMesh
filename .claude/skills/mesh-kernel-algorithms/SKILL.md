---
name: mesh-kernel-algorithms
description: ManuMesh 网格内核算法工程经验库——从 M001-M044 论文蒸馏的 QEM 简化、特征检测、鲁棒性与性能技巧。修改 src/feature_detection、src/simplification 的算法逻辑，或设计新网格算法/调参/评审算法代码时必读。
---

# ManuMesh 网格内核算法 Skill

本 skill 汇集从 `documentation/papers/`（M001-M044 索引见 `documentation/papers/README.md`）精读蒸馏出的工程决策，
每条都映射到 ManuMesh 具体代码位置。修改算法前先查对应参考文件，避免重新发明或踩过论文已解决的坑。

## 参考文件

| 文件 | 覆盖内容 | 什么时候读 |
| --- | --- | --- |
| [references/feature-detection.md](references/feature-detection.md) | 二面角与 Normal Tensor 证据、特征曲线网络、圆/椭圆拟合（Taubin/Halíř-Flusser）、性能模式 | 改 `src/feature_detection/**`、调特征阈值、加新证据通道 |
| [references/simplification-remeshing.md](references/simplification-remeshing.md) | placement 策略族（GH/Lindstrom-Turk/line quadrics）、link condition 与边界拓扑、误差度量量纲、队列效率、弱特征保护（CWF）、remeshing 对编辑层的约束 | 改 `src/simplification/**`、`src/mesh_edit/**`、设计新算子 |

## 全局工程原则（两份参考的公约数）

1. **量纲一致性**：所有代价项统一到 length⁴（面积加权 QEM 的量纲）；所有阈值无量纲化
   （除以局部尺度的合适幂次），网格均匀缩放不得改变任何离散决策。
2. **硬/弱证据分离**：离散不连续（boundary/non-manifold/dihedral）与 Normal Tensor
   弱证据走独立通道、独立阈值，只在显式图结构汇合；弱证据需要 consolidation + confidence 才能升级为约束。
3. **拒绝规则优于代价惩罚**：拓扑合法性（link condition 含边界扩展）、翻转、自交、chart 归属
   是硬过滤器，不进代价函数；代价函数只负责排序。
4. **确定性三件套**：unordered 容器出口处排序、比较器带字典序 tiebreak、浮点归约按固定序——
   任何新算法落地前逐一检查。
5. **局部性**：每次编辑操作只做 O(k) 局部更新（k=one-ring）；任何全网格辅助结构构建一次、
   经 Context 复用，不允许在算法内部重建。
6. **数值退化路径**：quadric 最优点和拟合等数值问题必须有文献支持的显式退化判据
   与逐级候选（最优点 → 沿边一维最优 → 端点/中点），每一级仍评估同一代价。这是
   算法定义的一部分，不等同于静默切换依赖、路径或配置的环境兜底。
7. **短公共入口**：参考 CGAL 的主算法入口与 geometry-central 的小配置对象。少量同职责
   参数直接表达；参数增多时按目标、代价、约束、质量和输出职责分组。互斥目标使用
   显式 mode/tagged value，不依赖两个字段的隐式优先级。
8. **紧凑结果**：参考 pmp-library 的小型分析结果与 VTK 的可选属性输出。主结果只放
   输出、完成数量和终止原因；调参计数进入可选 diagnostics，逐元素数据进入属性或
   专用 analysis。不要为一个长 report 再建立多组字段投影 summary 或重复 getter。
9. **单点兼容**：旧 API 的转换集中在一个适配边界，内部只消费一种规范表示。不要在
   新旧结构之间双向手工镜像所有字段，也不要为假想调用方增加兼容分支。
10. **按需要拆分**：helper 应表达真实概念、消除复杂分支或被复用；不要添加只转发一次的
    包装函数。阶段形成独立不变量或测试边界后再拆文件，不预建空框架。
