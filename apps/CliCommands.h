/**
 * @file apps/CliCommands.h
 * @brief 声明命令注册表及其处理函数类型。
 * @ingroup manumesh_cli
 *
 * @details 注册表由帮助文本和命令分发逻辑共享，确保两者使用同一命令集合。
 */

#pragma once

#include "CliArguments.h"

#include <map>
#include <string>

namespace manumesh::cli {

using CommandHandler = int (*)(const Args&); ///< 处理一组已解析参数并返回进程退出码。

/// @return 用于帮助文本和分发的稳定命令名到处理函数映射。
const std::map<std::string, CommandHandler>& commandRegistry();

} // 命令行命名空间
