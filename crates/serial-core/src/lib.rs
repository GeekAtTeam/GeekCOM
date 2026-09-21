use serde::{Deserialize, Serialize};
use serialport::{DataBits, FlowControl, Parity, SerialPort, StopBits};
use std::{
    collections::VecDeque,
    fs::File,
    io::Read,
    path::PathBuf,
    sync::{mpsc, Arc, Mutex},
    thread,
    time::{Duration, Instant, SystemTime, UNIX_EPOCH},
};

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Config {
    pub port: String,
    pub baud: u32,
    pub data_bits: u8,
    pub parity: String,
    pub stop_bits: String,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Payload {
    pub text: String,
    pub hex: bool,
    pub ending: String,
}
pub fn encode(p: &Payload) -> Result<Vec<u8>, String> {
    if p.hex {
        let text = p.text.trim();
        if text.is_empty() {
            return Ok(vec![]);
        }
        let parts: Vec<_> = text.split_whitespace().collect();
        if parts
            .iter()
            .any(|s| s.len() % 2 != 0 || !s.bytes().all(|c| c.is_ascii_hexdigit()))
            || (parts.len() > 1 && parts.iter().any(|s| s.len() != 2))
        {
            return Err("HEX 必须是完整字节对，例如 01 23 或 0123；不会自动补零。".into());
        }
        let compact = parts.concat();
        (0..compact.len())
            .step_by(2)
            .map(|i| u8::from_str_radix(&compact[i..i + 2], 16).map_err(|e| e.to_string()))
            .collect()
    } else {
        let end = match p.ending.as_str() {
            "none" => "",
            "lf" => "\n",
            "cr" => "\r",
            "crlf" => "\r\n",
            _ => return Err("未知行尾".into()),
        };
        Ok(format!("{}{end}", p.text).into_bytes())
    }
}
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct PortInfo {
    pub name: String,
    pub description: String,
}
pub fn ports() -> Result<Vec<PortInfo>, String> {
    serialport::available_ports()
        .map(|ps| {
            ps.into_iter()
                .map(|p| {
                    let description = match p.port_type {
                        serialport::SerialPortType::UsbPort(u) => format!(
                            "{} · {:04X}:{:04X}",
                            u.product.unwrap_or_else(|| "USB Serial".into()),
                            u.vid,
                            u.pid
                        ),
                        _ => "Serial port".into(),
                    };
                    PortInfo {
                        name: p.port_name,
                        description,
                    }
                })
                .collect()
        })
        .map_err(|e| e.to_string())
}
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Event {
    pub id: u64,
    pub session: u64,
    pub timestamp: u64,
    pub direction: String,
    pub data: Vec<u8>,
}
#[derive(Debug, Clone, Default, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Status {
    pub session: u64,
    pub connected: bool,
    pub port: String,
    pub rx: u64,
    pub tx: u64,
    pub auto_running: bool,
    pub auto_count: u64,
    pub file_running: bool,
    pub file_sent: u64,
    pub file_size: u64,
    pub error: Option<String>,
    pub dropped: u64,
}
#[derive(Serialize)]
pub struct Batch {
    pub status: Status,
    pub events: Vec<Event>,
}
struct Shared {
    status: Status,
    events: VecDeque<Event>,
    bytes: usize,
    seq: u64,
}
impl Shared {
    fn clear_events(&mut self) {
        self.events.clear();
        self.bytes = 0;
    }
    fn event(&mut self, direction: &str, bytes: &[u8]) {
        self.seq += 1;
        self.bytes += bytes.len();
        self.events.push_back(Event {
            id: self.seq,
            session: self.status.session,
            timestamp: SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default()
                .as_millis() as u64,
            direction: direction.into(),
            data: bytes.to_vec(),
        });
        while self.bytes > 8 * 1024 * 1024 || self.events.len() > 10_000 {
            if let Some(e) = self.events.pop_front() {
                self.bytes -= e.data.len();
                self.status.dropped += e.data.len() as u64;
            }
        }
    }
}
type Reply = mpsc::Sender<Result<(), String>>;
enum Command {
    Open(Config, Reply),
    Close(Reply),
    Send(Vec<u8>, Reply),
    Auto(Vec<u8>, u64, Reply),
    Stop(Reply),
    File(PathBuf, Reply),
    Reset(Reply),
    Shutdown,
}
#[derive(Clone)]
pub struct Engine {
    inner: Arc<EngineInner>,
}
struct EngineInner {
    commands: mpsc::SyncSender<Command>,
    shared: Arc<Mutex<Shared>>,
}
impl Drop for EngineInner {
    fn drop(&mut self) {
        let _ = self.commands.send(Command::Shutdown);
    }
}
impl Default for Engine {
    fn default() -> Self {
        Self::new()
    }
}
impl Engine {
    pub fn new() -> Self {
        let shared = Arc::new(Mutex::new(Shared {
            status: Status::default(),
            events: VecDeque::new(),
            bytes: 0,
            seq: 0,
        }));
        let (tx, rx) = mpsc::sync_channel(64);
        let state = shared.clone();
        thread::spawn(move || worker(rx, state));
        Self {
            inner: Arc::new(EngineInner {
                commands: tx,
                shared,
            }),
        }
    }
    fn request(&self, f: impl FnOnce(Reply) -> Command) -> Result<(), String> {
        let (tx, rx) = mpsc::channel();
        self.inner
            .commands
            .try_send(f(tx))
            .map_err(|_| "串口任务繁忙或已退出".to_string())?;
        // The worker uses short I/O timeouts. Wait for a definitive result rather
        // than reporting a timeout while a queued hardware command can still run.
        rx.recv().map_err(|_| "串口任务已退出".to_string())?
    }
    pub fn open(&self, cfg: Config) -> Result<(), String> {
        self.request(|r| Command::Open(cfg, r))
    }
    pub fn close(&self) -> Result<(), String> {
        self.request(Command::Close)
    }
    pub fn send(&self, data: Vec<u8>) -> Result<(), String> {
        self.request(|r| Command::Send(data, r))
    }
    pub fn start_auto(&self, data: Vec<u8>, ms: u64) -> Result<(), String> {
        self.request(|r| Command::Auto(data, ms, r))
    }
    pub fn stop(&self) -> Result<(), String> {
        self.request(Command::Stop)
    }
    pub fn send_file(&self, path: PathBuf) -> Result<(), String> {
        self.request(|r| Command::File(path, r))
    }
    pub fn reset(&self) -> Result<(), String> {
        self.request(Command::Reset)
    }
    pub fn dismiss_error(&self) {
        self.inner.shared.lock().unwrap().status.error = None;
    }
    pub fn status(&self) -> Status {
        self.inner.shared.lock().unwrap().status.clone()
    }
    pub fn poll(&self) -> Batch {
        let mut s = self.inner.shared.lock().unwrap();
        let n = s.events.len().min(256);
        let events: Vec<_> = s.events.drain(..n).collect();
        s.bytes -= events.iter().map(|e| e.data.len()).sum::<usize>();
        Batch {
            status: s.status.clone(),
            events,
        }
    }
}
fn open_port(c: &Config) -> Result<Box<dyn SerialPort>, String> {
    let bits = match c.data_bits {
        5 => DataBits::Five,
        6 => DataBits::Six,
        7 => DataBits::Seven,
        8 => DataBits::Eight,
        _ => return Err("数据位必须为 5–8".into()),
    };
    let parity = match c.parity.as_str() {
        "none" => Parity::None,
        "odd" => Parity::Odd,
        "even" => Parity::Even,
        _ => return Err("未知校验位".into()),
    };
    if c.baud == 0 {
        return Err("波特率必须大于零".into());
    }
    let stop = match c.stop_bits.as_str() {
        "1" | "1.5" => StopBits::One,
        "2" => StopBits::Two,
        _ => return Err("未知停止位".into()),
    };
    #[cfg(not(windows))]
    if c.stop_bits == "1.5" {
        return Err("1.5 停止位仅支持 Windows，且需要设备支持。".into());
    }
    let builder = serialport::new(&c.port, c.baud)
        .data_bits(bits)
        .parity(parity)
        .stop_bits(stop)
        .flow_control(FlowControl::None)
        .timeout(Duration::from_millis(10));
    #[cfg(windows)]
    {
        use std::os::windows::io::AsRawHandle;
        use windows_sys::Win32::Devices::Communication::{
            GetCommState, SetCommState, DCB, ONE5STOPBITS,
        };
        let port = builder
            .open_native()
            .map_err(|e| format!("无法打开 {}: {e}", c.port))?;
        if c.stop_bits == "1.5" {
            // COMPort owns the handle for this entire call. DCB is initialized
            // by GetCommState before changing only the requested stop-bit field.
            unsafe {
                let mut dcb: DCB = std::mem::zeroed();
                dcb.DCBlength = std::mem::size_of::<DCB>() as u32;
                if GetCommState(port.as_raw_handle(), &mut dcb) == 0 {
                    return Err(std::io::Error::last_os_error().to_string());
                }
                dcb.StopBits = ONE5STOPBITS;
                if SetCommState(port.as_raw_handle(), &dcb) == 0 {
                    return Err(format!(
                        "设备不支持此停止位组合: {}",
                        std::io::Error::last_os_error()
                    ));
                }
            }
        }
        Ok(Box::new(port))
    }
    #[cfg(not(windows))]
    builder
        .open()
        .map_err(|e| format!("无法打开 {}: {e}", c.port))
}
fn write_bytes(
    port: &mut dyn SerialPort,
    bytes: &[u8],
    s: &Arc<Mutex<Shared>>,
) -> Result<(), String> {
    let mut offset = 0;
    while offset < bytes.len() {
        match port.write(&bytes[offset..]) {
            Ok(0) => return Err(format!("写入停止，已提交 {offset}/{} 字节", bytes.len())),
            Ok(n) => {
                let mut state = s.lock().unwrap();
                state.status.tx += n as u64;
                state.event("TX", &bytes[offset..offset + n]);
                offset += n;
            }
            Err(e) if e.kind() == std::io::ErrorKind::Interrupted => continue,
            Err(e) => {
                return Err(format!(
                    "发送失败（已提交 {offset}/{} 字节）: {e}",
                    bytes.len()
                ))
            }
        }
    }
    Ok(())
}
struct Auto {
    data: Vec<u8>,
    interval: Duration,
    next: Instant,
}
struct Transfer {
    file: File,
    size: u64,
    sent: u64,
}
fn worker(commands: mpsc::Receiver<Command>, shared: Arc<Mutex<Shared>>) {
    let mut port: Option<Box<dyn SerialPort>> = None;
    let mut automatic: Option<Auto> = None;
    let mut transfer: Option<Transfer> = None;
    loop {
        // Process a bounded command count so continuous input cannot starve RX.
        for _ in 0..16 {
            let cmd = match commands.try_recv() {
                Ok(c) => c,
                Err(mpsc::TryRecvError::Empty) => break,
                Err(_) => return,
            };
            if matches!(cmd, Command::Shutdown) {
                return;
            }
            let (result, reply) = match cmd {
                Command::Open(c, r) => {
                    if port.is_some() {
                        (Err("请先关闭当前连接".into()), r)
                    } else {
                        match open_port(&c) {
                            Ok(p) => {
                                port = Some(p);
                                automatic = None;
                                transfer = None;
                                let mut s = shared.lock().unwrap();
                                s.clear_events();
                                s.status = Status {
                                    session: s.status.session + 1,
                                    connected: true,
                                    port: c.port,
                                    ..Status::default()
                                };
                                (Ok(()), r)
                            }
                            Err(e) => (Err(e), r),
                        }
                    }
                }
                Command::Close(r) => {
                    port = None;
                    automatic = None;
                    transfer = None;
                    // Closing is also a presentation boundary. Retain the
                    // counters, but never replay queued RX after acknowledging it.
                    shared.lock().unwrap().clear_events();
                    (Ok(()), r)
                }
                Command::Send(bytes, r) => {
                    let result = if automatic.is_some() || transfer.is_some() {
                        Err("请先停止正在运行的发送任务".into())
                    } else if bytes.is_empty() {
                        Err("没有可发送的数据".into())
                    } else if bytes.len() > 1024 * 1024 {
                        Err("单次发送上限为 1 MiB，请使用文件发送".into())
                    } else if let Some(p) = port.as_mut() {
                        write_bytes(p.as_mut(), &bytes, &shared)
                    } else {
                        Err("串口未连接".into())
                    };
                    (result, r)
                }
                Command::Auto(bytes, ms, r) => {
                    if port.is_none()
                        || transfer.is_some()
                        || automatic.is_some()
                        || bytes.is_empty()
                        || bytes.len() > 65536
                        || !(100..=60000).contains(&ms)
                    {
                        (
                            Err(
                                "周期任务需已连接、无其他任务、1–65536 字节及 100–60000 ms 间隔"
                                    .into(),
                            ),
                            r,
                        )
                    } else {
                        automatic = Some(Auto {
                            data: bytes,
                            interval: Duration::from_millis(ms),
                            next: Instant::now() + Duration::from_millis(ms),
                        });
                        shared.lock().unwrap().status.auto_count = 0;
                        (Ok(()), r)
                    }
                }
                Command::Stop(r) => {
                    automatic = None;
                    transfer = None;
                    (Ok(()), r)
                }
                Command::File(path, r) => {
                    if port.is_none() || automatic.is_some() || transfer.is_some() {
                        (Err("需连接串口并停止其他发送任务".into()), r)
                    } else {
                        let result = File::open(path).and_then(|file| {
                            let metadata = file.metadata()?;
                            if !metadata.is_file() {
                                return Err(std::io::Error::other("请选择普通文件"));
                            }
                            Ok(Transfer {
                                file,
                                size: metadata.len(),
                                sent: 0,
                            })
                        });
                        match result {
                            Ok(t) => {
                                let mut s = shared.lock().unwrap();
                                s.status.file_size = t.size;
                                s.status.file_sent = 0;
                                transfer = Some(t);
                                (Ok(()), r)
                            }
                            Err(e) => (Err(e.to_string()), r),
                        }
                    }
                }
                Command::Reset(r) => {
                    let mut s = shared.lock().unwrap();
                    s.status.rx = 0;
                    s.status.tx = 0;
                    s.status.auto_count = 0;
                    (Ok(()), r)
                }
                Command::Shutdown => return,
            };
            {
                let mut s = shared.lock().unwrap();
                if let Err(e) = &result {
                    s.status.error = Some(e.clone());
                } else {
                    s.status.error = None;
                }
                s.status.connected = port.is_some();
                s.status.auto_running = automatic.is_some();
                s.status.file_running = transfer.is_some();
            }
            let _ = reply.send(result);
        }
        let mut fatal = None;
        if let Some(p) = port.as_mut() {
            let mut buffer = [0u8; 4096];
            match p.read(&mut buffer) {
                Ok(0) => fatal = Some("串口已断开".into()),
                Ok(n) => {
                    let mut s = shared.lock().unwrap();
                    s.status.rx += n as u64;
                    s.event("RX", &buffer[..n]);
                }
                Err(e)
                    if matches!(
                        e.kind(),
                        std::io::ErrorKind::TimedOut
                            | std::io::ErrorKind::WouldBlock
                            | std::io::ErrorKind::Interrupted
                    ) => {}
                Err(e) => fatal = Some(format!("接收失败: {e}")),
            }
            if fatal.is_none() {
                if let Some(a) = automatic.as_mut() {
                    if Instant::now() >= a.next {
                        match write_bytes(p.as_mut(), &a.data, &shared) {
                            Ok(()) => shared.lock().unwrap().status.auto_count += 1,
                            Err(e) => fatal = Some(e),
                        }
                        a.next = Instant::now() + a.interval;
                    }
                }
                if let Some(t) = transfer.as_mut() {
                    // Keep the OS output queue bounded on slow serial links. Each
                    // iteration returns to command handling and RX before sending more.
                    let queued = p.bytes_to_write().unwrap_or(0);
                    if queued > 2048 {
                        continue;
                    }
                    let length = t.size.saturating_sub(t.sent).min(512) as usize;
                    match t.file.read(&mut buffer[..length]) {
                        Ok(0) => transfer = None,
                        Ok(n) => {
                            let before = shared.lock().unwrap().status.tx;
                            let result = write_bytes(p.as_mut(), &buffer[..n], &shared);
                            let mut state = shared.lock().unwrap();
                            t.sent += state.status.tx - before;
                            state.status.file_sent = t.sent;
                            if let Err(e) = result {
                                fatal = Some(e);
                            }
                        }
                        Err(e) => {
                            shared.lock().unwrap().status.error =
                                Some(format!("读取文件失败: {e}"));
                            transfer = None;
                        }
                    }
                }
            }
        } else {
            thread::sleep(Duration::from_millis(10));
        }
        if let Some(e) = fatal {
            port = None;
            automatic = None;
            transfer = None;
            let mut state = shared.lock().unwrap();
            state.clear_events();
            state.status.error = Some(e);
        }
        let mut s = shared.lock().unwrap();
        s.status.connected = port.is_some();
        s.status.auto_running = automatic.is_some();
        s.status.file_running = transfer.is_some();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn strict_hex() {
        for s in ["123", "1 23", "GG", "0x01", "01 2345", "中文"] {
            assert!(
                encode(&Payload {
                    text: s.into(),
                    hex: true,
                    ending: "none".into()
                })
                .is_err(),
                "{s}"
            );
        }
        for s in ["00FF0D0A", " 00 FF\t0D\n0A "] {
            assert_eq!(
                encode(&Payload {
                    text: s.into(),
                    hex: true,
                    ending: "none".into()
                })
                .unwrap(),
                [0, 255, 13, 10]
            );
        }
    }
    #[test]
    fn unicode_and_endings() {
        for (mode, ending) in [("none", ""), ("cr", "\r"), ("lf", "\n"), ("crlf", "\r\n")] {
            assert_eq!(
                encode(&Payload {
                    text: "中文".into(),
                    hex: false,
                    ending: mode.into()
                })
                .unwrap(),
                format!("中文{ending}").as_bytes()
            );
        }
    }
    #[test]
    fn disconnected_write_is_error() {
        let e = Engine::new();
        assert!(e.send(vec![1]).is_err());
        assert_eq!(e.poll().status.tx, 0);
    }
}
