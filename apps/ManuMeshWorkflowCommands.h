/**
 * @file apps/ManuMeshWorkflowCommands.h
 * @brief 声明演示和验证工作流的 CLI 命令处理函数。
 * @ingroup manumesh_cli
 *
 * @details 工作流命令负责组织输入模型、运行批处理并生成验证输出。
 */

#pragma once

#include "CliArguments.h"

namespace manumesh::cli::workflow_commands {

/// 运行内置的端到端演示工作流。
int demo(const Args& args);
/// 执行解析模型上的特征保护验收用例。
int validateFeatures(const Args& args);
/// 对调用方提供的外部网格执行数据集验证。
int validateExternal(const Args& args);

} // namespace manumesh::cli::workflow_commands
