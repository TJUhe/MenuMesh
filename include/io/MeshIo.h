/**
 * @file include/io/MeshIo.h
 * @brief 声明 ManuMesh 网格 I/O 模块的网格读写设施。
 * @ingroup manumesh_io
 *
 * @details 导入和导出路径在具体化或序列化三角几何前校验索引和数值。
 */

#pragma once

#include "Export.h"
#include "core/Mesh.h"

#include <string>

namespace manumesh {

/// 加载 STL 文件，自动处理 ASCII 和二进制编码。
///
/// 大小超过精确 `84 + 50 * n` 记录布局的二进制文件也会接受，并忽略尾部填充字节。
/// 由于 STL 不携带 UV，`mesh` 中已有的纹理坐标会被清除。函数返回 false 时，`mesh` 内容未定义。
/// @param[in] path UTF-8 输入文件路径。
/// @param[out] mesh 成功时替换为解析后的几何。
/// @param[out] error 可选诊断信息，成功时清空。
/// @param[in] mergeRelativeEpsilon 重合顶点合并相对容差；零值只使用数值下限。
/// @return 完整解析和校验成功时为 true；失败时 `mesh` 内容未定义。
MANUMESH_API bool
loadStl(const std::string& path, Mesh& mesh, std::string* error = nullptr, double mergeRelativeEpsilon = 1e-9);
/// 加载 OBJ 多边形网格，将面三角化并保留逐角 `vt` 坐标。
///
/// 严格凸多边形保持确定性的扇形三角化。凹多边形使用投影耳切法；重复、退化或自交的多边形面会被拒绝，
/// 而不是生成无效三角形。顶点法向（`vn`）和未知指令会被忽略。函数返回 false 时，`mesh` 内容未定义。
/// @param[in] path UTF-8 输入文件路径。
/// @param[out] mesh 成功时替换为三角化后的 OBJ 几何。
/// @param[out] error 可选诊断信息，成功时清空。
/// @return 完整解析和三角化成功时为 true。
MANUMESH_API bool loadObj(const std::string& path, Mesh& mesh, std::string* error = nullptr);
/// 根据文件扩展名加载网格。支持 STL 和 OBJ 格式。
///
/// 函数返回 false 时，`mesh` 内容未定义。
/// @param[in] path UTF-8 输入路径；不区分大小写的扩展名决定加载器。
/// @param[out] mesh 成功时替换。
/// @param[out] error 可选诊断信息。
/// @param[in] mergeRelativeEpsilon STL 重合顶点合并容差；OBJ 忽略此参数。
/// @return 文件格式受支持且有效时为 true。
MANUMESH_API bool
loadMesh(const std::string& path, Mesh& mesh, std::string* error = nullptr, double mergeRelativeEpsilon = 1e-9);
/// 将网格写为 ASCII STL 文件。
///
/// ASCII STL 只存储几何，因此不会写入纹理坐标。当网格无效或文件无法创建/完整写入（例如磁盘已满）时，
/// 返回 false 并设置 `error`。
/// @param[in] path UTF-8 目标路径。
/// @param[in] mesh 严格有效的三角网格。
/// @param[in] solidName ASCII STL 实体标签。
/// @param[out] error 可选的校验或 I/O 诊断信息。
/// @return 仅在完整文件成功写入后返回 true。
MANUMESH_API bool saveAsciiStl(
    const std::string& path, const Mesh& mesh, const std::string& solidName = "mesh", std::string* error = nullptr
);
/// 将网格写为标准小端二进制 STL 文件。
///
/// 二进制 STL 将每个坐标存为 IEEE-754 float32，最多存储 UINT32_MAX 个三角形。不写入纹理坐标。
/// 当网格无效、坐标超出 float32 范围或文件无法创建/完整写入时返回 false。
/// @param[in] path UTF-8 目标路径。
/// @param[in] mesh 严格有效的三角网格。
/// @param[out] error 可选的校验或 I/O 诊断信息。
/// @return 仅在完整的 `84 + 50 * faceCount` 字节文件写入后返回 true。
MANUMESH_API bool saveBinaryStl(const std::string& path, const Mesh& mesh, std::string* error = nullptr);

} // namespace manumesh
