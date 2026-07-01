# VSCode 1.70.2 C++ Extension Bundle

This directory contains VSIX packages that can be installed offline on
VSCode 1.70.2 for this C++/CMake project.

| File | Extension | Version | `engines.vscode` | Role |
| --- | --- | --- | --- | --- |
| `llvm-vs-code-extensions.vscode-clangd-0.1.34.vsix` | `llvm-vs-code-extensions.vscode-clangd` | 0.1.34 | `^1.65.0` | C/C++ language server integration: completion, go-to-definition, diagnostics. |
| `ms-vscode.cmake-tools-1.19.52.vsix` | `ms-vscode.cmake-tools` | 1.19.52 | `^1.67.0` | CMake configure/build/debug workflow integration. |

Both engine ranges include VSCode 1.70.2.

Install from the repository root:

```powershell
code --install-extension thirdParty\vscode-extensions\llvm-vs-code-extensions.vscode-clangd-0.1.34.vsix
code --install-extension thirdParty\vscode-extensions\ms-vscode.cmake-tools-1.19.52.vsix
```

Checksums:

```text
6a27769b7dc73556b6ed252375440cc6e3524d4dfc4db87a9b7cff4b91ddf370  llvm-vs-code-extensions.vscode-clangd-0.1.34.vsix
0ec49507ccaa3a7b98b2ffbce755be158ec9f65da186b2fc727101a23751ee52  ms-vscode.cmake-tools-1.19.52.vsix
```
