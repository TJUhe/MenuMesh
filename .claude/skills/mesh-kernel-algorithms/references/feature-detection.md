# 特征检测算法工程参考（论文蒸馏版）

日期：2026-07-12。来源：M007/M009/M011/M013/M014/M016/M021/M042/M044 的方法章节精读
（页码为各 PDF 物理页），加圆/椭圆拟合经典文献（无本地 PDF，标注原始出处）。
本文件是**工程决策清单**，不是论文摘要：每条 = 技巧名 / 出处 / 数学要点 /
为什么鲁棒或快 / 映射到 ManuMesh 的具体改进点（file:line 基于 2026-07-12
的 `codex/mesh-edit-quality-foundation` 分支）。

约定：`κ_max ≥ κ_min` 为主曲率，`t_max/t_min` 为主方向，
extremality `e_i = ∇κ_i · t_i`（曲率沿自身主方向的方向导数）。

---

## 1. 曲率与曲率导数估计

### 1.1 Per-face 张量差分 + Voronoi 加权（默认估计器）
- 出处：M014 Rusinkiewicz 2004, p.3-4。
- 数学要点：每个三角形在自身切平面坐标系 `(u_f, v_f)` 内，用 3 条边的
  法向差分列 6 个线性约束解对称 `II = [e f; f g]`（3 未知量最小二乘）：
  `II · (e_j·u, e_j·v)^T = ((Δn_j)·u, (Δn_j)·v)^T`，j=0,1,2。
  曲率导数张量 `C`（2×2×2，仅 4 独立分量 a,b,c,d）用完全相同的模式解：
  把各顶点 `II` 转到 face 坐标系，沿边差分 `C · e_j ≈ ΔII`。
  面→顶点聚合用 Voronoi 面积权重；坐标系转换必须**先绕两法线叉积旋转到共面**
  再投影，否则每次换系损失 `cos²θ` 的曲率（p.3-4）。
- 为什么鲁棒/快：一次遍历 faces + 一次遍历 vertices，无需邻域搜索结构；
  唯一退化情形是三点共线的退化三角形；球面上无论三角形形状结果精确。
  1.5M 面网格曲率 4s、导数 5.2s（2004 年硬件）。
- ManuMesh 映射：当前唯一的曲率来源是逐顶点逐尺度的 Monge 拟合
  （src/feature_detection/SmoothCurvature.cpp:114-270），每顶点每尺度一次
  BFS 过滤 + 5×5 QR（SmoothCurvature.cpp:198-230），成本高一个数量级。
  应新增 `RusinkiewiczCurvature.cpp` 作为**默认廉价路径**（干净网格 + 轻噪声），
  把现有 robust 拟合降级为噪声扫描件的 opt-in 路径。`C` 张量直接给出
  extremality `e = C(t,t,t)`，替代 1.3 所述的邻居差分判据。

### 1.2 纯离散 edge-based shape operator（无拟合备选）
- 出处：M042 Hildebrandt 2005, p.2-3。
- 数学要点：边上 `S(e) = 2|e|cos(θ_e/2) · (ē×N_e)(ē×N_e)^T`，
  `N_e = (N_1+N_2)/‖N_1+N_2‖`；顶点 `S(p) = ½ Σ_{e∋p} ⟨N_p,N_e⟩ S(e)`，
  再乘 `3/area(star(p))` 把积分量重标定为逐点量（lumped mass）。
- 为什么鲁棒/快：只用 dihedral angle 和边长，无线性求解；积分量→逐点量的
  重标定使结果可直接线性插值（PL 函数），支撑 3.2 的 level-set 追踪。
- ManuMesh 映射：可作为 NormalTensor 路径（src/feature_detection/NormalTensor.cpp:40-62）
  的姊妹实现：同一次边遍历里同时累加 `n n^T`（法向张量）和 `S(e)`（曲率张量），
  一份邻域两种证据。

### 1.3 Extremality 判据用零交叉，不用"比邻居大"
- 出处：M011 Ohtake 2004, p.1-2；M021 Yoshizawa 2005, p.1；M042 p.2。
- 数学要点：ridge 的正确定义是 `e_max = 0` 的零交叉附加
  `∂e_max/∂t_max < 0` 与 `κ_max > |κ_min|`（valley 对偶：`e_min = 0`、
  `∂e_min/∂t_min > 0`、`κ_min < −|κ_max|`）。特征在**边内部的插值点**，
  不是"曲率比邻居平均高的顶点"。
- 为什么鲁棒：零交叉是符号事件，对曲率幅值的系统偏差不敏感；
  "比邻居大"的极值测试在均匀高曲率带（圆角面）上整片误报，
  在采样不均时漏报。
- ManuMesh 映射：SmoothCurvature.cpp:283-364 `classifyScaleCandidate` 正是
  "邻居法曲率平均对比"判据（:334-338），且 :317 的 `|alignment|<0.2` 硬剔除
  在各向异性网格上丢证据。应改为：顶点只输出 `(κ_max, κ_min, t_max, e_max, …)`，
  边级判据用 1.5 的 Ohtake 零交叉规则（落点在 FeatureEvidence.cpp:211-260
  `smoothCurvatureEdgeCandidate`）。

### 1.4 隐式/高阶拟合的平滑作用要参数化，不要绑死
- 出处：M011 Ohtake 2004, p.2-3。
- 数学要点：全局 CS-RBF 拟合 `F(x)=0` 后解析求导：
  `e = (F_ijl t_i t_j t_l + 3κ F_ij t_i n_j)/|∇F|`（Eq.2，Einstein 求和）。
  平滑度旋钮有三个：Tikhonov 正则 `(Φ+λI)`（λ=0.1）、Wendland C³ 基函数
  支撑半径加倍、**层级截断**（用简化层 `F_k, k<n` 求导 = 一种 mesh smoothing，
  显著减少 ridge 碎片化）。
- 为什么鲁棒：全局光滑函数的三阶导数连续，消除了局部邻域跳变导致的
  "jerky" 曲率导数；但 4M 面要 1 小时——**不要照抄全局拟合**，
  抄的是"平滑发生在求导之前、且平滑量是显式参数"这一结构。
- ManuMesh 映射：SmoothCurvature.cpp:129-145 的法向高斯平滑是隐式的
  半吊子版本。更便宜的等效：Yoshizawa 预平滑（1.5）或 Hildebrandt 的
  extremality 场平滑（3.3），二选一，平滑步数暴露为 option。

### 1.5 Cubic 拟合一次求出曲率+extremality（Yoshizawa 配方）
- 出处：M021 Yoshizawa 2005, p.2-3。
- 数学要点：(a) 预平滑：`p' :=` 相邻三角形 centroid 的算术平均；
  (b) Max 权重法向；(c) 局部坐标系拟合无线性项 cubic：
  `h = ½(b₀x² + 2b₁xy + b₂y²) + ⅙(c₀x³ + 3c₁x²y + 3c₂xy² + c₃y³)`，
  邻域 = k-ring **剔除法向与中心成钝角的顶点**；
  (d) extremality 化简为三次型 `e ∝ c₀t₁³ + 3c₁t₁²t₂ + 3c₂t₁t₂² + c₃t₂³`。
- 为什么鲁棒/快：法向参与拟合（adjacent-normal）+ 钝角剔除抑制跨特征污染；
  对 sliver 三角形和不规则连接不敏感（原始 vs 重网格化 bunny 结果一致，p.3）；
  吞吐 ~20K/k 三角形每秒；一次拟合同时供曲率与三阶量。
- ManuMesh 映射：SmoothCurvature.cpp:170-196 的设计矩阵只有
  `u², uv, v², u, v` 五列——**无三次项，原理上取不到 extremality**。
  最小改动：扩到 9 列（加 `u³,u²v,uv²,v³`），解出 c 系数后按三次型直接给
  每顶点 `e_max/e_min`；邻域过滤加钝角法向剔除（现在只有 depth 过滤，
  SmoothCurvature.cpp:176-179）。

### 1 行动清单
1. 新增 Rusinkiewicz per-face `II` + `C` 张量估计器（一次遍历，默认路径）。
2. SmoothCurvature 拟合升级为 cubic（9 参数），输出解析 extremality。
3. 顶点级"邻居对比"极值判据（SmoothCurvature.cpp:283-364）退役，
   改为边零交叉（见 3.1）。
4. 平滑（法向/extremality）从隐式行为改为显式参数。

---

## 2. 锐边（硬证据）

### 2.1 二面角取向：`abs(dot)` 是错的，>90° 折边会被吃掉
- 出处：M007 Jiao 2008, p.9-10, p.17；M011 p.2（追踪时 t 与 e 同步翻转）。
- 数学要点：ManuMesh 当前 `acos(|n₀·n₁|)` 把角度折叠进 [0°,90°]：
  法向夹角 120°（`dot=−0.5`）被读成 60°，薄片折边 ~180°（`dot≈−1`）
  被读成 0° 而**完全漏检**。正确做法：先对每个连通分量做一次绕向一致化
  （BFS 传播，相邻面共享边的方向应相反；不一致则翻转），
  然后用带符号 `acos(clamp(n₀·n₁))`，值域 [0°,180°]。
  凸凹判别用 Jiao 的 signed volume：edge `v̄w` 两侧面 `vwx, wvy`，
  `sign(d_vw · (d_vx × d_vy))`，负=凸、正=凹——比质心法便宜且无 epsilon 死区。
- 为什么鲁棒：Jiao 专门给 acute edge（DA>90°）设 disjoint 规则（p.17），
  因为锐折边附近 false-positive quasi-strong 频发；法向张量的平方性质
  `m mᵀ` 无法区分 near-cusp 和平面（p.11）——所以**锐折边只能靠带符号
  二面角**，张量补不回来。
- ManuMesh 映射：FeatureEvidence.cpp:94（`markDiscreteFeatureVertices`）和
  :294-295（`DihedralEvidenceStrategy`）两处 `std::abs`。加绕向一致化
  预处理后去掉 abs；signedDihedralKind（FeatureEvidence.cpp:133-157）的
  质心叉积法换成 signed volume，去掉 :152 的 `1e-12` 早退。

### 2.2 Normal voting：投票旋转 + 三类顶点分类
- 出处：M013 Page 2002, p.210-214。
- 数学要点：facet 法向 `N` 传播到顶点 `v` 时沿密切圆弧旋转：
  `N_i = N + 2cosθ_i · vc_i/‖vc_i‖`，`cosθ_i = −N·vc_i/‖vc_i‖`，
  `vc_i = c_i − v`；权重 `w_i = (A_i/A_max)·exp(−g_i/σ)`，`3σ = g_m`
  （geodesic 邻域半径，唯一用户参数，取 `k·l_ave`）。
  投票张量 `V = Σ w_i N_i N_iᵀ`，特征值 `λ₁≥λ₂≥λ₃`，三类分类取
  `max{S_s, αS_c, βS_n}`：`S_s=λ₁−λ₂`（surface）、`S_c=λ₂−λ₃`（crease，
  切向=E₃）、`S_n=λ₃`（corner/无向）。系统常数 `α=β=2`；
  可检测最小折角 `γ` 满足 `tan(γ/2)=1/(1+α)`。
- 为什么鲁棒：外积投票不怕法向 sign 翻转；圆弧旋转消除高曲率处
  平移投票的系统偏差；大 geodesic 邻域平均掉噪声，而分类是**三个
  特征值差的相对比较**（无量纲），阈值不随网格尺度漂移。
- ManuMesh 映射：NormalTensor.cpp:42-56 只累加**关联面**的 `A·n nᵀ`
  且不旋转投票，:71-97 用各向同性高斯平滑近似扩大支撑——会跨 crease
  糊化。改进：(a) 支撑扩到 k-ring/半径邻域并用 Page 权重与投票旋转；
  (b) analyzeNormalTensor（NormalTensor.cpp:11-28）的
  `featureScore = max(creaseSaliency, cornerSaliency)` 改成三路
  `max{S_s, 2S_c, 2S_n}` 相对分类，输出离散 label（surface/crease/corner），
  corner label 直接供 FeatureGraph 做 junction 锚点；
  (c) FeatureEvidence.cpp:195-197 的 crease/corner 比较随之变成 label 判断。

### 2.3 邻域在 crease 处截断（second-pass 规则）
- 出处：M013 Page 2002, p.215-216, Fig.13。
- 数学要点：估计曲率（或任何二阶量）时，邻域扩张**遇到非 surface-patch
  顶点即停**，防止 crease 另一侧的样本污染本侧估计。
- 为什么鲁棒：crease 两侧是不同光滑面，混合样本产生的不是噪声而是
  系统性错误（拟合出斜面），任何 robust 权重都救不回来。
- ManuMesh 映射：SmoothCurvature.cpp:70-95 `gatherNeighborhood` 的 BFS
  应接受一个 `stopMask`（用 FeatureEvidence.cpp:88-103 已经算好的
  `discreteFeatureVertex`），跨过被标记顶点就不再扩张。当前
  discreteFeatureVertex 只用来事后否决边候选（FeatureEvidence.cpp:171-175），
  没有反馈进拟合邻域。

### 2.4 Jiao 的 C1/C2 分离：相对强度 + 曲线级过滤
- 出处：M007 Jiao 2008, p.10-21。
- 数学要点：C1（法向不连续）检测不靠单一 DA 阈值，而是
  **l-strong（局部相对最强）/ u-strong（无条件强）**双层判据：
  l-strong DA = 同顶点同侧 halfedge 里 DA 最大且 `DA>θ_f`；
  u-strong DA = `DA>θ_F`。再经 attachment → quasi-strong edge →
  曲线级迭代剥离 obscure curves。C2（曲率不连续）判据：quasi-obscure
  curve 两侧做 flat/non-flat 分类——某侧无 quasi-strong edge 为 flat，
  或一侧 quasi-strong edge 平均正交距离 ≥2.5× 另一侧则远侧为 flat；
  两侧分类不同 → C2。推荐阈值（p.19）：`θ_f≈10°`（要 C2 时取 1°）、
  `θ_F≈65°`、`θ_D≈60°`（angle defect corner）、`θ_T≈40°`（turning angle）、
  `t≈20°`（OSTA）、`θ_e≈25°`。复杂度 `O(n + m log m)`。
- 为什么鲁棒：主要依赖相对强度而非绝对阈值，参数基本免调；
  false-negative 概率是 p²、false-positive 是 q⁴（quasi-strong 判据的
  概率分析，p.14-15）；曲线级过滤把孤立误检整条清掉。
- ManuMesh 映射：当前 dihedral 是单阈值（FeatureEvidence.cpp:296-297）。
  引入两级阈值 `θ_f/θ_F`：`>θ_F` 直接进图；`θ_f..θ_F` 之间只有当
  该 halfedge 在顶点处是局部 DA 最大（l-strong）才进图。这一改动
  几乎免费（一次顶点邻接扫描），能同时降低 tessellation 噪声误报和
  浅折角漏报。angle defect `ad(v)=2π−Σθ_i > θ_D` 作为 corner 硬证据
  补进 FeatureGraph 的 junction 判定（当前只有度数>2 的图判据，
  FeatureGraphCleanup.cpp:243-247）。

### 2 行动清单
1. 绕向一致化 + 去 abs：修复 >90° 折边漏检（FeatureEvidence.cpp:94, :294）。
2. dihedral 判据升级为 Jiao 双层 l-strong/u-strong。
3. NormalTensor 升级为 Page 投票（旋转 + 衰减权重 + 三路相对分类）。
4. 拟合/平滑邻域在 discreteFeatureVertex 处截断。
5. angle defect corner 证据接入 junction 判定。

---

## 3. 弱特征 / 光滑特征（crest / ridge / valley）

### 3.1 边零交叉的正确实现（方向对齐是关键）
- 出处：M011 Ohtake 2004, p.2（Eq.3-4）；M021 p.3。
- 数学要点：对边 `[v₁,v₂]`：
  (1) 若 `t_max(v₁)·t_max(v₂)<0`，翻转 `t_max(v₂)` **并同步翻转**
  `e_max(v₂)`（e 是沿 t 的方向导数，符号绑定）；
  (2) 判据：两端都满足 `κ_max>|κ_min|` 且 `e_max(v₁)·e_max(v₂)<0`；
  (3) 极大（非极小）测试用一阶替代：`e_max(v_i)·(v_{3−i}−v_i)·t_max(v_i)>0`
  （Ohtake 明确说二阶导测试"对复杂模型不实用"）；
  (4) 子顶点位置反比插值：`p = (|e₂|v₁+|e₁|v₂)/(|e₁|+|e₂|)`。
  三角形内 2 边命中 → 连线段；3 边命中 → 连 centroid（triple junction）。
- 为什么鲁棒：主方向是 line field（±t 等价），不做符号对齐时零交叉
  检测是随机的——这是所有 naive crest 实现的第一死因。
- ManuMesh 映射：这是 smoothCurvature 边证据的替换算法。落点：
  FeatureEvidence.cpp:211-260 重写为零交叉判据；SmoothCurvatureVertex
  增加 `extremality` 字段（由 1.5 的 cubic 拟合或 1.1 的 C 张量提供）。
  注意 ManuMesh 特征图以 mesh 顶点为节点，可先把"边上有零交叉"作为
  边证据（不引入子顶点），保持图结构不变。

### 3.2 Hildebrandt regular/singular 三角形与符号感知平滑
- 出处：M042 Hildebrandt 2005, p.3-5。
- 数学要点：三角形 regular ⇔ 三顶点 `t_i` 可选符号使两两内积为正；
  singular 三角形（0.6%-10% 频率，来自噪声制造的伪脐点）单独处理：
  相邻 regular 三角形的 segment 端点落在共享边上，按标记边数
  2/3/1 分别连线段 / barycenter trisector / 忽略。
  extremality 平滑用符号感知 Laplacian：
  `Δe(p) = Σ_q w_pq (σ_pq e(q) − e(p))`，`σ_pq = sign⟨t(p),t(q)⟩`，
  cotan 权重、隐式积分、**≤5 步**即可，结果与初始符号选择无关。
- 为什么鲁棒：e 含三阶导数，是管线里噪声最敏感的量；直接平滑曲面到
  三阶质量代价大且变形不可控，平滑 e 标量场便宜且不动几何。
- ManuMesh 映射：在零交叉检测（3.1）之前插入 2-5 步 σ_pq 平滑，
  作为 `smoothCurvatureRobustFitIterations` 之外的独立 option。
  singular 三角形判据同时是天然的 junction 检测器——比
  FeatureGraphCleanup.cpp:233-291 的"距离近就桥接"更有原理。

### 3.3 无量纲强度：长度 × 沿线积分，而不是逐点分数
- 出处：M021 Yoshizawa 2005, p.4（Eq.5-6）；M011 p.2（Eq.5）。
- 数学要点：Ohtake 强度 `T = ∫κ_max ds`（梯形近似；κ~1/L、ds~L，
  天然无量纲）。Yoshizawa 更优：`T = (∫ds)·(∫√(e_max²+e_min²) ds)`，
  第二因子是 cyclideness（偏离 Dupin cyclide 的程度；球/柱/锥/环面上
  `|e_max|²+|e_min|²≡0`，这些面上的伪 crest 被自动压灭）。
  e~1/L²，∫C ds~1/L，乘长度后 scale-independent；典型阈值 0.9-3.2。
  设计取向：**长而弱的线优先于强而短的线**——短脊被长度因子自动压低。
- 为什么鲁棒：逐点/逐边阈值无法区分"一段长而浅的真实圆角线"和
  "一撮短而强的噪声刺"；沿线积分把判断推迟到曲线级，恰好是
  信息完整的时刻。
- ManuMesh 映射：当前弱特征过滤全在顶点/边级
  （FeatureEvidence.cpp:240-255 的多重 min/阈值链）。应把最终裁决移到
  组件级：FeatureGraphCleanup.cpp:321-343 `computeConfidence` 增加
  `strengthIntegral = Σ_edge 0.5(s_a+s_b)·len` 项（s 用 persistentScore
  或 extremality 模），组件低于无量纲阈值整体丢弃——替代
  removeWeakSpurs（:93-145）按边数硬剪的做法。

### 3.4 多尺度 persistence 的正确姿势（Luo-Zha）
- 出处：M009 Luo-Zha 2008, p.2-3。
- 数学要点：尺度空间 = anisotropic diffusion 迭代序列（t=0..10），
  edge-stopping 量 `‖F_i‖ = Σ_{相邻面对} arccos⟨n_j,n_k⟩`，
  conduction `g(x)=1/(1+x²/c²)`；顶点跨尺度**恒等对应**（只动位置
  不动连接），persistence = `score_i = Σ_t [κ_max(v_i,t) > k_th]` 计票；
  几何输出取自 **t=0 原始网格**（平滑序列只做 gating，不贡献坐标）。
- 为什么鲁棒：各向异性扩散在平坦区强扩散（噪声几步就死）、在特征处
  `g≈0`（特征跨尺度存活），投票天然分离真伪；恒等对应回避了
  跨尺度位置漂移匹配问题。36.5K 顶点 11 尺度全流程 4s。
- ManuMesh 映射：两条现有 persistence 都基本正确但有两个偏差：
  (a) NormalTensor 的各向同性高斯平滑（NormalTensor.cpp:71-97）会把
  crease 本身糊掉——平滑核应乘 edge-stopping 因子 `g(‖F‖)`（一行改动：
  weight 里乘 `g`）；(b) SmoothCurvature.cpp:459 强制要求最粗尺度支持
  （`coarsestScaleSupported`）会杀掉真实的细小特征——Luo-Zha 是计票制
  （≥m 票即保留），不要求存活到最粗尺度。建议把 coarsest 要求改成
  `persistentScales ≥ minPersistentScales` 纯计票（该 option 已存在：
  FeatureEvidence.cpp:235-240，只需删除 :459-461 的一票否决）。

### 3.5 Gap-jumping 角度规则（追踪级链化）
- 出处：M021 Yoshizawa 2005, p.3, Fig.4。
- 数学要点：某顶点 one-ring 内出现两条 crest line 的端点时，当且仅当
  `α ≥ π/3`、`α' ≥ π/3`、`β ≤ π/2` 才连接（α, α' 为两条末段与连接段的
  夹角，β 为两末段间夹角）——即连接段必须近似延续两条线的切向。
- 为什么鲁棒：只看距离的桥接会把平行走向的两条特征焊在一起；
  角度规则以 O(1) 代价保证只补"断在同一条线上的缝"。
- ManuMesh 映射：FeatureGraphCleanup.cpp:169-178
  `endpointGapDirectionsCompatible` 的容忍度 `min(align) ≥ −0.15` 过松
  （允许几乎垂直甚至略回折）。改为 Yoshizawa 三角规则：两端外向切向与
  连接方向夹角 ≤ 60°（即 `align ≥ 0.5`）且两外向切向近对向。

### 3 行动清单
1. 边零交叉判据替换 smoothCurvature 边证据（含 t/e 同步翻转）。
2. 组件级无量纲强度 `T = 长度 × ∫强度 ds` 过滤，替代按边数剪毛刺。
3. NormalTensor 平滑乘 edge-stopping 因子，变各向异性。
4. 删除 coarsest-scale 一票否决，persistence 改纯计票。
5. gap 桥接从距离判据升级为 Yoshizawa 角度三条件。
6. extremality 场加 σ_pq 符号感知平滑（≤5 步隐式）。

---

## 4. 特征曲线网络

### 4.1 Lu 2019 的真实结构：quadric 拟合是过滤器 + 扩展代价，不是交替优化
- 出处：M044 Lu 2019, p.2-3（短文，无 curve-surface 迭代循环，
  初始线来自 Yoshizawa crest lines）。
- 数学要点：两个曲线级过滤器：
  (a) `R = mean(√(κ_max²+κ_min²))` 沿线平均 RMS 曲率，剔平坦区伪线
  （示例阈值 0.005）；
  (b) **单 quadric 拟合误差 E**：取曲线切过的三角形集合做 3 次邻接膨胀，
  两侧合在一起拟合一张 general quadric，若平均误差 E 低于阈值
  （示例 3e-6）→ 该"曲线"整体躺在一张光滑二次曲面内部，**不是**
  两面交线，删除。
  曲线补全：端点向 1-ring 前方（夹角<90°）扩展，代价
  `F(v_k) = E[v_k] + G(t_min 方向夹角)`，全局单一 priority queue 调度；
  junction 规则 = 撞到已属于另一条曲线的顶点即停。
- 为什么鲁棒：过滤器 (b) 是唯一能区分"高曲率但光滑（圆柱侧壁）"与
  "两面交线"的判据——曲率幅值判据对此无能为力。
- ManuMesh 映射：这是弱特征假阳性的最强武器。在
  FeatureGraphCleanup.cpp:321-343 `computeConfidence` 里加
  "单 quadric 残差"证据：对弱证据组件收集两侧 2-3 ring 面片拟合一张
  quadric（10 参数隐式二次型 `x^T A x + b^T x + c`，最小特征向量解），
  残差小 → 组件置信度大幅降权。当前 confidence 只有证据比例 + 闭合率 +
  primitive 残差，缺"这条线是否真的分隔两张面"的判据。

### 4.2 Vidal 2011 的两侧切平面角 θ̂ʳ（噪声网格上最强的边判据）
- 出处：M016 Vidal 2011, p.2-3, p.5。
- 数学要点：以边中点为球心、半径 r（取 bbox 最小维的 4%-28%），
  球内三角形按"重心到边分隔面的距离"从近到远入 priority queue，
  按分隔面符号分左右两侧各自生长平面：候选面法向与当前侧估计法向
  夹角 < 23° 才接纳（inlier），接纳后面积加权更新侧法向；
  生长结束后**把先前被拒的三角形再测一遍**（估计已变，outlier 可能
  变 inlier）。特征量 = 两侧平面法向夹角 `θ̂ʳ`。
- 为什么鲁棒：支撑域由欧氏半径决定，与连接性/三角形形状解耦
  （sliver 免疫）；非邻接生长跳过噪声 outlier 面；F-score 实验证明
  噪声网格上 `θ̂ʳ` 的判别力远超单三角 dihedral 与全部曲率量（p.5, Table 1）。
- ManuMesh 映射：作为 dihedral 证据的**噪声档升级**：新增
  `robustDihedralRadius` option，>0 时 DihedralEvidenceStrategy
  （FeatureEvidence.cpp:285-302）对 `θ_f..θ_F` 之间的灰区边用 θ̂ʳ 复核
  （只对灰区算，控制成本）。干净 CAD 网格保持单三角 dihedral。

### 4.3 链化 = 能量问题：相似度 + 共线性成对项
- 出处：M016 Vidal 2011, p.3-4（Eq.1-4）。
- 数学要点：逐边二值标注能量
  `E = Σ_e E_d(ω_e) − μ Σ_{e,e'} E_h`，成对奖励项（仅 ω=ω'=1 时）：
  `α_ee' = exp(−λ(|cosθ_e − cosθ_e'|/σ + 1 − cos T_ee'))`，
  `T_ee'` 为相邻边转角。similarity 项防止把二面角差异大的边串成一条线
  （毛刺抑制），alignment 项奖励共线延续（补 gap）；ω=ω'=0 奖励 β
  （平坦区整体归零 → 孤立误检自动消失）。能量 submodular，
  graph cuts 全局最优。参数：μ=2、β=1e-3、λ=15、σ=10（dihedral 版）。
- 为什么鲁棒：gap 填补、毛刺剔除、junction 一致性由同一个全局最优
  同时决定，无顺序依赖；贪心后处理（先剪毛刺再桥接）的顺序敏感性
  被消除。
- ManuMesh 映射：完整 graph cuts 是大改，但成对项可以立刻低成本复用：
  removeWeakSpurs（FeatureGraphCleanup.cpp:93-145）删边前先算该 spur
  与主链连接处的 `α_ee'`，高对齐 + 高相似的 spur 是真实分支不该删；
  bridgeEndpointGaps（:180-231）的候选排序从纯距离改为
  `distance / α_ee'`。长期路线：弱证据边的最终取舍改为小规模
  graph-cut/动态规划标注（组件内局部求解即可，无需全网格）。

### 4.4 Junction 处理的三个层次
- 出处：M011 p.2（triple junction = 三边命中连 centroid）；
  M042 p.4（singular triangle 的 trisector case）；M007 p.10
  （junction 处 DA/TA 可任意小，必须靠 ridge valence 识别）。
- 数学要点：junction 不是被"检测"出来的，而是三类互补证据的汇合：
  (a) 追踪级：一个三角形三边都有零交叉；(b) 顶点级：corner 分类
  （Page 三路分类的 S_n 支、Jiao 的 angle defect u-strong）；
  (c) 图级：ridge valence ≥ 3。Jiao 明确警告：junction 顶点的局部
  微分量可以完全平凡，只有图结构能识别它。
- ManuMesh 映射：当前只有图级度数判据（FeatureGraphCleanup.cpp:243-247）
  和 confidence 的 junction 惩罚（:334-336）。junction 惩罚方向反了——
  多分支若有 corner 证据支持（2.2/2.5 的 label）应该**加分**而不是减分；
  只有无 corner 证据的高 valence 才是可疑的。

### 4 行动清单
1. 弱证据组件加"单 quadric 残差"假阳性过滤（Lu 4.1）。
2. 灰区二面角边用两侧切平面角 θ̂ʳ 复核（Vidal 4.2）。
3. spur 删除与 gap 桥接决策引入 similarity+alignment 成对分数（4.3）。
4. junction 惩罚改为 corner 证据条件化（4.4）。

---

## 5. 圆 / 椭圆拟合

（经典文献，无本地 PDF：Kåsa 1976 IEEE TIM；Taubin 1991 PAMI；
Pratt 1987 SIGGRAPH；Fitzgibbon-Pilu-Fisher 1999 PAMI；
Halíř-Flusser 1998 WSCG；Chernov《Circular and Linear Regression》2010。）

### 5.1 现状诊断
- ManuMesh 圆拟合（src/feature_detection/PrimitiveFit.cpp:60-92）是
  **Kåsa 代数拟合**：解 `[2x 2y 1]·[cx cy c]ᵀ = x²+y²` 的正规方程。
  Kåsa 最小化 `Σ(r_i²−R²)² = Σ(r_i−R)²(r_i+R)²`，等效给远点加权
  `(r+R)²`：完整均匀采样的闭环上无碍，但**部分弧 / 采样不均 / 噪声下
  系统性低估半径**（弧越短偏差越大，偏差 ~O(σ²·R/弧长跨度)）。
- 椭圆估计（PrimitiveFit.cpp:94-99）用二阶矩 `a = √(2·E[x²])`：
  仅当顶点在完整椭圆上按参数角**均匀分布**时成立（`x=a·cosθ ⇒
  E[x²]=a²/2`）。特征 loop 顶点间距由网格密度决定，弯曲密集区
  顶点更密 → 轴长系统性偏差；且 PCA 主轴（:45-47）在近圆时方向
  由噪声决定，轴比 `axisRatio` 不可靠。这影响
  classifyPrimitiveFit（:150-178）的 Circle/NearCircle/Ellipse 三分。

### 5.2 推荐替换：Taubin 圆拟合
- 数学要点：代数圆 `f = A(x²+y²) + Bx + Cy + D`，Taubin 约束
  `Σ‖∇f‖² = n`（一阶归一化），化为 3×3/4×4 广义特征值问题：
  中心化数据（x̄=ȳ=0）后，设 `z_i = x_i²+y_i²`，求
  `M q = η N q`，`M = (1/n)Σ(z,x,y,1)(z,x,y,1)ᵀ`，
  `N = diag 型约束阵`（`N = (1/n)Σ∇f 的二次型`，
  显式 `N = [[4z̄, 2x̄, 2ȳ, 0],[2x̄,1,0,0],[2ȳ,0,1,0],[0,0,0,0]]`），
  取最小正 η 的特征向量；`center = (−B/2A, −C/2A)`，
  `R = √(B²+C²−4AD)/(2|A|)`。
- 为什么更好：Taubin 是一阶无偏（essential bias O(σ⁴)，Kåsa 是 O(σ²)），
  Chernov 证明其方差达到代数拟合的理论下界；对部分弧退化平缓；
  代价仍是一次 3-4 阶特征分解，无迭代。若要再进一步，可用
  Taubin 结果做 2-3 步 Gauss-Newton 几何精化（残差 `r_i − R`）。
- ManuMesh 映射：solvePlaneCircle（PrimitiveFit.cpp:60-107）替换求解核，
  接口（center/radius）不变；数据已在 PCA 平面内且已中心化
  （frame.mean），条件数天然良好。

### 5.3 推荐替换：Halíř-Flusser 直接椭圆拟合
- 数学要点：conic `f = ax² + bxy + cy² + dx + ey + g`，Fitzgibbon 约束
  `4ac − b² = 1` 保证解必为椭圆，化为 6×6 广义特征值问题
  `Sᵃ = μ C a`。原版 C 奇异、S 近奇异导致数值不稳；Halíř-Flusser
  分块：`D₁=[x² xy y²]`、`D₂=[x y 1]`，
  `M = C₁⁻¹(S₁ − S₂ S₃⁻¹ S₂ᵀ)`（3×3，`C₁=[[0,0,2],[0,−1,0],[2,0,0]]`），
  取满足 `4a c − b² > 0` 的特征向量，`a₂ = −S₃⁻¹S₂ᵀ a₁`。
  conic → 几何参数：中心 `(2cd−be, 2ae−bd)/(b²−4ac)`；
  轴长 `= √(2(ae²+cd²+gb²−bde−4acg)/((b²−4ac)(±√((a−c)²+b²)−(a+c))))`；
  转角 `θ = ½·atan2(b, a−c)`。
- 为什么更好：保证输出椭圆（矩量法在噪声下可给出无意义轴比）；
  轴向来自拟合而非 PCA，近圆时轴比估计的方差显著低；
  数值条件靠**先中心化 + 除以 RMS 半径归一化**（设计矩阵含 4 次量，
  不归一化条件数 ~(R/σ)⁴）。已知偏差：短弧上偏向低离心率——
  ManuMesh 的 loop 是闭合的，此偏差不触发。
- ManuMesh 映射：替换 PrimitiveFit.cpp:94-99 的矩量轴长，并让
  `fit.majorAxis/minorAxis` 来自 conic 转角而非 PCA（PrimitiveFit.cpp:45-47
  的 PCA 只保留做平面法向估计）。measurePrimitiveFitErrors（:109-148）
  的椭圆残差公式随之用 conic 的代数距离归一化版本或保持现状均可。

### 5.4 决策表
| 场景 | 推荐 | 理由 |
| --- | --- | --- |
| 闭合 loop、判 Circle/NearCircle | Taubin（+可选 GN 精化） | 无偏、单次特征分解 |
| 闭合 loop、判 Ellipse | Halíř-Flusser | 保证椭圆、轴向可靠 |
| 未来部分弧（开曲线段拟圆） | Taubin 必须、禁 Kåsa | Kåsa 短弧半径坍缩 |
| 快速初值 / 完整均匀采样 | 现状 Kåsa 可留作 fallback | 成本最低 |

### 5 行动清单
1. solvePlaneCircle 换 Taubin 核（PrimitiveFit.cpp:60-107）。
2. 椭圆改 Halíř-Flusser 直接拟合，轴向弃 PCA（:94-99, :45-47）。
3. 拟合前统一中心化 + RMS 归一化，拟合后反变换（条件数保障）。
4. 加固定断言测试：解析圆/椭圆 + 非均匀采样 + 噪声下的半径/轴比误差上界。

---

## 6. 性能模式

### 6.1 一次遍历、多量并出
- 出处：M014 p.4（II 与 C 同一循环骨架）；M021 p.2-3（一次 cubic 拟合
  同时出曲率+extremality）；M007 p.15（5 个 pass 全部线性）。
- 决策：每个几何遍历应产出所有同源量。ManuMesh 反例：
  FeatureEvidence 构造时 NormalTensor 和 SmoothCurvature 各自调
  `buildVertexNeighbors` + `computeVertexAverageEdgeLength`
  （NormalTensor.cpp:64-65、SmoothCurvature.cpp:384-385），
  FeatureGraphCleanup 又算两次 `computeVertexAverageEdgeLength`
  （FeatureGraphCleanup.cpp:185, :239）。应建 `FeatureDetectionCache`
  （neighbors、avgEdgeLength、faceNormals、vertexNormals、edges）
  在 EdgeEvidenceContext（FeatureEvidence.cpp:14-49）一次构建、
  全管线传引用。

### 6.2 邻域收集：一次 BFS 服务所有尺度 + 提前截断
- 出处：M013 p.207（fast-marching 邻域 O(m log m)，遇 crease 停）；
  M021 p.3（k-ring + 钝角剔除）。
- 决策：SmoothCurvature.cpp:390-403 已经做对了"一次 BFS 存 depth、
  各尺度过滤"，但 fitScale 每尺度重建设计矩阵行（:170-196）。
  改为按 ring 分桶累加正规方程 `AᵀA/Aᵀb`（5×5 或 9×9），
  尺度 s 的解 = 前 s 桶之和的一次 LDLT——把每顶点成本从
  `O(scales × n_pts × QR)` 降到 `O(n_pts + scales × 常数)`。
  注意：robust 重加权路径不能增量化，保留为 opt-in 慢路径。

### 6.3 平滑序列的 ping-pong 缓冲与恒等对应
- 出处：M009 p.3（恒等顶点对应使跨尺度零成本）；M042 p.6
  （隐式积分允许大步长，≤5 步）。
- 决策：NormalTensor.cpp:72 每次 `next = current` 整体拷贝
  `vector<Matrix3d>`——改两个缓冲 swap。多尺度序列沿用恒等对应
  （已满足），不做任何跨尺度最近点搜索。

### 6.4 昂贵判据只花在灰区
- 出处：M016 p.5（θ̂ʳ 只在 SVM/能量框架需要处评估）；M007 p.14
  （quasi-strong 概率设计：便宜判据先滤，贵判据只看边界情形）。
- 决策：分层证据管线——单三角 dihedral（O(1)）→ 只对 `θ_f..θ_F`
  灰区边跑 θ̂ʳ 两侧平面生长（O(球内面数)）→ 只对弱证据组件跑
  单 quadric 残差（O(组件邻域)）。ManuMesh 当前所有边平等对待
  （FeatureEvidence.cpp:345-359 对每条边跑全部 5 个 strategy，
  好在 NormalTensor/SmoothCurvature strategy 有 :308/:321 的
  硬证据短路——保持并推广该模式）。

### 6.5 全局结构只建一次，图修补用局部操作
- 出处：M016 p.4（graph cuts 一次全局求解 vs 迭代后处理）；
  M007 p.18（曲线过滤用 ICH 列表增量删除，不重建）。
- 决策：FeatureGraphCleanup 的 removeWeakSpurs 每 pass 全图重扫
  （:108-144，最多 8 pass）+ `rebuildTraceGraphEdges`；改成
  端点队列驱动：删一条 spur 只把新暴露的端点入队。
  bridgeEndpointGaps 的 O(端点²) 配对（:194-209）在 512 上限内可接受，
  超限时应换网格哈希/KD 树而不是直接放弃（当前 :189 直接 return）。

### 6 行动清单
1. FeatureDetectionCache 消灭重复的邻接/边长/法向构建。
2. fitScale 按 ring 分桶增量正规方程，多尺度复用。
3. NormalTensor 平滑 ping-pong 缓冲。
4. 证据分层：便宜判据全量、昂贵判据只跑灰区。
5. spur 清理改端点队列驱动；gap 配对超限换空间索引。

---

## 行动清单汇总（按预期收益排序）

1. **[正确性] 绕向一致化 + 去掉二面角 abs**（§2.1）——修复 >90° 折边
   与薄片折边的漏检/误读，是唯一的"结果错误"级问题。
2. **[鲁棒] 边零交叉 extremality 判据 + cubic 拟合**（§1.5, §3.1)——
   弱特征检测从启发式极值对比升级为论文标准判据，直接决定
   ridge/valley 召回质量。
3. **[鲁棒] 组件级无量纲强度过滤 T = 长度 × ∫强度**（§3.3）——
   用曲线级信息替代边数剪枝，同时解决短毛刺和长弱线两类错误。
4. **[鲁棒] Jiao 双层 l-strong/u-strong 二面角**（§2.4）——近乎免调参地
   降低 tessellation 噪声误报，成本一次邻接扫描。
5. **[精度] Taubin 圆 + Halíř-Flusser 椭圆替换 Kåsa+矩量法**（§5）——
   半径/轴比偏差从 O(σ²) 降到 O(σ⁴)，近圆分类可靠。
6. **[鲁棒] 单 quadric 残差假阳性过滤**（§4.1）——唯一能区分
   "光滑高曲率面"与"两面交线"的判据。
7. **[性能] FeatureDetectionCache + fitScale 增量正规方程**（§6.1-6.2）——
   多尺度弱特征路径的主要成本项，预计数倍加速。
8. **[鲁棒] Page 投票旋转 + 三路相对分类**（§2.2）——normal tensor
   路径的无量纲化与 corner label，同时喂给 junction 判定（§4.4）。
9. **[鲁棒] persistence 改纯计票 + 各向异性平滑**（§3.4）——保住细小
   真实特征，平滑不跨 crease。
10. **[鲁棒] gap 桥接角度规则 + 成对相似度分数**（§3.5, §4.3）——
    图修补从纯距离贪心升级为方向感知。
11. **[鲁棒] 灰区边 θ̂ʳ 两侧平面复核**（§4.2）——噪声扫描件路线的
    硬证据升级，可延后到扫描 fixture 就绪。
