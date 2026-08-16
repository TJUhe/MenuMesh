# 示例目录结构

用于调用库的小型示例。

| 路径 | 用途 |
| --- | --- |
| `basic_simplify.cpp` | 使用规范 `SimplifyConfig` 的最小 C++ API 示例。 |
| `c_api_basic.c` | 最小 C API 示例。 |
| `feature_workflow_demo.cpp` | 面向特征的质量门禁：只运行一次特征检测，并通过 `QEMSimplifier::simplify(input, features, &report)` 复用分析结果。 |
| `sdk_consumer/` | 仅针对已安装 SDK 根目录构建的独立下游工程。 |
| `CMakeLists.txt` | 示例构建集成。 |

`basic_simplify.cpp` 和 `c_api_basic.c` 是链接到源码树目标的冒烟测试。
`sdk_consumer/` 是更严格的集成检查：先安装 SDK，再使用
`-DMANUMESH_SDK_ROOT=<install-prefix>` 配置该工程，使其只看到发布后的
`include/`、`lib/`、`bin/` 和 `share/` 目录布局。

C++ 新代码应从 `SimplifyConfig` 的 `target`、`cost`、`features`、`quality`、
`texture` 五个分组开始，运行日志由顶层 `verbose` 控制；`SimplifyOptions` 仅用于
0.x 源码兼容迁移。

演示输出应放在 `output/` 下。验证输入应放在 `tests/data/` 下，验证输出应放在
`tests/output/` 下。
