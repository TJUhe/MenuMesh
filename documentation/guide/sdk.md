# SDK 集成

ManuMesh 有三条公开调用边界。新代码先选择边界，再决定是否需要算法诊断；不要 include
`src/` 下的私有头。

| 边界 | 入口 | 适用场景 |
| --- | --- | --- |
| C++ SDK | CMake target `ManuMesh::manumesh`；C++ 命名空间 `manumesh` | 同编译器、同 C++ ABI 的应用 |
| Eigen-free C++ | `manumesh::PlainMesh`、`algorithms/simplification/PlainSimplifier.h` | 不希望公共交换类型暴露 Eigen |
| C ABI v1 | CMake target `ManuMesh::c_api`；全局 `manumesh_*` 符号 | 跨语言、插件和稳定二进制边界 |

C++ SDK 只承诺同编译器/同 ABI 的源码集成；0.x 版本不承诺跨 SDK 版本直接复用旧 C++ 二进制，
升级后应重新编译 consumer。需要跨版本或跨语言的稳定边界时使用 C ABI v1。

## C++ 最小流程

```cpp
#include "algorithms/simplification/QEMSimplifier.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "core/Mesh.h"
#include "io/MeshIo.h"

#include <string>

manumesh::Mesh input;
std::string error;
if (!manumesh::loadMesh("input.stl", input, &error)) return 1;

manumesh::simplification::SimplifyConfig config;
config.target = manumesh::simplification::SimplifyTarget::ratio(0.25);
config.cost.lineQuadrics =
    manumesh::simplification::LineQuadricConfig::uniform(1e-3);
config.features.enabled = true;

manumesh::simplification::QEMSimplifier simplifier;
simplifier.setConfig(config);
manumesh::simplification::SimplifyReport report;
manumesh::Mesh output = simplifier.simplify(input, &report);
if (!manumesh::saveBinaryStl("output.stl", output, &error)) return 1;
```

规范配置是 `SimplifyConfig` 的 `target`、`cost`、`features`、`quality`、`texture` 五组。
扁平 `SimplifyOptions` 只为 0.x 源码迁移保留；`algorithms/simplification/Metrics.h` 也只是
旧统计 API 的弃用转发头，新代码使用 `algorithms/analysis/MeshAnalysis.h`。

特征分析可独立运行并复用：

```cpp
manumesh::feature::FeatureOptions featureOptions;
manumesh::feature::FeatureDetector detector(featureOptions);
manumesh::feature::FeatureAnalysis features = detector.analyze(input);
manumesh::Mesh output = simplifier.simplify(input, features, &report);
```

`FeatureAnalysis` 绑定生成时的顶点坐标、面顺序和面角索引；不匹配的网格会被拒绝。数据流是
`Mesh -> FeatureAnalysis -> simplification`，特征检测不反向依赖简化。

光滑自由曲面可从 `feature::makeFeatureOptions(feature::FeatureProfile::SmoothSurface)` 开始；
该 profile 启用多尺度 `SmoothCurvature` 证据并关闭 `NormalTensor`。将预计算分析交给简化器时，
`WeightMode::NormalTensor` 和 `WeightMode::SmoothCurvature` 分别要求分析包含覆盖全部输入顶点的
`normalTensorVertexWeights` 或 `smoothCurvatureVertexWeights`；简化器会复用检测时的分数，不按另一套
简化参数重新检测或阈值化。

## PlainMesh 与 C ABI

`PlainMesh` 提供 `PlainVec3`、`PlainVec2`、三角面和逐角 UV 的 Eigen-free 交换格式；内部仍
转换为 `Mesh` 执行算法。需要 ABI 稳定性时使用 `CApi.h`：

1. 创建 `ManuMeshContext` 和 mesh handle；
2. 用 `manumesh_load_mesh` 或 `manumesh_mesh_set_data*` 填充输入；
3. 调用 `*_options_init_with_size` 初始化输入结构；
4. 调用 `manumesh_simplify_mesh_with_report_size` 或其他 size-aware 入口；
5. 对顶点、面、UV、特征边等变长数组先做容量查询再复制；统计和 report 结构则先用
   size-aware 初始化器并传入可写字节数；
6. 销毁 handle 和 context。

C ABI 的 `struct_size`/`abi_version` 允许尾部扩展；变长数组容量不足时不会部分写入，size-aware
报告/统计结构则按调用方提供的兼容前缀写入。旧的无容量符号由头文件兼容别名转到当前入口。异常不会越过 C 边界，失败通过
`ManuMeshStatus` 和 context 错误文本返回。

mesh handle 自带同步，可并发读写同一 handle；context 本身不保证线程安全，跨线程应使用
独立 context 或传 `nullptr`。销毁对象前必须等待所有在途调用结束。

## I/O 与纹理

`loadMesh`/`save*` 支持 STL 和 OBJ。OBJ 凹多边形使用投影 ear clipping，逐角 `vt` 保存在
`faceTexCoords`；重复、退化、自交 polygon 或混用纹理角会被拒绝。纹理感知简化是显式
`SimplifyConfig::texture.preserveTexture = true` 的 opt-in 功能，几何 quadric 仍为 4x4，
UV 只参与局部失真排序和 chart/面积合法性检查。CLI 默认关闭该选项。

## 安装后的 CMake

```cmake
find_package(ManuMesh CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE ManuMesh::manumesh)
```

纯 C consumer 链接 `ManuMesh::c_api`。安装示例和独立 consumer 位于 `examples/sdk_consumer/`，
用 `vs2019-release-sdk` 或 `vs2019-release-static-sdk` 验证。
