# 更新日志

## 2026-07-11

### Texture-aware 4x4 QEM

- Added face-corner UV ownership to `Mesh` and `PlainMesh`, including OBJ `vt`
  parsing, validation, compaction, conversion, and simplification output.
- Kept the geometry quadric and placement solve at 4x4. Opt-in texture preservation is
  implemented as a scale-normalized local scalar cost plus explicit UV-chart,
  seam, signed-area, and degeneration checks.
- Added deterministic seam-chart pairing so compatible collapses along a seam
  remain possible while collapses that merge unrelated charts are filtered.
- Added focused tests for matrix dimension, scalar weighting, UV-scale
  invariance, seam compatibility, UV degeneration, OBJ corner ownership,
  output propagation, and exact legacy geometry when protection is disabled.

### Deterministic smooth feature detection

- Added opt-in multiscale smooth ridge/valley evidence based on robust local
  quadric fitting, generalized principal-curvature estimation, signed
  directional extrema, tangent consistency, and scale persistence.
- Kept smooth-curvature edges distinct from boundary, dihedral, non-manifold,
  and normal-tensor evidence throughout `FeatureGraph`, component confidence,
  cleanup, diagnostics, CSV output, and the feature CLI.
- Added focused tests for exact-plane rejection, smooth-feature response,
  scale invariance, persistence filtering, graph ownership, and invalid options.
- Added recent 2017-2025 deterministic literature notes and a source-level map to
  OpenMesh, CGAL PMP, pmp-library, libigl, and geometry-central. AI and learned
  feature scoring remain explicitly outside this implementation.

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
- 为 `feature_detection/` 补充光滑特征线、polygonal-surface ridge/ravine 和 quadric
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
- CMake 会对内部 C++ object target force-include `debugUtil/debugUtil.h`；Release 或未开启 debugUtil 时宏保持 no-op，不进入 public SDK install headers。

### 工程工具链

- 参考 `TJUhe/WeldPathExtract` 对齐 `.clang-format`：切换为 4 空格缩进、120 列、参数/实参不 bin-pack、BlockIndent 参数换行、lambda/构造函数初始化列表等规则，并按新规则全量格式化 C/C++ 源码和测试。
- 参考 `TJUhe/WeldPathExtract` 扩展 Doxygen：启用 UTF-8 输入、source browser、treeview、搜索、引用关系、private/static/local class 抽取，以及可选 Graphviz 图；新增 `MANUMESH_DOXYGEN_ENABLE_GRAPHS`，在检测到 `dot` 时生成类图、include 图和调用关系图。

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
  `src/`、`apps/manumesh/`、`tests/`、`examples/`、`adm/` 分别维护库、CLI、
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
