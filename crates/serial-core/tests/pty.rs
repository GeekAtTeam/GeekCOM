#![cfg(unix)]
use geekcom_core::{Config, Engine, Event, Status};
use serialport::{SerialPort, TTYPort};
use std::{
    io::{Read, Write},
    thread,
    time::{Duration, Instant},
};

fn setup() -> (Engine, TTYPort, Config) {
    let (mut master, slave) = TTYPort::pair().unwrap();
    master.set_timeout(Duration::from_secs(2)).unwrap();
    let cfg = Config {
        port: slave.name().unwrap(),
        baud: 115200,
        data_bits: 8,
        parity: "none".into(),
        stop_bits: "1".into(),
    };
    drop(slave);
    let engine = Engine::new();
    engine.open(cfg.clone()).unwrap();
    (engine, master, cfg)
}
fn until(engine: &Engine, predicate: impl Fn(&Status) -> bool) -> (Status, Vec<Event>) {
    let deadline = Instant::now() + Duration::from_secs(4);
    let mut events = vec![];
    loop {
        let batch = engine.poll();
        events.extend(batch.events);
        if predicate(&batch.status) {
            return (batch.status, events);
        }
        assert!(
            Instant::now() < deadline,
            "status did not converge: {:?}",
            batch.status
        );
        thread::sleep(Duration::from_millis(10));
    }
}

#[test]
fn duplex_raw_bytes_counters_and_reconnect() {
    let (engine, mut peer, cfg) = setup();
    let rx = b"hello\r\n\x00\xff\xe4\xb8\xad";
    peer.write_all(rx).unwrap();
    let (_, events) = until(&engine, |s| s.rx == rx.len() as u64);
    assert_eq!(
        events
            .into_iter()
            .filter(|e| e.direction == "RX")
            .flat_map(|e| e.data)
            .collect::<Vec<_>>(),
        rx
    );
    let tx = b"AT\r\n\x00\xff";
    engine.send(tx.to_vec()).unwrap();
    let mut got = [0; 6];
    peer.read_exact(&mut got).unwrap();
    assert_eq!(&got, tx);
    assert_eq!(engine.poll().status.tx, tx.len() as u64);
    assert!(engine.open(cfg.clone()).is_err());
    engine.reset().unwrap();
    let status = engine.poll().status;
    assert_eq!((status.rx, status.tx, status.auto_count), (0, 0, 0));
    engine.close().unwrap();
    assert!(!engine.poll().status.connected);
    assert!(engine.send(vec![1]).is_err());
    engine.open(cfg).unwrap();
    engine.send(vec![42]).unwrap();
    let mut got = [0];
    peer.read_exact(&mut got).unwrap();
    assert_eq!(got, [42]);
}

#[test]
fn periodic_send_stops_and_never_resumes_after_reconnect() {
    let (engine, mut peer, cfg) = setup();
    assert!(engine.start_auto(vec![1], 99).is_err());
    engine.start_auto(vec![0x12, 0x34], 100).unwrap();
    assert!(engine.send(vec![9]).is_err());
    let mut got = [0; 4];
    peer.read_exact(&mut got).unwrap();
    assert_eq!(got, [0x12, 0x34, 0x12, 0x34]);
    engine.stop().unwrap();
    let status = engine.poll().status;
    assert!(!status.auto_running);
    assert!(status.auto_count >= 2);
    thread::sleep(Duration::from_millis(220));
    assert_eq!(engine.poll().status.tx, status.tx);
    engine.start_auto(vec![7], 100).unwrap();
    engine.close().unwrap();
    engine.open(cfg).unwrap();
    thread::sleep(Duration::from_millis(150));
    let status = engine.poll().status;
    assert_eq!(status.tx, 0);
    assert!(!status.auto_running);
}

#[test]
fn file_transfer_preserves_binary_and_reports_completion() {
    let (engine, mut peer, _) = setup();
    let path = std::env::temp_dir().join(format!("geekcom-pty-{}.bin", std::process::id()));
    let data: Vec<u8> = (0..32768).map(|i| (i % 256) as u8).collect();
    std::fs::write(&path, &data).unwrap();
    engine.send_file(path.clone()).unwrap();
    let mut got = vec![0; data.len()];
    peer.read_exact(&mut got).unwrap();
    assert_eq!(got, data);
    let (status, _) = until(&engine, |s| !s.file_running);
    assert_eq!(status.file_sent, data.len() as u64);
    assert_eq!(status.file_size, status.file_sent);
    assert_eq!(status.tx, status.file_sent);
    std::fs::remove_file(path).unwrap();
}

#[test]
fn unplug_cancels_tasks_and_updates_connection_state() {
    let (engine, peer, _) = setup();
    engine.start_auto(vec![1], 100).unwrap();
    drop(peer);
    let (status, _) = until(&engine, |s| !s.connected);
    assert!(!status.auto_running);
    assert!(!status.file_running);
    assert!(status.error.is_some());
    assert!(engine.send(vec![2]).is_err());
}
