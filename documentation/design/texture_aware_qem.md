# 不扩展矩阵的纹理感知 QEM

## 决策

ManuMesh 将几何 quadric 保持为 `Mat4`，在齐次位置 `(x, y, z, 1)` 上计算。纹理坐标不会追加到该向量中，
也不会扩大 placement 求解。

对于边坍缩位置 `p`，队列使用

```text
E_total(p) = E_geometry_4x4(p) + textureWeight * E_uv_local(p)
```

其中 `E_uv_local` 是仅在坍缩已触及的面上计算的标量。placement 候选仍为现有的端点、中点和稳定 3D QEM 最优候选。

## UV 归属

UV 按面角存储在 `Mesh::faceTexCoords` 中。顶点拥有的 UV 无法表示纹理 seam，因为一个几何顶点可能属于多个
UV chart。因此 OBJ `vt` 索引会针对每个三角化面角独立保留。

面角数组要么为空，要么与 `Mesh::faces` 对齐。当 OBJ 包含无纹理面时，对齐项可能无效。纹理与无纹理入射面之间
的过渡会被视为受保护的 chart 边界。

## 局部 Chart 策略

对于每个坍缩端点，使用容差网格哈希对入射面角 UV 分组。坍缩边的入射面定义端点 chart 之间的一一配对。

出现以下情况时拒绝坍缩：

- 一个端点 chart 在另一个端点没有对应 chart；
- 配对存在歧义或合并了无关 chart；
- 保留下来的 UV 三角形改变了带符号方向；或
- 保留下来的 UV 三角形面积低于原带符号面积的 `minTextureAreaRatio`。

当 seam 两侧的 chart 都一致配对时，这允许沿双侧 seam 坍缩。当操作会合并 chart 归属时，它会阻止跨 seam 或
远离 seam 顶点的坍缩。

## 标量畸变代价

对于兼容的坍缩，每个配对 chart 在 `p` 的 3D 边参数处接收线性插值的 UV。局部代价为

```text
E_uv_local = edgeLength^2
             * sum(faceArea * cornerUvDisplacement^2)
             / meanLocalUvEdgeLength^2
```

面面积和边长因子使该项与面积加权几何 QEM 具有相同的长度幂次。除以局部 UV 边尺度使结果对 UV atlas 的
统一缩放保持不变。

接受坍缩后，仅更新受影响且仍存活的面角。不同的配对 chart 在同一几何顶点保留不同的合并 UV。

## Plan/apply 分离

纹理处理拆分为三个明确阶段（`src/simplification/detail/TextureProtection.h`）：

- `evaluate()` 对一次坍缩位置进行评分，用于排序和拒绝，但不具体生成任何 UV 重写；队列和候选过滤器只承担标量代价。
- `buildPlan()` 对接受的位置执行相同评估，并在允许时返回 `TextureUpdatePlan`，即应用坍缩所需的逐面角 UV 重写
  （`TextureFaceUpdate` 项）。
- `apply()` 将预先构建的计划写入 `Mesh::faceTexCoords`。

由于计划只为接受的位置构建一次并直接应用，`applyCollapse` 不再第二次重建相同的 chart 配对和插值。
报告计数器 `textureApplyFailures` 统计计划无法重新应用的已接受坍缩；这表示内部不一致，应该保持为零。

## 复杂度

几何求解仍是由 4x4 齐次 quadric 支持的固定 3D 求解。纹理处理针对局部一环大小 `k` 使用期望 O(k) 的 chart
哈希和三角形检查，然后继续使用现有优先队列。因此没有全局参数化、atlas 遍历或属性空间矩阵分解，边坍缩的
渐近复杂度不变。

## 控制项和诊断

- `preserveTexture`：存在 UV 数据时启用 chart 和带符号面积保护；默认 `false`，调用方必须显式选择启用。
- `textureWeight`：仅缩放局部标量排序代价。
- `textureSeamTolerance`：局部 chart 分组的相对容差。
- `minTextureAreaRatio`：保留带符号 UV 面积的硬下限。
- `textureProtectedEdges`：初始时没有有效中点纹理的坍缩边。
- `textureRejectedCollapses`：placement 评估后因纹理检查而被拒绝的当前队列候选。
- `textureApplyFailures`：预构建 UV 更新计划无法重新应用的已接受坍缩（内部一致性检查，预期为零）。

默认 `preserveTexture = false` 会使候选排序和几何输出与旧的无纹理路径完全一致。面角 UV 仍会传播，但不会提供
畸变或 seam 保证。

保护启用时，可选的固定拓扑质量精修轮目前会跳过带纹理输入，因为该重定位阶段尚未优化或约束 UV 畸变。

## 与文献的关系

Garland 和 Heckbert 的属性感知工作是将颜色和纹理纳入简化目标的历史参考（M003）。ManuMesh 采用当前边坍缩
管线文献（M033）提出的另一种工程分离：几何 QEM 对固定 3D placement 排序，而拓扑、特征和属性有效性仍是
显式的局部策略。这也与 4x4 line-quadric 骨干（M004/085）保持兼容。
