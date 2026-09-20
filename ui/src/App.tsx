import {
  useCallback,
  useEffect,
  useRef,
  useState,
  type ReactNode,
} from "react";
import { invoke, isTauri } from "@tauri-apps/api/core";
import {
  Cable,
  RefreshCw,
  PanelLeftClose,
  PanelLeftOpen,
  Plug,
  Unplug,
  TerminalSquare,
  List,
  Trash2,
  Download,
  Pause,
  Play,
  Send,
  FileUp,
  Square,
  SlidersHorizontal,
  CircleHelp,
  ChevronDown,
  RotateCcw,
  Radio,
} from "lucide-react";
import { TerminalView, type TerminalHandle } from "./TerminalView";
import {
  appendRows,
  ascii,
  emptyStatus,
  exportText,
  hex,
  rates,
  terminalInput,
  time,
  type Batch,
  type Config,
  type LogRow,
  type Port,
  type Status,
  type View,
} from "./model";

function preference<T>(key: string, fallback: T): T {
  try {
    return JSON.parse(localStorage.getItem(key) || "null") ?? fallback;
  } catch {
    return fallback;
  }
}
function Field({ label, children }: { label: string; children: ReactNode }) {
  return (
    <label className="field">
      <span>{label}</span>
      {children}
    </label>
  );
}
export default function App() {
  const [config, setConfig] = useState<Config>(() =>
    preference("connection", {
      port: "",
      baud: 115200,
      dataBits: 8,
      parity: "none",
      stopBits: "1",
    }),
  );
  const [ports, setPorts] = useState<Port[]>([]);
  const [status, setStatus] = useState<Status>(emptyStatus);
  const [rows, setRows] = useState<LogRow[]>([]);
  const [view, setView] = useState<View>("text");
  const [mode, setMode] = useState<"debug" | "terminal">("debug");
  const [sidebar, setSidebar] = useState(() => preference("sidebar", true));
  const [theme, setTheme] = useState<"system" | "light" | "dark">(() =>
    preference("theme", "system"),
  );
  const [timestamps, setTimestamps] = useState(true);
  const [autoClear, setAutoClear] = useState(false);
  const [paused, setPaused] = useState(false);
  const [text, setText] = useState("");
  const [txHex, setTxHex] = useState(false);
  const [ending, setEnding] = useState("none");
  const [preview, setPreview] = useState<number[]>([]);
  const [invalid, setInvalid] = useState("");
  const [interval, setIntervalMs] = useState(1000);
  const [localEcho, setLocalEcho] = useState(false);
  const [crlf, setCrlf] = useState(true);
  const [file, setFile] = useState<{ name: string; size: number } | null>(null);
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState("");
  const [help, setHelp] = useState(false);
  const [menu, setMenu] = useState(false);
  const [selected, setSelected] = useState<number | null>(null);
  const terminal = useRef<TerminalHandle>(null);
  const log = useRef<HTMLDivElement>(null);
  const decoder = useRef(new TextDecoder());
  const decoderSession = useRef(0);
  const terminalQueue = useRef(Promise.resolve());
  const inputEpoch = useRef(0);
  const live = useRef({ autoClear, mode, status });
  live.current = { autoClear, mode, status };
  const desktop = isTauri();
  const task = status.autoRunning || status.fileRunning;
  const payload = { text, hex: txHex, ending };
  const run = useCallback(
    async (fn: () => Promise<unknown>, success?: string) => {
      setBusy(true);
      try {
        await fn();
        setMessage(success || "");
      } catch (e) {
        setMessage(String(e));
      } finally {
        setBusy(false);
      }
    },
    [],
  );
  const refresh = useCallback(async () => {
    try {
      const list = await invoke<Port[]>("list_ports");
      setPorts(list);
      setConfig((c) => (c.port ? c : { ...c, port: list[0]?.name || "" }));
    } catch (e) {
      setMessage(String(e));
    }
  }, []);
  useEffect(() => {
    if (desktop) void refresh();
  }, [desktop, refresh]);
  useEffect(() => {
    localStorage.setItem("connection", JSON.stringify(config));
  }, [config]);
  useEffect(() => {
    localStorage.setItem("sidebar", JSON.stringify(sidebar));
  }, [sidebar]);
  useEffect(() => {
    localStorage.setItem("theme", JSON.stringify(theme));
    const query = matchMedia("(prefers-color-scheme: dark)");
    const apply = () =>
      (document.documentElement.dataset.theme =
        theme === "system" ? (query.matches ? "dark" : "light") : theme);
    apply();
    query.addEventListener("change", apply);
    return () => query.removeEventListener("change", apply);
  }, [theme]);
  useEffect(() => {
    if (!desktop) return;
    let disposed = false;
    let timer: ReturnType<typeof setTimeout>;
    const tick = async () => {
      try {
        const batch = await invoke<Batch>("poll");
        if (disposed) return;
        setStatus(batch.status);
        live.current.status = batch.status;
        const incoming: LogRow[] = [];
        for (const e of batch.events) {
          if (e.direction === "RX") {
            if (decoderSession.current !== e.session) {
              decoder.current = new TextDecoder();
              decoderSession.current = e.session;
            }
            if (live.current.mode === "terminal") {
              terminal.current?.write(new Uint8Array(e.data));
            } else
              incoming.push({
                ...e,
                text: decoder.current.decode(new Uint8Array(e.data), {
                  stream: true,
                }),
              });
          }
        }
        if (incoming.length)
          setRows((old) => appendRows(old, incoming, live.current.autoClear));
      } catch (e) {
        if (!disposed) setMessage(String(e));
      } finally {
        if (!disposed) timer = setTimeout(tick, 40);
      }
    };
    timer = setTimeout(tick, 0);
    return () => {
      disposed = true;
      clearTimeout(timer);
    };
  }, [desktop]);
  useEffect(() => {
    if (!paused && log.current)
      log.current.scrollTop = log.current.scrollHeight;
  }, [rows, paused, view]);
  useEffect(() => {
    if (!desktop) return;
    let cancelled = false;
    setPreview([]);
    invoke<number[]>("preview", { payload: { text, hex: txHex, ending } })
      .then((b) => {
        if (!cancelled) {
          setPreview(b);
          setInvalid("");
        }
      })
      .catch((e) => {
        if (!cancelled) {
          setPreview([]);
          setInvalid(String(e));
        }
      });
    return () => {
      cancelled = true;
    };
  }, [text, txHex, ending, desktop]);
  const connect = () => {
    inputEpoch.current += 1;
    return run(() =>
      status.connected
        ? invoke("disconnect_serial")
        : invoke("connect_serial", { config }),
    );
  };
  const send = () =>
    run(() => invoke("send", { payload }), "数据已提交 · 不代表设备已收到");
  const save = async () => {
    setBusy(true);
    try {
      const saved = await invoke<boolean>("save_receive", {
        text: exportText(rows, view, timestamps),
      });
      setMessage(saved ? "接收数据已保存" : "");
    } catch (e) {
      setMessage(String(e));
    } finally {
      setBusy(false);
    }
  };
  const clear = () => {
    if (mode === "terminal") terminal.current?.clear();
    else {
      setRows([]);
      setSelected(null);
      decoder.current = new TextDecoder();
    }
  };
  const toggleMode = (next: "debug" | "terminal") => {
    if (next !== mode && status.connected) {
      setMessage("切换模式前请先关闭串口连接");
      return;
    }
    setMode(next);
  };
  const terminalSend = (input: string) => {
    if (!status.connected || task || busy || mode !== "terminal") return;
    const data = terminalInput(input, crlf);
    const session = status.session;
    const epoch = inputEpoch.current;
    terminalQueue.current = terminalQueue.current
      .then(async () => {
        if (
          epoch !== inputEpoch.current ||
          !live.current.status.connected ||
          live.current.status.session !== session
        )
          return;
        await invoke("send_bytes", { data });
        if (localEcho) terminal.current?.write(new Uint8Array(data));
      })
      .catch((e) => setMessage(String(e)));
  };
  const selectedRow = rows.find((r) => r.id === selected);
  return (
    <div className="app-shell">
      <header className="menubar">
        <div className="wordmark">
          <span className="brand-icon">
            <Cable size={17} />
          </span>
          Geek<span>COM</span>
          <small>WORKBENCH</small>
        </div>
        <div className="menu-wrap">
          <button
            className={menu ? "menu active" : "menu"}
            onClick={() => setMenu(!menu)}
          >
            文件 <ChevronDown size={12} />
          </button>
          {menu && (
            <div className="dropdown">
              <button
                disabled={!desktop || !rows.length}
                onClick={() => {
                  void save();
                  setMenu(false);
                }}
              >
                保存接收数据 <small>.txt</small>
              </button>
              <button
                onClick={() => {
                  clear();
                  setMenu(false);
                }}
              >
                清空当前视图
              </button>
            </div>
          )}
        </div>
        <button className="menu" onClick={() => setSidebar(!sidebar)}>
          视图
        </button>
        <button className="menu" onClick={() => setHelp(true)}>
          帮助
        </button>
        <div className="header-end">
          <span className="version">0.1 · TAURI PREVIEW</span>
          <select
            aria-label="外观"
            value={theme}
            onChange={(e) => setTheme(e.target.value as typeof theme)}
          >
            <option value="system">跟随系统</option>
            <option value="dark">深色</option>
            <option value="light">浅色</option>
          </select>
        </div>
      </header>
      <div className="connection-toolbar">
        <button
          className="icon-button"
          title={sidebar ? "隐藏配置" : "显示配置"}
          aria-label="切换配置区域"
          onClick={() => setSidebar(!sidebar)}
        >
          {sidebar ? <PanelLeftClose size={18} /> : <PanelLeftOpen size={18} />}
        </button>
        <span className="toolbar-divider" />
        <button
          className={status.connected ? "connection connected" : "connection"}
          disabled={!desktop || busy || (!status.connected && !config.port)}
          onClick={() => void connect()}
        >
          {status.connected ? <Unplug size={15} /> : <Plug size={15} />}{" "}
          {busy ? "处理中…" : status.connected ? "断开连接" : "连接串口"}
        </button>
        <span className="device-summary">
          {status.connected ? status.port : config.port || "未选择设备"}
          <span>
            {config.baud.toLocaleString()} baud · {config.dataBits}
            {config.parity[0]?.toUpperCase()}
            {config.stopBits}
          </span>
        </span>
        <span className="toolbar-spacer" />
        <span className="connection-indicator">
          <i className={status.connected ? "dot online" : "dot"} />
          {status.connected ? "CONNECTED" : "DISCONNECTED"}
        </span>
      </div>
      {!desktop && (
        <div className="notice">
          浏览器预览 · 串口与文件功能请通过 <code>npm run tauri dev</code>{" "}
          启动桌面应用。
        </div>
      )}
      <div className="workspace">
        {sidebar && (
          <aside className="sidebar">
            <div className="section-heading">
              <SlidersHorizontal size={14} />
              连接配置<span>SERIAL</span>
            </div>
            <div className="config-form">
              <Field label="串口设备">
                <div className="port-field">
                  <input
                    aria-label="串口设备"
                    list="ports"
                    placeholder="选择或输入端口路径"
                    value={config.port}
                    disabled={status.connected || busy}
                    onChange={(e) =>
                      setConfig({ ...config, port: e.target.value })
                    }
                  />
                  <button
                    className="icon-button"
                    title="刷新串口"
                    aria-label="刷新串口"
                    disabled={!desktop || status.connected || busy}
                    onClick={() => void refresh()}
                  >
                    <RefreshCw size={14} />
                  </button>
                </div>
                <datalist id="ports">
                  {ports.map((p) => (
                    <option key={p.name} value={p.name}>
                      {p.description}
                    </option>
                  ))}
                </datalist>
              </Field>
              <Field label="波特率">
                <select
                  aria-label="波特率"
                  disabled={status.connected || busy}
                  value={config.baud}
                  onChange={(e) =>
                    setConfig({ ...config, baud: Number(e.target.value) })
                  }
                >
                  {rates.map((r) => (
                    <option key={r} value={r}>
                      {r.toLocaleString()}
                    </option>
                  ))}
                </select>
              </Field>
              <div className="field-pair">
                <Field label="数据位">
                  <select
                    aria-label="数据位"
                    disabled={status.connected || busy}
                    value={config.dataBits}
                    onChange={(e) =>
                      setConfig({ ...config, dataBits: Number(e.target.value) })
                    }
                  >
                    {[5, 6, 7, 8].map((n) => (
                      <option key={n}>{n}</option>
                    ))}
                  </select>
                </Field>
                <Field label="停止位">
                  <select
                    aria-label="停止位"
                    disabled={status.connected || busy}
                    value={config.stopBits}
                    onChange={(e) =>
                      setConfig({ ...config, stopBits: e.target.value })
                    }
                  >
                    {["1", "1.5", "2"].map((n) => (
                      <option key={n}>{n}</option>
                    ))}
                  </select>
                </Field>
              </div>
              <Field label="校验位">
                <select
                  aria-label="校验位"
                  disabled={status.connected || busy}
                  value={config.parity}
                  onChange={(e) =>
                    setConfig({ ...config, parity: e.target.value })
                  }
                >
                  <option value="none">None · 无校验</option>
                  <option value="odd">Odd · 奇校验</option>
                  <option value="even">Even · 偶校验</option>
                </select>
              </Field>
              <p className="field-hint">
                无流控 · 参数在连接期间锁定
                <br />
                1.5 停止位取决于系统与设备支持
              </p>
            </div>
            <div className="sidebar-note">
              <Radio size={18} />
              <strong>本地通信，数据由你掌控。</strong>
              <p>设备数据仅在本机处理。此验证版本不包含云端或 AI 服务。</p>
            </div>
            <div className="sidebar-bottom">
              <i className="dot" /> SINGLE SESSION
            </div>
          </aside>
        )}
        <main className="main-pane">
          <div className="tabs">
            <button
              className={mode === "debug" ? "tab selected" : "tab"}
              onClick={() => toggleMode("debug")}
            >
              <List size={16} />
              串口调试
            </button>
            <button
              className={mode === "terminal" ? "tab selected" : "tab"}
              onClick={() => toggleMode("terminal")}
            >
              <TerminalSquare size={16} />
              串口终端
            </button>
            <span className="toolbar-spacer" />
            <span className="tab-caption">
              {mode === "debug" ? "RECEIVE / TRANSMIT" : "INTERACTIVE TERMINAL"}
            </span>
          </div>
          <section className="receive-pane" hidden={mode !== "debug"}>
            <div className="pane-toolbar">
              <div className="segmented" aria-label="接收显示格式">
                {(["text", "hex", "dual"] as View[]).map((v) => (
                  <button
                    key={v}
                    className={view === v ? "chosen" : ""}
                    onClick={() => setView(v)}
                  >
                    {v === "text"
                      ? "文本"
                      : v === "hex"
                        ? "HEX"
                        : "HEX / ASCII"}
                  </button>
                ))}
              </div>
              <label className="check">
                <input
                  type="checkbox"
                  checked={timestamps}
                  onChange={(e) => setTimestamps(e.target.checked)}
                />
                时间戳
              </label>
              <label className="check">
                <input
                  type="checkbox"
                  checked={autoClear}
                  onChange={(e) => setAutoClear(e.target.checked)}
                />
                自动清空
              </label>
              <span className="toolbar-spacer" />
              <button
                title={paused ? "恢复跟随" : "暂停跟随"}
                aria-label="暂停跟随"
                className={paused ? "icon-button toggled" : "icon-button"}
                onClick={() => setPaused(!paused)}
              >
                {paused ? <Play size={15} /> : <Pause size={15} />}
              </button>
              <button
                className="icon-button"
                title="清空接收区"
                aria-label="清空接收区"
                onClick={clear}
              >
                <Trash2 size={15} />
              </button>
              <button
                className="icon-button"
                title="保存接收数据"
                aria-label="保存接收数据"
                disabled={!desktop || !rows.length}
                onClick={() => void save()}
              >
                <Download size={15} />
              </button>
            </div>
            <div className="log-column-head">
              <span>接收数据</span>
              <span>
                {rows.length
                  ? `${rows.length} 条读取记录 · 当前保留范围`
                  : "UTF-8 / RAW BYTES"}
              </span>
            </div>
            <div
              className="log-surface"
              ref={log}
              tabIndex={0}
              aria-label="接收数据"
            >
              {!rows.length ? (
                <div className="empty-state">
                  <div className="empty-glyph">
                    <Cable size={30} strokeWidth={1.2} />
                  </div>
                  <h2>等待设备数据</h2>
                  <p>
                    {status.connected
                      ? "连接已建立，收到的数据将在这里显示。"
                      : "选择串口参数并连接，开始你的调试会话。"}
                  </p>
                  <div className="empty-tags">
                    <span>TEXT</span>
                    <span>HEX</span>
                    <span>ASCII</span>
                  </div>
                </div>
              ) : (
                rows.map((r) => (
                  <div
                    className={
                      selected === r.id ? "log-row highlighted" : "log-row"
                    }
                    key={r.id}
                    onClick={() => setSelected(r.id)}
                  >
                    {timestamps && (
                      <span className="timestamp">{time(r.timestamp)}</span>
                    )}
                    <span className="direction">RX</span>
                    <span className="row-content">
                      {view === "text" ? r.text || "" : hex(r.data)}
                    </span>
                    {view === "dual" && (
                      <span className="ascii-column">{ascii(r.data)}</span>
                    )}
                  </div>
                ))
              )}
            </div>
            {selectedRow && (
              <div className="byte-detail">
                #{selectedRow.id} · {selectedRow.data.length} bytes{" "}
                <code>
                  {hex(selectedRow.data.slice(0, 32))}
                  {selectedRow.data.length > 32 ? " …" : ""}
                </code>
                <button
                  onClick={() => setSelected(null)}
                  aria-label="关闭字节详情"
                >
                  ×
                </button>
              </div>
            )}
          </section>
          <section className="terminal-pane" hidden={mode !== "terminal"}>
            <div className="pane-toolbar">
              <label className="check">
                <input
                  type="checkbox"
                  checked={localEcho}
                  onChange={(e) => setLocalEcho(e.target.checked)}
                />
                本地回显
              </label>
              <label className="check">
                <input
                  type="checkbox"
                  checked={crlf}
                  onChange={(e) => setCrlf(e.target.checked)}
                />
                回车发送 CRLF
              </label>
              <span className="toolbar-spacer" />
              <button
                className="quiet"
                onClick={() => terminal.current?.clear()}
              >
                <Trash2 size={14} />
                清屏
              </button>
            </div>
            <TerminalView
              ref={terminal}
              active={mode === "terminal"}
              onInput={terminalSend}
            />
            <div className="terminal-footnote">
              直接键入以发送 · Ctrl+C 发送中断字符 · 方向键与功能键支持
            </div>
          </section>
          {mode === "debug" && (
            <section className="send-pane">
              <div className="send-heading">
                <span>
                  发送数据 <small>TRANSMIT</small>
                </span>
                <label className="check">
                  <input
                    type="checkbox"
                    checked={txHex}
                    disabled={task || busy}
                    onChange={(e) => setTxHex(e.target.checked)}
                  />
                  HEX 发送
                </label>
                <select
                  aria-label="发送行尾"
                  value={ending}
                  disabled={txHex || task || busy}
                  onChange={(e) => setEnding(e.target.value)}
                >
                  <option value="none">无行尾</option>
                  <option value="lf">LF</option>
                  <option value="cr">CR</option>
                  <option value="crlf">CRLF</option>
                </select>
              </div>
              <textarea
                aria-label="发送数据"
                placeholder={
                  txHex ? "例如 01 03 00 00 00 02" : "输入要发送的文本…"
                }
                value={text}
                disabled={task || busy}
                onChange={(e) => setText(e.target.value)}
                onKeyDown={(e) => {
                  if (
                    e.ctrlKey &&
                    e.key === "Enter" &&
                    status.connected &&
                    !invalid &&
                    preview.length &&
                    !task
                  ) {
                    e.preventDefault();
                    void send();
                  }
                }}
              />
              <div
                className={invalid ? "send-preview invalid" : "send-preview"}
              >
                {invalid || (
                  <>
                    <span>{preview.length} bytes</span>
                    <code>
                      {hex(preview.slice(0, 40)) || "—"}
                      {preview.length > 40 ? " …（前 40 字节）" : ""}
                    </code>
                  </>
                )}
              </div>
              <div className="send-actions">
                <button
                  className="quiet"
                  disabled={!desktop || busy || task}
                  onClick={() =>
                    void run(async () => {
                      const f = await invoke<typeof file>("choose_file");
                      if (f) setFile(f);
                    })
                  }
                >
                  <FileUp size={15} />
                  {file ? file.name : "选择文件"}
                </button>
                {file && (
                  <button
                    className="quiet"
                    disabled={!status.connected || busy || task}
                    onClick={() => void run(() => invoke("send_file"))}
                  >
                    发送文件 <small>{file.size.toLocaleString()} B</small>
                  </button>
                )}
                <span className="toolbar-spacer" />
                <div className="periodic">
                  <input
                    aria-label="周期发送间隔"
                    type="number"
                    min={100}
                    max={60000}
                    value={interval}
                    disabled={task || busy}
                    onChange={(e) => setIntervalMs(Number(e.target.value))}
                  />
                  <span>ms</span>
                  <button
                    className={status.autoRunning ? "quiet toggled" : "quiet"}
                    disabled={
                      !desktop ||
                      busy ||
                      (!status.autoRunning &&
                        (!status.connected ||
                          status.fileRunning ||
                          !preview.length ||
                          !!invalid))
                    }
                    onClick={() =>
                      void run(() =>
                        status.autoRunning
                          ? invoke("stop_task")
                          : invoke("start_auto", { payload, interval }),
                      )
                    }
                  >
                    {status.autoRunning ? (
                      <Square size={13} />
                    ) : (
                      <RotateCcw size={13} />
                    )}
                    周期发送
                  </button>
                </div>
                <button
                  className="primary"
                  disabled={
                    !desktop ||
                    !status.connected ||
                    busy ||
                    task ||
                    !!invalid ||
                    !preview.length
                  }
                  onClick={() => void send()}
                >
                  <Send size={15} />
                  发送 <kbd>⌃ ↵</kbd>
                </button>
              </div>
            </section>
          )}
        </main>
      </div>
      {(message ||
        status.error ||
        status.dropped > 0 ||
        status.fileRunning) && (
        <div className="message-bar" role="status">
          <span>
            {status.error ||
              message ||
              (status.fileRunning
                ? `文件提交中 ${status.fileSent.toLocaleString()} / ${status.fileSize.toLocaleString()} bytes`
                : `界面传输缓冲已淘汰 ${status.dropped} 字节`)}
          </span>
          {status.fileRunning && (
            <button onClick={() => void run(() => invoke("stop_task"))}>
              停止文件发送
            </button>
          )}
          {(message || status.error) && (
            <button
              aria-label="关闭提示"
              onClick={() => {
                setMessage("");
                if (status.error) void invoke("dismiss_error");
              }}
            >
              ×
            </button>
          )}
        </div>
      )}
      <footer className="statusbar">
        <span>
          <i className={status.connected ? "dot online" : "dot"} />
          {status.connected ? "已连接" : "未连接"}
        </span>
        <span>
          RX <b>{status.rx.toLocaleString()}</b> B
        </span>
        <span>
          TX 已提交 <b>{status.tx.toLocaleString()}</b> B
        </span>
        <button
          disabled={!desktop}
          onClick={() => void run(() => invoke("reset_stats"))}
        >
          计数清零
        </button>
        <span className="toolbar-spacer" />
        <span>
          {status.autoRunning ? "周期运行中" : "周期已停止"} ·{" "}
          {status.autoCount} 次
        </span>
        <span>UTF-8</span>
        <button aria-label="帮助" onClick={() => setHelp(true)}>
          <CircleHelp size={14} />
        </button>
      </footer>
      {help && (
        <div className="modal-backdrop" onClick={() => setHelp(false)}>
          <div
            role="dialog"
            aria-modal="true"
            aria-label="关于 GeekCOM"
            className="modal"
            onClick={(e) => e.stopPropagation()}
          >
            <span className="eyebrow">GEEK TOOLS / SERIAL WORKBENCH</span>
            <h2>GeekCOM</h2>
            <p>Tauri + React + Rust 迁移验证版</p>
            <p>
              支持单串口收发、文本与 HEX
              对照、文件发送、周期发送和交互终端。数据在本地处理。
            </p>
            <p>
              TX 表示向系统提交的字节数，不代表设备已收到。界面最多保留 4,000 条
              / 2 MiB 接收数据；保存导出的是当前视图。暂停跟随不会停止采集。
            </p>
            <p>
              切换模式前需断开连接。串口参数是否可用取决于系统、驱动与设备。
            </p>
            <button className="primary" onClick={() => setHelp(false)}>
              知道了
            </button>
          </div>
        </div>
      )}
    </div>
  );
}
