/**
 * @file apps/main.cpp
 * @brief Implements main facilities for the ManuMesh command-line application.
 * @ingroup manumesh_cli
 *
 * @details CLI parsing, validation, dispatch, and reporting are kept outside the geometry library so SDK behavior is independent of process-global command-line state.
 */

#include "ManuMeshCli.h"

#include <iostream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>

#include <shellapi.h>

namespace {

bool wideToUtf8(const wchar_t* value, std::string& output) {
    if (!value) {
        return false;
    }
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return false;
    }
    std::vector<char> buffer(static_cast<std::size_t>(size));
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, buffer.data(), size, nullptr, nullptr) <= 0) {
        return false;
    }
    output.assign(buffer.data(), static_cast<std::size_t>(size - 1));
    return true;
}

} // namespace

int main() {
    int argc = 0;
    wchar_t** wideArgv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wideArgv) {
        std::cerr << "error: failed to read the Windows command line\n";
        return 1;
    }

    std::vector<std::string> utf8Args(static_cast<std::size_t>(argc));
    bool converted = true;
    for (int i = 0; i < argc; ++i) {
        if (!wideToUtf8(wideArgv[i], utf8Args[static_cast<std::size_t>(i)])) {
            converted = false;
            break;
        }
    }
    LocalFree(wideArgv);
    if (!converted) {
        std::cerr << "error: command-line argument is not valid Unicode\n";
        return 1;
    }

    std::vector<char*> argv;
    argv.reserve(utf8Args.size());
    for (std::string& argument : utf8Args) {
        argv.push_back(argument.data());
    }
    return manumesh::cli::run(argc, argv.data());
}
#else
int main(int argc, char** argv) { return manumesh::cli::run(argc, argv); }
#endif
