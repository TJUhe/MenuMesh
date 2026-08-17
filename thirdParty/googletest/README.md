# GoogleTest 依赖说明

GoogleTest 只用于本仓库的回归测试，不属于安装 SDK 的公共运行时。仓库只保留一套
可离线构建的 vendored 源码：

```text
source/googletest/
  CMakeLists.txt
  include/gtest/...
  src/...
```

测试依赖与主工程使用同一基线：Visual Studio 16 2019、MSVC v142、x64、C++14 和
DLL 版 MSVC 运行时。仓库不再提供 GoogleTest 预编译二进制包，Debug 和 Release
均由当前构建使用的 v142 工具链生成，并统一使用 `/MD`。

提供方式：

- 默认值和根目录的 `vs2019-*` presets 均使用
  `MANUMESH_GOOGLETEST_PROVIDER=source`，从 vendored 源码构建测试依赖。
- 显式选择 `auto` 时，依次尝试 vendored 源码、`find_package(GTest)` 可发现的 system 包和
  `FetchContent` 下载。
- `source` 只使用仓库内源码，缺失时立即配置失败，适合离线和可复现构建。
- `system` 只接受 `find_package(GTest)` 可发现且与 v142、x64、C++14、`/MD`
  兼容的安装包。
- `fetch` 通过 `FetchContent` 下载 GoogleTest 1.15.2，需要网络访问。

推荐配置：

```powershell
cmake --preset vs2019-debug
cmake --build --preset vs2019-debug-tests --parallel
ctest --preset vs2019-debug-unit
```
