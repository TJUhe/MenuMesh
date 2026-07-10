#pragma once

#include "CliArguments.h"

namespace manumesh::cli::workflow_commands {

int demo(const Args& args);
int validateFeatures(const Args& args);
int validateExternal(const Args& args);

} // namespace manumesh::cli::workflow_commands
