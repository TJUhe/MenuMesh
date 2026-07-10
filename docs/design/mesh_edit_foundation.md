# Mesh Edit 基础层

`mesh_edit` 是 ManuMesh 的内部可编辑网格基础层。它位于静态 `Mesh` 数据结构与
QEM、未来 remeshing/repair 等算法之间，负责表达“如何维护编辑状态并提交为新
Mesh”，但不决定“为什么执行这次编辑”。

这一边界参考了 OpenMesh 文档中 mesh kernel、status/property、garbage collection
与 Decimater/Remesher 算法分离的组织方式。ManuMesh 当前只吸收其中的职责划分，
没有复制 OpenMesh 的半边 API。参考入口：
[OpenMesh Documentation](https://www.graphics.rwth-aachen.de/media/openmesh_static/Documentations/OpenMesh-Doc-Latest/)。

## 当前能力

```text
src/mesh_edit/detail/MeshEditTypes.h
  EditableFace              保持稳定 face index 的活动三角面记录

src/mesh_edit/detail/DynamicTopology.h
src/mesh_edit/DynamicTopology.cpp
  DynamicTopology           vertex -> incident faces 增量缓存
  collectActiveEdges        活动面上的无向边集合
  areAdjacent               当前活动拓扑邻接判断
  activeIncidentFaceCountForEdge
  duplicate-face tracking

src/mesh_edit/detail/MeshCompaction.h
src/mesh_edit/MeshCompaction.cpp
  compactActiveMesh         活动编辑状态提交为稠密 Mesh
  oldToNewVertices          顶点旧索引到输出索引
  oldToNewFaces             面旧索引到输出索引
```

这些类型目前是库内部 API，不安装到 SDK。`Mesh` 仍是外部交换格式，`mesh_edit`
只服务算法实现。

## 与 simplification 的边界

`mesh_edit` 不包含以下 QEM 专属概念：

- quadric 和 line-quadric cost；
- feature loop/component ownership；
- collapse candidate queue 和 generation version；
- boundary/feature protection policy；
- normal、quality、error envelope 和 self-intersection 接受条件。

这些内容继续位于 `src/simplification/`。其中 `CollapseTopology.cpp` 把通用动态
邻接与 QEM 的 boundary policy、link condition 组合起来；`SimplificationRun.cpp`
在运行结束时把 position/activity 数组交给 `compactActiveMesh()`。

因此依赖方向保持为：

```text
core <- common <- mesh_edit <- simplification
                               future remeshing
                               future repair
```

`mesh_edit` 不得 include `simplification/detail/...`，未来 remeshing 也不得通过
QEM 私有类型复用拓扑编辑能力。

## remeshing 扩展顺序

后续加入 remeshing 时，优先按下面顺序扩展 `mesh_edit`：

1. 稳定 typed handle 与 generation-aware 生命周期。
2. `splitEdge`、`collapseEdge`、`flipEdge` 的纯拓扑提交操作。
3. 局部 patch/transaction，用于失败时不污染主状态。
4. property channel 和 compaction remap，传播 feature、source face、normal 等属性。
5. 必要时升级为半边 connectivity；在需求明确前不提前引入完整半边内核。

remeshing 模块负责目标边长、质量目标、操作调度、投影和 feature policy；
`mesh_edit` 只负责状态一致性和索引映射。

## 测试要求

`tests/unit/mesh_edit/` 应覆盖：

- inactive/invalid face 的压缩行为；
- deterministic vertex/face remap；
- add/remove face 后的 incidence 一致性；
- duplicate face 和 edge incidence；
- 后续 split/collapse/flip 的局部拓扑不变量。

算法测试仍需验证最终行为，例如简化目标、边界数量、feature retention、误差包络和
三角形质量。基础层测试不能替代算法回归。
