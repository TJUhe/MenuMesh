/**
 * @file apps/CliArguments.h
 * @brief 声明命令行参数解析、校验和取值辅助函数。
 * @ingroup manumesh_cli
 *
 * @details 参数表同时驱动帮助文本、选项归属校验和严格的数值解析。
 */

#pragma once

#include <string>
#include <vector>

namespace manumesh::cli {

struct Args {
    std::vector<std::string> values; ///< 不含可执行文件名的原始参数令牌。
};

/// @return `args` 中存在精确开关或值选项时返回 true。
bool hasFlag(const Args& args, const std::string& name);
/// @return 任意命令注册了 `value` 时返回 true。
bool isKnownFlag(const std::string& value);
/// @return 已注册选项需要消费后续令牌时返回 true。
bool takesValue(const std::string& value);
/// 校验所有选项是否属于 `command`，并确认值选项都有对应值。
/// @throws 选项属于其他命令或缺少值时抛出 `std::invalid_argument`。
void validateArgsForCommand(const std::string& command, const Args& args);
/// @return 根据同一校验表生成的分组帮助文本。
std::string optionsHelpText();
/// 返回选项值；选项缺失时返回调用方提供的默认值。
std::string getArg(const Args& args, const std::string& name, const std::string& defaultValue = "");
/// 解析完整的十进制整数令牌；失败时抛出带选项名的诊断。
int parseIntStrict(const std::string& value, const std::string& name);
/// 解析完整且有限的浮点令牌；失败时抛出带选项名的诊断。
double parseDoubleStrict(const std::string& value, const std::string& name);
/// 获取并严格解析整数选项。
int getIntArg(const Args& args, const std::string& name, int defaultValue);
/// 获取并严格解析浮点选项。
double getDoubleArg(const Args& args, const std::string& name, double defaultValue);
/// @return 未被注册选项或其值消费的剩余令牌。
std::vector<std::string> positionalArgs(const Args& args);
/// 解析逗号分隔的有限权重列表。
std::vector<double> parseWeights(const std::string& text);
/// 解析逗号分隔的正整数面数列表。
std::vector<int> parseFaceCounts(const std::string& text);

} // 命令行参数命名空间
