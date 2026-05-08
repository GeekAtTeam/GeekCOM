# GeekCOM - 串口调试工具

跨平台串口调试工具，基于 Qt6 + C++ Widget，支持 Windows / macOS / Linux。

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
├── resources/
│   └── resources.qrc
└── src/
    ├── main.cpp                  # 入口，Fusion 深色主题
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
