#include "SlimCli.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>

#include <shellapi.h>

namespace {

struct LocalMemoryDeleter {
    void operator()(wchar_t** memory) const noexcept {
        if (memory) {
            (void)LocalFree(memory);
        }
    }
};

bool toUtf8(const wchar_t* input, std::string& output) {
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return false;
    }
    std::vector<char> buffer(static_cast<std::size_t>(size));
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input, -1, buffer.data(), size, nullptr, nullptr) <= 0) {
        return false;
    }
    output.assign(buffer.data(), static_cast<std::size_t>(size - 1));
    return true;
}

} // namespace

int main() {
    int argc = 0;
    std::unique_ptr<wchar_t*, LocalMemoryDeleter> wideArguments(CommandLineToArgvW(GetCommandLineW(), &argc));
    if (!wideArguments) {
        std::cerr << "error: cannot read command line\n";
        return 1;
    }
    std::vector<std::string> arguments(static_cast<std::size_t>(argc));
    std::vector<const char*> argv;
    argv.reserve(arguments.size());
    for (int index = 0; index < argc; ++index) {
        if (!toUtf8(wideArguments.get()[index], arguments[static_cast<std::size_t>(index)])) {
            std::cerr << "error: command line contains invalid Unicode\n";
            return 1;
        }
        argv.push_back(arguments[static_cast<std::size_t>(index)].c_str());
    }
    return manumesh::slim::cli::run(argc, argv.data());
}
#else
int main(int argc, char** argv) { return manumesh::slim::cli::run(argc, argv); }
#endif
