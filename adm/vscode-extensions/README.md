# VSCode 离线扩展包

此目录包含可为本 C++/CMake 项目离线安装的 VSIX 包。C++ 工作流扩展保持与
VSCode 1.70.2 的兼容性；简体中文语言包对应 VSCode 1.128.0。

| 文件 | 扩展 | 版本 | `engines.vscode` | 作用 |
| --- | --- | --- | --- | --- |
| `ms-vscode.cmake-tools-1.19.52.vsix` | `ms-vscode.cmake-tools` | 1.19.52 | `^1.67.0` | CMake 配置/构建/调试工作流集成。 |
| `vscodevim.vim-1.24.3.vsix` | `vscodevim.vim` | 1.24.3 | `^1.67.0` | VSCode 的 Vim 按键绑定。 |
| `llvm-vs-code-extensions.vscode-clangd-0.1.34.vsix` | `llvm-vs-code-extensions.vscode-clangd` | 0.1.34 | `^1.65.0` | 旧版可选 clangd 包；当前工作区改用 Microsoft 的 C/C++ 扩展。 |
| `johnpapa.vscode-peacock-4.2.3.vsix` | `johnpapa.vscode-peacock` | 4.2.3 | `^1.49.0` | 工作区颜色标识。 |
| `ms-ceintl.vscode-language-pack-zh-hans-1.128.2026071013.vsix` | `MS-CEINTL.vscode-language-pack-zh-hans` | 1.128.2026071013 | `^1.128.0` | VSCode 1.128.x 的简体中文语言包。 |

除版本匹配的语言包外，所有软件包都支持 VSCode 1.70.2。

请在仓库根目录执行安装：

```powershell
code --install-extension adm\vscode-extensions\ms-vscode.cmake-tools-1.19.52.vsix
code --install-extension adm\vscode-extensions\vscodevim.vim-1.24.3.vsix
code --install-extension adm\vscode-extensions\johnpapa.vscode-peacock-4.2.3.vsix
code --install-extension adm\vscode-extensions\ms-ceintl.vscode-language-pack-zh-hans-1.128.2026071013.vsix
```

校验和：

```text
6a27769b7dc73556b6ed252375440cc6e3524d4dfc4db87a9b7cff4b91ddf370  llvm-vs-code-extensions.vscode-clangd-0.1.34.vsix
4958ee5a3bd0a20b53ef84110f118956f957534df954c5ad2e20f3dfde544511  johnpapa.vscode-peacock-4.2.3.vsix
d7591f535eb168289f71ab99bd1ed7f7a25497f3b4f41315a7d1bdb8a899b236  ms-ceintl.vscode-language-pack-zh-hans-1.128.2026071013.vsix
0ec49507ccaa3a7b98b2ffbce755be158ec9f65da186b2fc727101a23751ee52  ms-vscode.cmake-tools-1.19.52.vsix
e1f3e8441b6d59513c6777b83ba62b91fd3888ebbd125b294053f9ec738f5409  vscodevim.vim-1.24.3.vsix
```
