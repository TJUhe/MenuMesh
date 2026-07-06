# GoogleTest 依赖说明

GoogleTest 只用于本仓库的回归测试，不属于安装 SDK 的公共运行时。

仓库保留了两套历史预编译包：

```text
prebuilt/msvc-x64-static/
  include/gtest/...
  lib/Debug/gtest.lib
  lib/Debug/gtest_main.lib
  lib/Release/gtest.lib
  lib/Release/gtest_main.lib

prebuilt/mingw-x64-shared/
  include/gtest/...
  lib/libgtest.dll.a
  lib/libgtest_main.dll.a
  bin/libgtest.dll
  bin/libgtest_main.dll
```

仓库同时保留 `source/googletest/` 源码树，供 MinGW 默认测试构建使用。

当前默认策略：

- MSVC 可以继续通过 `LQ_GOOGLETEST_PROVIDER=auto` 使用匹配的静态预编译包。
- MinGW 下 `LQ_GOOGLETEST_PROVIDER=auto` 会跳过预编译 `libgtest*.dll`，改为从 `source/googletest/` 为当前 `gcc/g++` 工具链构建 GoogleTest，避免不同 MinGW 运行时之间的 DLL 符号不匹配，也避免配置阶段依赖在线下载。
- 只有确认预编译包和当前编译器 ABI 匹配时，才显式使用 `-DLQ_GOOGLETEST_PROVIDER=prebuilt`。

MinGW 推荐配置：

```powershell
cmake -S . -B build/mingw-ninja-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DLQ_GOOGLETEST_PROVIDER=auto
```
