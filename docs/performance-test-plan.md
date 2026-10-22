# Tauri 性能测试与验收

日期：2026-09-21（初稿）；2026-10-22 起主分支仅保留 Tauri。产品基线曾对照 `539d3a5` 的 Qt 实现，历史对照数据见 [性能基线结果](performance-baseline.md)；Qt 源码在 `qt` 分支。

## 测试顺序

| 步骤 | 场景 | 目的 / 完成标准 |
|---|---|---|
| 1：工具冒烟 | 每模式 15 秒 | 核对标记、计时、启动和清理机制 |
| 2：基础 | 每模式 60 秒；96 B × 100 Hz | 获取延迟分布、前后段变化、界面调度及资源基线 |
| 3：复测与定位 | 同配置至少 3 次；再测 HEX 对照、突发大块、长历史 | 区分稳定瓶颈和桌面环境噪声 |
| 4：交互 | 接收中输入、发送、切换显示、暂停滚动、清空、断开 | 测输入到控件状态和命令执行的延迟 |
| 5：持续压力 | 两模式分别 60 分钟；另测较高速率 | 延迟不持续累积、内存进入稳定范围、缓存淘汰有提示 |
| 6：实机验收 | 真实 USB 串口、相同参数 | 确认驱动及设备真实数据行为 |

步骤 2 的 9,600 B/s 是 115200 8N1 理论有效字节速率约 83%。PTY 仅按主机节拍写入，不模拟电气传输。

## 工具与指标

- `tests/serial-performance.py`：生成带连续序号和主机毫秒时间的 ASCII 记录，只打开新建的 PTY；通过 WebKitWebDriver 驱动 release 可执行文件。
- 每 16 ms 观察一次调试区域或 xterm 可见行尾部文本；标记首次可读时间减去发送时间为延迟。
- 这是**带采样误差的文本可用延迟**，不是屏幕绘制延迟。
- 2026-09-24 起，采样每 10 秒取回 Python 并清空 WebView 数组；增加每分钟进度及 `latency_by_minute`。
- xterm 只观察可见行；“未观察到的标记”不能直接当成串口丢字节。
- `timer_gap_ms` 反映主线程调度延迟；`early_latency_ms` / `late_latency_ms` 使用序号前/后 25%。
- RSS 为进程树求和（含 WebDriver 辅助进程）；CPU 由进程树 CPU 时间差估算。

## 构建和运行

需要 Linux 桌面会话、Rust/Tauri 构建依赖、Python 3、tauri-driver、WebKitWebDriver。

```bash
npm run build
cargo build --release --manifest-path src-tauri/Cargo.toml --features tauri/custom-protocol

# 若 WebKitWebDriver 不在 PATH，先 export WEBKIT_WEBDRIVER=/path/to/WebKitWebDriver
python3 tests/serial-performance.py --duration 60 --output benchmark-results/performance-60s

python3 tests/serial-performance.py --mode debug --tauri-no-timestamps --duration 60 --output benchmark-results/performance-no-timestamps
python3 tests/serial-performance.py --mode debug --duration 60 --output benchmark-results/performance-repeat
python3 tests/serial-performance.py --mode terminal --duration 3600 --output benchmark-results/performance-soak-terminal
```

工具使用本机 4449/4450 端口。请保持桌面解锁、窗口可见。原始 JSON 写入已忽略的 `benchmark-results/`。

## 交互对照

`--exercise-controls` 可在持续接收时切换左侧配置区，测量按钮至状态改变的时间（含 WebDriver 往返），不混入无操作基线：

```bash
python3 tests/serial-performance.py --duration 20 --exercise-controls --output benchmark-results/performance-controls
```

实现与复测结果见 [性能优化记录](performance-optimization.md)。
