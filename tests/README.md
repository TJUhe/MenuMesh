# 测试目录说明

| 路径 | 用途 |
| --- | --- |
| `support/` | GoogleTest 公共辅助代码，例如 fixture 读取、简化报告断言和测试工具函数。 |
| `unit/api/` | C ABI 单元测试。 |
| `unit/feature_detection/` | 特征检测、feature graph、normal tensor 和 primitive loop 单元测试。 |
| `unit/simplification/` | QEM/line quadrics、参数、合法性过滤、特征保护和 core mesh 行为单元测试。 |
| `performance/` | 大模型、外部数据集和性能/压力测试源码。默认构建关闭，由 `MANUMESH_BUILD_PERFORMANCE_TESTS=ON` 启用。 |
| `data/` | 纳入版本管理的 fixture、外部回归网格和数据集 case 列表。 |
| `output/` | CLI 验证命令生成的本地输出；被 git 忽略。 |

普通非性能测试仍然编译到一个 `manumesh_tests` 可执行文件中，方便 VS Code task、CTest 标签和本地调试保持简单。性能测试单独编译到 `manumesh_performance_tests`，并带 `performance` 标签。

GTest 和 CTest 运行日志应留在构建目录。`validate-features`、`validate-external` 等 CLI 验证产物，例如复制的 STL、CSV 和报告，放在 `tests/output/` 下。
