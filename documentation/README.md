# ManuMesh 文档

本目录只记录当前仓库可以从源码、构建配置或测试复现的事实。程序是唯一的行为依据：

- 公共 API 以 `include/` 头文件和 Doxygen API 站点为准。
- CLI 命令和选项以 `apps/ManuMeshCli.cpp`、`apps/CliArguments.cpp` 和 `manumesh --help` 为准。
- 构建、安装和测试以 `CMakeLists.txt`、`CMakePresets.json` 和 `tests/` 为准。
- 文档与代码冲突时，先修正文档；不能确认的内容不写成能力承诺。

## 项目导览

需要先建立全局脉络，或重点了解 MMPD 大网格分块和 oneTBB 并行边界时，阅读
[项目导览](guide/project-overview.md)。

## 文档层次

| 层次 | 回答的问题 | 维护位置 |
| --- | --- | --- |
| 项目导览 | 程序由哪些数据流、模块与边界组成 | `guide/project-overview.md` |
| 操作指南 | 怎样构建、调用、调试或使用 CLI | `guide/` |
| 设计契约 | 为什么某个边界、所有权、线程或验收规则必须成立 | `design/` |
| API 参考 | 某个公共符号的精确参数和返回语义 | `include/` 注释和 `docs-api` 输出 |
| 内部源码参考 | 私有类型、调用关系和实现细节 | `src/` 注释和 `docs-internal` 输出 |

同一事实只应有一个规范来源：宏观解释放项目导览，稳定行为写公共头和设计契约，命令的实际
选项以 CLI 自检为准。指南只链接这些来源，不复制会随实现漂移的长清单。

## 从哪里开始

| 目的 | 文档 |
| --- | --- |
| 编译、测试、生成 Doxygen | [`guide/build.md`](guide/build.md) |
| 使用 MeshCore 核心工作区 | [`guide/meshcore-workspace.md`](guide/meshcore-workspace.md) |
| 使用命令行工具 | [`guide/cli.md`](guide/cli.md) |
| 集成 C++、PlainMesh 或 C ABI SDK | [`guide/sdk.md`](guide/sdk.md) |
| 在 VS Code/Debug 中定位算法 | [`guide/debugging.md`](guide/debugging.md) |
| 了解模块和依赖方向 | [`design/architecture.md`](design/architecture.md) |
| 了解特征检测和简化管线 | [`design/algorithms.md`](design/algorithms.md) |
| 了解校验、错误、ABI 和并发契约 | [`design/contracts.md`](design/contracts.md) |
| 了解测试分层和验收方式 | [`design/testing.md`](design/testing.md) |
| 新增模块或公共入口 | [`design/extension.md`](design/extension.md) |
| 公共符号、参数和调用图 | `docs/doxygen/html/index.html`（运行 `docs-api` 后生成） |
| 私有实现和源码调用关系 | `docs/internal/html/index.html`（运行 `docs-internal` 后生成） |

最后两项是源码工作树中的生成站点；普通 SDK 安装只携带本目录的文档源文件，不会预生成
Doxygen HTML。

## 当前范围

ManuMesh 是面向三角表面网格的 C++14 几何内核；运行 `manumesh --version` 查看当前版本。它提供：

- `Mesh`、`PlainMesh`、不可变 `MeshTopology`、几何统计和确定性测试网格生成器；
- STL/OBJ 读写（OBJ 多边形确定性三角化并保留逐角 UV）；
- `manumesh::feature` 特征证据、图、曲线/环、primitive、component 和 surface patch；
- `manumesh::simplification` 的 QEM/line-quadrics 受约束边坍缩、可选纹理保护和质量精修；
- `manumesh::analysis` 统计与确定性采样距离比较；
- C++ SDK、Eigen-free `PlainMesh`、size-aware C ABI、CLI、示例和 CTest/GoogleTest 验证；
- `PartitionedMeshDataset` 的有界内存二进制 STL 三角记录导入/校验。

以下内容不是当前产品能力：完整 B-Rep/CAD feature tree、Boolean/offset/thickening、补洞和
shape healing、点云重建、全局 Hausdorff/envelope 认证、全局分区拓扑/FeatureGraph/QEM，
以及制造公差合规保证。

## 文档维护规则

1. 新增或修改公共能力时，先更新对应头文件注释和本目录中的契约文档，再补充示例。
2. 不在这里复制构建输出、实验快照、PDF 导出或整段源码；需要细节时链接源码和测试。
3. 版本号、测试数量、性能数字和默认参数不要手写成永久事实；命令可查询的内容应引用命令。
4. 设计文档必须区分“当前实现”“明确限制”和“未来候选”，未来候选不得写进能力列表。
5. Doxygen 输出在 `docs/`，该目录由 `.gitignore` 排除，不应手工提交生成文件。

历史设计、导出 HTML/PDF 和本地论文副本已从工作树移除；需要追溯时使用 Git 历史。论文不是
运行时依赖，也不是 API 契约。
