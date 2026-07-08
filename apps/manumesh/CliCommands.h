#pragma once

#include "CliArguments.h"

#include <map>
#include <string>

namespace manumesh::cli {

using CommandHandler = int (*)(const Args&);

const std::map<std::string, CommandHandler>& commandRegistry();

} // namespace manumesh::cli
