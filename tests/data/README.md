# 测试数据目录结构

小型确定性 fixture 和较大的外部验证网格。

| 路径 | 用途 |
| --- | --- |
| `feature_fixtures/` | 用于针对性特征检测测试的小型手工 OBJ fixture。 |
| `qem_test/` | 数据集测试和参数化 QEM 测试使用的案例列表文件。 |
| `external/` | 纳入版本控制的外部 STL 验证网格，包括较大的回归输入。 |

新的单元测试几何应放在 `feature_fixtures/` 或 `qem_test/` 下的专用子目录中。
大型网格或第三方网格如果作为项目回归资产，应放在 `external/` 中。
