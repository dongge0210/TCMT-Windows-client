# project-monitor-TC

本项目遵循 [GNU General Public License v3.0 (GPL-3.0)](https://www.gnu.org/licenses/gpl-3.0.html) 开源协议。

## 此项目使用的子仓库

本项目使用了以下仓库：

- [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor)  
  - **协议**: [Mozilla Public License v2.0 (MPL-2.0)](https://mozilla.org/MPL/2.0/)  
  - **使用方式**: 本项目直接使用了 LibreHardwareMonitor 的 `net8` 版本编译后的库（动态链接库 DLL 文件），未对其源代码进行修改。  
  - **源代码获取**: 您可以在其 [GitHub 仓库](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor) 中找到完整的源代码及其协议文件。

- [curl](https://github.com/curl/curl)  
  - **协议**: 没有  
  - **使用方式**: 本项目将其作为 Git 子模块引入，用于处理 URL 数据传输。  
  - **源代码获取**: 您可以在其 [GitHub 仓库](https://github.com/curl/curl) 中找到完整的源代码及其协议文件。

- [nlohmann/json](https://github.com/nlohmann/json)  
  - **协议**: [MIT License](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT)  
  - **使用方式**: 本项目将其作为 Git 子模块引入，用于 JSON 数据处理。  
  - **源代码获取**: 您可以在其 [GitHub 仓库](https://github.com/nlohmann/json) 中找到完整的源代码及其协议文件。

- [openssl](https://github.com/openssl/openssl)  
  - **协议**: [Apache License 2.0](https://github.com/openssl/openssl/blob/master/LICENSE.txt)  
  - **使用方式**: 本项目将其作为 Git 子模块引入，用于 TLS/SSL 和加密功能。  
  - **源代码获取**: 您可以在其 [GitHub 仓库](https://github.com/openssl/openssl) 中找到完整的源代码及其协议文件。

- [PDCurses](https://github.com/wmcbrine/PDCurses)  
  - **协议**: 没有  
  - **使用方式**: 本项目将其作为 Git 子模块引入，用于终端界面处理。  
  - **源代码获取**: 您可以在其 [GitHub 仓库](https://github.com/wmcbrine/PDCurses) 中找到完整的源代码及其协议文件。

- [zlib](https://github.com/madler/zlib)  
  - **协议**: 其他  
  - **使用方式**: 本项目将其作为 Git 子模块引入，用于数据压缩。  
  - **源代码获取**: 您可以在其 [GitHub 仓库](https://github.com/madler/zlib) 中找到完整的源代码及其协议文件。
