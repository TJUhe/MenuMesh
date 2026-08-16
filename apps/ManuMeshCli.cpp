/**
 * @file apps/ManuMeshCli.cpp
 * @brief 解析顶层 CLI 命令并统一处理帮助、版本和错误退出。
 * @ingroup manumesh_cli
 *
 * @details 入口只负责解析顶层命令；具体参数校验和业务操作由命令模块完成。
 */

#include "ManuMeshCli.h"

#include "CliArguments.h"
#include "CliCommands.h"
#include "api/CApi.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace manumesh {
namespace cli {
namespace {

void printUsage() {
    std::cout << "ManuMesh 网格简化命令行工具\n\n"
              << "命令：\n"
              << "  manumesh generate --type clustered-plane --n 50 --out input.stl\n"
              << "  manumesh simplify input.stl output.stl [options]\n"
              << "  manumesh compare original.stl simplified.stl [--samples 3000]\n"
              << "  manumesh feature-report input.stl [--csv report.csv]\n"
              << "  manumesh feature-benchmark input.stl labels.csv [--csv report.csv]\n"
              << "  manumesh feature-compare original.stl simplified.stl [--csv report.csv]\n"
              << "  manumesh sweep input.stl out_dir [options]\n\n"
              << "  manumesh ratio-sweep input.stl out_dir [options]\n\n"
              << "  manumesh face-sweep input.stl out_dir [options]\n\n"
              << "  manumesh demo [--quick] [--samples N]\n\n"
              << "  manumesh summarize-metrics [output_root] [summary.csv]\n\n"
              << "  manumesh validate-features [--ratio R] [--samples N] "
                 "[--input-dir dir] [--output-dir dir]\n\n"
              << "  manumesh validate-external [--input-dir dir] [--ratio R] "
                 "[--output-dir dir]\n";
    std::cout << optionsHelpText();
    std::cout << "\n版本选项：\n"
              << "  --version                             显示 ManuMesh 版本\n";
    std::cout << "\n生成器类型：\n"
              << "  plane, clustered-plane, hole-plane, ridge, noisy-plane,\n"
              << "  sine-terrain, terrace, bump, cylinder, torus, cube, thin-fin,\n"
              << "  stepped-shaft, pipe-coupling, pulley\n";
}

} // 命名空间

int run(int argc, const char* const* argv) {
    try {
        if (argc < 2) {
            printUsage();
            return 0;
        }

        const std::string command = argv[1];
        Args args;
        for (int i = 2; i < argc; ++i) {
            args.values.emplace_back(argv[i]);
        }

        if (command == "--help" || command == "-h" || command == "help") {
            printUsage();
            return 0;
        }
        if (command == "--version" || command == "version") {
            std::cout << "ManuMesh " << manumesh_version() << "\n";
            return 0;
        }

        const auto& commands = commandRegistry();
        const auto it = commands.find(command);
        if (it == commands.end()) {
            throw std::invalid_argument("Unknown command: " + command);
        }

        validateArgsForCommand(command, args);
        return it->second(args);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n\n";
        printUsage();
        return 1;
    } catch (...) {
        std::cerr << "error: unknown fatal error\n\n";
        printUsage();
        return 1;
    }
}

} // namespace cli
} // namespace manumesh
