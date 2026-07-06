# ManuMesh SDK 集成指南

ManuMesh 当前提供 C++ API 和 C ABI 两条集成路径。C++ API 适合同编译器、同 ABI 的消费工程；C ABI 适合插件、宿主程序、跨语言绑定或 ABI 边界更严格的场景。当前二进制、include 路径和 namespace 仍保留 `line_quadrics_qem` / `lq` 兼容标识。

## 构建 SDK

MinGW + Ninja 的 Release 构建：

```powershell
cmake -S . -B build/mingw-ninja-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
  -DLQ_GOOGLETEST_PROVIDER=auto `
  -DLQ_BUILD_PERFORMANCE_TESTS=OFF
cmake --build build/mingw-ninja-release --parallel
```

如果要生成本地安装布局：

```powershell
cmake -S . -B build/mingw-ninja-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DLQ_GOOGLETEST_PROVIDER=auto `
  -DLQ_ENABLE_INSTALL=ON
cmake --build build/mingw-ninja-release --target sdk-install-local --parallel
```

默认本地 SDK 前缀在构建目录的 `sdk/` 下，可通过 `LQ_LOCAL_SDK_PREFIX` 修改。

## C++ API 最小用法

Eigen-backed 便利入口适合同编译器、同 C++ ABI 的消费工程：

```cpp
#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"
#include "line_quadrics_qem/core/Mesh.h"

lq::SimplifyOptions options;
options.targetRatio = 0.25;
options.useLineQuadrics = true;
options.lineWeight = 1e-3;
options.preserveFeatureCurves = true;

lq::SimplifyReport report;
lq::Mesh output = lq::simplifyMesh(input, options, &report);
```

需要复用配置时使用 `lq::QEMSimplifier` 对象。`SimplifyReport` 中的拒绝计数是“每个当前候选第一次被哪个硬过滤拒绝”的诊断信息，不应当当作互斥之外的总失败次数相加解释。

如果宿主程序不希望自己的 C++ 交换类型暴露 Eigen，使用 `PlainMesh` 入口：

```cpp
#include "line_quadrics_qem/algorithms/simplification/PlainSimplifier.h"

lq::PlainMesh input;
lq::SimplifyOptions options;
lq::SimplifyReport report;
lq::PlainMesh output = lq::simplifyPlainMesh(input, options, &report);
```

`SimplificationTypes.h` 包含 `SimplifyOptions`、`SimplifyReport` 和相关枚举，不依赖 Eigen；`QEMSimplifier.h` 仍包含 Eigen-backed `Mesh`。

独立特征检测入口：

```cpp
#include "line_quadrics_qem/algorithms/feature_detection/FeatureDetector.h"

lq::FeatureOptions featureOptions;
lq::FeatureDetector detector(featureOptions);
lq::FeatureAnalysis features = detector.analyze(input);
```

## C ABI 最小流程

1. `lq_context_create()` 创建上下文。
2. `lq_mesh_create()` 创建输入和输出 mesh handle。
3. 用 `lq_load_mesh()` 或 `lq_mesh_set_data()` 填充输入。
4. 调用 `lq_simplify_options_init()` 初始化 `LqSimplifyOptions`。
5. 调用 `lq_simplify_mesh()`。
6. 用 `lq_mesh_copy_vertices()` / `lq_mesh_copy_faces()` 取回数据，或用 `lq_save_ascii_stl()` 保存。
7. 销毁 mesh handle 和 context。

所有带 `struct_size` / `abi_version` 的结构体都必须先调用对应初始化函数。当前 `LQ_ABI_VERSION` 为 `1`。同一 ABI 版本内，库接受尾部较短的旧 `struct_size`，只读取调用方结构体中实际存在的字段，缺失的新尾部字段使用库默认值；未初始化结构体或 ABI 版本不匹配仍会返回 `LQ_STATUS_INVALID_ARGUMENT`。

## CMake config 与 Eigen

安装 SDK 时如果打开 `-DLQ_INSTALL_CMAKE_CONFIG=ON`，下游可以使用：

```cmake
find_package(line_quadrics_qem CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE line_quadrics_qem::line_quadrics_qem)
line_quadrics_qem_copy_runtime_dependencies(my_app)
```

`line_quadrics_qemConfig.cmake` 会先为 SDK 内安装的 vendored Eigen 创建 `Eigen3::Eigen` imported target；如果 SDK 内没有 vendored Eigen，则调用 `find_dependency(Eigen3 3.3 NO_MODULE)`。因此包含 `Mesh.h` 的 C++ 消费方可以从导出的目标获得一致的 Eigen include 契约。

## 运行时文件

Windows + MinGW 使用共享库时，应用目录需要：

- `libline_quadrics_qem.dll`
- 当前 MinGW 工具链对应的 `libstdc++-6.dll`、`libgcc_s_*.dll`、`libwinpthread-1.dll` 等运行时 DLL

当前 CMake 会在构建目标时把必要 MinGW 运行时复制到构建目录 `bin`。MinGW 下 `LQ_GOOGLETEST_PROVIDER=auto` 默认从仓库源码为当前编译器构建 GoogleTest，测试程序不再依赖仓库历史预编译的 `libgtest.dll` 和 `libgtest_main.dll`。外部分发时仍要确保运行库 DLL 和 exe 放在同一目录，或由部署系统提供。

## 示例工程

仓库包含两个外部消费示例：

| 示例 | 说明 |
| --- | --- |
| `examples/sdk_consumer/sdk_cpp_simplify.cpp` | 使用 C++ SDK 简化网格。 |
| `examples/sdk_consumer/sdk_c_abi_basic.c` | 使用 C ABI 创建、简化和读取网格。 |

启用安装目标后可运行：

```powershell
cmake --build build/mingw-ninja-release --target sdk-consumer-test --parallel
```

## 集成边界

- 公共头只从 `include/line_quadrics_qem/` 引入；新代码优先使用 `algorithms/feature_detection` 和 `algorithms/simplification`。
- 不要依赖 `src/simplification/detail/`，这些是私有实现。
- 不要依赖 `src/feature_detection/detail/`，primitive fitting、trace/cycle 恢复等 helper 仍是私有实现。
- 当前 ManuMesh SDK 不承诺通用布尔、offset、修复或去噪能力。
- STL/OBJ 文件读写主要服务当前 CLI 和测试；生产系统如需更多格式，应在宿主侧或未来 IO 模块中扩展。
