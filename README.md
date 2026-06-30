# Line Quadrics QEM Reproduction

Industrial library work is documented in
[`docs/industrial_library.md`](E:/code/codex/line-quadrics-qem/docs/industrial_library.md).
This branch builds the algorithm as a cross-platform shared library with a
separate CLI, external-style example program, GoogleTest/CTest coverage,
clang-format targets, and Doxygen API documentation.

这是基于 `Controlling Quadric Error Simplification with Line Quadrics`（Hsueh-Ti Derek Liu, Mehdi Rahimzadeh, Victor Zordan, SGP 2025）的 C++/Eigen 复现项目。目标不是做一个完整工业级网格库，而是把论文里最关键的算法机制做成可跑、可观察、可替换 STL 的实验程序。

## 论文核心到代码映射

标准 QEM 对每个三角面构造平面 quadric：

```text
Q_plane = [n, -n dot p] [n, -n dot p]^T
```

代码位置：[src/QEMSimplifier.cpp](E:/code/codex/line-quadrics-qem/src/QEMSimplifier.cpp)

- `planeQuadric(...)`：平面 quadric。
- `computeInitialQuadrics(...)`：按三角形面积的 1/3 累加到顶点 quadric，对应论文中的 barycentric vertex area。
- `solveOptimal(...)`：求解 `A x = -b`，奇异时退回端点/中点候选。

论文的 line quadric 是过顶点、方向平行于顶点法线的直线到点距离。实现上用 Gram-Schmidt 找到两个与法线正交的单位方向 `x_i, y_i`，然后把两张过该直线的平面 quadric 相加：

```text
Q_line = Q_x + Q_y
Q_augmented_i = Q_i + w_i * area_i * Q_line_i
```

代码位置：

- `lineQuadric(...)`：构造 `Q_x + Q_y`。
- `computeInitialQuadrics(...)`：实现 `Q_i + w_i a_i Q_i^l`。
- `computeFeatureScores(...)`：提供 uniform、dihedral、height、xband 几种权重模式，用来观察论文中的软特征保留/自适应简化思想。

## 构建

```powershell
cd E:\code\codex\line-quadrics-qem
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target linequadrics
```

如果系统没有 Eigen，CMake 会自动下载 Eigen 3.4.0。Eigen 是 header-only 依赖。

## VSCode 推荐入口

本仓库现在优先按 VSCode 使用配置，根目录 PowerShell 脚本已经清理掉。打开工程后建议安装 C/C++ 和 CMake Tools 扩展，然后从 `Terminal > Run Task...` 选择：

- `build: mingw+ninja release`
- `build: mingw makefiles release`
- `build: msvc vs2022 release`
- `build: msvc+ninja release`
- `run: quick viewer data`
- `viewer: start`

三套工具链（MinGW、Ninja、MSVC）都在 `CMakePresets.json` 和 `.vscode/tasks.json` 中配置好了。Windows 7 / 4GB 内存机器优先用 `mingw-makefiles-release`，CMake 建议 3.20.x 到 3.25.x，不建议把 CMake 4.x 作为 Win7 基线。详细说明见：[docs/vscode_setup.md](E:/code/codex/line-quadrics-qem/docs/vscode_setup.md)。

## Windows 7 / 4GB 内存 / MinGW 离线运行

老机器上建议只运行 C++ 命令行程序，不建议运行 Three.js viewer。viewer 依赖现代 Node/Vite/浏览器环境，Windows 7 上兼容性和内存都比较吃紧；把生成的 STL 拷到另一台机器或用 MeshLab/轻量 STL 查看器看会更稳。

推荐工具链：

- MinGW-w64：推荐 `GCC 10.5.0 x86_64 POSIX SEH MSVCRT`。本分支不再把安装包放进 `third_party`，需要自行准备或从 WinLibs 等来源下载。
- Eigen：推荐 `3.4.0`。本分支不再把 Eigen 压缩包放进 `third_party`；可以安装到 `C:\libs\eigen-3.4.0`，或让 CMake 在线 FetchContent 下载。
- CMake：推荐 `3.20.x` 到 `3.25.x` 的 Windows zip/installer；如果 CMake 在 Win7 上不好装，可以直接用下面的 `g++` 命令编译。

为什么推荐这个 MinGW 版本：

- 当前代码使用 C++17 和 `std::filesystem`。GCC 10.5 对 `std::filesystem` 比 GCC 8.x 更省心，通常不需要额外链接 `-lstdc++fs`。
- 选择 MSVCRT 版 WinLibs 是为了兼容老 Windows；新版 MSYS2 runtime 已不适合作为 Windows 7 首选环境。
- 如果是离线老机器，建议在新机器上提前下载好 MinGW 和 Eigen，再用 U 盘拷过去。

安装步骤：

1. 解压你自行准备的 MinGW-w64：

   ```text
   winlibs-x86_64-posix-seh-gcc-10.5.0-mingw-w64msvcrt-11.0.1-r2.7z
   ```

   例如解压到：

   ```text
   C:\tools\mingw64
   ```

2. 把 `C:\tools\mingw64\bin` 加入 `PATH`，或在命令行临时设置：

   ```bat
   set PATH=C:\tools\mingw64\bin;%PATH%
   ```

3. 解压你自行准备的 Eigen：

   ```text
   eigen-3.4.0.tar.gz
   ```

   例如解压到：

   ```text
   C:\libs\eigen-3.4.0
   ```

用 CMake + MinGW 构建：

```bat
cd C:\path\to\line-quadrics-qem
cmake -G "MinGW Makefiles" -S . -B build-mingw -DCMAKE_BUILD_TYPE=Release -DEigen3_DIR=C:\libs\eigen-3.4.0\share\eigen3\cmake
cmake --build build-mingw -- -j1
```

如果 `Eigen3_DIR` 不好用，可以直接跳过 CMake，使用单条 `g++` 命令：

```bat
g++ -std=c++17 -O2 -DNDEBUG ^
  -Isrc -IC:\libs\eigen-3.4.0 ^
  src\main.cpp src\FeatureDetection.cpp src\Mesh.cpp src\MeshGenerators.cpp src\Metrics.cpp src\QEMSimplifier.cpp ^
  -o linequadrics.exe
```

如果旧版 GCC 报 `std::filesystem` 链接错误，再在末尾加：

```bat
-lstdc++fs
```

Windows 7 / 4GB 内存建议先跑小案例：

```bat
linequadrics.exe generate --type flange --n 36 --out flange.stl
linequadrics.exe simplify flange.stl flange_simplified.stl --method line --ratio 0.2 --line-weight 1e-3 --samples 500
linequadrics.exe face-sweep flange.stl out_flange --faces "1000,800,600,400,200,100" --samples 500
```

注意事项：

- 4GB 内存机器上构建用 `-j1`，不要并行编译。
- 不建议在老机器上跑完整 viewer demo，先用 `--n 36` 或 `--n 48` 的几何验证。
- `--samples` 默认是 3000；老机器可以降到 500，只影响距离指标精度，不影响简化 STL 输出。
- 本分支已经移除 `third_party`，仓库不再附带 MinGW/Eigen 安装包。

## 快速复现实验

```powershell
.\build\mingw-ninja-release\linequadrics.exe demo --samples 1000
```

脚本会生成多类 STL：

- `clustered_plane.stl`：平面但采样不均，最能暴露标准 QEM 在平面区域的奇异退化。
- `hole_plane.stl`：带内外边界的平面，用来观察边界 quadric 与 line quadrics 的互补关系。
- `ridge.stl`：带一条折脊，用来看三角质量与特征保留的取舍。
- `noisy_plane.stl`：带噪平面，用来看 line quadrics 不是去噪器。
- `sine_terrain.stl`：连续曲率起伏，用来看权重变大时均匀性与几何误差的交换。
- `terrace.stl`：阶梯状硬边/近 CAD 地形，用来看 dihedral 权重是否保留折线。
- `bump.stl`：软凸起和弱特征，用来看 height 权重的自适应采样。
- `cylinder.stl`：光滑曲面加边界，用来看 line quadrics 对非平面曲率面的正则化。
- `torus.stl`：闭合光滑曲面，无边界，用来看普通曲面上的权重 sweep。
- `cube.stl`：硬边 STL，可用来测试 dihedral 权重。
- `thin_fin.stl`：薄片结构，用来看软约束和拓扑保护不足时的风险。

输出在 `examples/output/**`，每个 sweep 目录都有 `metrics.csv` 和若干简化后的 STL。

## 演示案例地图

| 输入 STL | 输出目录 | 主要观察点 | 推荐先看 |
| --- | --- | --- | --- |
| `clustered_plane.stl` | `clustered_plane` | 平面零误差导致标准 QEM 随机/病态折叠；line quadrics 让点分布更均匀 | `mean_triangle_quality`, `edge_length_cv` |
| `clustered_plane.stl` | `clustered_plane_boundary` | 加边界 quadric 后，仍能看到内部均匀性收益 | `min_triangle_quality`, `edge_length_cv` |
| `hole_plane.stl` | `hole_plane_boundary` | 内外边界是否被保持；边界保护与 line quadrics 不是同一件事 | `boundary_edges`, `non_manifold_edges` |
| `ridge.stl` | `ridge_uniform` | 小权重改善三角质量，高权重可能开始牺牲尖锐脊线 | 低/高 `line_weight` 对比 |
| `ridge.stl` | `ridge_dihedral` | 用二面角启发式给折脊附近更高权重，模拟论文里的软特征保留 | 输出 STL 外观 |
| `noisy_plane.stl` | `noisy_plane` | 噪声法线下 line quadrics 不是去噪器 | `mean_orig_to_simp`, 外观 |
| `sine_terrain.stl` | `sine_terrain` | 光滑曲面上从几何保真到均匀采样的连续取舍 | `line_weight=1e-3` 和 `1e-1` |
| `terrace.stl` | `terrace_uniform` / `terrace_dihedral` | 单一阈值二面角能保硬边，但对软/弱特征有限 | 两个目录对比 |
| `bump.stl` | `bump_height` | 自适应权重会把采样留给高处/重要区域，但可能增加误差 | `mean_orig_to_simp` |
| `cylinder.stl` | `cylinder` | 有边界的光滑曲面，边界 quadric 与 line quadrics 联合作用 | `boundary_edges` |
| `torus.stl` | `torus` | 无边界光滑闭曲面，观察 line quadrics 的纯正则化行为 | `edge_length_cv` |
| `cube.stl` | `cube_dihedral` | CAD/STL 硬边，二面角权重比 uniform 更能表达“重要边” | 输出 STL 外观 |
| `thin_fin.stl` | `thin_fin_uniform` / `thin_fin_dihedral` | 薄结构容易暴露拓扑保护不足，这是当前复现的风险案例 | `non_manifold_edges`, 外观 |

## 换自己的 STL

更详细的“论文建议如何映射到程序参数和 viewer 观察方式”见：[docs/paper_guided_usage.md](E:/code/codex/line-quadrics-qem/docs/paper_guided_usage.md)。

标准 QEM：

```powershell
.\build\Release\linequadrics.exe simplify input.stl output_standard.stl --method standard --ratio 0.15
```

论文默认风格的小权重 line quadrics：

```powershell
.\build\Release\linequadrics.exe simplify input.stl output_line.stl --method line --ratio 0.15 --line-weight 1e-3
```

对 CAD/STL 硬边更友好的 dihedral 自适应权重：

```powershell
.\build\Release\linequadrics.exe simplify input.stl output_feature.stl --ratio 0.15 --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 30
```

扫多个权重并输出 CSV：

```powershell
.\build\Release\linequadrics.exe sweep input.stl output_dir --ratio 0.15 --weights "0,1e-5,1e-4,1e-3,1e-2,1e-1"
```

如果你想看“网格简化程度”本身，而不是同一目标面数下的质量差异，用 ratio sweep：

```powershell
.\build\Release\linequadrics.exe ratio-sweep input.stl output_ratio_dir --method line --line-weight 1e-3 --ratios "0.8,0.5,0.25,0.15,0.08,0.05"
```

`sweep` 改的是 line quadric 权重，目标 faces 基本固定；`ratio-sweep` 改的是目标面数比例，所以 faces 会明显一档档下降。

## 指标怎么看

`metrics.csv` 中最有用的列：

- `mean_triangle_quality`：越接近 1 越接近等边三角形。
- `min_triangle_quality`：很低时通常说明出现细长/退化三角形。
- `edge_length_cv`：边长变异系数，越低说明采样越均匀。
- `mean_orig_to_simp` / `max_orig_to_simp`：采样点到简化网格的距离，越低几何误差越小。
- `non_manifold_edges`：非流形边数量。这个复现不是拓扑严格保护算法，所以这个值用于提醒风险。

本机 demo 的典型现象：

- `clustered_plane` 中标准 QEM 的 `mean_triangle_quality` 约为 `0.62`，`edge_length_cv` 约为 `2.63`；加入 `1e-5` 级别 line quadrics 后，质量升到约 `0.84`，边长 CV 降到约 `0.23`。
- `ridge` 中标准 QEM 的 `mean_triangle_quality` 约为 `0.41`，line quadrics 可升到 `0.87` 到 `0.88`。
- `noisy_plane` 中 line quadrics 的提升很小，而且高权重会增加对输入噪声法线的依赖。这与论文结论一致：line quadrics 不是 denoising 技术。

## 可视化窗口

先生成 demo STL 和总表：

```powershell
.\build\mingw-ninja-release\linequadrics.exe demo --samples 1000
```

启动 Three.js viewer：

```powershell
pnpm install
pnpm run viewer
```

然后打开：

```text
http://127.0.0.1:5174/viewer/
```

窗口会读取 `examples/output/demo_summary.csv`，自动列出所有案例和权重结果。你可以切换 `Case`、`Result`，用滑条逐个看标准 QEM 和 line quadrics 的变化；`Original` 会叠加原始 STL，`Wire` 会显示简化网格线框，`Cycle` 会自动轮播同一案例下的不同权重。

## 我观察到的短板

1. Line quadrics 依赖顶点法线。输入网格噪声大、法线乱时，它会尊重这些错误法线，因此不能替代双边滤波、normal voting 或其他去噪/稳健法线估计。
2. 权重太大时会把“均匀顶点分布”压过几何保真，尤其在尖锐特征或高曲率区域可能牺牲外形。论文建议默认小权重，例如 `1e-3`，通常是合理起点。
3. 对 STL 这种无语义属性的数据，所谓“重要顶点”只能由 dihedral、空间位置或用户外部标注猜测。没有 skinning weights、feature labels 时，软特征保留只是启发式。
4. 本复现的边折叠拓扑保护很轻量，主要服务算法观察。用于生产前应加入 link condition、自交检测、边界策略和更强的非流形约束。
5. Line quadrics 解决的是标准 QEM 在平面区域的零误差/奇异系统问题，并改善均匀性；它不是 Hausdorff 误差有界简化，也不会自动保证目标应用中的最优外观或物理仿真质量。

## 相关背景

本地 mesh-feature-literature 资料库对简化/保特征方法的归纳也支持这个取舍：特征保留型简化通常是在 QEM/边折叠成本中加入特征敏感权重、saliency 或约束，但主要风险是保特征与三角质量、拓扑质量互相拉扯。对应资料库中的 simplification anchors 包括 005、037、048、050、072 等。

## Curve feature constraints branch

This branch adds an experimental circular-feature preservation path for CAD/STL
parts:

```powershell
.\build\Release\linequadrics.exe simplify input.stl output_curve.stl `
  --method line `
  --ratio 0.20 `
  --line-weight 1e-3 `
  --weight-mode dihedral `
  --feature-angle-deg 25 `
  --preserve-feature-curves `
  --feature-curve-weight 0.08 `
  --circle-fit-threshold 0.04 `
  --min-feature-loop-vertices 16
```

It adds `feature-report` and `feature-compare` commands, plus native
`validate-features` and `validate-external` workflows for generated industrial
cases and OBJ benchmark models. The
detailed explanation, literature anchors, and directional error results are in
[`docs/feature_curve_experiment.md`](E:/code/codex/line-quadrics-qem/docs/feature_curve_experiment.md).
External-model findings are recorded in
[`docs/external_model_validation.md`](E:/code/codex/line-quadrics-qem/docs/external_model_validation.md).

## 可选外部几何来源

默认 demo 使用程序生成几何，保证离线可复现。如果你想加入真实/经典模型，可以从这些来源挑选后转成 STL 再跑：

- [common-3d-test-models](https://github.com/alecjacobson/common-3d-test-models)：收集 bunny、fandisk、Suzanne 等常见 OBJ 测试模型，仓库说明会保留已知原始来源。
- [Stanford 3D Scanning Repository](https://graphics.stanford.edu/data/3Dscanrep/)：经典扫描模型，例如 Bunny、Dragon、Happy Buddha；通常面数较高，本复现的简化器是教学实现，建议先降采样或选较小模型。
- [Thingi10K](https://github.com/Thingi10K/Thingi10K)：真实 3D 打印 STL 数据集，适合测试非流形、薄壁、自交、真实三角汤等脏输入；注意每个模型许可不同，复用前要查对应 license 字段。
