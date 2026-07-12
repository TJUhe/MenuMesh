# 简化 / QEM / Edge-Collapse / Remeshing 算法工程参考

蒸馏自 `docs/papers/` 论文归档（M 编号见 `docs/papers/README.md`），面向 ManuMesh 简化算法的
鲁棒性与性能强化。每条格式：

> **技巧名** / 出处（M 编号 + 页码）/ 数学要点 / 为什么鲁棒或快 / 映射到 ManuMesh（file:line）

代码行号基于分支 `codex/mesh-edit-quality-foundation`（2026-07-12）。
本文档是"蒸馏"：只保留可直接指导实现决策的内容，推导细节回读原文（M033 有全套公式重推导）。

---

## 1. Placement 策略族

### 1.1 GH 最优点求解的数值条件与三级 fallback 链

- **出处**：M002 §4 Eq.(1) + §5.1（pp.2-3）；M003 §3.3（p.3）；M033 §5.1 + §5.8（pp.9-11, 27）。
- **数学要点**：
  - 最优位置解线性系统 `A v̄ = -b`，其中 `Q = [A b; bᵀ c]`。
  - A 是面法向的加权协方差矩阵（sample covariance of normals，M030 §3.1 同一解释）。
    特征值谱刻画局部形状：平面区 rank(A)=1，直棱/折痕区 rank(A)=2，只有非退化椭球
    （rank 3）时 A 可逆，此时 v̄ 位于误差椭球中心。
  - GH97 的 fallback 是**三级链**：
    1. 全空间最优 `v̄ = -A⁻¹b`；
    2. A 奇异 → **沿线段 v1v2 的一维最优**：对 `f(t) = h(t)ᵀQh(t)`，`h(t) = (1-t)v1 + t·v2`
       求导的标量二次问题。分母 `(v2-v1)ᵀA(v2-v1) ≥ 0`（A 半正定），为 0 时才退化；
    3. 一维也退化 → 在 {v1, v2, midpoint} 中取 quadric 误差最小者。
  - 三级都严格评估同一个二次型 → fallback 不改变代价的量纲与可比性。
- **为什么鲁棒**：平面/直棱区 A 奇异是 CAD/STL 网格的**常态**而非异常；一维最优在
  rank-2 情形（直棱、边界折痕）恰好良定且显著优于端点/中点，缺了这一级会把最常见的
  特征边折叠推向次优位置。
- **ManuMesh 映射**：
  - `src/simplification/Quadrics.cpp:192-240`（`solvePlacementCandidates`）已有
    {a, b, midpoint, 全空间解} 候选并按 cost 稳定排序；条件数检查
    `minEval/maxEval > 1e-12`（:206）。
  - **缺第 2 级（沿边一维最优）**：rank-2 情形全空间解被谱检查拒绝后直接落到端点/中点。
    建议在 :209 之后追加一维候选（~15 行，标量除法 + 一次 clamp）。
  - 条件数阈值 1e-12 偏保守：M004 证明加入 line quadric 后 A 恒为 ≥3 个非平行平面之和、
    必然可逆；`useLineQuadrics` 开启时可跳过谱检查（见 1.3），省一次 3×3 特征分解/边。

### 1.2 Lindstrom-Turk：体积守恒 + 边界守恒的约束求解框架

- **出处**：M032 §4.2-4.3（pp.2-4）；M033 §5.3-5.7 + §6-7（pp.14-31，含参考实现与 α=5°）。
- **数学要点**：placement 表述为**按优先级逐个添加线性约束平面 `aᵢᵀv = bᵢ`，凑满 3 个
  后解 3×3 系统**。约束按序：
  1. **Volume preservation**（1 个约束）：所有受影响三角形扫出的有向四面体体积之和为 0，
     化简为平面方程 `(Σᵢ nᵢ)ᵀ v = Σᵢ det[t₀ⁱ t₁ⁱ t₂ⁱ]`，nᵢ 为面积加权法向。
     局部守恒 ⇒ 全局守恒（所有变动面都计入求和）。
  2. **Boundary preservation**（2 个约束，仅涉及边界边时）：最小化有向面积变化
     `‖½(v×e₁ + e₂)‖²`，其中 `e₁ = Σ(v₁ᵉ - v₀ᵉ)`、`e₂ = Σ(v₀ᵉ × v₁ᵉ)` 对关联边界边求和。
     解空间 = 两平面之交：
     `(e₁ᵀe₁)·e₃ᵀv + e₃ᵀe₃ = 0` 与 `(e₁×e₃)ᵀv = 0`（e₃ = e₁×e₂）。
     这是"新顶点贴回边界、且边界包围面积不变"的闭式表达——非平面边界也成立
     （面积改为有向面积向量，M033 §9 附录）。
  3. **Volume/boundary optimization**：最小化 Σ V²（每三角形扫出体积平方，
     可微处处、H = Σnnᵀ 与 QEM 同构）补满剩余自由度；用零空间投影
     `I₍₃₋ₙ,₃₎ Z⁻¹(Hv + c) = 0` 生成与已有约束正交的补充约束。
  4. **Triangle shape optimization**（最后手段）：最小化 Σ‖v-vᵢ‖² → 解 = 一环邻居质心。
  - **α-compatibility**（M032 §4.2；M033 §6 用 α=5°）：新约束法向与已有约束夹角必须
    > α，判据 `(a₁ᵀa₂)² < ‖a₁‖²‖a₂‖² cos²α`（第 2 条）与三重积版本（第 3 条）。
    近平行平面之交对系数扰动极端敏感，α 门限是数值稳定的关键。
  - **边代价** Eq.(12)：`f_C = ½·f_V + ½·L(e)²·f_B` —— 面积项乘 L(e)² 把两项统一为
    length⁶ 量纲。**这个"乘边长平方补齐量纲"的手法是 §3.1 边界 quadric 修复的原型**。
- **为什么鲁棒/快**：
  - **memoryless**：只依赖当前网格，无 quadric 累积漂移；每边处理 O(ring)，
    内存 O(当前网格)。
  - 体积守恒消除 midpoint/最优点折叠在凸区收缩、凹区膨胀的系统性体积偏差。
  - 边界守恒把边界保持从惩罚项升级为**硬约束子空间**——对开边界网格，
    mean geometric error 与 Mesh Optimization 持平而速度快一个数量级
    （M032 §5.2 Table 1 + Fig.6-7）。
- **ManuMesh 映射**（quadric-accumulation 路线不必整体切换，借两个部件）：
  1. **边界边折叠 placement**：`src/simplification/FeatureConstraints.cpp:274`
     （`projectBoundaryPlacement`，由 `CollapseAttempt.cpp:92` 调用）目前把候选位置
     投影回旧边界折线；LT 两平面约束解最小化的是**有向面积变化**，对锯齿/噪声边界
     产生更平滑的边界演化，建议作为边界边的第一候选。
  2. **混合候选**：`Quadrics.cpp:192` 候选列表追加"体积守恒平面约束下的 QEM 最优"
     （拉格朗日乘子一步闭式：`v = -A⁻¹b + λA⁻¹n`，λ 由平面方程解出），
     消除 QEM 在低曲率区的体积漂移，代价一次 3×3 回代。

### 1.3 Line quadrics：tangential drift 控制的精确机制与权重选择

- **出处**：M004 §4.1-4.3（pp.3-4）、§4.4.1-4.4.2（pp.4-5）、Fig.1/3/4/9。
- **数学要点**：
  - 每顶点增广 `Q̃ᵢ = Qᵢ + wᵢ·aᵢ·Qᵢˡ`；`Qᵢˡ = Qᵢˣ + Qᵢʸ` 是**点到线距离平方**
    （线过顶点 vᵢ、方向 = 面积加权顶点法向 n̂ᵢ；勾股分解为两个正交平面 quadric 之和，
    正交基 xᵢ, yᵢ 由 Gram-Schmidt 从 n̂ᵢ 构造）。
  - **性质 1（数值）**：Q̃ 恒为 ≥3 个非平行平面之和 → A 恒正定可逆，
    无需 SVD/谱手术（§4.2）。
  - **性质 2（Fréchet mean，Lemma 4.1）**：平面区上 line quadric 精确退化为该平面内的
    2D 点到点距离平方；memory 式 quadric 累加后，最优点 = 已折叠历史顶点的面积加权
    **Fréchet 均值** → 均匀顶点分布 + 高三角形质量。
    这就是 tangential drift 控制的机制：切向漂移的代价从 0（纯 QEM 平面区）
    变为到历史顶点均值的距离平方。
  - **性质 3（权重）**：wᵢ < 1e-1 防止视觉退化，**默认 1e-3**（§4.2）。
    极端权重（自适应密度控制）改用混合模式（§4.4.1）：先加小 line quadric（w≈0.01），
    再对整个 Q̃ 做标量缩放——避免"直接缩放 QEM"在零误差平面区完全失效（Fig.1）。
    退化元素兜底：1e-6 × probabilistic point quadric。
  - **Dihedral plane quadric 对比**（§4.4.2）：edge 级变体（平面过边 ij、法向 ⊥
    面积加权边法向 n̂ᵢⱼ，权重 aᵢⱼ/2）适合**边级**软保护（edge loop）；
    但作全局正则项时在 Hausdorff / P2M / 三角质量三指标上 Pareto 劣于
    line quadric（Fig.9）——line quadric 把最优点引向质心，dihedral 引向内心。
- **为什么鲁棒/快**：只改初始 quadric 构造，运行时复杂度与原始 QEM 完全相同；
  数值兜底与三角质量正则合并为同一机制，无额外分支。
- **ManuMesh 映射**：
  - `Quadrics.cpp:33-52`（`lineQuadric`）+ :134-158（应用，`lineWeight` 默认 1e-3，
    `appliedWeight * vertexArea[i] * ql` 与 M004 Eq.16 逐项一致）——**已正确实现**。
  - `:149-152` 的 `adaptiveScale` 分支（加基础 line quadric 后整体乘
    `1 + featureBoost·score`）正是 §4.4.1 的"先加后缩"；
    但 :153 的 else 分支把 boost 直接加进 line weight，属于 M004 警告的大权重加法路径，
    高 boost 时挤占几何保真。建议 featureBoost 显著时默认走 adaptiveScale。
  - 特征曲线 quadric（:160-185）：沿 `vf.tangent` 的 line quadric 允许特征顶点沿曲线
    滑动、惩罚离线移动，与 M004 §5.3（skinning 权重软保护）同一哲学；
    `confidenceScale = 0.35 + 0.65·conf` 是合理的连续降级。

### 1.4 属性网格的 placement 联动（optimal placement 的隐含义务）

- **出处**：M003 §5-6（pp.4-5）；M031 §4.3-4.4（pp.5-6）。
- **数学要点**：optimal placement 的新位置一般**不在原表面上** → 属性不能沿边插值，
  必须与位置联合求解：M003 用扩维 quadric 一步解出（位置+属性）；
  M031 对固定几何参数化解属性最小二乘，并对 α ∈ {0, ½, 1} 三个起点分别优化取最优。
  只有 subset placement（端点择优）才天然免除该义务。
- **ManuMesh 映射**：`TextureProtection.cpp:220-229` 用"新位置在旧边上的投影参数 t"
  插值 UV，隐含假设 placement 近似在边上——对偏离边的 GH 最优点有系统性 UV 漂移。
  低成本修正：改用新位置到两端点 3D 距离反比的权重。完整路线取舍见 §3.4。

**§1 ManuMesh 行动清单**

1. `Quadrics.cpp:209` 后追加"沿边一维最优"候选（rank-2 直棱/边界情形收益最大）。
2. `FeatureConstraints.cpp:274` 边界投影升级为 LT 两平面边界约束解（M032 §4.2.2），
   与 §2.1 的拓扑修复配套。
3. 追加"体积守恒约束下的 QEM 最优"混合候选（拉格朗日闭式）。
4. line quadric 开启时短路 `solvePlacementCandidates` 的谱检查。
5. 高 featureBoost 场景默认 `adaptiveScale = true`。
6. 纹理场景的 UV 插值权重从边投影 t 改为端点距离反比（配合 §3.4）。

---

## 2. 合法性检查族

### 2.1 Link condition 的完整形式（含边界情形——修 pinch 的理论依据）

- **出处**：Dey-Edelsbrunner-Guha-Nekhayev 1999（M004 §2 引 [DEGN99]，拓扑保持折叠的
  标准出处）；M033 §4 checks 2-3（pp.7-8）为其工程化投影。
- **数学要点**：
  - 2-流形（可带边界）上折叠边 ab 保持拓扑 ⇔ `Lk(a) ∩ Lk(b) = Lk(ab)`。
  - **边界情形的正确表述**：在复形上添加虚拟顶点 ω（dummy vertex），ω 与每个边界顶点
    连边、与每条边界边成三角形（对边界做一点紧化），然后在增广复形上验证同一条件。
    展开为工程判据：
    - 内部边（2 关联面）：a、b 公共邻居 == 恰好 2 个对面顶点；
    - 边界边（1 关联面）：公共邻居 == 恰好 1 个对面顶点（ω 自动在两侧 link 与
      edge link 中，条件不受影响）；
    - **a、b 均为边界顶点但 ab 是内部边（"边界弦" chord）**：
      ω ∈ Lk(a)∩Lk(b) 但 ω ∉ Lk(ab) → **条件失败，必须拒绝**。
      折叠它会把边界两段捏合成 pinch 点（非流形顶点 / 把带边界曲面的柄压塌）。
  - M033 check 3（boundary merge check）只拦截"面的另两条边都在边界上"的特例；
    dummy-vertex 判据是无遗漏的完整版。
- **为什么鲁棒**：这是拓扑保持的**充要条件**而非启发式；ω 形式把内部/边界统一为
  一个集合相等判断，无特判组合遗漏。
- **ManuMesh 映射**：
  - `src/simplification/CollapseTopology.cpp:74-126`（`collapseWouldPreserveLinkCondition`）
    实现了顶点 link 交集判定，:103-104 允许 incidentFaceCount ∈ {1,2}——内部与边界边
    都覆盖，**但没有 ω**。
  - `boundaryCollapseDecision`（`CollapseTopology.cpp:5-23`）在 `preserveBoundary=true`
    时拒绝弦（keep/remove 均 boundary 但边非 boundary → {false,false}）；
    但 **`preserveBoundary` 默认为 false**（`include/.../SimplificationTypes.h:77`），
    :8-10 直接放行 → 默认配置下边界弦可通过全部检查被折叠 → **边界捏合缺口确认**。
  - **修复**：在 :108-125 的交集判定中加入等价判据——
    "若 a、b 均为边界顶点且 incidentFaceCount == 2，则返回 false"。
    该判断属于拓扑正确性，应**独立于** `preserveBoundary` 选项生效。
  - 注意状态时效：`vertices[i].isBoundary` 在 `SimplificationRun.cpp:106-129` 初始化后
    不随折叠更新；修复需改为从 `DynamicTopology` 实时查询边界性
    （或在 `applyCollapse` 中增量维护），否则中后期误判。

### 2.2 法向翻转（mesh inversion）

- **出处**：M002 §6（p.4）；M031 §4.2（p.5，变体：最大二面角超阈值即禁用）；
  M033 §4 check 1（p.7）。
- **数学要点**：对每个受影响面比较折叠前后法向，`n_old · n_new < τ` 则拒绝。
  文献用 τ=0（纯翻转）；工程上 τ ∈ (0, 1)（如 cos75°≈0.26）还能拦截接近翻转的剪切
  退化。新法向范数 ≈ 0（退化面）必须先行拒绝，否则点积无意义。
- **ManuMesh 映射**：`CollapseLegality.cpp:129-134` 已实现（可配 `minNormalDot` +
  退化保护），与文献一致。仅建议：把 τ 推荐值写进 `SimplificationTypes.h` 注释，
  默认从 0 收紧到 cos75° 量级。

### 2.3 几何自交的高效局部检查

- **出处**：M004 §2 引 Gumhold-Borodin-Klein 2003 [GBK03]（intersection-free
  simplification 专文）；工程通行做法。
- **数学要点**：只测**新三角形 × 空间邻近的非关联面**：
  AABB 索引查询候选 → 跳过共享顶点的面（相邻面由 flip/quality 检查兜底）→
  精确三角形相交测试。折叠后增量更新索引而非重建。
- **ManuMesh 映射**：
  - `CollapseLegality.cpp:202-241` + `SpatialFaceIndex` +
    `SimplificationRun.cpp:392-417` 的增量 remove/update——结构正确。
  - **性能悬崖**：:214/:220 当索引未启用时回退为每边 O(F) 全网格扫描；
    `preventLocalIntersections=true` 时应保证索引必然可用（断言或强制构建）。
  - 检查顺序（`CollapseLegality.cpp:245-278`：link → 逐面 area/quality/flip →
    local error → self-intersection）已是 cheap-first，保持。

### 2.4 局部误差包络（local error envelope）

- **出处**：M031 §4.3（Edist 双向采样，pp.5-6）；M004 §2 引 [BF05]（Hausdorff
  envelope）；M033 §2（simplification envelopes 谱系）。
- **数学要点**：双向采样距离（旧面样本→新面集、新面样本→旧面集+原始参考面）近似
  局部双向 Hausdorff；每三角形 7 样本（顶点+边中点+质心）是精度/代价折中，
  系统性弱点是对细长三角形欠采样。
- **ManuMesh 映射**：`CollapseLegality.cpp:142-189` 已实现双向 + `referenceSurface`
  （原始网格全局距离索引，`SimplificationRun.cpp:178-180`）——比多数文献实现更强。
  热点是样本×新面的暴力最近距离（:153-159），优先级低于 §4 队列问题，暂不动。

### 2.5 特征曲线的拓扑保持测试（Hoppe 判据）

- **出处**：M031 §4.5（p.7）。
- **数学要点**：折叠 (vs, vt) 改变不连续/特征曲线拓扑，当且仅当命中以下任一
  （sharp(e) = 边是特征边，#sharp(v) = 顶点关联特征边数）：
  1. sharp(vs,vl) ∧ sharp(vt,vl)（左翼两条特征边将合并）；
  2. sharp(vs,vr) ∧ sharp(vt,vr)；
  3. #sharp(vs)≥1 ∧ #sharp(vt)≥1 ∧ ¬sharp(vs,vt)（两条独立特征线被焊接）；
  4. #sharp(vs)≥3 ∧ #sharp(vt)≥3 ∧ sharp(vs,vt)（两个 junction 被合并）；
  5. sharp(vs,vt) ∧ #sharp(vs)=1 ∧ #sharp(vt)≠2（曲线端点被内吞）；
  6. sharp(vs,vt) ∧ #sharp(vt)=1 ∧ #sharp(vs)≠2。
  Hoppe 对命中者不直接禁止、而是加显式惩罚或延迟（保持队列可行性）。
- **为什么鲁棒**：纯组合判据，O(deg) 可查；覆盖了"特征线焊接/端点内吞/junction 合并"
  这些几何误差检测不到的拓扑事件。
- **ManuMesh 映射**：`FeatureConstraints.cpp` 的 `collapseRejectKind` 基于
  loopId/junction/primitive 标记（`CollapseAttempt.cpp:60-66`），语义上覆盖判据 3/4 的
  大部分，但判据 1/2（**翼顶点 vl/vr 的特征边合并**）没有对应检查——两条平行特征边
  折叠中间地带时会把翼上特征边焊接。建议把 6 条判据实现为
  `FeatureCollapsePolicy` 的独立组合检查（输入只需 per-edge sharp 标记 + 度数）。

**§2 ManuMesh 行动清单**

1. **[最高优先] 边界 pinch 修复**：`CollapseTopology.cpp:74-126` 加 dummy-vertex ω
   等价判据（双边界端点 + 内部边 → 拒绝），独立于 `preserveBoundary` 生效；
   `isBoundary` 改为实时/增量维护。
2. `preventLocalIntersections` 开启时消除 O(F)/边 回退路径。
3. Hoppe 六判据补齐特征曲线拓扑检查（重点判据 1/2 的翼顶点情形）。
4. `minNormalDot` 默认值从 0 收紧并文档化。

---

## 3. 误差度量设计

### 3.1 边界约束 quadric 的量纲一致性（确认缺陷 + 修复）

- **出处**：M032 §4.3 Eq.(12)（p.4，fB 乘 L(e)² 统一为 length⁶）；
  M002 §6（p.3，boundary constraint plane + large penalty）；M003 §4（p.3，同一机制 +
  attribute boundary 推广）；M033 §5.2（pp.12-13，boundary drift 的失效画面）。
- **数学要点**：
  - 面 quadric 权重 = 面积（length²），点到面距离平方 = length²，
    故面项量纲 = **length⁴**。
  - 边界 quadric 若只乘边长（length¹），边界项量纲 = **length³**——比面项低一阶。
    后果：网格整体缩放 s 倍时边界项相对权重变化 1/s；同一 `boundaryWeight`
    在不同尺度、不同密度网格上不可移植；细分越密边界保持越弱。
  - 修复基线：边界 quadric 权重取 length² 量纲——`w·len(e)²` 或
    `w·len(e)·localScale`；GH98 的大惩罚系数（工程常取 10²-10³）叠加在
    量纲正确的基底上。
- **ManuMesh 映射**：`src/simplification/Quadrics.cpp:75`：
  `boundaryWeight * edge.norm() * planeQuadric(...)`，而面 quadric 是
  `(area/3) * q`（:105-110）——**低一阶确认**。修复：
  `edge.norm()` → `edge.squaredNorm()`（或 × 顶点平均边长），
  重标定默认值：当前 `boundaryWeight = 0.0`（`SimplificationTypes.h:75`）建议
  默认给小正值（与面项同阶，如 1.0），使开边界网格默认有软边界保持——
  与 §2.1 硬拓扑修复互补（硬防 pinch、软防收缩侵蚀）。

### 3.2 面积加权顶点 quadric（已对齐，作回归基线）

- **出处**：M004 §3 Eq.(7)-(8)（pp.2-3，barycentric area a/3）；M003 §3.1（p.2）。
- **ManuMesh 映射**：`Quadrics.cpp:105-111` 用 `baryArea = area/3` 加权、
  `normalSum` 面积加权——与 M004 逐项一致；退化面兜底
  `1e-6 * pointQuadric`（:98-102）对应 M004 的 probabilistic point quadric 兜底。
  **无需改动**；后续任何 quadric 变更以此为回归基线。

### 3.3 特征敏感度量（Wang 2008 FS metric）——可搬运的是解耦模式

- **出处**：M029 §3.1-3.3（pp.597-599）、§4.1-4.2（pp.599-600）。
- **数学要点**：
  - 顶点提升为 6D `v = (p, w·n)`（模型归一化到单位立方后 w ≈ 0.03），
    quadric 为 6×6 投影余算子 `K = I - Aᵀ(AAᵀ)⁻¹A`（到三角形张成的 2D 超平面）。
  - **洞见 1（约束优化）**：6D 中位置与法向不独立——无约束最优会产出与局部几何矛盾的
    (p, n)。必须迭代：固定 n 解 p → 由新 p 的局部几何重算 n → 线搜索；
    2-4 次收敛（§3.3 Fig.5）。
  - **洞见 2（blow-up 权重解耦）**：特征顶点的折叠代价乘 λ
    （edge 顶点 λ = ⌈θ/t⌉+1，corner λ = Σ邻居λ+1），
    **λ 只进队列优先级、绝不进 placement 优化**（§4.1）。
  - 边界折叠的线搜索约束在边界折线上；折叠后 flip 检出时代价 ×3 惩罚而非硬拒绝（§4.2）。
- **为什么鲁棒**：法向差进入度量后，"位置几乎不动但法向急变"的弱折痕获得非零代价
  （纯位置 QEM 的盲区）；优先级与求解解耦避免大权重污染数值。
- **ManuMesh 映射**：不建议上 6D（line quadric + featureBoost 已覆盖软保护）。
  **搬运解耦模式**：当前 featureBoost 加进 quadric 本身（`Quadrics.cpp:144-153`），
  同时扭曲 placement；改为在 `CandidateQueue::pushEdge`
  （`src/simplification/CandidateQueue.cpp:20-31`）对 cost 施加乘法优先级因子
  （`additionalCost` 通道已存在，可加乘法版本），placement 用干净 quadric 求解。

### 3.4 属性 QEM：扩维 vs 标量附加项的取舍

- **出处**：M003 §5-6（pp.4-5）；M031 §4.4（p.6）；M033 §8.1-8.2（pp.31-35）。
- **数学要点**：
  - **扩维**（M003）：n 维点（xyzst=5D, xyzrgb=6D），
    `A = I - e₁e₁ᵀ - e₂e₂ᵀ`（e₁,e₂ 为三角形在 Rⁿ 的正交基，Gram-Schmidt）。
    优点：位置-属性交叉相关进同一二次型，optimal placement 一步**合成**新属性；
    缺点：存储 O(n²)（xyzst 21 系数 vs 3D 的 10）、n×n 求逆、
    属性须归一到与位置同尺度、结果须 clamp（色域）/renormalize（法向）。
  - **分离标量项**（M031）：几何参数化只由位置决定，
    `Escalar = c²scalar Σ‖xᵢ - X_v(b̂ᵢ)‖²` 作附加代价 + 属性单独最小二乘。
    优点：存储线性、属性独立可加；缺点：placement 不感知属性梯度
    （属性急变区只通过代价升高间接加密）。
  - **接缝**：两路线都要求属性连续；wedge（每顶点多值）+ 接缝按边界约束处理是标准做法
    （M003 §6.1；M031 corner attributes）。M003 §5 明确：subset placement 下
    分离项就够了，扩维的必要性**只来自 optimal placement**。
- **取舍结论**：属性维度少 + 需要属性合成 → 扩维；属性多或只需"保接缝、防拉伸" →
  分离项 + 接缝边界约束，成本低一个量级。
- **ManuMesh 映射**：`TextureProtection.cpp` 走第三条路（UV 更新计划 + 标量代价 +
  硬拒绝）：
  - cost 公式（:284-287）`weight · L(e)² · Σ(area·Δuv²) / meanUvScale` ——
    **L(e)² 正是 M032 Eq.12 的量纲手法，UV 尺度归一亦正确**；
  - chart mismatch / UV 三角形翻转硬拒绝（:262-269）对应接缝保护；
  - 与扩维相比缺属性合成（UV 只能沿边插值 → §1.4 的修正）。
  - **保持分离项路线**；另外 `countProtectedEdges`（:323-340）初始化时对全部活动边
    做完整 evaluate（O(E×ring)，只喂报表），改为惰性或抽样。

**§3 ManuMesh 行动清单**

1. **[高优先] 修 `Quadrics.cpp:75` 量纲**：`edge.norm()` → `edge.squaredNorm()`
   （或 ×localScale），重标定并默认开启 `boundaryWeight`。
2. 特征优先级从 quadric 解耦到队列 cost 乘法因子（Wang 2008 模式，
   改 `CandidateQueue.cpp:20-31` + `Quadrics.cpp:144-153`）。
3. `TextureProtection` UV 插值权重修正（§1.4）；`countProtectedEdges` 惰性化。
4. 未来加顶点色/法向：默认分离项 + "attribute boundary 视作边界"（M003 §4）；
   仅在确需属性合成时上扩维。

---

## 4. 队列与效率

### 4.1 Lazy deletion 与过期候选

- **出处**：M002 §4.1（p.3）；M031 §4.2（p.5）；工程通行（CGAL/meshoptimizer 均为
  lazy 变体）。
- **数学要点**：折叠后邻域边代价失效。decrease-key（可寻址堆）vs
  **lazy deletion**（重推新候选 + pop 时版本戳校验丢弃过期项）。
  lazy 堆膨胀到 O(E + 折叠数×环大小) 但常数小；均摊每折叠 O(deg·logE)。
  健壮性参数：连续过期 pop 上限（防长退化序列空转），超限全量重建。
- **ManuMesh 映射**：
  - `SimplificationRun.cpp:268-274`（版本戳校验）+ :276-281（stalePops > 10000 重建）+
    :420-422（折叠后重推邻边）——标准 lazy 实现，正确。
  - **三重求解浪费**：`pushEdgeCandidate`（:196-216）纹理分支调
    `solvePlacementCandidates`，`CandidateQueue::pushEdge`（`CandidateQueue.cpp:25`）
    经 `solveOptimal` 再算一遍完整候选列表，pop 后 `tryCollapse`（:286-287）第三次求解
    ——每条边最多 3 次 3×3 特征分解 + 求解。
    **修复**：Candidate 结构携带已解 position（版本戳保证 quadric 未变则解未变），
    pop 校验通过后直接复用；可砍掉约 2/3 求解开销（求解是主循环最热路径）。
  - 重建阈值 10000 与规模无关；建议 `max(10000, activeEdges/4)`。
- **终止健壮性**（ManuMesh 自有、文献少提但必要）：
  `maxAttemptsWithoutCollapse = max(1000, 6×E)`（:182-184）+
  `terminationReason` 枚举（ReachedTarget/NoCandidates/RejectionLimit）——保留；
  加 §2 的修复后拒绝率会变化，需回归观测 `report_.{topology,boundary}RejectedCollapses`。

### 4.2 Garland-Shaffer 大模型策略（out-of-core 两遍扫描）

- **出处**：M030 §3-4（pp.2-4）。
- **数学要点**：
  - Pass 1：均匀网格量化，每 cell 流式累积 **primal quadric**（点到平面集）+
    **dual quadric** `P = (Σvvᵀ, Σv, k)`——其协方差 `Z = D - eeᵀ/f` 的最小特征向量 =
    最小二乘拟合平面法向，最大特征向量 = 点扩展主方向。
  - BSP 构建：优先分裂 primal 误差最大的叶；分裂平面法向取点扩展最大方向；
    特征值比 < 2（点分布不连贯，可能多层 sheet）时改取最小方向防止不同 sheet 压合。
  - Pass 2：重扫原网格把顶点映射到叶、重算 quadric、每叶出一代表顶点。
  - 内存 O(输出) 与输入无关；质量比均匀聚类误差低 10-20%，时间 2.5-3×。
- **为什么快**：quadric 的**可加性**使表面信息可在单遍流式扫描中无损压缩到
  每 cell 固定 20 系数；BSP 附带 LOD 层级。
- **ManuMesh 映射**：当前全内存（VertexState/FaceState 全量驻留）。
  百万面级够用，**不建议实现完整 out-of-core**；两点可借：
  1. >5M 面输入的实用路径 = "量化聚类预简化到内存舒适区 → 精细 edge collapse"
     两段式；Pass1 约 200 行可作为独立 `mesh_edit` 预处理工具。
  2. dual quadric 协方差解释（拟合平面 + 主方向）可复用于 `feature::` 侧的流式统计。

### 4.3 内存布局

- **出处**：M002 §5（p.3，10 floats/quadric）；M003 §3.4 + §6（pp.3,5，
  (A,b,c) 形式避免 4×4 运算、齐次形式求逆明显更贵）；M030 §4.3。
- **数学要点**：4×4 对称 quadric 仅 10 个独立系数；(A,b,c) 三元组
  （3×3 对称压缩 [6] + 3 + 1）比齐次 Mat4 省 37% 且求解直接用 3×3。
- **ManuMesh 映射**：`VertexState` 存满 `Mat4 q`（128B/顶点）且求解时反复
  block 提取（`Quadrics.cpp:199-200`）。改 (A,b,c) 压缩形式侵入面大
  （`detail/SimplificationTypes.h` + 全部 quadric 代码），建议与 4.1 的
  position 缓存改造同批执行。

**§4 ManuMesh 行动清单**

1. **[高收益/低风险] 消除三重求解**：Candidate 携带已解 position，
   push/pop/tryCollapse 复用（`CandidateQueue.cpp:20-31`、
   `SimplificationRun.cpp:196-216, 283-290`）。
2. stale-pop 重建阈值与活动边数挂钩（`SimplificationRun.cpp:277`）。
3. quadric 存储改 (A,b,c) 压缩 + 3×3 求解路径。
4. >5M 面输入增加量化预简化前置段（独立工具，不动核心管线）。

---

## 5. 弱特征保护（CWF consolidation）与显著性

### 5.1 CWF 的 consolidation 思想

- **出处**：M026 §3.1-3.2（pp.3-4）、§4.1-4.2（pp.4-5）；M004 Fig.2（速度对比）。
- **数学要点**：
  - 简化重述为可移动点集上的全局能量：`E = λ_NA·E_NA + λ_CVT·E_CVT`。
    normal anisotropy 项 `E_NA = Σᵢ ∫_Ωᵢ ((x-xᵢ)ᵀn_x)² dσ` 是 **QEM 的区域积分推广**
    （QEM 对关联三角形求和，CWF 对该点支配的 Restricted Voronoi 区域积分）；
    CVT 项管三角形质量。L-BFGS 交替优化分解与点位。
  - **弱特征的本质**（§1 + Fig.4）：强特征 = 局部法向剧变；弱特征 = 大尺度缓变的形状
    转折。逐边贪心 QEM 每步局部误差都小、看不见弱特征；区域积分能量看得见。
    ——这从原理上解释了"调大特征权重救不了弱特征"，必须有独立证据通道或区域能量。
  - **Decaying weight**（§4.2）：E_NA 与 E_CVT 量级天然不可比
    （CAD 模型 E_NA→0、有机模型不会）。解法：CVT 权重每轮 ×0.95 衰减
    （先均匀化、后特征对齐的退火），并监测 E_CVT 反弹 > 1.05×历史 min 即早停
    （防有机模型三角质量崩坏）。
  - **Consolidation 画面**（Fig.3）：平面区点不动、跨特征线的点被吸到线上、
    角区点被吸到角上——顶点**主动迁移**到弱特征，而非被动不折叠。
  - 代价：分钟级（M004 Fig.2：同模型 CWF >3min vs edge collapse <1s）。
- **ManuMesh 映射**：不作主管线，作**局部后处理注入**：
  - `QualityRefinement.cpp:190-218` 的 `tangentialCentroidCandidate`
    已是 CVT 梯度的一阶近似（质心 + 切向投影）。
  - 加 NA 语义：位移 d 除切向投影外再乘法向锥惩罚
    `d ← d - Σⱼ(areaⱼ·nⱼnⱼᵀ)·d / Σareaⱼ` 的软化版本（邻域法向锥内移动便宜、锥外贵）；
  - 对 `weakFeature` 标记顶点（`SimplificationRun.cpp:140` 已有字段），
    目标点从"邻居质心"换成"邻域弱特征证据的加权质心"——consolidation 语义，
    O(ring)/顶点。

### 5.2 顶点覆盖与不确定度（Hussain 2008）

- **出处**：M028 §3（pp.2-3）。
- **数学要点**：贪心 minimum vertex cover 选高连接度顶点做保留骨架；
  代价 = QEM + 位置不确定度 `μ_pos = max_j |‖vs-vj‖ - ‖vt-vj‖|` +
  曲率不确定度 `μ_cur`（折叠前后邻域面法向最小点积之差）。
- **价值评估**：vertex cover 对 ManuMesh 价值有限（feature graph 已提供更强骨架）；
  **μ_cur 是无需特征检测的廉价弱特征代理**（O(ring) 纯法向统计），
  可作为无特征模式下 `pushEdgeCandidate` 的乘法因子备选（与 §3.3 同通道）。

### 5.3 显著性评分的统一接入位

- **出处**：M027（学习式显著性，未实现）；M004 §5.3（skinning → line quadric 权重
  `wᵢ = c(1-sᵢ)`，c=0.15 使 wᵢ ≲ 0.1）。
- **要点**：任何 [0,1] 标量显著性场（persistence、normal tensor 得分、外部标注、
  学习模型输出）的正确接入位只有两个：
  1. **per-vertex line quadric 权重**——软约束、保 placement 数值健康；
  2. **队列 cost 因子**——硬优先级、不碰 placement。
  直接缩放 QEM 是错误接入位（平面区零误差 × 任意权重 = 0，M004 Fig.1）。
- **ManuMesh 映射**：`computeFeatureWeightScores` → featureBoost 通道
  （`Quadrics.cpp:123-156`）已是接入位 1；补齐接入位 2（§3.3）后接口即完备，
  新显著性来源只需产出标量场，不再改管线。

**§5 ManuMesh 行动清单**

1. `QualityRefinement` 注入 NA 法向锥惩罚 + weakFeature 顶点的 consolidation 目标点。
2. 借 decaying weight：refinement 迭代内质量项权重衰减 + 质量反弹早停
   （`QualityRefinement.cpp:262-286` 已有零接受早停，可加反弹条件）。
3. μ_cur 作为无特征检测模式的队列因子备选。

---

## 6. 重网格化对简化层的约束需求（mesh_edit 扩展接口依据）

### 6.1 局部算子四件套与共享 collapse 内核

- **出处**：M038 §4（p.4）；M040 §2（pp.1-2）。
- **数学要点**：isotropic remeshing = 迭代 5-10 轮：
  1. **split**：边长 > 4/3·L 处对半分；
  2. **collapse**：边长 < 4/5·L 时折叠到中点（4/3 与 4/5 是 [BK04] 推导的
     防振荡"启发式最优"阈值对）；
  3. **flip**：最小化两关联三角形 4 顶点 valence 与目标（内部 6、边界 4）偏差平方；
  4. **tangential relaxation**：`p ← p + λ(I - nnᵀ)(g - p)`，g 为（面积加权）质心，
     可选 kD-tree 投影回原表面。
  - 面积均衡精调（M038 §4）：gravity-weighted 质心
    `gᵢ = Σ A(pⱼ)pⱼ / Σ A(pⱼ)`，< 20 轮把顶点 Voronoi 面积方差降 ~5×。
- **接口结论**：remeshing 的 collapse 与简化的 collapse 是**同一算子**，
  差别仅在触发条件（边长 vs 误差代价）与合法性子集
  → `mesh_edit` 应暴露 `collapse(edge, position, policy)`，
  policy = {legality 检查集合, placement 策略}。
- **ManuMesh 映射**：`CollapseLegalityInput` / `CollapseAttemptInput` 已是雏形，
  但触发逻辑焊死在 `SimplificationRun`；解耦后 flip 是唯一净新增算子
  （M029 §4.2 Fig.7 也在简化后处理用 flip 消近退化三角形）。
  flip 合法性：不产生重复边、不翻法向、**特征边禁 flip**。

### 6.2 自适应 sizing field（单参数 ε）

- **出处**：M040 §3（p.2）。
- **数学要点**：`L(x) = √(6ε/κ - 3ε²)`，κ = max(|κmin|, |κmax|)
  （cotan 离散：κ = H + √(H²-K)）；边 sizing 取端点 min；clamp 到 [Lmin, Lmax]。
  推导 = 圆弧弦高勾股 + 等边三角形外接圆缩放 √3/12。单参数 ε（几何容差）即全部输入。
- **接口结论**：sizing field 是 per-vertex 标量场，与 §5.3 显著性场**同构**——
  `mesh_edit` 调度器接受统一 `VertexScalarField`（简化当优先级、remeshing 当目标边长）。

### 6.3 特征/边界约束在 remeshing 中的形态（约束清单）

- **出处**：M038 §2/§4（pp.2,4：有尖特征的面 remesh 会破坏特征对齐，须约束）；
  M040 §2（边界 valence 目标 4）；M004 §5.2（line quadric 平衡特征保持与均匀化）。
- **remeshing 层将对简化/mesh_edit 层提出的需求**：
  1. 特征边不可 flip；特征顶点 relaxation 约束到**特征曲线切向**（一维）——
     `FeatureVertexGuidance.tangent`（`SimplificationRun.cpp:131-155`）可直接复用；
  2. 边界顶点 relaxation 约束在边界折线上，valence 目标 4；
  3. **split 的特征语义**：新顶点落在特征边上须继承 loop id / 插值 tangent——
     当前 `VertexState` 特征字段只有折叠传播（keep 端保留），无 split 插值语义，
     这是数据流缺口；
  4. junction/corner 顶点（`featureJunction`）完全冻结。
- **面积均衡的下游价值**（M038 §5, pp.4-6）：等 Voronoi 面积 → cotan Laplacian
  可对称化 → SPD → CG/带限 Cholesky（比 BiCG 快 ~2× 且收敛有保证；
  带限 Cholesky O(bn) 比 multigrid 还快一个量级）。
  若 ManuMesh 未来做变形/平滑，remeshing 的面积均衡是**求解器选择问题**而非美观问题。

**§6 ManuMesh 行动清单**

1. collapse policy（legality 集合 + placement 策略）从 `SimplificationRun` 解耦为
   `mesh_edit` 可复用组件。
2. `mesh_edit` 新增 edge flip 算子（valence 目标 + 特征边禁 flip）。
3. `VertexState` 特征字段定义 split 插值语义（loop id 继承 + tangent 插值）。
4. relaxation 支持三档约束：自由（切平面）/ 特征曲线（一维切向）/ 冻结（junction）
   ——当前只有自由与跳过两档（`QualityRefinement.cpp:266-270`）。
5. sizing field `L(x) = √(6ε/κ - 3ε²)` 实现为独立 `VertexScalarField` 工具，
   简化优先级与 remeshing 共用。

---

## 附：论文速查

| M | 论文 | 本文档取用的核心内容 |
|---|---|---|
| M002 | Garland-Heckbert 1997 | quadric 推导、三级 placement fallback、boundary constraint plane、normal flip |
| M003 | Garland-Heckbert 1998 | 扩维属性 quadric、(A,b,c) 形式、面积加权、attribute-boundary-as-boundary |
| M004 | Liu et al. 2025 line quadrics | Fréchet mean 机制、w<0.1/默认 1e-3、先加后缩、dihedral plane quadric 对比 |
| M026 | Xu et al. 2024 CWF | NA 区域积分能量、decaying weight 退火、弱特征 consolidation |
| M028 | Hussain 2008 | vertex cover 骨架、μ_pos/μ_cur 不确定度代理 |
| M029 | Wang 2008 FS metric | 6D 约束优化、blow-up 权重只进队列不进求解、flip 后处理 |
| M030 | Garland-Shaffer 2002 | 两遍流式扫描、dual quadric 协方差、BSP 自适应聚类 |
| M031 | Hoppe 1996 | 能量项分解、优先队列、自适应 spring、不连续曲线六判据 |
| M032 | Lindstrom-Turk 1998 | 体积/边界守恒约束求解、α-compatibility、L(e)² 量纲统一 |
| M033 | Rose et al. 2025 | 三项 validity check、约束选择 α=5°、全套公式重推导 + 参考实现 |
| M038 | Botsch-Kobbelt 2004 | split/collapse/flip/relax 四件套、4/3-4/5 阈值、面积均衡 → SPD 求解器 |
| M040 | Dunyach et al. 2013 | 曲率自适应 sizing field 闭式解 |
