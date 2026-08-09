/**
 * @file apps/ManuMeshCli.h
 * @brief 声明 CLI 入口及异常到进程退出码的转换。
 * @ingroup manumesh_cli
 *
 * @details 入口负责参数校验、命令分发和统一的错误显示。
 */

#pragma once

namespace manumesh::cli {

/// 校验全局参数，分发一个命令并转换异常。
/// @return 成功返回 0；用法或操作失败返回非零值。
int run(int argc, char** argv);

} // 命令行命名空间
