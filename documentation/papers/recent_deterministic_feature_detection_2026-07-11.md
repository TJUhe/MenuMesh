# 近期确定性表面网格特征参考

快照日期：2026-07-11。

此列表仅限三角形/多边形表面网格和确定性几何处理。神经、学习式、仅点云、B-Rep、实体建模和 CAD 特征树方法
不纳入实现路线。

| ID | 年份 | 标题 | DOI / 来源 | 本地状态 | 对 ManuMesh 的作用 |
| --- | ---: | --- | --- | --- | --- |
| RFD001 | 2017/2018 | Feature Edge Extraction Via Angle-Based Edge Collapsing and Recovery | `10.1115/1.4037227` | 元数据和提取语料笔记；出版商 PDF 端点当前受交互式挑战保护 | 用于小型圆角中心边的尺度无关法向误差简化和恢复。 |
| M044 | 2019 | Feature Curve Network Extraction via Quadric Surface Fitting | `10.2312/pg.20191338` | 本地 PDF | 局部 quadric、连续性、junction 和曲线网络的主要锚点。 |
| RFD002 | 2020 | HT-Based Identification of 3D Feature Curves and Their Insertion into 3D Meshes | `10.1016/j.cag.2020.05.012` | 公共元数据；未记录公共 PDF | 全局曲线识别及显式插入网格。 |
| M026 | 2024 | CWF: Consolidating Weak Features in High-quality Mesh Simplification | `10.1145/3658159` | 本地 PDF | 弱特征整合和下游保护策略。 |
| RFD003 | 2025 | Feature Line Extraction Based on Winding Number | `10.1016/j.gmod.2025.101296` | 公共元数据；未记录公共 PDF | 碎片化局部特征线的未来全局证据。 |

## 实现选型

这些工作作为后续曲线识别研究参考；当前运行时仅保留 boundary/dihedral 与 Normal Tensor 证据，
不再实现本表中的曲面曲线通道。M044 的网络组织思想仍可作为未来 graph-level 研究参考，
使用 M026 制定弱证据策略。现在还包含可选的稳定参考尺度选择、共享来源/符号/方向兼容性、局部跨组件端点整合
和 junction 延续配对。这是文献原则的工程子集，不是 CWF 重定位或全局网络优化器的复现。RFD002 和 RFD003
仍是路线图参考，因为它们需要独立的全局曲线恢复阶段。

噪声法向预处理另见 `open_source_mesh_libraries.md`：当前面积加权法向松弛有意小于 L0/非局部/描述子驱动的去噪，
并且不会移动网格顶点。

书目信息在快照日期通过 Crossref 和 OpenAlex 核对。此处有意省略引用数量，因为它会随时间变化，不能决定实现质量。
