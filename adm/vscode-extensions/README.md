# VSCode Offline Extension Bundle

This directory contains VSIX packages that can be installed offline for this
C++/CMake project. The C++ workflow extensions retain VSCode 1.70.2
compatibility; the Simplified Chinese language pack matches VSCode 1.128.0.

| File | Extension | Version | `engines.vscode` | Role |
| --- | --- | --- | --- | --- |
| `ms-vscode.cmake-tools-1.19.52.vsix` | `ms-vscode.cmake-tools` | 1.19.52 | `^1.67.0` | CMake configure/build/debug workflow integration. |
| `vscodevim.vim-1.24.3.vsix` | `vscodevim.vim` | 1.24.3 | `^1.67.0` | Vim keybindings for VSCode. |
| `llvm-vs-code-extensions.vscode-clangd-0.1.34.vsix` | `llvm-vs-code-extensions.vscode-clangd` | 0.1.34 | `^1.65.0` | Legacy optional clangd package; this workspace now uses Microsoft's C/C++ extension instead. |
| `johnpapa.vscode-peacock-4.2.3.vsix` | `johnpapa.vscode-peacock` | 4.2.3 | `^1.49.0` | Workspace color identification. |
| `ms-ceintl.vscode-language-pack-zh-hans-1.128.2026071013.vsix` | `MS-CEINTL.vscode-language-pack-zh-hans` | 1.128.2026071013 | `^1.128.0` | Simplified Chinese language pack for VSCode 1.128.x. |

All packages except the version-matched language pack support VSCode 1.70.2.

Install from the repository root:

```powershell
code --install-extension adm\vscode-extensions\ms-vscode.cmake-tools-1.19.52.vsix
code --install-extension adm\vscode-extensions\vscodevim.vim-1.24.3.vsix
code --install-extension adm\vscode-extensions\johnpapa.vscode-peacock-4.2.3.vsix
code --install-extension adm\vscode-extensions\ms-ceintl.vscode-language-pack-zh-hans-1.128.2026071013.vsix
```

Checksums:

```text
6a27769b7dc73556b6ed252375440cc6e3524d4dfc4db87a9b7cff4b91ddf370  llvm-vs-code-extensions.vscode-clangd-0.1.34.vsix
4958ee5a3bd0a20b53ef84110f118956f957534df954c5ad2e20f3dfde544511  johnpapa.vscode-peacock-4.2.3.vsix
d7591f535eb168289f71ab99bd1ed7f7a25497f3b4f41315a7d1bdb8a899b236  ms-ceintl.vscode-language-pack-zh-hans-1.128.2026071013.vsix
0ec49507ccaa3a7b98b2ffbce755be158ec9f65da186b2fc727101a23751ee52  ms-vscode.cmake-tools-1.19.52.vsix
e1f3e8441b6d59513c6777b83ba62b91fd3888ebbd125b294053f9ec738f5409  vscodevim.vim-1.24.3.vsix
```
