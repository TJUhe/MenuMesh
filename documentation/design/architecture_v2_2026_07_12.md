# ManuMesh 架构升级蓝图 v2（2026-07-12）

> 当前构建基线已经统一为 Visual Studio 16 2019 / MSVC v142。本文是带日期的架构演进记录；其中构建和验收命令均按当前 `vs2019-*` presets 执行。

本文是把 ManuMesh 从"结构良好的单产品内核"推进到"商用/一流开源水准（10/10）"的架构蓝图。
对标对象：Polygonica（商用网格内核）、OpenMesh、pmp-library、
CGAL Polygon Mesh Processing（PMP）。

> **后续同步（2026-07-15）**：特征检测已继续按本蓝图的职责拆分原则演进为九阶段编排，并新增独立的 normal filter、graph compatibility/consolidation、benchmark 和 segmentation translation units。当前实现契约见 [`feature_recognition_system_upgrade_2026_07_15.md`](feature_recognition_system_upgrade_2026_07_15.md)；本文其余性能数字和立项描述保留为 2026-07-12 决策快照。

> **实施状态（2026-07-12 已落地）**：R1–R7 的第一至三批与 R6 已全部实现并合入本分支，
> 具体状态见 R8 汇总表的"状态"列。本文其余部分保留立项时的论证与方案原文，
> 作为决策记录；与实现的逐项对应见 `CHANGELOG.md` 2026-07-12 小节。落地要点：
>
> - R4：`manumesh_internal` STATIC 聚合库（复用同一批 object libraries，源码只编译
>   一次），`manumesh_tests` 只链接它，shared build 下不再重编 `src/*.cpp`，ODR 风险
>   清零；`gtest_discover_tests(DISCOVERY_MODE PRE_TEST)` 全面替代 `gtest_add_tests`；
>   外部大模型用例拆入 `manumesh_external_tests` 并打 `external` 标签，
>   `ctest -LE "performance|external"` 成为约 17.5 秒的快速套件。
> - R1：新增 `manumesh::analysis` 模块（`include/algorithms/analysis/MeshAnalysis.h`
>   + `src/analysis/`），旧 `algorithms/simplification/Metrics.h` 变为带弃用注释的
>   转发头；CSV 拼装迁入 `apps/CliCsv.{h,cpp}`。
> - R2：`projectBoundaryPlacement` 迁入 `src/simplification/detail/Placement.{h,cpp}`，
>   并同时升级为 Lindstrom-Turk 边界守恒约束解（超出本蓝图原定的纯搬迁范围）。
> - R3：`matchCircularLoops()` 下沉为公共库函数
>   （`algorithms/feature_detection/FeatureComparison.h`，三级阈值成为
>   `LoopMatchOptions` 带默认值字段，默认值等于原 CLI 硬编码值）。
> - R5：决策表落地为 `documentation/design/error_handling_policy.md`，
>   `include/core/Status.h` 引用该策略。
> - R6：`src/common` 已改名为 `manumesh::common`，旧的
>   `namespace detail = common;` 过渡别名已经删除；`manumesh::feature` 与
>   `algorithms/` 前缀两处"接受现状"已登记。
> - R7-a/b/d：扩展点协议落地为 `documentation/design/algorithm_extension_protocol.md`
>   （7 步路径、`validateOptions` 协议、诊断字段命名规范、mesh_edit 公共化判据）。
> - R7-c（filter/placement 编译期列表化）为第四批"锦上添花"项，**尚未实施**，
>   仍按第 5 节的建议与下一个新增 collapse 过滤器合并执行。

前置事实（已由架构审核确认，直接采用，不再重复论证）：

- 依赖方向零违规：`tests/support/check_include_boundaries.py` 的 `MODULE_DEPENDENCIES`
  图与实际 include 完全一致。
- 错位项：`Metrics`（通用统计 + CSV 拼装）在 `simplification`；`projectBoundaryPlacement`
  在 `FeatureConstraints.cpp`；圆环 loop 匹配算法在 CLI 层 `ManuMeshFeatureCommands.cpp`。
- 测试在 shared build 下重编内部源码访问 DLL 符号，存在 ODR 风险。
- 错误处理四轨并行：IO 的 `bool + std::string* error`、`core/Status.h` 的
  `Status`/`Result<T>`、算法层 options 校验抛 `std::invalid_argument`、C API 状态码。
- 命名空间与目录错位：`src/common` 用 `manumesh::detail`、`feature_detection` 用
  `manumesh::feature`、`src/mesh_edit` 用 `manumesh::mesh`、公共头前缀 `algorithms/`
  在命名空间中无对应层级。
- `gtest_add_tests` 基于源码扫描，新增/改名测试需重配置；外部大模型用例混在 `unit` 标签。

## 0. 目标与评分判据

"10/10"在本仓库语境下定义为：

1. 每个模块只承载本层语义，无跨层错位实现（审核发现的三处错位清零）。
2. 新增一个算法模块或一个 collapse 过滤器，路径是机械化的（见 2.2），不需要读旧算法源码。
3. 错误处理有一页决策表，任何新 API 不需要再讨论"抛不抛异常"。
4. 测试基础设施无 ODR 风险、测试发现自动化、标签划分让 CI 分级执行。
5. 与对标库相比，缺的是能力面（booleans/offset 等尚未做），而不是架构机制。

## 1. 对标分析

### 1.1 Polygonica（商用基线）

- 能力面按功能族组织：healing/fixing、booleans、offsetting/shelling、simplification、
  sectioning/slicing、collision、point cloud、lattice。各族共享同一实体模型，
  商业上可按模块授权。
- API 组织：纯 C ABI；不透明实体 handle（`PTEnvironment`/`PTWorld`/`PTSolid` 等）；
  函数统一 `PF` 前缀按实体分组（`PFSolid*`）；参数不走长参数列表，而是实体上的
  property（`PFEntitySetProperty` 系列 + `PV_*` 枚举键），新算法加参数不破坏函数签名。
- 错误处理：函数返回状态码/错误实体，配合错误回调；不跨 C 边界抛异常。
- 线程：库内部并行（环境级线程数 property），调用方视 solid 为整体、按实体粒度隔离并发。
- 对 ManuMesh 的启示：C ABI 的 `struct_size + abi_version` 已对齐这一思路；
  "功能族并列 + 共享实体模型"正是 `include/algorithms/<domain>/` 的方向。

### 1.2 OpenMesh

- 泛型 kernel：`TriMesh_ArrayKernelT<Traits>` 等由 traits 参数化；动态 property 系统
  （`add_property` + `VPropHandleT<T>`）允许算法把私有数据挂在 mesh 上而不改 mesh 类型。
- Decimater 框架是"新增算法便捷"的教科书样本：`DecimaterT` 只负责队列与坍缩循环；
  准则以 module 插件表达（`ModBaseT` 派生，`decimater.add(ModQuadricT::Handle)`）。
  一个 continuous module 提供优先级（`collapse_priority()`），任意多个 binary module
  做合法性否决（返回 `ILLEGAL_COLLAPSE`），另有 `initialize()/preprocess()/postprocess()`
  钩子。新增一个准则 = 写一个 module 类 + `add()` 一行，不触碰 Decimater 本体。
- 错误处理：以返回值/assert 为主，几乎不用异常；mesh 对象非线程安全，由调用方隔离。
- 对 ManuMesh 的启示：吸收"编排器与准则解耦"的职责划分（3.7 给出等价物），
  不吸收模板 kernel 与 string-keyed property（见第 4 节）。

### 1.3 pmp-library

- 单一具体类型 `pmp::SurfaceMesh`（半边结构）+ 类型化 property
  （`add_vertex_property<T>("v:name")`）。
- 算法组织极简：`pmp/algorithms/` 下每个能力一个头，入口是自由函数
  （`pmp::decimate/remeshing/fill_hole/...`），参数就是普通实参；实现内部用类分阶段。
- 错误处理：统一异常族（`InvalidInputException`/`TopologyException`/`SolverException`），
  文档明确"输入不满足前置条件即抛"。
- 对 ManuMesh 的启示：pmp 证明"单一 mesh 类型 + 自由函数入口 + 目录即模块"足以支撑
  一流开源内核；ManuMesh 的 `simplifyMesh()`/`detectFeatureCurves()` 形态与其一致，
  应坚持而非模板化。不取其"全异常"策略（我们有 C ABI 与批处理场景，见 3.5）。

### 1.4 CGAL PMP

- 全部算法是 `CGAL::Polygon_mesh_processing` 命名空间下的自由函数，泛型基于 BGL
  FaceGraph concept，任何满足 concept 的 mesh 都能用。
- Named Parameters（`CGAL::parameters::vertex_point_map(...).do_project(true)`）解决
  "算法参数多且大多有默认值"的问题，加参数不破坏签名。
- 并行以 `Concurrency_tag` 模板参数显式选择（串行/TBB），逐算法声明是否支持。
- 错误处理：前置条件靠 assertion/异常，函数级文档写明前置条件。
- 对 ManuMesh 的启示：取"逐算法声明并行能力/线程契约"的文档纪律与"options 全部有
  可校验默认值"；不取 FaceGraph 泛型与 Named Parameters 模板技术（编译成本与
  DLL/pimpl 边界冲突），ManuMesh 的等价物是扁平 options struct + `*_init` C ABI。

### 1.5 "新增一个算法很便捷"的机制本质

四家的机制不同（property/module、自由函数、concept/named parameters、C property），
但本质相同，共四条：

1. mesh 数据模型稳定且唯一，算法不改 mesh 类型，只读写标准结构或旁挂数据。
2. 算法入口形态统一（自由函数或 handle+options），调用方无需了解实现分层。
3. 编排器与准则解耦：坍缩/修复循环是通用件，"接受/拒绝/打分/放置"是可替换件。
4. 新增算法的物理路径固定：一个新目录/新头/新注册点，不修改既有算法文件。

ManuMesh 已具备 1、2 和 4 的大部分（`algorithms/<domain>/` + object library +
boundary checker），差距集中在 3（准则组合仍是 `CollapseAttempt.cpp` 内的硬编码顺序）
和工程细节（测试链接、错误处理分轨）。

### 1.6 取舍表

| 机制 | 来源 | 取/不取 | 理由 |
| --- | --- | --- | --- |
| 功能族并列 + 共享实体模型 | Polygonica | 取（已在做） | `algorithms/<domain>/` 即该形态 |
| C ABI struct_size/abi_version | Polygonica | 取（已在做） | 跨语言与长期 ABI |
| 库内并行 + 线程数配置 | Polygonica | 暂不取，先文档化线程契约 | 单 run 性能未成瓶颈 |
| 泛型 traits kernel | OpenMesh | 不取 | 破坏 pimpl/DLL 边界，模板成本无对应收益 |
| decimation module 插件 | OpenMesh | 取其职责划分，不取运行时注册 | 见 3.7 |
| string-keyed property 系统 | OpenMesh/pmp | 暂不取 | 见第 4 节 |
| 自由函数 + 扁平 options | pmp | 取（已在做） | 保持 |
| 统一异常族 | pmp | 部分取 | 仅编程错误用异常，见 3.5 |
| Named Parameters | CGAL | 不取 | options struct 已覆盖同一需求 |
| 逐算法线程/前置条件文档 | CGAL | 取 | 纳入公共头 doc comment 规范 |

## 2. 目标架构

### 2.1 分层图与职责

```text
core                Mesh/PlainMesh/MeshTopology/Status/typed handles：稳定交换格式与只读拓扑
  ↑
common (src/common/detail)   跨算法只读几何/拓扑查询与空间索引，不含可变编辑状态
  ↑
mesh_edit           可编辑活动状态、增量邻接、compact/remap：管"怎么改"，不管"为什么改"
io                  STL/OBJ 读写：只依赖 core
  ↑
algorithms/*        平级算法模块（analysis、feature_detection、simplification、
                    未来 repair/remeshing/offset）：options→run→report，可消费彼此的公共结果类型
  ↑
api                 C ABI：handle + 显式初始化 struct，屏蔽异常
apps/examples       CLI 与示例：纯消费者，只做参数解析与输出格式化，不承载算法
```

依赖规则维持 `check_include_boundaries.py` 现状，新增模块必须先在
`MODULE_DEPENDENCIES` 登记——这条"新模块是显式架构决策"的机制是本仓库相对
OpenMesh/pmp 的独有优势，保留并继续作为 CTest `architecture` 标签强制执行。

### 2.2 新增一个算法的标准路径（7 步）

以未来 `repair` 为例，每步一个动作，全部机械化：

1. 目录：建 `include/algorithms/repair/` 与 `src/repair/` + `src/repair/detail/`；
   在 `check_include_boundaries.py` 的 `MODULE_DEPENDENCIES` 与
   `INCLUDE_MODULE_PREFIXES` 登记 `repair`。
2. 公共头：`RepairTypes.h`（扁平 options/report，不依赖 Eigen）+ `Repairer.h`
   （pimpl 对象 + `repairMesh()` 自由函数）。
3. options 校验：实现 `Status validateOptions(const RepairOptions&)` 公共函数
   （协议见 3.7-a），对象入口在构造时调用。
4. 诊断：report 字段按 3.7-b 命名规范填写；每个拒绝/降级路径必须有对应计数字段。
5. CLI 注册：`apps/` 新增 `commandRepair()` 并注册到 command registry；
   CLI 只绑定公共 options 与格式化 report，禁止出现算法逻辑（R3 的教训）。
6. 测试：`tests/unit/repair/` 按行为拆文件；黑盒用例进 `manumesh_tests`；
   需要 `detail` 符号的白盒用例进内部测试目标（见 3.4）；外部数据用例打 `external` 标签。
7. 文档：更新 `architecture.md` 依赖图、`source_organization.md` 目录契约、
   `CHANGELOG.md`；CMake 按 `adding_new_algorithm.md` 的清单接入 object library。

## 3. 改造清单

约定：工作量 S（≤半天）/ M（1–2 天）/ L（>2 天）；"验收"均隐含
`cmake --build --preset vs2019-release` + `ctest --preset vs2019-release-unit` + `check-format` +
include boundary check 通过。

### R1（a）Metrics 拆分：通用统计上浮为 analysis 模块，CSV 留 CLI

- 现状：`include/algorithms/simplification/Metrics.h` 混装三种语义——
  `MeshStats`/`computeMeshStats()`（对任意 mesh 的通用统计）、
  `DistanceStats`/`compareMeshesBySampledDistance()`（双 mesh 比较）、
  `statsHeaderCsv()`/`statsRowCsv()`（表现层字符串拼装）。前两者与简化无关，
  未来 repair/remeshing 同样需要；后者是 CLI 职责。
- 方案：新建 `include/algorithms/analysis/MeshAnalysis.h` 与 `src/analysis/`，
  namespace `manumesh::analysis`，承载 `MeshStats`/`DistanceStats` 与两个计算函数
  （实现从 `src/simplification/Metrics.cpp` 迁移，底层继续用 `common/detail` 查询）。
  CSV 两函数迁入 `apps/CliCsv.cpp`。旧头 `Metrics.h` 保留一个过渡版本，
  内容只剩 `#include` 转发 + `using` 别名并标注 deprecated，下一个 minor 版本删除。
- 涉及文件：`include/algorithms/simplification/Metrics.h`、
  `src/simplification/Metrics.cpp`、新 `include/algorithms/analysis/MeshAnalysis.h`、
  新 `src/analysis/MeshAnalysis.cpp`、`src/CMakeLists.txt`（新 object library
  `manumesh_analysis_objects`，依赖 common+geometry；simplification 依赖它）、
  `tests/support/check_include_boundaries.py`（`analysis` 登记：依赖
  `{common, core}`；`simplification`、`api` 可依赖 `analysis`）、
  `apps/ManuMeshCommands.cpp`、`CliCsv.cpp`、
  `examples/sdk_consumer/sdk_cpp_simplify.cpp`、相关测试。
- 验收：`simplification` 模块内不再有通用 mesh 统计实现；`grep -r statsRowCsv src/`
  为空（只存在于 apps）；boundary checker 含 `analysis` 且通过；SDK consumer 示例
  改用 `manumesh::analysis` 编译通过。
- 工作量 M；风险：低（纯搬迁 + 转发头兜底）；依赖：R4 先行可降低搬迁回归成本，
  但无硬依赖。

### R2（a）projectBoundaryPlacement 归位

- 现状：`src/simplification/FeatureConstraints.cpp:324` 的
  `projectBoundaryPlacement()`（声明于 `detail/FeatureConstraints.h:21`）是
  boundary 坍缩的 placement 策略，与 feature-curve 约束无语义关系，仅因历史原因同文件。
  调用点在 `CollapseAttempt.cpp:59/73`。
- 方案：迁入新 `src/simplification/detail/Placement.h` + `Placement.cpp`，与未来的
  optimal/endpoint/midpoint/feature-projection placement 策略同居一个"placement 策略"
  单元（为 R7-c 的策略列表做物理准备）。不新建模块，仍在 simplification 内。
  备选（不推荐）：并入 `CollapseTopology.cpp`——该文件语义是拓扑合法性而非放置。
- 涉及文件：`src/simplification/FeatureConstraints.cpp`、
  `detail/FeatureConstraints.h`、`CollapseAttempt.cpp`、新 `Placement.{h,cpp}`、
  `src/CMakeLists.txt`。
- 验收：`FeatureConstraints.*` 中不再出现 boundary placement 代码；
  boundary/topology 相关单测（`simplification_boundary_topology_tests.cpp`）不变绿转红。
- 工作量 S；风险：极低（模块内搬迁）；依赖：无，可随时做；建议与 R7-c 同批实施。

### R3（a）matchCircularLoops 下沉为库函数

- 现状：`apps/ManuMeshFeatureCommands.cpp` 的 `compare()`
  （约 251–380 行）内嵌完整算法：圆环 loop 贪心匹配（center/radius/normal 组合评分）、
  plausible/matched/weak_match 三级阈值（0.08·diag / 0.20 radiusRel / 30°，
  0.04·diag / 0.08 / 15°）与 `measureLoopAgainstCircle` 调用。这是库能力
  （验收简化质量、未来 repair 回归都需要），却只有 CLI 能用，且无单测。
- 方案：新公共头 `include/algorithms/feature_detection/FeatureComparison.h`，
  namespace `manumesh::feature`：
  `LoopMatchOptions`（把三级阈值变成有默认值的字段）、`LoopMatch`/`LoopMatchReport`
  （逐 loop 匹配结果 + 汇总计数），入口
  `LoopMatchReport matchCircularLoops(const FeatureAnalysis&, const FeatureAnalysis&, const Mesh& simplified, const LoopMatchOptions&)`。
  实现放 `src/feature_detection/FeatureComparison.cpp`。CLI `compare()` 只剩
  load→detect→match→格式化。
- 涉及文件：新公共头与实现、`src/CMakeLists.txt`、
  `apps/ManuMeshFeatureCommands.cpp`、新
  `tests/unit/feature_detection/feature_comparison_tests.cpp`、
  `documentation/design/architecture.md`。
- 验收：CLI `compare()` 函数体 ≤ 80 行且无数值算法；新库函数有独立单测
  （完全匹配、半径漂移、缺失 loop 三类用例）；`feature-compare` CLI 输出与现有
  golden 输出一致（阈值默认值等于现硬编码值）。
- 工作量 M；风险：低-中（CLI 输出兼容需回归比对）；依赖：无。

### R4（b）测试链接改造：内部 STATIC 聚合库 + gtest_discover_tests + external 标签

- 现状（`tests/CMakeLists.txt`）：shared build 下 `manumesh_tests` 一边链接 DLL
  `ManuMesh::manumesh`，一边 `target_sources` 重编
  `src/simplification/CollapseTopology.cpp`、`TextureProtection.cpp`，Win32 下再拼
  `manumesh_common_objects`/`manumesh_mesh_edit_objects` 的 objects——同一批符号
  存在两份定义，是教科书式 ODR 风险；`gtest_add_tests` 靠源码扫描，改测试要重配置；
  `feature_detection_external_tests.cpp` 等外部数据用例全部打 `unit` 标签。
- 方案：
  1. `src/CMakeLists.txt` 新增
     `add_library(manumesh_internal STATIC ${MANUMESH_LIBRARY_OBJECTS})`
     （不 export、不 install，PIC 已开），与 `manumesh_core` 共享同一批 object
     library，编译恰好一次。
  2. `manumesh_tests` 改为只链接 `manumesh_internal`（不再链接 DLL），删除全部
     `target_sources` 重编与 `$<TARGET_OBJECTS:...>` 拼接。白盒 include
     （`${PROJECT_SOURCE_DIR}/src`）保留——符号唯一后 include detail 头是安全的。
  3. C API/SDK 黑盒验证由现有 `sdk-consumer-test` 与 examples 承担（它们链接
     安装后的 DLL），必要时把 `tests/unit/api/` 三个文件拆出独立的
     `manumesh_sdk_tests` 目标链接 `ManuMesh::manumesh`，专测 DLL 边界。
  4. `gtest_add_tests` 全部替换为 `gtest_discover_tests(... DISCOVERY_MODE PRE_TEST)`
     （Windows 下 PRE_TEST 避免构建阶段提前运行尚未部署完整的 exe）。
  5. `*_external_tests.cpp` 与 `qem_parameter_industrial_tests.cpp` 中依赖
     `MANUMESH_TEST_EXTERNAL_DATA_DIR` 的用例拆到独立源文件集合，经
     `gtest_discover_tests` 的 `TEST_FILTER`/独立目标打 `external` 标签；
     `unit-tests` 目标改为 `-LE "performance|external"`，新增 `external-tests` 目标。
- 涉及文件：`src/CMakeLists.txt`、`tests/CMakeLists.txt`、（可选）
  `tests/unit/api/` 目标拆分、CI 脚本/`README.md` 中的 ctest 命令。
- 验收：shared build 下 `manumesh_tests` 的链接输入不含任何 `src/*.cpp` 重编与
  object 拼接；`ctest -L external` 与 `-LE external` 集合互斥且并集等于原集合；
  新增一个 `TEST()` 不改 CMake 即被发现；Visual Studio 16 2019 / MSVC v142 共享与静态配置均通过。
- 工作量 M；风险：中（共享/静态链接行为、GoogleTest 与 MSVC runtime 部署路径），
  用 VS2019 Debug/Release/静态矩阵兜底；依赖：无，且应最先做——它是 R1/R2/R3 搬迁的安全网。

### R5（c）错误处理统一：一页决策表 + 渐进迁移

决策表（写入 `architecture.md`，作为新 API 的强制规范）：

| 错误类别 | 判据 | 机制 | 现状与迁移 |
| --- | --- | --- | --- |
| 数据错误 | 合法调用 + 不合规输入（非流形、非有限坐标、空 mesh） | 返回 `Status` / `Result<T>`，不抛异常 | 新算法模块一律采用；simplify/detect 对"输入 mesh 不合规"当前抛 `invalid_argument`，在 v-next 增加 `Result` 入口后改判为数据错误 |
| 编程错误 | 调用方违反 API 契约（options 越界、句柄越界、未 init） | 抛异常（`std::invalid_argument`/`out_of_range`）或 assert | 保留现状：`SimplificationValidation.cpp`、`FeatureDetector.cpp` 的 options 校验、`MeshTopology.cpp` 的越界即此类，语义正确，不迁移 |
| C ABI 边界 | 任何跨 `api/CApi.h` 的失败 | 状态码 + 可查询 last-error 文本；`CApi.cpp` 捕获全部异常并映射 | 保留现状（`CApi.cpp:56` 已映射 `invalid_argument`），补一条"新增映射必须双向测试"的规则 |
| IO 错误 | 文件不存在/解析失败 | 目标 `Result<Mesh> loadMesh(path, options)`；现有 `bool + std::string*`（`include/io/MeshIo.h`）保留为薄包装 | 渐进：新增 Result 重载 → CLI/examples 切换 → 旧签名标 deprecated，一个 minor 版本后评估删除 |

- 迁移路径（三步，均可独立合入）：
  1. 文档化决策表并在 code review checklist 引用（S）。
  2. `io` 增加 `Result<Mesh>` 重载，旧签名内部转调（S）。
  3. 为 `simplifyMesh`/`detectFeatureCurves` 增加 `Result` 形态的姊妹入口
     （`trySimplifyMesh` 或 `simplifyMeshChecked`），把"输入 mesh 不合规"从异常
     改走 `Status`；对象入口行为不变（M）。
- 验收：决策表进入 `architecture.md`；`include/` 每个公共入口的 doc comment 标明
  错误机制；四轨收敛为"表中三轨 + 过渡期 IO 包装"。
- 工作量：合计 M；风险：低（全部增量式，无破坏性签名变更）；依赖：无。

### R6（d）命名空间/目录对齐：改一处、接受三处

成本收益评估结论：只有纯内部且无 ABI 暴露的错位值得改。

| 错位 | 处置 | 理由 |
| --- | --- | --- |
| `src/common` → `manumesh::detail` | 改为 `manumesh::common`（S） | 纯内部符号，`detail` 一词在本仓库另有含义（各模块私有层），撞名导致 `manumesh::detail` 与 `simplification/detail` 语义混淆；机械替换约 10 个文件 + 引用点 |
| `src/mesh_edit` → `manumesh::mesh` | 改为 `manumesh::mesh_edit`（S） | 同上，纯内部；`mesh` 一词过于泛化，与 core 的 `Mesh` 类型近身冲突 |
| `feature_detection` 目录 → `manumesh::feature` | 接受现状，记录 | 公共 API 已发布（头、C API、示例、文档全量引用 `manumesh::feature`），改名是破坏性变更；`feature` 作为 API 词面更短更好，错位方向是"目录长、命名空间短"，可接受 |
| 公共头前缀 `algorithms/` 无命名空间对应 | 接受现状，记录 | `algorithms/` 是 SDK 安装目录的分组手段（对齐 pmp 的 `pmp/algorithms/` 前缀），不承载语义层级；引入 `manumesh::algorithms::` 只会加长全名 |
- 涉及文件：`src/common/**`、`src/mesh_edit/**` 及其所有引用点（feature_detection、
  simplification、tests 的白盒用例）；`architecture.md` 增加"命名空间约定"小节把
  两处"接受"记录在案。
- 验收：`grep -rn "namespace manumesh::detail" src/` 与
  `namespace manumesh::mesh\b` 为空；约定小节写明"目录名 = 模块名 = 内部命名空间，
  公共命名空间可短于目录名，须在表中登记"。
- 工作量 S+S；风险：低（编译器兜底）；依赖：在 R1/R2 搬迁之后做，避免同一批文件
  反复 rebase。

### R7（e）扩展点机制

**a. options 统一校验协议。** 现状：simplification 与 feature_detection 各自在
构造/入口抛 `invalid_argument`，CLI（`CliOptionBinding.cpp`）与 C API 只能靠异常
兜底、无法预检。协议：每个算法模块公共头提供
`MANUMESH_API Status validateOptions(const XOptions&)` 自由函数；对象构造与函数
入口内部调用它并按 R5 决策表转成异常（编程错误轨）；CLI/C API 先调用它做预检并
输出 `status.message()`。现有 throw 语句改为"生成 Status → 入口处 throw"，
错误文案不变。验收：`validateOptions` 对全部现有非法值用例返回与原异常一致的
message（单测断言 message 相等）。工作量 M；风险低。

**b. 诊断字段命名规范。** 基于 `SimplifyReport` 现状归纳成规范写入
`architecture.md`：计数字段用 `<过去分词/名词>+<名词>` 的 int（`collapsedEdges`、
`rejectedCollapses`、`solverFallbacks`）；终止原因用模块级 enum
（`SimplifyTerminationReason` 样式）；比率/几何量用 double 并在注释标注单位与
归一化基准；每新增一个拒绝路径/降级路径必须新增对应计数字段（"诊断跟着分支走"）。
新模块 report 按此规范评审。工作量 S（纯文档 + review 规则）。

**c. collapse 过滤器与 placement 策略的组合机制（OpenMesh module 的 ManuMesh 等价物）。**

- 对标：OpenMesh 的价值 = "Decimater 循环不知道准则的存在"；实现手段（虚基类 +
  运行时 `add()`）只是其中一种。ManuMesh 现状：`CollapseAttempt.cpp` 把 feature、
  boundary、curve budget、texture、legality 检查以硬编码顺序内联组合，新增一个
  过滤器要改 `CollapseAttempt.cpp` 本体——这是与 OpenMesh 的真实差距。
- 设计（编译期显式列表，不引入运行时注册）：在 `src/simplification/detail/` 定义
  两个窄接口：
  `CollapseFilter`（输入 `CollapseCandidateContext`，输出 accept/reject + reason 枚举，
  reason 直通诊断计数）与 `PlacementPolicy`（输入候选边与约束上下文，输出
  `Vec3 position` + 置信标记）。`SimplificationPolicies.cpp` 依据 options 构造
  **有序的** `std::vector<FilterEntry>`（普通 struct + 函数指针/`std::function`
  均可，无需虚表），`CollapseAttempt.cpp` 退化为"遍历 filter 列表 + 选
  placement + 汇总 reason"。新增过滤器 = 新文件实现 + Policies 列表一行 +
  report 一个计数字段，`CollapseAttempt.cpp` 不再改动。
- 取舍评估：运行时注册（OpenMesh 式 `add()`）的收益是外部调用方可注入自定义准则；
  ManuMesh 的 SDK 边界是 options struct + C ABI，外部注入 C++ 回调会破坏 ABI 承诺，
  且当前无此需求。**推荐：做编译期列表化（M），不做运行时注册；当出现"SDK 用户要求
  自定义准则"的真实需求时再评估回调注入。** 现有 detail 静态组合的唯一优势是零间接
  开销——filter 列表用值语义 struct 数组即可保住该优势。
- 验收：`CollapseAttempt.cpp` 中不再出现具体过滤器逻辑（只有循环与汇总）；
  每个 filter 单文件可独立单测；以 `TextureProtection` 为样板迁移后全部简化回归不变。
- 工作量 M-L；风险：中（触及热路径，需以 `performance` 标签测试确认无回退）；
  依赖：R2（Placement 文件就位）之后。

**d. mesh_edit 升格公共 API 的判据与时机。** 判据（同时满足才升格）：
① 至少两个已发布的公共算法模块需要把编辑操作暴露给调用方（而非仅内部复用）；
② 出现真实 SDK 用户需求"增量编辑 + 回读映射"；③ typed handle +
generation-aware 生命周期已实现（`mesh_edit_foundation.md` 扩展顺序第 1 步），
否则公共化会把不稳定索引语义写进 ABI。时机：remeshing 模块落地并稳定一个版本后
评估；在此之前 mesh_edit 保持 internal，公共层继续以"输入 Mesh → 输出 Mesh +
old-to-new 映射"表达。工作量：本条为判据记录（S）。

### R8（f 汇总）清单一览

| 编号 | 内容 | 工作量 | 风险 | 依赖 | 状态 |
| --- | --- | --- | --- | --- | --- |
| R4 | 测试 STATIC 聚合 + discover + external 标签 | M | 中 | 无（最先做） | 已实现 |
| R2 | projectBoundaryPlacement → Placement.{h,cpp} | S | 极低 | 无 | 已实现（含 LT 边界守恒升级） |
| R1 | Metrics 拆分 → analysis 模块 + CSV 归 CLI | M | 低 | 建议在 R4 后 | 已实现 |
| R3 | matchCircularLoops 下沉库函数 | M | 低-中 | 无 | 已实现 |
| R5 | 错误处理决策表 + IO/算法 Result 入口 | M | 低 | 无 | 决策表已落地（error_handling_policy.md）；Result 姊妹入口按表渐进 |
| R7-a | validateOptions 协议 | M | 低 | 与 R5 同批 | 协议已成文（algorithm_extension_protocol.md） |
| R7-b | 诊断命名规范 | S | 无 | 无 | 已成文 |
| R6 | common/mesh_edit 命名空间改名 + 记录两处接受 | S | 低 | R1/R2 之后 | 已实现（保留过渡别名） |
| R7-c | filter/placement 编译期列表化 | M-L | 中 | R2 之后 | 未实施（第四批，随下一个新过滤器） |
| R7-d | mesh_edit 公共化判据（仅记录） | S | 无 | 无 | 已记录 |

## 4. 不做清单（过度设计防线）

| 不做 | 为什么现在不做 | 何时再评估 |
| --- | --- | --- |
| 泛型 mesh kernel（OpenMesh traits / CGAL FaceGraph） | 单一 `Mesh` + `PlainMesh` + C ABI 是产品定位；模板化摧毁 pimpl/DLL 边界并把编译成本转嫁用户，pmp 证明单类型可达一流水准 | 出现"必须适配外部 mesh 类型零拷贝"的付费需求时 |
| 运行时插件 / 准则注册（dll 加载、回调注入） | SDK 边界是 options struct，外部 C++ 回调破坏 ABI 承诺；内部扩展用 R7-c 编译期列表已够 | SDK 用户提出自定义准则需求时 |
| 属性系统全面化（string-keyed property maps） | 当前"typed 数组旁挂 + compaction remap"满足 UV/feature 传播；通用 property 系统是为不可预知属性设计的，现阶段属性集合可枚举 | remeshing 需要传播 ≥3 种新属性且各算法组合爆炸时 |
| 提前引入半边内核 | `mesh_edit_foundation.md` 已定顺序：typed handle → split/collapse/flip → patch/transaction，半边是最后一步；提前引入会为 QEM 单一消费者付全价 | remeshing 的 flip/split 频率证明增量邻接不够用时 |
| 库内并行框架 | 单 run 性能未成瓶颈；先在公共头文档化线程契约（对象无共享可变状态、const 成员线程安全、不同 mesh 可并行处理） | 客户级 batch 吞吐需求出现时，优先做"多 mesh 并行"而非"单 run 并行" |
| Named Parameters 仿制 | 扁平 options + `*_init` 已解决"参数多且有默认值"，再造一层只增加 API 表面积 | 不评估（与 C ABI 长期冲突） |

## 5. 执行顺序建议

按"安全网 → 搬迁 → 规范 → 机制"四批推进，每批独立可合入：

1. **第一批（安全网，必须）**：R4。测试链接的 ODR 风险是当前唯一可能产生
   未定义行为的项，且它是后续所有搬迁的回归保障。
2. **第二批（错位清零，必须）**：R2 → R1 → R3。三处错位是"10/10 判据 1"的直接缺口。
3. **第三批（规范固化，必须）**：R5 + R7-a + R7-b + R7-d 判据记录。全部低风险、
   以文档与增量 API 为主。
4. **第四批（机制与打磨，锦上添花）**：R6 命名空间改名、R7-c 列表化。R7-c 建议
   与"下一个新增 collapse 过滤器"的功能需求合并实施，摊薄热路径回归成本。

到 10 分的必要集合：第一、二、三批 + R6 的"记录两处接受"部分（约定成文即可）。
第四批提升的是"下一个过滤器"的边际成本与内部一致性，属于加分项而非门槛。
