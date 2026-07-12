#include "ManuMeshCli.h"

#include "CliArguments.h"
#include "CliCommands.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace manumesh::cli {
namespace {

void printUsage() {
    std::cout << "ManuMesh mesh simplification CLI\n\n"
              << "Commands:\n"
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
    std::cout << "\nGenerator types:\n"
              << "  plane, clustered-plane, hole-plane, ridge, noisy-plane,\n"
              << "  sine-terrain, terrace, bump, cylinder, torus, cube, thin-fin,\n"
              << "  stepped-shaft, pipe-coupling, pulley\n";
}

} // namespace

int run(int argc, char** argv) {
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

} // namespace manumesh::cli
