# Qt / Tauri 性能测试与迁移验收

日期：2026-09-21。产品基线：`539d3a5`。当前任务是定位响应和数据显示延迟；本轮不优化产品代码、不合并主分支、不移除 Qt。

## 测试顺序

| 步骤 | 场景 | 目的 / 完成标准 |
|---|---|---|
| 1：工具冒烟 | 每实现、每模式 15 秒 | 核对标记、计时、启动和清理机制 |
| 2：基础对照 | 每实现、每模式 60 秒；96 B × 100 Hz | 获取延迟分布、前后段变化、界面调度及资源基线 |
| 3：复测与定位 | 同配置至少 3 次；再测 HEX 对照、突发大块、长历史 | 区分稳定瓶颈和桌面环境噪声；必要时增加 Rust→IPC→视图分段计时 |
| 4：交互 | 接收中输入、发送、切换显示、暂停滚动、清空、断开 | 测输入到控件状态和命令执行的延迟；不能只靠定时器推断按钮体验 |
| 5：持续压力 | 两模式分别 60 分钟；另测较高速率 | 延迟不持续累积、内存进入稳定范围、缓存淘汰有提示、无静默丢失 |
| 6：实机验收 | 同一 IMU、相同参数，Qt/Tauri release 对照 | 确认 USB 驱动及设备真实数据行为；用户体验改善后才讨论默认切换 |

步骤 2 的 9,600 B/s 是 115200 8N1 理论有效字节速率 11,520 B/s 的约 83%。PTY 仅按主机节拍写入，不模拟电气传输、USB 分包或真实波特率时序。后续 640 B × 100 Hz 为压力测试，不代表 115200 物理串口能达到该速率。

## 当前工具与指标定义

- `tests/serial-performance.py`：生成带连续序号和主机毫秒时间的 ASCII 记录，只打开新建的 PTY；输出每组原始样本及摘要。
- `tests/qt-performance.cpp`：可选编译的 Qt 适配器，使用实际 MainWindow、SerialManager、调试页和终端页。没有替换串口读取或显示代码。
- Tauri 通过真实 WebKitWebDriver 启动 release 可执行文件，使用实际按钮、IPC、Rust 串口和 xterm。
- 两者每 16 ms 观察一次文本：Qt 读取 QTextDocument 尾部最多 8,192 字符；Tauri 读取调试区域或 xterm 可见行的尾部文本。标记首次可读时间减去发送时间为延迟。
- 这是**带采样误差的文本可用延迟**，不是屏幕绘制/像素出现延迟。观察本身也有开销，Qt 文档观察与 WebKit DOM 观察并不完全等价。
- 当前 Qt 在关闭自动清空时没有与 Tauri 相同的 4,000 条 / 2 MiB 保留上限；测试比较实际默认主线的数据结构，不宣称是同样历史容量下的纯框架跑分。
- 采样数组与资源监控也会增加开销，尤其长时测试应结合关闭观测的对照运行；不能把采样数据增长误认为产品泄漏。
- 2026-09-24 起，Tauri 采样每 10 秒取回 Python 并清空 WebView 数组，去重集合限制为 8,192 项；增加每分钟进度文件及最终 `latency_by_minute` 摘要。CPU 估算改用单调时钟的实际采样跨度。新的测量有周期性 WebDriver 取回开销，不能视为完全无扰动测试。
- xterm 只观察可见行，刷新或吞吐过高时标记可能在观察之前滚出；“未观察到的标记”不能直接当成串口丢字节。保留发送统计和 Qt RX / Tauri 状态文本以便核对。
- `timer_gap_ms` 是实际 16 ms 定时器间隔，反映主线程调度延迟，不是输入或点击延迟。
- `early_latency_ms` / `late_latency_ms` 分别使用序号前 25% / 后 25% 的标记。
- RSS 是被测进程树的 RSS 求和，会重复计算共享页；Tauri 树含 WebDriver 辅助进程。CPU 由进程树 CPU 时间差估算，100% 约为一个核心；子进程退出等因素会产生误差。不可把这些数值当作精确的框架内存对比。
- 当前 Tauri 断开指标是在发送停止后测量，包含 WebDriver 往返开销；接收中的按钮延迟仍属于步骤 4。
- 发送端非阻塞写入会保留未完成的字节；遇到背压会单独计数，超过容许时间则将本轮标为异常。

## 构建和运行

需要 Linux 桌面会话、Qt 6 开发依赖、Rust/Tauri 构建依赖、Python 3、tauri-driver、WebKitWebDriver。不依赖真实设备，不安装或替换用户当前启动的应用。

```bash
npm run build
cargo build --release --manifest-path src-tauri/Cargo.toml --features tauri/custom-protocol
cmake -S . -B /tmp/geekcom-perf-release -DCMAKE_BUILD_TYPE=Release -DGEEKCOM_BUILD_BENCHMARKS=ON
cmake --build /tmp/geekcom-perf-release --target GeekCOMPerf -j4

# 若 WebKitWebDriver 不在 PATH，先 export WEBKIT_WEBDRIVER=/path/to/WebKitWebDriver
python3 tests/serial-performance.py --duration 60 --output benchmark-results/performance-60s

# 关闭 Tauri 调试时间戳的控制变量对照
python3 tests/serial-performance.py --implementation tauri --mode debug --tauri-no-timestamps --duration 60 --output benchmark-results/performance-no-timestamps

# 单模式复测 / 长时测试（所有测试应顺序执行）
python3 tests/serial-performance.py --implementation tauri --mode debug --duration 60 --output benchmark-results/performance-repeat
python3 tests/serial-performance.py --implementation tauri --mode terminal --duration 3600 --output benchmark-results/performance-soak-terminal
```

工具使用本机 4449/4450 端口，需要保持空闲。测试会打开窗口；请保持桌面解锁、窗口可见，不同时编译或运行其他负载。每次使用独立输出目录，原始 JSON 和驱动日志保存在忽略的 `benchmark-results/` 下。需共享结果时提供去除本机路径等环境信息的摘要。

## 合并门槛（待基线复测后确认）

建议目标：常规速率下文本可用延迟 P95 ≤ 100 ms，持续运行时无明显增长；输入和断开 P95 ≤ 100 ms，且无持续卡死；所有必须保留的原始字节能够核对，缓存淘汰可见。以上是建议的验收目标，不是当前实现已通过的指标。

只有基础功能、长时接收、交互延迟和实机回归通过后，才将 Tauri 作为默认实现合并。Qt 先保留稳定标签作为回退；不以一次短测或自动化通过替代迁移验收。

## 本轮执行结果

已完成工具冒烟、四组 60 秒基线及一组关闭时间戳的对照，详情见 [性能基线结果](performance-baseline.md)。这些是测量完成状态，不代表性能验收通过。


## 优化后的补充测试

实现与三轮复测结果见 [性能优化记录](performance-optimization.md)。`--exercise-controls` 可在持续接收时每约 2 秒切换左侧配置区，测量按钮调用至控件状态改变的时间（包含 WebDriver 往返），仅适用于 Tauri。这是单独的交互场景，不混入无操作基线：

```bash
python3 tests/serial-performance.py --implementation tauri --duration 20 --exercise-controls --output benchmark-results/performance-controls
```

最终基线每次启动均固定显示左侧配置区，避免上一次交互测试保存的布局影响后续结果。

性能结果使用独立的 `benchmark-results/`，避免 Playwright 在启动时清理默认 `test-results/` 导致原始样本丢失。
