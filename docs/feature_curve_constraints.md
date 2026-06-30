# QEM, Line Quadrics, and Curve Feature Constraints

这份说明回答一个核心问题：

> 传统 QEM 是否是在法向上约束？line quadrics 是否是在切向上靠近原来点？对于圆孔、圆环、弧线这类曲线特征，为了保留特征还应该加什么约束？

## 1. 先校正直觉

传统 QEM 更准确地说不是“约束法向移动”，而是：

```text
惩罚新点到原始三角面平面的距离
```

一个三角面给出的误差近似为：

```text
E_plane(x) = (n dot (x - p))^2
```

其中 `n` 是面法向，`p` 是面上一点。这个误差主要惩罚点离开原来的切平面。于是，在一个大平面区域里，如果点沿着平面内滑动，普通 QEM 认为误差几乎为零。这就是普通 QEM 在平面区域容易出现病态求解、随机折叠和顶点分布不均的原因。

line quadrics 补的是另一半自由度。它使用：

```text
过原始顶点、方向为顶点法向的直线
```

并惩罚新点到这条直线的距离：

```text
E_line(x) = dist(x, normal_line)^2
```

因为这条线沿法向方向，点在切平面内漂移时会迅速远离这条法向线。因此，line quadrics 可以理解为：

```text
在切向方向上把新点拉回原始顶点附近
```

所以“传统 QEM 约束法向误差，line quadrics 补切向漂移”这个直觉基本是对的，但更严谨的说法是：

```text
QEM: 惩罚离开原始面平面的距离。
Line quadrics: 惩罚偏离原顶点法向线的距离，从而补充切向正则化。
```

## 2. 曲线特征需要什么约束

圆孔、圆环、弧线这类结构的关键不是“点一定不能动”，而是：

```text
点应该主要沿特征曲线方向移动，而不应该离开这条曲线。
```

可以把不同几何对象的约束理解为“允许点沿哪个子空间动”：

| 几何对象 | 合理自由度 | 应惩罚的偏移 |
| --- | --- | --- |
| 普通曲面 | 沿切平面移动 | 法向偏移 |
| 平面正则化 | 沿法向线移动 | 切向漂移 |
| 特征曲线/圆孔边界 | 沿曲线切线移动 | 径向偏移和平面外偏移 |
| 尖角/端点 | 尽量不移动 | 所有方向偏移 |

因此，对圆孔或弧线，最自然的扩展是：

```text
feature-curve line quadric
```

也就是对特征曲线上的顶点，不再只使用“法向线”作为 line quadric，而是使用“曲线切线”：

```text
E_curve(x) = dist(x, tangent_line)^2
```

对于圆孔边界，`tangent_line` 是过当前孔边界点、方向为圆周切线的直线。这个误差会惩罚：

1. 往圆心内外跑，导致半径变化。
2. 离开孔所在平面，导致孔边界翘曲。
3. 但允许沿圆周方向滑动，因为圆周方向是这个特征曲线的自然自由度。

## 3. 圆孔保护不能只靠一个误差项

只加 curve quadric 还不够。一个稳定的圆孔/弧线特征保护方案应该分成三层。

### 3.1 拓扑约束

先识别孔边界环或硬边环：

```text
edge adjacency -> boundary / dihedral feature edges -> loop tracing
```

然后在边折叠时加规则：

- 禁止跨越 feature loop 的 collapse。
- 禁止把孔环折没。
- 如果孔环剩余点数低于阈值，例如 16 或 24，直接锁住。
- 不允许 collapse 造成非流形、自交或法向翻转。

这类约束回答的是：

```text
这个孔还在不在？
```

### 3.2 几何约束

对闭合孔环做圆拟合，得到：

```text
center c
radius r
normal n
```

然后可以加入圆约束：

```text
E_circle = radial_error^2 + plane_error^2
```

其中：

```text
radial_error = ||project_to_plane(x - c)|| - r
plane_error = n dot (x - c)
```

这类约束回答的是：

```text
这个孔还是不是圆？
```

### 3.3 放置约束

普通 QEM 会先解一个最优点。对于圆孔边界，这个点不能完全自由使用，而应该投影回圆上：

```text
solve QEM position
if vertex belongs to circular feature loop:
    project position back to fitted circle
```

对于一般弧线，可以投影回拟合圆弧、折线或样条曲线。

这类约束回答的是：

```text
collapse 后的新点应该放在哪里？
```

## 4. 与当前工程最匹配的公式

当前工程可以保留原来的 QEM + line quadrics 框架，然后增加曲线特征项：

```text
Q_total =
    Q_plane
  + w_line * Q_normal_line
  + w_curve * Q_curve_tangent
  + w_circle * Q_circle_local
```

其中：

- `Q_plane` 是传统 QEM 的面平面 quadric。
- `Q_normal_line` 是当前论文复现里的 line quadric，方向为顶点法向。
- `Q_curve_tangent` 是针对孔环/弧线的切线 line quadric。
- `Q_circle_local` 可以用局部二次近似或投影规则实现，第一版不一定需要写成完整 4x4 quadric。

工程上更现实的第一版是：

```text
detect feature loops
fit circle for near-circular loops
reject collapses that cross loops
project circular-loop vertices back to the fitted circle
lock loops below a minimum vertex count
```

这比直接实现完整的高维特征敏感度量更简单，也更直接解决法兰圆孔变形的问题。

## 5. 论文和工程依据

### Garland-Heckbert QEM

原始 QEM 使用平面 quadric 累积误差，核心是点到平面的距离。因此它本身并不知道圆孔、硬边或 CAD 特征语义。

- Garland, M. and Heckbert, P. S. 1997. *Surface Simplification Using Quadric Error Metrics*.
- 链接：https://www.cs.cmu.edu/~garland/Papers/quadrics.pdf

### Line Quadrics

Line quadrics 使用过顶点、方向为法向的直线，给 QEM 增加软控制，主要解决平面区域中的切向漂移和奇异系统问题。它是正则化和软控制，不是圆孔识别或 CAD 特征保持算法。

- Liu, H.-T. D., Rahimzadeh, M., and Zordan, V. 2025. *Controlling Quadric Error Simplification with Line Quadrics*.
- 链接：https://www.dgp.toronto.edu/~hsuehtil/pdf/lineQuadric.pdf

### Feature Sensitive Metric

Feature Sensitive Metric 把普通 QEM 扩展到位置与法向相关的高维度量中，用 normal deviation 辅助保留特征。这支持一个重要观点：只看点到面平面的距离不够，特征保持还需要法向、方向或特征子空间信息。

- Wang et al. *Feature Preserving Mesh Simplification Using Feature Sensitive Metric*.
- 链接：https://cg.cs.tsinghua.edu.cn/papers/weijin.pdf

### CGAL constrained simplification

CGAL 的 Surface Mesh Simplification 支持 constrained edges 和 constrained placement。这对应本文中的“孔环拓扑约束”和“collapse 后位置投影/限制”思想。

- CGAL Surface Mesh Simplification
- 链接：https://doc.cgal.org/latest/Surface_mesh_simplification/index.html

### OpenMesh decimation framework

OpenMesh 的 decimation 框架把简化看成一个连续误差模块加多个合法性模块，例如锁定顶点、normal flipping、normal deviation、aspect ratio 等。这说明实际工程里的特征保持通常不是单一 QEM，而是：

```text
cost + legality constraints + placement rules
```

- OpenMesh Mesh Decimation Framework
- 链接：https://www.graphics.rwth-aachen.de/media/openmesh_static/Documentations/OpenMesh-6.2-Documentation/a00004.html

### CWF: Consolidating Weak Features

CWF 进一步把 accuracy、triangle quality 和 feature alignment 结合到一个目标中，适合弱特征、圆角、细节等更复杂情况。不过它比当前工程需要的圆孔保护第一版复杂得多。

- Xu et al. 2024. *CWF: Consolidating Weak Features in High-quality Mesh Simplification*.
- 链接：https://arxiv.org/abs/2404.15661

## 6. 我的理解总结

可以把当前问题统一成一个“自由度选择”问题：

```text
QEM:
  让点保持在原始切平面附近。

Line quadrics:
  让点不要在切平面内随意漂移。

Curve feature quadrics:
  让点沿曲线方向保留自由度，但惩罚离开曲线。

Circle / loop constraints:
  让孔的拓扑和圆形几何都不要被 collapse 破坏。
```

所以，对于法兰圆孔这类 CAD 特征，最自然的改进不是继续增大 `line_weight`，而是加入：

```text
feature loop detection
curve tangent line quadrics
circle fitting / projection
collapse legality constraints
```

它们和 line quadrics 的思想是一脉相承的：都是通过“允许方向”和“惩罚方向”控制边折叠。区别在于，line quadrics 的允许方向是表面法向，而圆孔/弧线特征的允许方向应该是曲线切线。
