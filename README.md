# GeekCOM - 串口调试工具

![](./docs/images/GeekCOM_banner.png)

跨平台串口调试工具，基于 **Tauri 2 + React + Rust**。历史 Qt6 / C++ 实现完整保留在 [`qt`](https://github.com/GeekAtTeam/GeekCOM/tree/qt) 分支，可对照或回退。

## 功能

- 串口配置：端口、波特率（1200~1500000，含 SBUS 100000）、校验位、数据位、停止位
- **接收区**：文本 / HEX / HEX·ASCII 对照、时间戳、自动清空、保存到文件；可见区虚拟化，历史最多约 4,000 条 / 2 MiB
- **发送区**：文本 / HEX、行尾与发送预览、文件发送、周期发送
- **串口终端**：xterm 基础 VT100、方向键与功能键、本地回显、回车 `\r` / `\r\n`
- 底部状态栏：RX / TX 计数、周期次数、错误与文件进度
- 主题：浅色 / 深色，偏好本地保存；可隐藏左侧配置区

## 快速开始

```bash
# Ubuntu 24.04：先安装 Node.js 22.12+、Rust stable，再安装桌面开发依赖
sudo apt install build-essential pkg-config libudev-dev libwebkit2gtk-4.1-dev \
  libxdo-dev libssl-dev libayatana-appindicator3-dev librsvg2-dev
npm ci
npm run tauri dev

# Linux 安装包
npm run tauri build -- --bundles deb

# Windows（本机需 MSVC + WebView2）
npm run tauri build -- --bundles nsis
```

`npm run dev` 仅启动浏览器预览，实际串口和文件操作需要 Tauri 桌面进程。Windows / macOS 系统准备参见 [Tauri 官方说明](https://v2.tauri.app/start/prerequisites/)。当前以 Linux 桌面验证为主；Windows 已可构建，macOS 仍需原生设备测试。

### Linux：Too many open files

若 `npm run tauri dev` 在创建文件监听时崩溃：

```bash
ulimit -n
cat /proc/sys/fs/inotify/max_user_instances
```

额度不足时可临时提高 inotify 实例上限后重试：

```bash
sudo sysctl -w fs.inotify.max_user_instances=1024
npm run tauri dev
```

该设置重启后恢复；长期调整可将 `fs.inotify.max_user_instances = 1024` 写入 `/etc/sysctl.d/99-geekcom-inotify.conf` 并执行 `sudo sysctl --system`。

只需测串口 / 性能、不需要热更新时，直接跑 Release：

```bash
./src-tauri/target/release/geekcom

# 或重新构建（绕过 tauri-cli 开发监听）
npm run build
cargo build --release --manifest-path src-tauri/Cargo.toml --features tauri/custom-protocol
./src-tauri/target/release/geekcom
```

`src-tauri/target/` 是 Cargo / Tauri 的默认产物目录，属于标准布局。

## 工程结构

```text
GeekCOM/
├── ui/                 # React 前端
├── src-tauri/          # Tauri 壳、命令、图标与打包配置
├── crates/serial-core/ # 独立 Rust 串口引擎
├── packaging/linux/    # 本机桌面入口安装脚本
├── resources/          # Logo 设计源（SVG / PNG）
└── tests/              # 原生冒烟与性能脚本
```

## 连接与接收控制

发送计数表示操作系统实际接受的字节，不代表设备已收到。短写会报告已提交/请求字节数，不自动重发；周期或文件发送失败会停止任务并关闭连接。

- 串口设备使用可编辑选框：输入端口名或从列表选择（`/dev/ttyXXX`、`COMx` 等）；刷新重新枚举。
- 「暂停滚动 / 恢复滚动」只影响自动滚动，串口仍继续接收。
- 接收区仅渲染可见记录及附近缓冲；保存导出全部保留内容。
- 断开会关闭串口、停止发送任务，并丢弃尚未显示的缓冲；已显示记录保留。

## 配色方案

配色以 `ui/src/styles.css` 的 CSS 变量为准。深色和浅色主题共用品牌主色，日志区与终端保持深色。

| 品牌色 Token | 色值 | 用途 |
| --- | --- | --- |
| `--accent` | `#E85552` | Logo 底色、主按钮、品牌文字、选中态、焦点边框、复选框及终端光标 |
| `--accent-hover` | `#D94B48` | 主按钮悬停 |
| `--accent-active` | `#C6413E` | 主按钮按下 |

主按钮使用白色文字；禁用按钮不透明度为 `0.38`。已连接时的断开按钮使用中性色，连接状态由绿色指示灯表达。

| 界面 Token | 深色主题 | 浅色主题 |
| --- | --- | --- |
| `--bg` 页面背景 | `#14171C` | `#FFFFFF` |
| `--panel` 面板 | `#1B1F26` | `#FFFFFF` |
| `--raised` 凸起区域 | `#262B35` | `#F3F4F6` |
| `--field` 输入区域 | `#12161C` | `#FFFFFF` |
| `--border` 边框 | `#4A5563` | `#C7CED8` |
| `--text` 正文 | `#E8EEF5` | `#111827` |
| `--muted` 次要文字 | `#A0AAB8` | `#4B5563` |
| `--hover` 中性色悬停背景 | `#313846` | `#EEF2F6` |

| 固定配色 | 色值 |
| --- | --- |
| 日志与终端背景 | `#0B0E12`（`--log`） |
| 接收 / 发送 / 终端文字 | `#E8EEF5`（`--log-text`） |
| 日志次要文字 | `#9AA8B8`（`--log-muted`） |
| 日志边框 | `#3A4654`（`--log-border`） |
| 终端选区 | `#445566`（`--log-selection`） |
| 已连接指示灯 | `#58BC97` |
| 未连接指示灯 | `#8B97A6` |

主题入口位于右上角图标按钮，在浅色 / 深色间切换。

## Logo 与 Ubuntu 本地安装

- `resources/GeekCOM_Logo.svg`：设计源文件，用于生成各平台应用图标。
- `resources/GeekCOM_Logo.png`：同版 PNG，用于界面左上角；显示尺寸固定为 27×27。
- 更新设计后同步这两份素材，再重新生成 `src-tauri/icons/`。

```bash
npm run tauri icon -- resources/GeekCOM_Logo.svg
npm run tauri build -- --bundles deb
bash packaging/linux/install-local.sh src-tauri/target/release/geekcom
```

脚本安装到 `~/.local/bin/geekcom`，并更新用户级 `geekcom.desktop`。图标以内容哈希命名，使用绝对路径，避免旧图标缓存干扰。

## 测试

```bash
npm test
npm run test:core
npx playwright install chromium
npm run test:ui

# Linux 原生冒烟（需图形会话）
cargo install tauri-driver --locked
sudo apt install webkit2gtk-driver xdotool
npm run tauri build -- --debug --no-bundle
python3 tests/native-smoke.py
```

## 扩展方向

- [ ] 脚本解析接收数据（Lua / JavaScript）
- [ ] 自动断帧（超时 / 长度）
- [ ] 波形显示
- [ ] 多标签多路串口
- [ ] TCP/UDP 模式
- [ ] 历史发送记录下拉
