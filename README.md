# GeekCOM - 串口调试工具

![](./docs/images/GeekCOM_banner.png)

跨平台串口调试工具。`feat/tauri-react-rust` 分支新增 **Tauri 2 + React + Rust** 桌面实现，用 GeekCOM 验证新技术栈；原 Qt6 / C++ 实现保留用于对照。

## Tauri 版本（当前分支）

已迁移串口配置、文本 / HEX 收发、行尾与发送预览、接收保存、文件与周期发送、计数、交互终端和主题；新增可隐藏的左侧配置、工具栏以及 HEX / ASCII 对照视图。

```bash
# Ubuntu 24.04：先安装 Node.js 22.12+、Rust stable，再安装桌面开发依赖
sudo apt install build-essential pkg-config libudev-dev libwebkit2gtk-4.1-dev libxdo-dev libssl-dev libayatana-appindicator3-dev librsvg2-dev
npm ci
npm run tauri dev

# Linux 安装包
npm run tauri build -- --bundles deb
```

`npm run dev` 仅启动浏览器预览，实际串口和文件操作需要 Tauri 桌面进程。Windows / macOS 的系统准备参见 [Tauri 官方说明](https://v2.tauri.app/start/prerequisites/)。本次完成 Linux 桌面验证，其他平台仍需原生构建与设备测试。

### Linux 开发模式提示 Too many open files

如果 `npm run tauri dev` 在 `tauri-cli` 创建监听器时崩溃，先检查两类额度：

```bash
ulimit -n
cat /proc/sys/fs/inotify/max_user_instances
```

当文件描述符额度充足而 inotify 实例额度耗尽时，增加 `ulimit` 或 `max_user_watches` 不会解决实例不足。关闭不使用的开发服务/编辑器，或临时提高每用户实例额度：

```bash
sudo sysctl -w fs.inotify.max_user_instances=1024
npm run tauri dev
```

该设置重启后恢复；如确需长期调整，可将 `fs.inotify.max_user_instances = 1024` 写入 `/etc/sysctl.d/99-geekcom-inotify.conf` 并执行 `sudo sysctl --system`。程序不会自行修改系统参数。

只需测试串口功能和性能时，直接运行 Release 版，不启动开发监听器：

```bash
# 已有最新构建时直接启动
./src-tauri/target/release/geekcom

# 需要重新构建时，绕过 Tauri CLI 的开发监听流程
npm run build
cargo build --release --manifest-path src-tauri/Cargo.toml --features tauri/custom-protocol
./src-tauri/target/release/geekcom
```

inotify 的实例限制与 `EMFILE` 含义见 [Linux 手册](https://man7.org/linux/man-pages/man2/inotify_init.2.html)。

### 连接与接收控制

发送计数表示操作系统实际接受的字节，不代表设备已收到。短写会报告已提交/请求字节数，不自动重发剩余内容；周期或文件发送遇到写入失败会停止任务并关闭连接。

- 串口设备使用同一个可编辑选框：直接输入端口名，或展开列表选择。列表仅显示 `/dev/ttyXXX`、`COMx` 等端口名；刷新重新枚举设备，不显示常驻数量提示，仅失败时提示。选框尺寸不随内容变化。
- “暂停滚动 / 恢复滚动”只控制接收区的自动滚动，串口仍继续接收。未连接时按钮禁用，每次连接或断开都会重置滚动状态。
- 接收区仅渲染可见记录及附近缓冲，历史仍按最多 4,000 条 / 2 MiB 保留；滚动可查看保留记录，保存会导出全部保留内容，不限于当前屏幕。
- 断开会关闭串口、停止发送任务，并丢弃尚未显示的缓冲；已显示记录保留。断开前发出但延迟返回的数据请求不会再追加记录或恢复旧的连接状态。

### 配色方案

配色以 `ui/src/styles.css` 的 CSS 变量为准。深色和浅色主题共用品牌主色，日志区与终端保持深色。

| 品牌色 Token | 色值 | 用途 |
| --- | --- | --- |
| `--accent` | `#E85552` | Logo 底色、主按钮、品牌文字、选中态、焦点边框、复选框及终端光标 |
| `--accent-hover` | `#D94B48` | 主按钮悬停 |
| `--accent-active` | `#C6413E` | 主按钮按下 |

主按钮使用白色文字；禁用按钮不透明度为 `0.38`。已连接时的断开按钮使用中性色，连接状态由绿色指示灯表达。

| 界面 Token | 深色主题 | 浅色主题 |
| --- | --- | --- |
| `--bg` 页面背景 | `#171A20` | `#F3F5F7` |
| `--panel` 面板 | `#1D2128` | `#FFFFFF` |
| `--raised` 凸起区域 | `#252A33` | `#EDF0F4` |
| `--field` 输入区域 | `#181C23` | `#F7F8FA` |
| `--border` 边框 | `#303640` | `#DBE0E7` |
| `--text` 正文 | `#D9DFE7` | `#253140` |
| `--muted` 次要文字 | `#87929F` | `#637185` |
| `--hover` 中性色悬停背景 | `#2B313C` | `#E5EAF0` |

| 固定配色 | 色值 |
| --- | --- |
| 日志与终端背景 | `#111419` |
| 接收日志文字 | `#D5DEE8` |
| 终端文字 | `#D7E0E9` |
| 终端选区 | `#374555` |
| 已连接指示灯 | `#58BC97` |
| 未连接指示灯 | `#687482` |

主题入口位于右上角，可选跟随系统、浅色、深色，偏好保存在本地。终端光标从 CSS 主色读取，避免单独维护另一套品牌色。

### Logo 与 Ubuntu 本地安装

- `resources/GeekCOM_Logo.svg`：设计源文件，用于生成各平台应用图标。
- `resources/GeekCOM_Logo.png`：同版 PNG，用于界面左上角，避免该 SVG 在 Ubuntu WebKit 中的小尺寸渲染异常；显示尺寸固定为 27×27，禁止 flex 压缩。
- 更新设计时应同步这两份素材，再重新生成 `src-tauri/icons/` 中的图标。

```bash
npm run tauri icon -- resources/GeekCOM_Logo.svg
npm run tauri build -- --debug --bundles deb
bash packaging/linux/install-tauri-local.sh
```

本地安装脚本将程序安装到 `~/.local/bin/geekcom`，备份并更新用户级 `geekcom.desktop`。图标以内容哈希命名并使用绝对路径，避免同名旧图标和缓存干扰。重新打开应用后生效；旧 Qt 二进制保留，原启动入口备份在 `~/.local/share/geekcom-tauri/`。

以下章节描述保留的 **Qt 版本**。

## 功能特性

### 串口调试模式
- 串口配置：端口、波特率（1200~1500000，含 SBUS 100000）、校验位、数据位、停止位
- **接收区**：十六进制 / ASCII 显示切换、时间戳标注、自动清空、保存到文件
- **发送区**：十六进制 / ASCII 发送、文件发送、定时自动发送
- 底部状态栏：RX/TX 字节计数、计数清零

### 串口终端模式
- 基础 VT100 终端仿真（ESC 序列解析）
- 方向键、功能键映射
- 本地回显开关
- 回车发送 `\r` 或 `\r\n` 可选

## 代码架构

```bash
GeekCOM/
├── CMakeLists.txt
├── packaging/linux/            # geekcom.desktop、本地安装脚本
├── resources/
│   └── resources.qrc
└── src/
    ├── main.cpp                  # 入口，ThemeManager 品牌色 / 主题
    ├── ThemeManager.h/cpp        # 系统/浅色/深色主题与语义色 Token
    ├── MainWindow.h/cpp          # 主窗口，Tab 切换，SerialManager 信号分发
    ├── SerialManager.h/cpp       # 串口封装（QSerialPort），RX/TX 统计
    ├── SerialDebugWidget.h/cpp   # 串口调试模式 UI
    ├── SerialTerminalWidget.h/cpp # 串口终端模式 UI
    ├── SerialBaudRates.h         # 统一波特率列表与下拉填充
    └── HexUtils.h/cpp            # HEX 字符串 ↔ QByteArray 转换工具
```

### 层次关系

```
MainWindow
  ├── SerialManager (共享实例)
  ├── Tab[0]: SerialDebugWidget
  └── Tab[1]: SerialTerminalWidget
```

`SerialManager` 是唯一的串口操作对象，两个模式 Widget 共享同一实例，
MainWindow 负责将 `dataReceived` 信号路由给当前激活的 Widget。

## 外观与主题（Qt 版本）

由 `ThemeManager` 统一管理，菜单 **视图 → 外观**：

| 模式 | 说明 |
|------|------|
| 跟随系统（默认） | 跟随 Ubuntu / GNOME 的 light / dark |
| 浅色 / 深色 | 手动固定 |

**设计原则：**

- 窗口 chrome（面板、Tab、按钮）跟随浅色/深色方案
- 接收区 / 终端等日志面保持深色（便于阅读 HEX / ASCII）
- 主色与 Logo 一致，仅用于强调（主按钮、焦点、选中），状态色仍用绿/红语义色

**品牌色（Logo）：**

| Token | 色值 | 用途 |
|-------|------|------|
| Accent | `#E42C2C` | 主按钮、焦点边框、高亮 |
| Accent Hover | `#C42525` | 主按钮悬停 |

偏好写入 `QSettings`（`ui/themeMode`）。

## 环境要求

| 依赖 | 版本 |
|------|------|
| Qt   | 6.x  |
| CMake | 3.16+ |
| C++  | 17   |

Qt 需包含 `Qt6::SerialPort` 模块（`qt6-serialport` 或 Qt Maintenance Tool 选装）。

## 编译运行

```bash
# 1. 安装依赖（Ubuntu/Debian 示例）
sudo apt install qt6-base-dev qt6-serialport-dev cmake build-essential

# 2. 配置
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. 编译
cmake --build build -j$(nproc)

# 4. 运行
./build/GeekCOM
```

在 **Ubuntu / GNOME** 下，若仅直接运行 `./build/GeekCOM`，Dock 可能仍显示通用图标。请安装 `.desktop` 与 **hicolor** 图标，并与 `QGuiApplication::setDesktopFileName("geekcom")` 配合（项目已配置）。一键安装到 `~/.local`：

```bash
./packaging/linux/install-local.sh
```

脚本会把 `.desktop` 里的 `Exec` / `TryExec` 改成 `~/.local/bin/GeekCOM` 的**绝对路径**（避免 GNOME 启动时找不到程序、Dock 只显示占位图标）。请从**应用程序网格**打开 GeekCOM，不要长期直接运行 `./build/GeekCOM`。

排查：用 `xprop WM_CLASS` 时，光标要点在 **GeekCOM 窗口**上再点一下；若点在终端上会得到 `gnome-terminal`，与 GeekCOM 无关。默认 **Wayland** 会话里很多应用无法用 `xprop` 查看，以「菜单安装启动」为准；仍异常时可尝试注销重登刷新图标缓存。

### macOS (Homebrew)
```bash
brew install qt cmake
export Qt6_DIR=$(brew --prefix qt)/lib/cmake/Qt6
cmake -B build && cmake --build build
open build/GeekCOM.app
```

### Windows (MSVC)
```bat
cmake -B build -G "Visual Studio 17 2022" -A x64 -DQt6_DIR=C:\Qt\6.x.x\msvc2022_64\lib\cmake\Qt6
cmake --build build --config Release
```

## 扩展方向

- [ ] 脚本解析接收数据（Lua / JavaScript 脚本引擎）
- [ ] 自动断帧（超时断帧 / 长度断帧）
- [ ] 波形显示（接收数据数值化实时曲线）
- [ ] 多标签页多路串口同时连接
- [ ] TCP/UDP 模式
- [ ] 历史发送记录下拉
