# SDK 集成指南

本库当前提供 C++ API 和 C ABI 两条集成路径。C++ API 适合同编译器、同 ABI 的消费工程；C ABI 适合插件、宿主程序、跨语言绑定或 ABI 边界更严格的场景。

## 构建 SDK

MinGW + Ninja 的 Release 构建：

```powershell
cmake -S . -B build/mingw-ninja-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
  -DLQ_BUILD_PERFORMANCE_TESTS=OFF
cmake --build build/mingw-ninja-release --parallel
```

如果要生成本地安装布局：

```powershell
cmake -S . -B build/mingw-ninja-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DLQ_ENABLE_INSTALL=ON
cmake --build build/mingw-ninja-release --target sdk-install-local --parallel
```

默认本地 SDK 前缀在构建目录的 `sdk/` 下，可通过 `LQ_LOCAL_SDK_PREFIX` 修改。

## C++ API 最小用法

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

## C ABI 最小流程

1. `lq_context_create()` 创建上下文。
2. `lq_mesh_create()` 创建输入和输出 mesh handle。
3. 用 `lq_load_mesh()` 或 `lq_mesh_set_data()` 填充输入。
4. 调用 `lq_simplify_options_init()` 初始化 `LqSimplifyOptions`。
5. 调用 `lq_simplify_mesh()`。
6. 用 `lq_mesh_copy_vertices()` / `lq_mesh_copy_faces()` 取回数据，或用 `lq_save_ascii_stl()` 保存。
7. 销毁 mesh handle 和 context。

所有带 `struct_size` / `abi_version` 的结构体都必须先调用对应初始化函数。当前 `LQ_ABI_VERSION` 为 `1`。

## 运行时文件

Windows + MinGW 使用共享库时，应用目录需要：

- `libline_quadrics_qem.dll`
- 当前 MinGW 工具链对应的 `libstdc++-6.dll`、`libgcc_s_*.dll`、`libwinpthread-1.dll` 等运行时 DLL
- 如果运行 GoogleTest 测试，还需要 `libgtest.dll` 和 `libgtest_main.dll`

当前 CMake 会在构建目标时把必要 MinGW 运行时复制到构建目录 `bin`。外部分发时仍要确保这些 DLL 和 exe 放在同一目录，或由部署系统提供。

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

- 公共头只从 `include/line_quadrics_qem/` 引入。
- 不要依赖 `src/simplification/detail/`，这些是私有实现。
- 当前 SDK 不承诺通用布尔、offset、修复或去噪能力。
- STL/OBJ 文件读写主要服务当前 CLI 和测试；生产系统如需更多格式，应在宿主侧或未来 IO 模块中扩展。
