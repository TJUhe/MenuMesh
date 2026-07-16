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
| [references/feature-detection.md](references/feature-detection.md) | 曲率/曲率导数估计、二面角与 normal voting 硬证据、crest line 与多尺度弱特征、特征曲线网络、圆/椭圆拟合（Taubin/Halíř-Flusser）、性能模式 | 改 `src/feature_detection/**`、调特征阈值、加新证据通道 |
| [references/simplification-remeshing.md](references/simplification-remeshing.md) | placement 策略族（GH/Lindstrom-Turk/line quadrics）、link condition 与边界拓扑、误差度量量纲、队列效率、弱特征保护（CWF）、remeshing 对编辑层的约束 | 改 `src/simplification/**`、`src/mesh_edit/**`、设计新算子 |

## 全局工程原则（两份参考的公约数）

1. **量纲一致性**：所有代价项统一到 length⁴（面积加权 QEM 的量纲）；所有阈值无量纲化
   （除以局部尺度的合适幂次），网格均匀缩放不得改变任何离散决策。
2. **硬/弱证据分离**：离散不连续（boundary/non-manifold/dihedral）与微分事件（curvature）
   走独立通道、独立阈值，只在显式图结构汇合；弱证据需要 consolidation + confidence 才能升级为约束。
3. **拒绝规则优于代价惩罚**：拓扑合法性（link condition 含边界扩展）、翻转、自交、chart 归属
   是硬过滤器，不进代价函数；代价函数只负责排序。
4. **确定性三件套**：unordered 容器出口处排序、比较器带字典序 tiebreak、浮点归约按固定序——
   任何新算法落地前逐一检查。
5. **局部性**：每次编辑操作只做 O(k) 局部更新（k=one-ring）；任何全网格辅助结构构建一次、
   经 Context 复用，不允许在算法内部重建。
6. **fallback 链**：数值求解（quadric 最优点、拟合）必须有显式退化判据 + 逐级回退
   （最优点 → 沿边一维最优 → 端点/中点），回退代价不得为 0。
