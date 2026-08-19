# 更新日志

> 本文件按发布日期保留历史实现和验证记录；其中命令、路径和工具链可能已过时，不是当前使用说明。
> 当前构建、测试和 SDK 流程请以 [`documentation/guide/build.md`](documentation/guide/build.md) 及
> `CMakePresets.json` 为准。

## 2026-08-15

### API、生命周期与诊断收口

- C API v2 的特征边结果现在同时返回 `feature_edge_index`、`input_edge_index`、`synthetic` 和 `geometric_constraint`；容量查询保持原子写入语义，并由纯 C/C++14 consumer 覆盖端点序号范围和重试路径。
- 新增 `FeatureAnalysisViews` 的证据、曲线、分区和诊断视图；视图禁止绑定临时 `FeatureAnalysis`，避免悬空引用，同时不改变分析结果的所有权和布局。
- C ABI 异常边界改为 `noexcept` 错误翻译，OOM 发生在参数校验和 catch 路径时均映射为 `MANUMESH_STATUS_OUT_OF_MEMORY`，不允许 C++ 异常穿过 ABI。
- Windows CLI 参数、DebugUtil 模块句柄和其他局部资源改用 RAII；64 轮生命周期压力测试累计 2,512,640 次分配，净未释放分配为 0，VS2019 ASan 316/316 通过。
- NormalTensor 每个尺度只执行一次证据传播和分解，简化侧复用规范缓存；Release 中位耗时约下降 26.8%，相关尺度、旋转协变、采样稳定性和权重复用测试通过。
- DebugUtil 当前暂不参与产品流程：特征快照调用已保留为注释，VS2019/Ninja DebugUtil preset、VS Code task/launch 和自动化测试入口已移除；实现和文档保留供后续诊断恢复。
- 普通 VS2019 Debug 回归执行 333 项全部通过，Debug/Release/ASan/SDK 的目标继续统一使用 C++14、MSVC v142 x64 和 `/MD`。

## 2026-08-14

### 特征识别与简化解耦

- 简化侧开始直接消费规范特征约束图，区分输入网格证据边、恢复连续边和可投影几何段；同时移除 `manumesh::detail` 到 `manumesh::common` 的内部过渡别名。
- 新增轻量公共头 `FeatureOptions.h`，将检测策略与包含完整分析结果的 `FeatureTypes.h` 分离；简化公共头只依赖配置，不再被迫包含特征图、环、primitive 和 patch 数据结构。
- `SimplifyOptions::featureOptionsOverride` 成为简化侧规范特征配置；旧扁平字段保留为源码兼容适配器，override 存在时不再参与解析。CLI 复用统一的 `parseFeatureOptions()`，同时保留简化命令历史上的最小环顶点数默认值。
- `FeatureAnalysis` 新增确定性的来源身份，绑定精确 indexed geometry 并明确排除 UV；预计算简化、特征比较、benchmark 和分区入口会校验来源、公开索引及图连接关系。
- 预计算分析新增紧凑的 `normalTensorVertexWeights`。Normal Tensor 权重模式直接复用检测时已解析的规范证据；显式传入分析但缺少逐顶点权重时拒绝调用，不在简化内部换一套参数重算。
- 特征检测和简化的 `minFeatureLoopVertices` 统一使用同一个归一化值，避免检测出的环与硬坍缩预算采用不同门限。

### Normal Tensor

- 法向张量边候选要求两个端点都满足折痕切向对齐；修正所选名义尺度报告，并让权重计算统一使用解析后的 `FeatureOptions`。独立 `computeNormalTensorFeatures()` 也支持相同的 normal filter，并与完整检测管线共享参数校验规则。
- 多尺度 persistence 改为先选择参考尺度，再只统计与参考类型一致的支持尺度；折痕主导候选还必须保持切向一致，稳定角点只贡献顶点权重而不会直接生成折痕边。
- 增加统一缩放、刚体旋转协变、非均匀采样、多尺度持久性、双端点对齐和预计算权重复用回归；`localScale` 明确表示所选尺度的名义核半径，而不是多轮扩散后的精确支持域。

### C ABI、构建与诊断

- `ManuMeshSimplifyOptions` 尾部新增调用期借用的 `feature_options` 指针；外层和嵌套结构分别按 `struct_size` 读取，非空时覆盖全部旧扁平特征字段。初始化器、C 示例、旧布局和安装后 consumer 同步更新。
- 移除内部全局 force-include；`debugUtil` 只保留通用 overlay 接口，特征快照转换由 feature-owned 适配器承担。新增特征检测和简化的独立链接边界测试，防止隐藏依赖回流。
- 纹理保护导致固定拓扑质量精修跳过时，C++ report、C ABI、CLI 文本和 CSV 均返回显式诊断。
- 安装包拆分为 `ManuMesh::manumesh_binary`、`ManuMesh::manumesh` 和 `ManuMesh::c_api`，并提供 `COMPONENTS CXX CApi`；纯 C ABI consumer 不再继承 Eigen 或 C++17 头文件要求。

### Visual Studio 2019 构建基线

- 新增 `vs2019-asan` 配置/构建/测试 preset，并增加跨 C API、Pimpl copy/move、容量重试和错误返回的所有权压力测试；Debug 仍固定使用 `/MD`。
- 仓库唯一受支持工具链收敛为 Visual Studio 16 2019 / MSVC v142（`_MSC_VER 1920-1929`）；顶层工程、安装后 package 和 SDK consumer 会在配置阶段拒绝其他编译器。
- 删除 MinGW 运行时复制、MinGW GoogleTest 选择、GNU big-object 分支、VS2022/v143 与 GDB 工作流，以及仓库内 `mingw-x64-shared` GoogleTest 二进制包。
- GoogleTest provider 收敛为 `auto`、`source`、`system` 和 `fetch`；删除包含 `/MDd` Debug 归档的 MSVC 预编译包及导入逻辑，所有测试依赖改由当前 v142、C++14、`/MD` 配置构建。
- `CMakePresets.json`、VS Code、README、SDK/调试/测试指南统一使用 VS2019 x64 preset；仍可在 VS2019 v142 Developer Command Prompt 中使用 Ninja Multi-Config。
- 删除依赖 MinGW 运行库且无法启动的仓库内 Doxygen 二进制；文档配置改为从开发环境发现 Doxygen，`MANUMESH_BUILD_DOCS=ON` 时缺失工具会在配置阶段明确失败，不再生成伪成功目标。
- 格式规则明确保留中文命名空间注释，并重新格式化全仓 C/C++ 文件；`format` / `check-format` 固定要求 clang-format 22.x，缺失或版本不匹配时明确失败，不再伪成功。

## 0.2.0 - 2026-07-17

### 离线发布加固

- 安装型 MinGW SDK 现在把 GCC C++ 运行时 DLL 一并安装到 `bin/`；MSVC 安装型 SDK 通过 CMake 的系统运行时模块携带对应 VC/UCRT 文件。
- 新增安装后隔离 `PATH` 启动检查，`sdk-consumer-test` 不再被开发机编译器目录中的 DLL 隐式兜底。
- Windows CLI 从宽字符命令行生成 UTF-8 参数，STL/OBJ 和 CLI CSV/目录路径统一通过 UTF-8 与原生 `std::filesystem::path` 转换，支持完整 Unicode 路径。
- 新增 `manumesh --version`，版本升级为 `0.2.0`；增加 Unicode CLI 输出和 UTF-8 STL round-trip 回归测试。
- `ManuMesh.props` 只随 MSVC SDK 安装，并复制 SDK `bin/` 内完整运行时 DLL 集合。

### 二进制 STL 导出

- 新增 C++ `saveBinaryStl()` 与 C ABI `manumesh_save_binary_stl()`，按标准 little-endian `84 + 50 * triangleCount` 布局写出 float32 坐标，并在落盘前拒绝超出格式范围的面数和坐标。
- CLI 的 generate、simplify、sweep、ratio-sweep 与 face-sweep STL 输出改为二进制；原 ASCII 写出接口继续保留兼容。
- 相关 GoogleTest 改为二进制 round-trip，并覆盖文件布局、C ABI 回读、float32 越界与既有网格错误路径。

## 2026-07-16

### VS Code 工作流

- `.vscode/tasks.json` 从 70 个任务收敛到 38 个：隐藏 configure/prepare 依赖，合并重复构建入口，保留 MinGW + Ninja、VS2022、性能、SDK、演示、验证、格式和文档工作流。
- 移除依赖完整 Visual Studio 2019 安装的固定任务和重复的自定义 MSVC 任务；统一的 `msvc selected` 任务可在执行时选择 v143（MSVC 2022）或 v142（MSVC 2019），并使用隔离构建目录。
- MinGW configure 任务显式设置 `CMAKE_MAKE_PROGRAM=ninja`，可覆盖旧构建缓存中指向已删除 `thirdParty/packages` 工具目录的 Ninja 路径，不新增第三方文件。
- `.vscode/launch.json` 收敛为 6 个 MinGW/MSVC 调试入口；MSVC 程序路径随 v143/v142 选择切换。
- MSVC Debug 为 `PrimitiveFit.cpp` 启用 `/bigobj`，修复特征检测对象库超过 COFF 节数限制的 `C1128`。
- MSVC 任务使用仓库已有的 GoogleTest 源码在构建目录中生成测试库，避免预编译 Debug 库缺失 PDB 导致的 `LNK4099`，不向 `thirdParty` 新增文件。
- 本机使用 VS2022 generator + v142 实际通过普通/性能 configure、Debug/Release build、Debug/Release CTest 与两个 MSVC launch 默认入口；非性能测试均为 267/267，性能测试均为 4/4。
- `feature-benchmark` 识别现有标签 fixture 的 `a,b` 表头，不再把合法表头报告为无法解析的标签行；CLI CTest 增加对应防回归检查。
- 新增独立 `docs-internal` Doxygen 目标和 VS Code 任务，输出 `docs/internal/html/index.html`；覆盖 `include/` 与 `src/` 的 private/static/local 符号、内联源码、调用关系和 `@internal` 内容，未注释实现仍可浏览。
- `src/` 的文档注释统一为显式 `/** ... */` 块：93/93 文件保留 `@file`、`@brief`、`@ingroup` 元数据，内部类型与头文件接口补齐说明；新增 `check-src-doxygen` 目标并作为两个 Doxygen 目标的前置检查，禁止重新混入 `///`、`//!`、`///<`。
- 新增 vendored `thirdParty/doxygen` 与 `thirdParty/graphviz` 工具包；`docs-api` / `docs-internal` 现在优先使用仓库内 Doxygen 1.17.0 与 Graphviz 15.0.0，离线也可生成 UML、include、caller/callee 等关系图。

## 2026-07-15

### 特征识别系统增强

- 新增 opt-in 法线域特征保持过滤：只稳定检测缓存中的面法向，不改输入顶点或拓扑；输出迭代数、变化面、保留边、角度变化和 edge-indicator 诊断。
- graph cleanup 的 endpoint/close-junction bridge 统一使用方向、evidence source 和 signed-kind 兼容规则；新增 opt-in component consolidation，恢复不同弱 component 之间的兼容短缺口。
- junction 现在保存逐分支切向和 continuation pair，并报告 ambiguous junction；continuation 只接受从 junction 向相反方向延续的分支，不把同侧近平行分支误配。
- 新增 feature-induced surface patch segmentation，输出 `facePatchIds`、patch 和 patch adjacency；非 mesh-edge recovery bridge 不作为分区 barrier。
- benchmark 从 edge/junction 扩展到 branch pair 和 face-patch adjacency；标签支持 `edge`、`junction`、`branch`、`face_patch` 四类记录，缺失 patch prediction 计为错误。
- 将 normal filter、graph compatibility/consolidation、segmentation 和 benchmark 拆成独立 translation units；新增系统升级说明并同步当前维护文档、CLI、C++/C ABI 与测试契约。

### 特征识别文档源码校准

- 扩写 `manumesh-feature-recognition-pipeline.html`、`manumesh-loop-construction.html` 和交付开发者指南；本轮进一步校准为 9 阶段 pipeline，并补入法线预处理、component consolidation、junction branch pairing、patch segmentation 与扩展 benchmark。
- 修正公共 `FeatureOptions` 与 CLI 帮助中的文档语义：`minFeatureLoopVertices` 是 recovered-cycle/primitive-fit 门槛，普通 traced chain 仍会报告；weak spur cleanup 覆盖 normal-tensor 证据。

### 拓扑与算法

- edge collapse 改为比较完整单纯复形 link：除共同邻点外同时检查 endpoint link 的共同对边，拒绝四面体边坍缩后生成重复面的情况；边界虚拟封口规则补充 isolated open triangle 拒绝，避免二维分量降维消失。
- 新增四面体拒绝、八面体合法边与简化到四面体后停止、孤立三角形分量保持、三面共边拒绝的 direct/端到端回归；coplanar fallback fixture 改为合法三角形 fan，不再依赖删除孤立分量达到目标。

### C ABI

- 新增返回 `ManuMeshStatus` 的 `manumesh_*_init_with_size` 初始化入口，按调用方显式容量有界清零和写入 ABI 头；过小容量或空指针返回 `MANUMESH_STATUS_INVALID_ARGUMENT`，超大容量只写库当前已知尺寸。
- 保留三个旧的无容量 ABI v1 初始化符号，并将其写入范围冻结到首次发布布局：options 截止 `feature_protection_mode`，report 截止 `max_applied_line_weight`，mesh stats 保持首发完整尺寸。新头文件用兼容宏把普通源码调用透明转发到 size-aware 入口，旧二进制继续解析旧符号且不会被当前扩展结构体尺寸越界覆盖。
- 新增 `manumesh_simplify_mesh_with_report_size` 和 `manumesh_compute_mesh_stats_with_size`：显式 output capacity 是唯一写入边界，不读取未初始化 report/stats 的 ABI 头；普通源码调用通过对象式 alias 传入当前 `sizeof`。旧输出符号始终按首发冻结容量写，因此恢复首发 v1 允许未初始化 output 的契约且不越界。
- 旧无容量符号以首发 v1 前缀的内存安全为优先；已经编译且依赖后加 v1 尾字段的中间版本客户端需要重新编译或显式迁移到 size-aware 入口，否则尾部 options/report 字段会按首发容量被忽略。
- C ABI 回归增加 options/report/stats 的 legacy、最小头、历史尺寸、当前尺寸、超大容量、过小容量、空指针和未初始化 output 哨兵测试；安装后 C consumer 同时覆盖 current alias 与 legacy DLL 符号。

### MSVC 2019 / v142

- 新增 `windows-2022` 托管工作流，使用 Visual Studio 2022 generator 配合 `-T v142` 在 Debug/Release 下编译并运行 fast、external 和安装后 SDK consumer，持续验证 VS2019 16.11 对应的编译器、STL 与 ABI。VS16 generator/MSBuild 16 的精确兼容仍需自托管 VS2019 runner。

## 2026-07-13

### 合并前收口

- `MeshAnalysis` 对损坏输入改为逐面筛选：非法索引、非有限坐标、重复索引、
  零面积及数值不安全面不进入统计或 BVH；无可用曲面时返回零测量值，原始
  container 数量仍保留。新增统计与双向距离的 4 个损坏输入回归。
- STL 重合顶点合并改用相对 `bboxMin` 的局部量化，并在量化桶内复核真实欧氏距离；
  避免大平移或极端坐标饱和后把不同顶点静默合并。新增 `1e12` 平移不变性与
  `1e30` 饱和碰撞测试。
- 圆环比较先应用 center/radius/normal 三项 plausible 硬门控，再在合法候选中按
  综合分排序，避免分数更低但越界的候选遮住真实匹配；新增对应竞争候选回归。
- `algorithms/simplification/Metrics.h` 恢复旧统计与 CSV 函数符号作为一个迁移
  周期的薄包装，内部统一转调 `manumesh::analysis`；新代码仍使用
  `MeshAnalysis.h`。文档明确 pre-1.0 C++ SDK 升级后必须重新编译，跨版本稳定
  二进制边界由 C ABI v1 承担。
- 安装型构建默认启用 `MANUMESH_INSTALL_CMAKE_CONFIG`，使已注册的
  `sdk_consumer_examples` 能从全新 SDK 目录通过 `find_package(ManuMesh)` 构建；
  显式关闭 package config 时不再注册依赖 `find_package` 的 consumer target/test，
  兼容旧 CMake cache。安装消费者同时覆盖旧 `Metrics.h` 的四个兼容导出符号。
- STL 顶点合并对溢出的 bbox 范数保持有限容差，并搜索相邻量化桶；圆环匹配 options
  增加 finite、范围及 matched 不宽于 plausible 的公共校验，非法阈值不能绕过 hard gate。
- 测试策略中的套件规模同步为当前实际发现结果：快速套件 225 个启用用例、
  external 11 个、全量非性能套件 236 个；墙钟 perf-guard 按设计继续属于快速
  `unit` 套件。

### 算法修复：签名二面角凸凹判决 + 绕向一致化（P0-1 / P1-4）

- 凸凹分类谓词修复（P0-1）：`FeatureEvidence.cpp` 原 `signedDihedralKind`
  用规范化端点序（a<b）的边方向配质心叉积判凸凹——绕向一致网格上两侧
  side 项恒一正一负，判决与几何完全无关（90° 凹谷判凸、L 形棱柱与楼梯
  真值凹边全部误判凸）。改为 Jiao 2008 配方：取 face 0 自身遍历共享边的
  方向 d，`sign((n0×n1)·d)` 正=凸、负=凹（数学推导与 90° 凸脊/凹谷手算例
  见 `orientedDihedralAngle` 注释），去掉质心法与其 epsilon 死区，凸凹与
  角度在同一次边遍历中一并算出。boss_pocket 夹具凸/凹从 53/7（凹边任意
  误标）修正为 48/12（凹边恰为 boss 底座圈 4 + pocket 底圈 4 + pocket
  竖直墙角 4，逐边真值断言）。
- 绕向一致化（P1-4）：检测入口新增一次 O(F) 确定性 BFS 面绕向一致化
  （`harmonizeFaceWindings`，按面索引播种、边按顶点序展开、多数派归一化，
  只建内部翻转标记视图、不改输入网格）。此前绕向不一致的边直接降级为
  `|dot|`，翻转 patch 边界上 150°-175° 刀口边被读成 5°-30° 而漏检；现在
  可定向网格上翻转任意连通 patch 得到与原网格完全相同的特征边集合
  （含凸凹符号），`inconsistentWindingEdges` 归零。不可定向局部
  （Möbius 类）仍回退无符号角并保留计数（恰好闭合缝一条边）。
- 新增测试：90° 凸脊/凹谷最小折边逐边真值 ×2、L 形棱柱 18 边逐边真值、
  楼梯 4 折边逐边真值（`feature_detection_discrete_tests.cpp`）；
  boss_pocket 凸凹精确计数 + 逐边几何真值断言（弱断言
  `EXPECT_GT(concave, 0)` 升级，`feature_detection_fixture_tests.cpp`）；
  翻转 patch 集合相等性、翻转绕向刀口边检出、Möbius 诊断保留
  （`feature_detection_robustness_tests.cpp`，原
  `InconsistentWindingFallsBackToUnsignedDihedral` 按新语义改为
  `HarmonizesInconsistentWindingOnFlatSurface`：平面翻转对现在被一致化
  修复而非仅诊断）。

### 测试体系重构

- 新增解析真值 fixture 库 `tests/support/AnalyticFixtures.{h,cpp}`：
  `makeUvSphere` / `makeCylinder`（可带盖）/ `makeTorus` / `makeChamferBox`，
  携带解析几何、真值特征边和真值圆访问器；
  `withDeterministicNoise` 用 Knuth MMIX 线性同余发生器加有界扰动，同一 seed
  跨平台可复现。设计理念是"断言界由被测几何的闭式解推导，而不是抄录历史输出"。
- 新增测试文件：`feature_detection_analytic_tests.cpp`（含圆柱 rim 圆恢复、倒角盒硬边
  precision/recall 与 junction precision）、`simplification_analytic_tests.cpp`（5 个：球弦高误差界、柱面 rim 圆
  1e-6 保真、带 UV 圆柱解析参数化偏差界、环面双向 Hausdorff、detect+simplify
  字节级确定性）、`tests/unit/perf/pipeline_perf_guard_tests.cpp`（2 个墙钟性能
  护栏，注释含机器基准与 3×/10× 上限设计依据）。
- 测试组织：通用 core/io 用例从 `simplification_core_tests.cpp` 迁出到
  `tests/unit/core/core_tests.cpp` 与 `tests/unit/io/mesh_io_tests.cpp`；快速套件
  `ctest -LE "performance|external"` 共 225 个启用用例，全量非性能套件
  `-LE performance` 共 236 个启用用例。新增测试策略文档
  `docs/design/testing_strategy.md`（五层划分、fixture 设计、确定性测试、套件命令
  与规模、新增测试的注册方式）。

### 性能修复

- `simplify` 主循环原先每次 collapse 尝试都重算输入包围盒对角线
  （`bboxDiag`，O(V) 扫描），使整体劣化到近似 O(n²)；现在对角线在
  `initializeBudget` 缓存为 `meshDiagonal_` 一次性计算并向下传递
  （`src/simplification/SimplificationRun.cpp`）。16k 面解析球 `simplify`
  从约 15 秒降到约 2.0 秒。

### 算法修复：解析测试暴露的两个缺陷

- 圆恢复编造圆 + junction 洪泛：`FeatureCircularRecovery.cpp` 三点圆种子从
  O(n³) 顶点三元组盲扫改为 trace 图长度-2 路径（O(Σdeg²)），并增加证据连通性
  门控（相邻候选对无特征边支撑的角跨度累计 ≤ 整圈 25%，纯几何共圆不算证据）；
  `FeatureGraph.cpp` 图级 junction 报告改为纯 valence>2 判据（M007），
  `FeatureLoopBuilder` 的逐顶点保护 flag 语义不变（真实分支点与被多 loop 共享的
  顶点仍钉住）。倒角盒 detect 从 1735 ms 降到约 7 ms，恢复 loop 从 116 降到 31，
  junction precision 从 0.29 恢复到 1.0。

### CLI

- `apps/CliArguments.cpp` 的 `--adaptive-scale` 帮助文本更正为
  "Scale queue priority by local curvature (placement unchanged)"，与其当前
  优先级/placement 解耦语义一致（不再描述为面积自适应权重）。

## 2026-07-12

### 算法强化：简化（QEM / edge collapse）

- 扩展 link condition：在既有顶点 link 交集判定之上加入"虚拟顶点"边界扩展判据
  ——当一条内部边（两关联面）的两个端点都是边界顶点时（边界弦 boundary chord），
  折叠会把开边界捏合成非流形 pinch 点，现在**无论 `preserveBoundary` 是否开启**
  一律拒绝（`src/simplification/CollapseTopology.cpp`）。`preserveBoundary`
  继续只控制"边界结构不收缩"的更严格策略。
- 边界边折叠 placement 升级为 Lindstrom-Turk 边界守恒约束（M032 §4.2.2）：
  新顶点投影到"最小化关联边界链有向面积变化"的直线上并夹取到折叠边投影区间，
  局部边界链退化时回退为线段夹取；实现从 `FeatureConstraints.*` 迁至新的
  placement 策略单元 `src/simplification/detail/Placement.{h,cpp}`。
- 补齐 GH97 三级 placement 回退链的第 2 级：全空间最优解被谱条件检查拒绝后，
  先尝试**沿折叠边的一维最优**（rank-2 quadric——直棱、边界折痕——的良定情形），
  仍失败才落回端点/中点；一维分母判据使用尺度不变的相对阈值
  （`src/simplification/Quadrics.cpp`）。
- Wang 2008 优先级解耦（`adaptiveScale` 模式）：`featureBoost` 不再放大 quadric
  本身（旧行为会扭曲 placement 并抬高边界项），改为逐顶点队列优先级因子
  `priorityScale = 1 + featureBoost * score`，只乘候选排序代价（取两端点最大值），
  placement 用干净的 `adaptiveBaseLineWeight` 基础 line quadric 求解；折叠时
  keep 端按 max 传播该因子。
- 几何相交谓词尺度不变化：三角形相交测试统一使用无量纲相对容差
  `kRelativeIntersectionEps = 1e-9`（Möller-Trumbore 行列式与其尺度上界比较；
  2D 谓词区分 `epsLen = eps*scale` 与 `epsArea = eps*scale^2`），网格均匀缩放
  不再改变自交拒绝决策（`src/common/GeometryPredicates.cpp`、
  `CollapseLegality.cpp`、`QualityRefinement.cpp`）。

### 算法强化：特征检测

- 有向二面角替代 `|dot|`：按共享边在两面中的遍历方向做绕向一致性判断，一致时用
  带符号法向点积区分浅折痕与 >90° 的反折刀边（旧的绝对值会把 120° 法向夹角读成
  60°、把薄片折边读成平面而漏检）；绕向不一致的边回退无符号角并计入新诊断
  `FeatureAnalysis::inconsistentWindingEdges`（`src/feature_detection/FeatureEvidence.cpp`）。
- 圆拟合从 Kåsa 正规方程升级为 Taubin 代数拟合（一阶无偏，短弧/噪声下不再系统性
  低估半径），Taubin 特征分解失败时保留 Kåsa 作确定性回退；椭圆估计从 PCA 轴向 +
  二阶矩轴长升级为 Halíř-Flusser 数值稳定的直接最小二乘拟合（Fitzgibbon 约束
  `4ac - b^2 = 1`，3x3 缩减系统，保证输出为椭圆，轴向来自 conic 而非 PCA）
  （`src/feature_detection/PrimitiveFit.cpp`）。
- 弱毛刺清理增加 Yoshizawa 组件级无量纲强度过滤：新增
  `FeatureOptions::featureGraphMinWeakSpurStrength`（默认 0.0 = 完全保留旧的
  按边数剪枝行为）；为正时按曲线强度 `T = (∫ds) * (∫strength ds)`（ds 以局部
  平均边长为单位，strength 为 persistence 分数除以对应通道阈值）裁决——长而弱的
  真实圆角线存活、短而强的噪声刺被剪除，追踪上限同时扩展到 64 条边。该选项目前
  仅在 C++ `FeatureOptions` 层暴露，未加入 CLI/C ABI。端点 gap 桥接同步引入
  Yoshizawa 角度三条件（连接段须近似延续两条线的切向）。
- 新增诊断计数：`inconsistentWindingEdges`、`graphCleanupSkippedByCap`
  （端点/junction 硬上限触发时跳过的清理 pass）、`circularRecoveryTruncated`
  （圆形恢复三点扫描被截断的组件数）。

### 性能（Release 构建实测：bump64 特征分析 71.8 ms；简化端到端提速约 30%，逐用例验证 collapse 计数不变）

- 新增 `FeatureDetectionCache`（`src/feature_detection/detail/FeatureDetectionCache.h`）：
  面法向、边信息、顶点邻接、顶点平均边长等全网格辅助结构只构建一次，证据、清理、
  recovery 各阶段传引用复用，消除跨阶段重复构建。
- 特征追踪图合并为单表 `TraceGraph`：边属性（signed kind、persistence 等）统一存
  `TraceEdgeAttrs` 一张按边键哈希的表（`traceEdgeAttrs()` 访问器），替代多张平行 map。
- 消除简化主循环的三重 placement 求解：`SolveResult` 候选
  （`std::array<SolveResult, 4>`）随队列 Candidate 携带，push/pop/tryCollapse
  复用同一次求解结果（版本戳保证 quadric 未变则解未变）。
- `TextureProtection` 拆分 `evaluate` / `buildPlan` / `apply`：排序阶段只评估
  不物化 UV 重写；被接受的 placement 构建一次 `TextureUpdatePlan` 并直接应用，
  避免 `applyCollapse` 内重建同一计划（`textureApplyFailures` 诊断应保持为零）。
- 特征曲线投影增加 `PolylineSegmentIndex` AABB 树：不少于 64 段
  （`kPolylineIndexMinSegments`）的 loop 最近点查询从线性扫描降为 O(log L)，
  短 loop 保留常数更小的线性扫描。
- `VertexState` 热路径瘦身：圆/椭圆拟合参数移出为紧凑 side table
  `FeaturePrimitiveFit`（经 `primitiveFitId` 引用），非特征顶点不再携带拟合负载。

### 工程加固（CLI / C API / IO / 基础工具）

- CLI：`OptionSpec` 表驱动的 help 生成与逐命令参数校验（`apps/
  CliArguments.cpp`），未知/拼错选项在命令入口统一报错而不是被静默忽略。
- C API：异常映射增加 `std::bad_alloc` → OOM 状态码 guard；数值参数增加 finite
  校验（如 `merge_relative_epsilon` 必须有限且非负）；`CApi.h` 顶部明确 v1 ABI
  不携带逐角纹理坐标，需要保纹理时应使用 C++ API。
- IO：`MeshIo` 解析器重写——数值解析改 `std::from_chars`、缓冲扫描替代逐行
  stringstream、新增 `probeStlFormat()`（ASCII/二进制探测 + 三角形数预读）、
  解析失败路径错误信息加固。
- 新增共享基础工具：`include/core/Tolerances.h`（统一退化三角形容差族，各检查
  共享同一最小面积尺度）、`include/core/MathConstants.h`（`kPi`）、
  `generateClosedCubeGrid()`（闭流形共享顶点立方体网格生成器，供测试与验证使用）。

### 架构升级 v2（R1–R7 第一至三批）

- 测试链接安全网（R4）：新增内部 STATIC 聚合库 `manumesh_internal`（复用既有
  object libraries，源码只编译一次），`manumesh_tests` 改为只链接它，删除 shared
  build 下对 `src/*.cpp` 的重编与 `$<TARGET_OBJECTS>` 拼接，消除 ODR 风险；
  `gtest_add_tests` 全部替换为 `gtest_discover_tests(DISCOVERY_MODE PRE_TEST)`；
  外部大模型用例拆入新目标 `manumesh_external_tests` 并打 `external` 标签，
  `ctest -LE "performance|external"` 成为秒级快速套件（另有 `external-tests`
  构建目标）。C ABI/DLL 边界的黑盒验证仍由 CLI 冒烟测试与 SDK consumer 承担。
- 通用统计上浮（R1）：新增 `analysis` 模块（`include/algorithms/analysis/
  MeshAnalysis.h`、`manumesh::analysis`），承载 `MeshStats`/`DistanceStats`/
  `computeMeshStats`/`compareMeshesBySampledDistance`；CSV 拼装
  （`statsHeaderCsv`/`statsRowCsv`）移入 CLI（`apps/CliCsv.*`）；旧
  `algorithms/simplification/Metrics.h` 保留为带弃用注释的转发头，下一个 minor
  版本删除；boundary checker 登记 `analysis -> {common, core}`。
- placement 归位（R2）：`projectBoundaryPlacement`/`BoundaryProjectionInput` 从
  `FeatureConstraints.*` 迁入新的 `src/simplification/detail/Placement.{h,cpp}`
  （placement 策略单元，为过滤器/放置策略列表化做物理准备）。
- 圆环 loop 匹配下沉（R3）：CLI `feature-compare` 内嵌的贪心
  center/radius/normal 匹配算法下沉为库函数
  `manumesh::feature::matchCircularLoops()`（新公共头
  `algorithms/feature_detection/FeatureComparison.h`；三级阈值成为
  `LoopMatchOptions` 的带默认值字段，默认值等于原硬编码值），CLI 只做
  load→detect→match→格式化；新增完全匹配/半径漂移/缺失 loop 单测。
- 错误处理统一（R5）：新增 `docs/design/error_handling_policy.md` 一页决策表
  （数据错误→Status/Result、编程错误→异常、C 边界→状态码、IO 渐进迁移到
  `Result<Mesh>`），`include/core/Status.h` 顶部注释引用该策略。
- 命名空间对齐（R6）：`src/common` 由 `manumesh::detail` 改名为
  `manumesh::common`（全部调用点更新，保留 `namespace manumesh::detail = common;`
  过渡别名一个 minor 版本）；`src/mesh_edit` 已为 `manumesh::mesh_edit`；
  `manumesh::feature` 与公共头 `algorithms/` 前缀两处"接受现状"连同
  "目录名 = 模块名 = 命名空间"约定登记进 `architecture.md` 命名空间约定表。
- 扩展点协议（R7）：新增 `docs/design/algorithm_extension_protocol.md`，固化
  新增算法的 7 步机械化路径、`validateOptions` 统一校验协议、诊断字段命名规范
  （"诊断跟着分支走"），并记录 mesh_edit 公共化的三条判据与时机（不实施）。

## 2026-07-11

### 纹理感知 4x4 QEM

- 为 `Mesh` 和 `PlainMesh` 增加面角 UV 所有权，覆盖 OBJ `vt` 解析、校验、压实、
  转换和简化输出。
- 保持几何 quadric 和 placement 求解为 4x4。可选的纹理保护通过尺度归一化的局部
  标量代价实现，并显式检查 UV chart、接缝、有向面积和退化情况。
- 增加确定性的接缝 chart 配对，使接缝上的兼容折叠仍可执行，同时过滤合并无关 chart
  的折叠。
- 增加矩阵维度、标量权重、UV 尺度不变性、接缝兼容性、UV 退化、OBJ 面角所有权、
  输出传播以及禁用保护时精确保持旧几何结果的专项测试。

### 可复用网格编辑基础层

- 新增内部 `mesh_edit` 层，将活动面、增量 vertex-face incidence、邻接/重复面查询和
  edit-state 到稠密 `Mesh` 的压实映射从 QEM 私有实现中拆出，作为后续 remeshing、repair
  和其他拓扑编辑算法的共享基础。
- 将边坍缩专属的拓扑更新保留在 `simplification/CollapseTopology.*`，quadrics、候选队列、
  特征约束和 collapse legality 继续由 simplification 策略层拥有，形成
  `core <- common <- mesh_edit <- simplification` 的单向依赖。
- 新增 `manumesh_mesh_edit_objects` 构建目标和 include-boundary 规则，禁止 `mesh_edit`
  反向依赖 simplification；内部编辑类型不安装到 SDK，不扩大公共 ABI/API。

### 几何基础与二轮质量优化

- 将三角形质量、距离、包围盒和相交谓词，以及通用空间候选索引和 mesh distance index
  下沉到 `common`，供简化、质量优化和后续 remeshing/validation 复用。
- 新增可选的 QEM 二轮局部质量优化，在不改变拓扑、参考面 envelope 和硬保护特征的前提下
  改善最差三角形质量；同步扩展 C++/C ABI options、report diagnostics 和 CLI 参数绑定。
- 将 CLI 的 feature/report/benchmark 与 workflow 命令拆分到独立 translation units，降低
  `ManuMeshCommands.cpp` 的体积和跨命令耦合。

### 测试与文档

- 新增 `mesh_edit` 单元测试，覆盖确定性顶点/面 remap、非活动/无效/退化面过滤、邻接缓存和
  重复面增量更新；新增 common 几何/空间索引测试和 quality-refinement 回归测试。
- 增加 include-boundary 检查器及其自测，测试中明确阻止 `mesh_edit -> simplification` 等
  反向依赖；补充圆孔、椭圆孔、boss/pocket 和多 junction 的 feature ground-truth 标签。
- 新增 `docs/design/mesh_edit_foundation.md`，并同步架构、源码组织、算法扩展、公共基础层、
  交付手册和 generated notes，说明未来 remeshing/repair 的复用边界。

### 本轮已验证

- `cmake --build build\mingw-ninja-release --target check-format`
- `ctest --test-dir build\mingw-ninja-release --output-on-failure -E '^ManuMeshDataset\\.'`：127/127 passed
- `ctest --test-dir build\mingw-ninja-release-performance --output-on-failure -R '^ManuMeshDataset\\.'`：4/4 passed
- `ctest --test-dir build\mingw-ninja-release-sdk --output-on-failure -R '^sdk_consumer_examples$'`：1/1 passed
- `git diff --check`

### Remeshing 与表面网格特征论文补充

- 新增 `docs/papers/remeshing/`，归档局部参数化、split/collapse/flip/smooth、
  metric-dependent Voronoi、自适应实时和 field-aligned 表面重网格化论文 M037-M041。
- 为 `feature_detection/` 补充 surface feature line、polygonal-surface ridge/ravine 和 quadric
  surface fitting 特征曲线网络论文 M042-M044。
- 新增 2026-07-11 OpenAlex 补充索引、公开下载来源与 SHA-256 记录，以及 OpenMesh、CGAL、
  pmp-library、libigl、geometry-central、Geogram、VCGlib、MMG 和 Instant Meshes 的表面网格
  能力对照。
- 明确论文和开源实现地图只服务三角/多边形表面网格算法测试，不扩展到 B-Rep、实体建模或
  CAD feature-tree。

## 2026-07-10

### 架构边界修复

- 收紧 Eigen provider 语义：CMake 现在记录实际解析到的 Eigen 来源，只有确实选择 vendored Eigen 时才把 `thirdParty/eigen` 暴露到 `manumesh_core` 的 public include 并安装到 SDK，避免 `system`/`fetch` 被源码树中的 vendored Eigen 静默覆盖。
- 强化 `Result<T>` 和 `MeshTopology` 公共契约：`Result<T>` 新增 `hasValue()`，错误状态下访问 `value()` 会抛出清晰异常；`MeshTopology::edge()` / `vertex()` 现在会对越界 handle 抛出 `std::out_of_range`，并新增 `hasEdge()` / `hasVertex()` 供调用方预检。
- 收紧 C ABI 输出结构体契约：`ManuMeshSimplifyReport` 和 `ManuMeshMeshStats` 在写入前必须通过 init 函数或有效的旧版 `struct_size` / `abi_version` 初始化；新增未初始化输出结构体拒绝测试，同时保留旧版短结构尾部兼容写入。
- 更新 C API 示例和 SDK consumer C 示例，显式初始化 report/stats 输出结构体。

### 本轮已验证

- `cmake --build build\mingw-ninja-release --target check-format --parallel`
- `cmake --build build\mingw-ninja-release --target unit-tests --parallel`：97/97 passed
- `cmake --build build\mingw-ninja-release-performance --target performance-tests --parallel`：4/4 passed
- `cmake --build build\cmakelists-maintain-install-check-mingw --target sdk-consumer-test --parallel`：2/2 passed

## 2026-07-09

### 文档同步

- 同步 README、docs 入口、源码组织说明、VS Code 调试手册、交付 HTML 和 generated notes HTML：补充 `include/io` / `src/io` 的当前 I/O 边界，以及默认关闭的 Debug-only `src/debugUtil` HTML wireframe 辅助工具用法。
- 新增 `docs/guide/debug_util_usage.md`，补充 debugUtil 开启方式、常用宏、颜色约定、推荐插入位置、注意事项和截图预览；新增 UseCase 颜色总览、模块特征识别、简化前后对比三张文档截图资产。
- 本轮未更新 `docs/papers/` 论文资料库和论文索引。

### Debug 辅助工具

- 新增内部 `debugUtil` HTML wireframe 工具，默认关闭，开启 `MANUMESH_ENABLE_DEBUG_UTIL` 且使用 Debug 构建时可通过一行宏输出本地 HTML，支持普通线框、按场景着色的边覆盖、feature analysis 叠加和简化前后对比。
- 内部 C++ 源文件在插桩点显式包含调试头：通用线框/边覆盖使用 `debugUtil/debugUtil.h`，特征快照使用 feature-owned `FeatureDebugInstrumentation.h` 适配器；Release 或未开启 debugUtil 时宏保持 no-op，且调试头不进入 public SDK install headers。

### 工程工具链

- 统一 `.clang-format`：切换为 4 空格缩进、120 列、参数/实参不 bin-pack、BlockIndent 参数换行、lambda/构造函数初始化列表等规则，并按新规则全量格式化 C/C++ 源码和测试。
- 扩展 Doxygen：启用 UTF-8 输入、source browser、treeview、搜索、引用关系、private/static/local class 抽取，以及可选 Graphviz 图；新增 `MANUMESH_DOXYGEN_ENABLE_GRAPHS`，在检测到 `dot` 时生成类图、include 图和调用关系图。

### 解耦重构与边界收紧

- 将 STL/OBJ 读写从 `core/Mesh.h` 拆到新的 `io/MeshIo.h` / `src/io/MeshIo.cpp`，让 `core` 继续专注 Mesh 数据结构、校验和基础几何；使用 `loadMesh()` / `saveAsciiStl()` 的 C++ 调用方现在需要显式包含 `io/MeshIo.h`。
- 收紧 `src/CMakeLists.txt` 的内部 target include 作用域：新增 `manumesh_io_objects`，不再把 `src/simplification` 私有 include 目录暴露给所有内部 object target。
- 清理 simplification detail 头文件对 public facade `QEMSimplifier.h` 的依赖，内部头只依赖 `SimplificationTypes.h`、`core/Mesh.h` 或真实需要的私有类型。
- 增加 `QEMSimplifier::simplify()` 和 `simplifyMesh()` 的预计算 `feature::FeatureAnalysis` 重载，使 simplification 可以直接消费外部 feature analysis，后续 repair/remesh/validation 可复用同一份特征结果。
- 将 C ABI 的 options/report/stats 字段映射、ABI 尺寸兼容写入和枚举转换集中到 `src/api/CApiConverters.cpp` / `src/api/detail/CApiConverters.h`，让 `CApi.cpp` 只保留边界生命周期、错误翻译和 API 调度。
- 拆出 CLI 横向工具层：`CliOptionBinding.*` 负责命令行参数到 SDK options 的绑定，`CliCsv.*` 负责 CSV 读写，降低 `ManuMeshCommands.cpp` 的基础设施耦合。
- 更新 README 最小 C++ 示例，补充新的 `io/MeshIo.h` include。

### 本轮已验证

- `cmake -S . -B build\mingw-ninja-release -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DMANUMESH_GOOGLETEST_PROVIDER=auto -DMANUMESH_BUILD_PERFORMANCE_TESTS=OFF -DCMAKE_BUILD_TYPE=Release`
- `cmake --build build\mingw-ninja-release`
- `ctest --test-dir build\mingw-ninja-release -LE performance --output-on-failure`：94/94 passed
- `cmake -S . -B build\mingw-ninja-release-performance -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DMANUMESH_GOOGLETEST_PROVIDER=auto -DMANUMESH_BUILD_PERFORMANCE_TESTS=ON -DCMAKE_BUILD_TYPE=Release`
- `cmake --build build\mingw-ninja-release-performance`
- `ctest --test-dir build\mingw-ninja-release-performance -L performance --output-on-failure`：4/4 passed
- `cmake --build build\debug-util-mingw --target manumesh_core --parallel`
- `clang-format --dry-run --Werror src\debugUtil\debugUtil.h src\debugUtil\debugUtil.cpp`
- `cmake --build build\mingw-ninja-release --target check-format --parallel`
- `cmake --build build\mingw-ninja-release --target unit-tests --parallel`：94/94 passed
- `cmake --build build\mingw-ninja-release --target docs-api --parallel`

### 本轮更新

- 强化特征检测入口校验：新增公共 `validateFeatureOptions()`，
  `FeatureDetector` 现在会统一拒绝非法阈值、尺度参数和空/无效 mesh。
- 强化 mesh/C API 防护：C API 增加 face count 到 `int` 的溢出保护；
  STL 读取遇到仅包含退化三角形的输入时会明确失败，而不是静默生成空网格。
- 重构内部构建分层：`src/CMakeLists.txt` 拆出 common、geometry、
  feature_detection、simplification 和 C API object targets，保持对外仍只导出
  `ManuMesh::manumesh` SDK 目标。
- 增加 `FeatureGuidance` 适配层，让 `simplification -> feature_detection -> core`
  的依赖关系显式化；QEM、collapse policy、约束和运行循环消费窄接口，
  不在 QEM 内部重复特征识别逻辑。
- 简化模块复用特征模块的 option validation，减少重复校验逻辑，并同步收紧
  Doxygen docs-api 输入，使 API 文档继续覆盖核心公开头文件。

### 本轮已验证

- `cmake --build build/mingw-ninja-release --target check-format --parallel`
- `cmake --build build/mingw-ninja-release --target unit-tests --parallel`：93/93 passed
- `cmake --build build/cmakelists-maintain-install-check-mingw --target sdk-consumer-test --parallel`：2/2 passed
- `cmake --build build/mingw-ninja-release-performance --target performance-tests --parallel`：4/4 passed
- `cmake --build build/mingw-ninja-release --target docs-api --parallel`
- `git diff --check`

### 修复

- 修复 C API 输出结构体的 ABI 写越界风险：`ManuMeshSimplifyReport` 和
  `ManuMeshMeshStats` 现在会按调用方传入的 `struct_size` 限定清零和字段写入，
  保留旧版尾部较短结构体的兼容性。
- 网格几何校验现在会拒绝重复顶点面和零面积三角形；C API
  `manumesh_mesh_set_data()` 在遇到退化面时不会替换调用方已有 mesh。

### 变更

- 重构 CMake 组织方式，移除项目自有 `.cmake` 模块，改为按目录维护：
  顶层 `CMakeLists.txt` 只保留全局选项、Eigen 解析、通用 helper 和目录装配；
  `src/`、`apps/`、`tests/`、`examples/`、`adm/` 分别维护库、CLI、
  测试、示例和开发/安装规则。
- 将 `manumesh_core`、CLI、GoogleTest provider、format/check-format、docs-api、
  SDK install/export/consumer test 等逻辑移动到对应目录级 `CMakeLists.txt`，
  保持 CMake 3.18 可用。

### 新增

- 新增 `docs/guide/adding_feature_workflow.md`，说明新增功能时如何判断落点、
  设计公共 API、拆实现文件、接入目录级 CMakeLists、补测试、扩展 CLI/C API
  和验证 SDK。
- 新增 `examples/feature_workflow_demo.cpp`，演示“特征检测 + feature-preserving
  QEM 简化 + 网格质量门禁”的 SDK 组合工作流，并接入 CTest 和 SDK samples 安装。
- 新增退化面拒绝、C ABI 旧结构体输出保护等回归测试。

### 已验证

- `cmake --build build\mingw-ninja-release --parallel`
- `cmake --build build\mingw-ninja-release --target check-format`
- `ctest --test-dir build\mingw-ninja-release --output-on-failure`：91/91 passed
- `cmake -S . -B build\cmakelists-maintain-install-check-mingw -G Ninja -DMANUMESH_ENABLE_INSTALL=ON -DMANUMESH_INSTALL_CMAKE_CONFIG=ON ...`
- `cmake --build build\cmakelists-maintain-install-check-mingw --target sdk-install-local --parallel`
- `cmake --build build\cmakelists-maintain-install-check-mingw --target sdk-consumer-test --parallel`：2/2 passed

### 文档

- 全量收紧 `docs/**/*.html` 响应式排版：统一加入换行、表格固定布局、代码/公式/长路径断行和移动端宽度保护，清理会导致横向溢出的 `nowrap`、固定单元格宽度和可见横向溢出规则。
- 大幅扩充特征识别说明：`current-program-principles.html` 增加从 edge evidence 到 `FeatureAnalysis` 的源码级数据流、失败信号、文献路线和下一轮算法落地清单；`normal-tensor-qem-notes.html` 增加 Normal Tensor 从论文公式到源码执行路径的逐步映射。
- 扩充 `manumesh-code-manual.html` 的特征识别函数级阅读顺序和测试保护建议，并在 `manumesh_kernel_developer_guide.html` 增加 Feature Detection Debug Contract，明确 evidence、trace ownership、cleanup、primitive fitting 和 QEM consumption 的模块边界。
- 同步更新 generated notes、delivery guide 和 `docs/archive/prototype-docs-2026-07-09/` 内的 HTML 副本，使正式文档、历史归档和论文引用说明保持一致。
- 扩充所有 HTML 的 QEM 二次型说明：统一展开 `Q=[[A,b],[b^T,c]]`、`E=x^T A x+2b^T x+c`，并用具体数值例子代入 `lineWeight=1e-3`、`featureBoost=0.08`、`boundaryWeight=5`、`featureCurveWeight=0.08` 和 component confidence，展示各项如何改变 `A/b/c` 与候选 collapse 代价。
- 澄清 QEM 深入页和执行计划中的 primitive 保护描述：当前代码没有独立的径向权重参数或径向二次型构造函数，圆/椭圆/多边形 loop 通过 tangent-line quadric、primitive projection、`maxFeatureCurveDeviationRatio` 与 hard feature policy 共同保护。
- 在 notes HTML 中补充 dihedral / normal-tensor 特征提高 line-quadric 权重的详细解释：明确该权重会偏离纯 plane-QEM 最优点，但用于抑制平坦区和特征附近的切向漂移、改变候选 collapse 排序；同时说明边界保护还依赖 boundary quadric / hard guards，弱特征需要 persistence、component confidence 与 benchmark 避免过度正则化。

### 测试

- 修正 performance 数据集测试中非圆硬特征用例的保护策略断言：`PrimitiveCurves` 模式负责验证可达到目标面数预算，`AllFeatureEdges` 模式单独验证 generic feature hard rejection，避免把严格锁定全部特征的保守模式误判为必须达到生产默认简化预算。

### 新增

- 增加 feature graph cleanup：在 loop recovery 前按局部边长归一化做短 gap bridge、近 junction bridge 和 tensor-only 弱 spur 删除；新增 `cleanupFeatureGraph`、`featureGraphGapLengthRatio`、`featureGraphMaxWeakSpurEdges`、`featureComponentMinConfidence` 选项及 CLI/C ABI 尾部字段。
- 增加 component-level confidence：`FeatureAnalysis::components` 统计强/弱证据比例、闭合率、junction/endpoint、cycle rank、tensor persistence、primitive residual 和 confidence；loop 与 vertex 记录 `componentId`、`confidence`、`weakFeature`。
- 增加 `feature-benchmark` CLI 和 `benchmarkFeatureEdges()`，支持用 vertex-index ground-truth edge labels 评估 precision/recall/F1、junction correctness、loop closure rate 和 component confidence。
- `SimplifyReport` / C ABI report / metrics CSV 增加 `feature_components`、`weak_feature_components`、`high_confidence_feature_components`、`graph_cleanup_*`、`mean_feature_component_confidence` 和 `min_feature_component_confidence`。

### 变更

- feature-curve soft quadric 权重按 component confidence 温和缩放，使强 CAD loop 保持接近原权重，弱证据 component 在 QEM 中先作为较软 support 使用。
- `feature-report` loop CSV 增加 `component_id`、`component_confidence`、`weak_feature` 和 `primitive_residual`，便于定位弱特征、破碎 loop 和 primitive fit 风险。

### 新增

- 增加 `docs/delivery/manumesh_kernel_developer_guide.html`，作为商用内核交付级开发者手册入口，覆盖定位、架构、模块边界、API/C ABI、构建、验证、扩展约束和交付清单。
- 将 2026-07-09 前的阶段性设计、指南和生成笔记归档到 `docs/archive/prototype-docs-2026-07-09/`，保留研发历史材料，同时避免和正式交付文档混用。
- 扩充 `docs/papers/feature_detection/`、`docs/papers/segmentation/` 和 `docs/papers/weak_features/`，补入特征线、normal voting/tensor、ridge/valley、线框提取、工程对象分割和弱特征整合论文。
- 增加 `docs/papers/feature_recognition_download_status.md` 和 `docs/papers/paper_index_openalex_2026-07-09.json`，记录论文下载状态、OpenAlex DOI/引用数量快照和未下载项线索。
- 增加 `FeatureOptions::loopTraceAngleDeg` / `SimplifyOptions::loopTraceAngleDeg`、CLI `--loop-trace-angle-deg`、C ABI `loop_trace_angle_deg`，用于把 feature evidence 阈值和 loop tracing 阈值分开。
- 增加 `tracedFeatureEdges` / `untracedFeatureEdges` 诊断，并同步到 feature report、simplify metrics CSV、C ABI report 和 VS Code demo/debug 配置。
- 增加 common 层 `computeVertexAverageEdgeLength`，作为 normal tensor 和后续 feature/QEM 策略共享的局部采样尺度。
- 增加 `normalTensorMinPersistentScales` / `--normal-tensor-min-persistent-scales` / C ABI `normal_tensor_min_persistent_scales`，用于要求 normal-tensor 弱特征至少被多个尺度支持。
- 增加 normal-tensor scored vertices、`max_normal_tensor_persistent_score`、`mean_normal_tensor_local_scale`、`mean_normal_tensor_persistence` 诊断，并同步到 FeatureAnalysis、SimplifyReport、C ABI、feature-report CSV、metrics CSV 和 VS Code 配置。
- 增加浅二面角 trace、严格 trace 下 untraced 诊断、tensor component 不阻塞独立圆孔 fallback 的 GoogleTest 回归保护。
- 增加 `docs/design/feature_detection_upgrade_2026_07_09.md`，记录本次特征识别升级、文献锚点和后续算法计划。

### 变更

- 重写 `docs/README.md`，将文档入口拆分为正式交付文档、历史归档和论文资料库，并明确当前交付范围不包括完整 B-Rep CAD kernel、通用 Boolean/offset、完整 CAD feature tree 恢复和全局 Hausdorff/envelope 认证。
- 重写 `docs/papers/README.md`，按 QEM、line quadrics、特征检测、分割、弱特征、特征保持简化、边折叠、神经/时间一致性 QEM 和网格生成分类索引 M001-M036，并在每篇论文标题后保留 OpenAlex 引用数量。
- 将论文索引用途从“零散 PDF 列表”调整为可支持特征识别、商用内核路线图和算法审核的本地 literature map。
- 移除 feature graph loop tracing 的 40 度硬下限，默认让浅特征按用户的 `featureAngleDeg` 进入 loop ownership；需要更严格 trace 时可显式设置 `loopTraceAngleDeg`。
- 修正 primitive recovery / circular fallback 的 loop id 分配时机，避免无效 primitive 造成非连续 id 和后续约束表漏建。
- 将 normal-tensor 对 small cycle basis 和 circular fallback 的影响从全局开关改成 trace connected component 级判断。
- normal tensor 平滑改为按局部边长归一化的距离权重，多尺度结果输出平均 feature score、persistence、persistent score 和 local scale；feature edge 接受与 QEM `weight-mode=normal-tensor` 共用 persistent score。

### 已验证

- `cmake --build build\mingw-ninja-release --target manumesh_tests manumesh --parallel`
- `ctest -R "FeatureDetection\.(TracesShallowDihedralLoopAtRequestedAngle|ReportsUntracedDihedralEdgesWhenTraceAngleIsStricter|TensorComponentDoesNotBlockSeparateCircularFallback)|CApiTest\.ExposesNormalTensorOptionsAndDiagnostics|CApiTest\.InitializesPrimitiveFitOptions" --output-on-failure`（5/5 passed）
- `ctest --test-dir build\mingw-ninja-release -R "NormalTensor|MeshQueriesComputeLocalVertexEdgeScale|CApi" --output-on-failure`（16/16 passed）
- `cmake --build build\mingw-ninja-release --parallel`
- `ctest --test-dir build\mingw-ninja-release -LE performance --output-on-failure`（85/85 passed）
- `.\build\mingw-ninja-release\bin\manumesh.exe feature-report tests\data\feature_fixtures\coaxial_hole_plate.obj --feature-angle-deg 25 --loop-trace-angle-deg -1 --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 --normal-tensor-threshold 0.06 --normal-tensor-edge-alignment 0.2 --normal-tensor-scales 3 --normal-tensor-min-persistent-scales 2 --csv output\vscode_demo\features.csv`
- `.\build\mingw-ninja-release\bin\manumesh.exe simplify tests\data\feature_fixtures\boss_pocket_plate.obj output\vscode_demo\normal_tensor.stl --method line --line-weight 1e-3 --weight-mode normal-tensor --feature-boost 0.08 --feature-angle-deg 179 --loop-trace-angle-deg -1 --normal-tensor-threshold 0.06 --normal-tensor-edge-alignment 0.2 --normal-tensor-scales 3 --normal-tensor-min-persistent-scales 2 --ratio 0.5 --samples 512 --metrics-csv output\vscode_demo\normal_tensor_metrics.csv`
- `.\build\mingw-ninja-release\bin\manumesh.exe validate-features --ratio 0.20 --samples 1000`
- `.\build\mingw-ninja-release\bin\manumesh.exe validate-external --ratio 0.25`
- HTML 静态审核：确认 `docs/**/*.html` 无残留 `white-space: nowrap`、移动端 `min-width:120px`、可见横向溢出规则、控制字符或错误转义引号；21 个 HTML 的 `table` / `section` / `style` 标签计数配平。
- 文档引用审核：`docs/papers/paper_index_openalex_2026-07-09.json` 可解析，generated/delivery HTML 中 18 个 `docs/papers/*.pdf` 引用均存在。
- Git 属性审核：新增 `.gitattributes` 将 `*.pdf` 作为二进制文件处理，避免论文 PDF 被文本 diff/check 误判为 trailing whitespace。
- `git diff --check`

## 2026-07-07

### 变更

- 将 CLI 入口从单一 `main.cpp` 拆成薄入口、`CliArguments.cpp`、
  `ManuMeshCli.cpp` 和 `ManuMeshCommands.cpp`，命令派发改为 command
  registry；新增命令时注册 handler，而不是继续扩张 main 的 if 链。
- 将简化阶段的候选坍缩评估抽出为 `CollapseAttempt.cpp`：
  `SimplificationRun.cpp` 保留运行循环、队列和状态应用，feature/boundary/
  curve-budget/legality 的接受拒绝流程由独立 evaluator 汇总结果，方便后续加入
  新过滤器或 placement 策略。
- 将特征检测内部从单一 `FeatureDetector.cpp` 拆成 pipeline 编排、edge
  evidence、feature graph、cycle/trace/primitive recovery、loop builder、
  circular fallback、normal tensor 和 primitive fit 等私有实现单元，保留公开
  `FeatureDetector`、`FeatureOptions` 和 `FeatureAnalysis` API 不变。
- 增加 feature detection 组合证据计数回归，确保 boundary、dihedral、
  non-manifold 和 normal-tensor evidence 后续扩展时仍保持来源计数、
  graph edge 数和关闭 tensor 后的诊断语义一致。
- 更新设计文档、调试指南、论文索引和测试说明中的 feature detection
  源码落点，使新增特征识别优先落到 `FeatureEvidence.cpp`、
  `FeatureCycleRecovery.cpp`、`FeatureTraceRecovery.cpp`、
  `FeaturePrimitiveRecovery.cpp` 或专属 recovery 文件，而不是继续扩张
  `FeatureDetector.cpp`。
- 更新架构和源码组织说明，明确 CLI 命令、collapse attempt、feature
  detection 各自的扩展落点。

### 已验证

- `cmake --build build\mingw-ninja-release --target manumesh_tests`
- `cmake --build build\mingw-ninja-release --target manumesh`
- `ctest --test-dir build\mingw-ninja-release --output-on-failure`（80/80 passed）

## 2026-07-06

### 变更

- 按 CMake 3.18 兼容语义重做测试注册：GoogleTest 用例由
  `gtest_add_tests` 从源码静态注册，不再依赖 `gtest_discover_tests`
  在构建或 CTest 枚举阶段启动测试可执行文件。
- 将 CMake 自定义测试目标、SDK consumer 测试和 VS Code 测试任务统一为
  `cmake -E chdir <builddir> ctest ...`，避免使用较新 CTest 才支持的
  `--test-dir`。
- 将 SDK consumer 清理步骤改为 `cmake -E remove_directory`，避免使用
  CMake 3.18 不支持的 `cmake -E rm -rf`。
- 固定 CMake Tools 默认 MinGW Release 构建目录，并默认关闭性能测试；
  性能测试继续通过独立的 performance 构建目录和 VS Code 任务运行。
- MinGW 下 `MANUMESH_GOOGLETEST_PROVIDER=auto` 不再优先使用预编译
  GoogleTest DLL，改为跳过该 DLL 包并为当前编译器构建 GoogleTest，
  避免 gcc 运行时和预编译 `libgtest*.dll` ABI 不匹配导致测试 exe
  启动时报 `0xc0000139`。
- `validate-features` 复制外部输入前会先删除旧目标文件，保证 VS Code
  验证任务可以重复运行。

### 已验证

- `git pull --ff-only`
- `.vscode/tasks.json`、`.vscode/launch.json`、`.vscode/settings.json`
  JSON 解析通过，69 个 task、8 个 launch 配置和 6 个输入项的引用链完整。
- `cmake -S . -B build/mingw-ninja-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DMANUMESH_GOOGLETEST_PROVIDER=auto -DMANUMESH_BUILD_PERFORMANCE_TESTS=OFF`
- `cmake --build build/mingw-ninja-release --parallel`
- `cmake -E chdir build/mingw-ninja-release ctest -N`
- `cmake -E chdir build/mingw-ninja-release ctest -LE performance --output-on-failure`
- `cmake -S . -B build/mingw-ninja-release-performance -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DMANUMESH_GOOGLETEST_PROVIDER=auto -DMANUMESH_BUILD_PERFORMANCE_TESTS=ON`
- `cmake --build build/mingw-ninja-release-performance --target performance-tests --parallel`
- `cmake -S . -B build/mingw-ninja-release-sdk -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DMANUMESH_GOOGLETEST_PROVIDER=auto -DMANUMESH_BUILD_PERFORMANCE_TESTS=OFF -DMANUMESH_ENABLE_INSTALL=ON -DMANUMESH_INSTALL_CMAKE_CONFIG=ON`
- `cmake --build build/mingw-ninja-release-sdk --target sdk-consumer-test --parallel`
- `cmake --build build/mingw-ninja-release --target docs-api --parallel`
- `.\build\mingw-ninja-release\bin\manumesh.exe validate-features --ratio 0.20 --samples 64 --output-dir tests/output/feature_curve_validation`

## 2026-07-05

### 新增

- 增加 `FeatureProtectionMode`，用于特征曲线简化策略：
  `none`、`circular-only`、`primitive-curves` 和 `all-feature-edges`。
  默认 `primitive-curves` 只硬保护圆、近圆和椭圆原语；普通多边形折线和二面角锐边继续作为软性的 line-quadric 代价，并由拓扑、法向、质量和局部误差过滤器约束。
- CLI 增加 `--feature-protection-mode`，C ABI 增加
  严格保护所有特征边请使用正式的 `feature_protection_mode = all-feature-edges`。
- C++ 和 C 报告中增加原语/普通特征拒绝计数，便于验证新策略是否减少普通特征的硬锁定。
- 测试辅助代码增加共享的简化报告计数不变量，每个 `simplifyWithReport`
  fixture 都会检查拒绝总数以及原语/普通特征拒绝子计数。
- 增加 `docs/design/feature_protection_roadmap.md`，记录外部模型探测结果，以及已实现的 CGAL/OpenMesh 风格策略拆分：原语曲线硬保护加普通锐边软约束。
- 在 `docs/generated/notes/` 下的生成 HTML 说明中加入共享的
  “Feature Protection Roadmap” 部分，使浏览器可读文档和 Markdown 设计文档保持同一算法方向。

### 变更

- 调整 `solverFallbacks` / `solver_fallbacks` 计数，只统计简化循环中实际处理的当前折叠候选。惰性队列插入仍会为排序计算临时位置，但不再抬高公开诊断计数。
- 将特征检测中的圆形顶点簇 fallback 限制为 32768 次确定性的三点圆扫描，避免破碎 CAD/STL 特征图耗时失控，同时保留已有图环和原语拟合路径。
- 扩展特征检测 API 与算法注释，说明 CAD/STL 图路径、张量弱特征路径和有界圆形修复 fallback 的适用范围与失败模式。
- 扩展简化 SDK 和 C ABI 注释，在 API 边界说明目标选择、line-quadric 排序代价、特征检测阈值、硬合法性过滤器、特征保护策略和拒绝计数。
- 重做特征曲线折叠策略：默认保护模式下，多边形/普通锐边顶点不再自动拒绝；严格保护模式仍可通过 `all-feature-edges` 使用。
- `validate-features` 默认改用完成态外部 STL fixture：Thingi10K spindle、NASA antenna azimuth track、Thingi10K mini pulley 和 OpenFOAM flange。旧的程序生成轴/联轴器/滑轮验证路径不再作为默认工业特征测试。
- 更新特征验证文档和生成 HTML 结果，报告新的原语/普通策略拆分，以及 `primitive-curves` 在破碎工业 STL 特征图上减少普通硬锁定的前后探测结果。
- 刷新文档，使用户命令、生成 HTML 说明和算法解释跟随当前 C++ 实现，而不是旧实验路径。
- 记录 Windows MinGW/Ninja 配置要求：需要同时指定
  `-DCMAKE_C_COMPILER=gcc` 和 `-DCMAKE_CXX_COMPILER=g++`；只指定 C++ 编译器可能让 CMake 混用 `cl.exe` 和 MinGW，并在构建前失败。
- 更新特征曲线文档和实践结果 HTML，说明当前 line/curve 验证输出、圆/椭圆/多边形特征策略、投影计数、曲线预算拒绝，以及曲线保护改进几何但不一定改进所有匹配数量的情况。
- 修正生成算法说明，使其使用当前 `collapseRejectReason(...)` 合法性路径，包括 link-condition 拓扑、三角形质量、法向偏差、局部误差和可选局部自相交保护。
- 澄清来源边界：Garland-Heckbert QEM、Liu/Rahimzadeh/Zordan line quadrics、Tsuchie-Higashi normal tensor feature lines 是来源思想；文档现在明确区分这些思想和仓库当前实现。
- 澄清工业验证文档和 SDK 头文件中 `solver_fallbacks` 的含义：它是执行期位置 fallback 指标，不是队列预排序统计。

### 已验证

- 删除 `build/` 后，使用
  `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DMANUMESH_ENABLE_INSTALL=ON -DMANUMESH_GOOGLETEST_PROVIDER=prebuilt -DMANUMESH_EIGEN_PROVIDER=vendored`
  配置。
- `cmake --build build --parallel`
- `cmake -E chdir build ctest -C Release --output-on-failure`
- `.\build\bin\manumesh.exe validate-features --ratio 0.20 --samples 1000 --input-dir tests\output\generated_inputs --output-dir tests\output\feature_curve_validation`
- `tests/output/feature_policy_validation/` 下的策略验证：
  `primitive-curves` 在 `nasa_mars2020_wheel` 上以 9066 个面达到目标，特征拒绝 31 次、普通特征拒绝 0 次；`all-feature-edges` 停在 10974 个面，特征拒绝 468702 次。
  在 `thingi10k_37880_functional_differential_gear_system` 上，
  `primitive-curves` 以 1236 个面达到目标且特征拒绝 0 次，而
  `all-feature-edges` 停在 2662 个面，特征拒绝 68993 次。
  在 `fandisk_2014` 上两者均达到目标，但普通特征拒绝从 513 降到 0。
- 对 `nasa_cubesat_middle`、`nasa_mars2020_wheel`、`casting_aimshape_2014`、
  `fandisk_2014`、`thingi10k_37880_functional_differential_gear_system` 和
  `large/rocker_arm.stl` 做外部探测；输出位于
  `tests/output/new_model_validation/`，用于暴露当前特征策略过度保护风险。
- `cmake -S . -B build/doccheck -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DMANUMESH_BUILD_DOCS=OFF`
- `cmake --build build/doccheck --parallel`
- `cmake -E chdir build/doccheck ctest --output-on-failure`
- `.\build\doccheck\bin\manumesh.exe --help`
- `.\build\doccheck\bin\manumesh.exe feature-report tests\data\feature_fixtures\coaxial_hole_plate.obj --feature-angle-deg 25 --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 --normal-tensor-threshold 0.06 --normal-tensor-edge-alignment 0.2 --csv output\doccheck\features.csv`
- `.\build\doccheck\bin\manumesh.exe simplify tests\data\feature_fixtures\coaxial_hole_plate.obj output\doccheck\simplified.stl --method line --ratio 0.50 --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 --preserve-feature-curves --feature-curve-weight 0.08 --max-feature-curve-deviation-ratio 0.05 --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 --min-circular-feature-loop-vertices 12 --samples 128 --metrics-csv output\doccheck\metrics.csv`
- `.\build\doccheck\bin\manumesh.exe validate-features --ratio 0.20 --samples 64 --output-dir output\doccheck\feature_validation`
- `.\build\doccheck\bin\manumesh.exe validate-external --ratio 0.25 --samples 64 --output-dir output\doccheck\external_validation`
- `.\build\doccheck\bin\manumesh.exe demo --quick --samples 64 --output-dir output\doccheck\demo --input-dir output\doccheck\demo_input`

## 2026-07-03

### 新增

- 增加边折叠的局部几何容差保护，包括 `maxLocalError`、
  `maxLocalErrorRatio`，以及超过局部漂移预算时的拒绝计数。
- 增加显式特征图层，用于特征环、共享顶点、交汇点、多特征归属和曲线感知折叠策略。
- 为圆形和椭圆特征环增加逐环特征曲线预算；圆形投影和重采样导向保护替代旧的仅依赖固定 `minFeatureLoopVertices` 的行为。
- 增加多尺度、局部归一化 normal-tensor 特征检测参数和报告，使弱特征可以和仅二面角检测进行比较。
- 增加数据集级验证覆盖：特征召回、曲线漂移、采样距离、拓扑、三角形质量和拒绝计数一致性。
- 增加 VS Code 演示任务，覆盖选定网格、算法预设、比例扫描、特征报告和算法对比。

### 变更

- 简化流程现在组合 QEM 排序、折叠前合法性和容差保护，不再只依赖事后采样距离。
- C API 报告暴露新的拒绝计数，包括曲线预算和局部误差拒绝。
- VS Code 工作流收敛到两条受支持的 Ninja 链路：主路径 `mingw+ninja` 和备用/调试路径 `msvc+ninja`。Ninja 任务移除固定 `--parallel 2` 限制，让 CMake/Ninja 使用可用并行度。
- 文档补充演示可用网格和参数示例，用于算法选择、特征保护、normal-tensor 检测和保守工业安全简化。

### 已验证

- `cmake -E chdir build\mingw-ninja-debug ctest --output-on-failure`
- `cmake --build build\mingw-ninja-release --target manumesh --parallel`
- `manumesh feature-report tests\data\feature_fixtures\coaxial_hole_plate.obj --feature-angle-deg 25 --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 --normal-tensor-threshold 0.06 --normal-tensor-edge-alignment 0.2 --csv examples\output\vscode_demo\coaxial_hole_plate\feature_report\features.csv`
- `manumesh simplify tests\data\feature_fixtures\coaxial_hole_plate.obj examples\output\vscode_demo\coaxial_hole_plate\feature-curves_r0_50\simplified.stl --method line --ratio 0.50 --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 --preserve-feature-curves --feature-curve-weight 0.08 --max-feature-curve-deviation-ratio 0.05 --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 --min-circular-feature-loop-vertices 12 --samples 128 --metrics-csv examples\output\vscode_demo\coaxial_hole_plate\feature-curves_r0_50\metrics.csv`

## 2026-07-01

### 新增

- 增加 `Status`、`Result`、类型化拓扑 handle 和 `MeshTopology`，作为网格内核方向的第一层可复用拓扑缓存。
- 增加 `docs/design/architecture.md`，说明 Polygonica 风格工业网格内核目标和模块边界。
- 增加公开的 Fandisk 与 AIM@SHAPE Casting STL fixture，用于本地复现 Tsuchie 和 Higashi 2014 CAD 模型实验。
- 增加 10 个超过 10k 面的公开大型 STL 验证网格，并加入
  `docs/design/large_model_validation.md` 记录 90% 和 50% line-quadrics 批处理结果。
- 增加 `docs/design/industrial_validation.md`，记录命令级验证覆盖、输出位置、指标和通过标准。
- 在 `thirdParty/googletest` 下加入 vendored GoogleTest，支持离线测试构建。

### 变更

- 调整 CMake 和 VS Code 工作流以适配 CMake 3.18.6 环境：移除 preset 命令，改用显式构建目录和生成器。
- 围绕 C++ 几何内核工作流重构仓库文档：CLI 生成的 STL/CSV 输出、CTest/API smoke 检查和外部 STL/CAD 查看器，替代此前以浏览器预览优先的路径。
- 更新 VS Code launch/tasks 和用户命令示例，以匹配库构建生成的 `bin/manumesh.exe` 运行时布局。
- 扩展工业库说明，补充源码布局边界、验证期望，以及将生成输出视为检查产物而不是源码依赖的指导。
- 刷新 flange、pipe coupling、pulley 和 stepped-shaft 场景的特征曲线验证 STL/CSV 输出。
- 网格 metric 中的边、边界和非流形计算改经 `MeshTopology`，不再在 `Metrics.cpp` 中重复构建临时 edge map。
- 为 QEM 边折叠增加 link-condition 拓扑合法性过滤器，避免闭合二流形输入被简化成意外孔洞或非流形边。
- 将 QEM 折叠验证和更新中的重复全量面扫描替换为增量 incident-face 拓扑，显著改善大型网格简化运行时间。

### 删除

- 移除 Vite/Node 浏览器查看器工作流。
- 移除 `CMakePresets.json`；CMake 3.18.6 不支持 presets。

## 2026-06-30

### 新增

- 增加跨平台 `manumesh` 共享库目标，公共头位于
  `include/manumesh`。
- 增加 Windows DLL export/import 处理，并为共享库构建设置默认符号可见性。
- 增加 install/export 规则，使外部 CMake 工程可以使用
  `find_package(ManuMesh CONFIG REQUIRED)`。
- 增加 `manumesh_copy_runtime_dependencies(target)`，供外部 Windows CMake consumer 将运行时 DLL 复制到可执行文件旁。
- 增加库消费示例程序 `examples/basic_simplify.cpp`。
- 增加 GoogleTest 覆盖和 CTest discovery，用于简化、特征检测和网格指标。
- 增加 clang-format 配置以及 `format`、`check-format` 目标。
- 增加 Doxygen 配置和 `docs-api` 目标。
- 增加 `docs/design/industrial_library.md`，记录集成、安装、运行时和工具说明。

### 变更

- CLI 改为链接新的可复用库目标，不再直接把算法源码编进可执行文件。
- 公共 API 声明移动到可安装头文件，同时保留旧 `src/*.h` 头作为兼容转发头。
- 使用新的 clang-format 配置格式化已有 C++ 源码。

### 已验证

- `cmake --build build\codex-industrial --config Debug --parallel`
- `cmake -E chdir build\codex-industrial ctest -C Debug --output-on-failure`
- `cmake --build build\codex-industrial --config Debug --target check-format`
- `cmake --build build\codex-industrial --config Debug --target docs-api`
- `cmake --install build\codex-industrial --config Debug --prefix build\codex-industrial\stage-copy-helper-3`
