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

#[test]
fn close_discards_a_large_rx_backlog_and_releases_the_device() {
    let (engine, mut peer, cfg) = setup();
    // More than one poll can consume. Do not drain the queue while receiving.
    let data = vec![0x42; 2 * 1024 * 1024];
    peer.write_all(&data).unwrap();
    let deadline = Instant::now() + Duration::from_secs(4);
    while engine.status().rx < data.len() as u64 {
        assert!(Instant::now() < deadline);
        thread::sleep(Duration::from_millis(5));
    }
    engine.close().unwrap();
    let after = engine.poll();
    assert!(!after.status.connected);
    assert_eq!(after.status.rx, data.len() as u64);
    assert!(
        after.events.is_empty(),
        "closed sessions must not replay queued data"
    );
    thread::sleep(Duration::from_millis(80));
    assert_eq!(engine.status().rx, after.status.rx);
    assert!(engine.poll().events.is_empty());

    // TIOCEXCL would reject this if the old worker still owned the slave.
    let port = serialport::new(&cfg.port, 115200).open().unwrap();
    drop(port);
    engine.open(cfg).unwrap();
    let batch = engine.poll();
    assert!(batch.status.connected);
    assert!(batch.status.session > after.status.session);
    assert!(
        batch.events.is_empty(),
        "no old-session data after reconnect"
    );
}

#[test]
fn open_failure_preserves_error_and_allows_a_clean_retry() {
    let (engine, mut peer, cfg) = setup();
    engine.close().unwrap();
    let previous_session = engine.status().session;
    let mut missing = cfg.clone();
    missing.port = format!("/dev/geekcom-missing-{}", std::process::id());
    let error = engine.open(missing.clone()).unwrap_err();
    assert!(error.contains(&missing.port));
    let status = engine.status();
    assert!(!status.connected);
    assert!(!status.auto_running && !status.file_running);
    assert_eq!(status.error.as_deref(), Some(error.as_str()));
    assert_eq!(status.session, previous_session);
    assert!(engine.start_auto(vec![1], 100).is_err());
    assert!(engine.send_file("unused.bin".into()).is_err());
    assert_eq!(engine.status().tx, 0);
    engine.open(cfg).unwrap();
    assert!(engine.status().error.is_none());
    assert_eq!(engine.status().session, previous_session + 1);
    engine.send(vec![0, 255]).unwrap();
    let mut got = [0; 2];
    peer.read_exact(&mut got).unwrap();
    assert_eq!(got, [0, 255]);
}

#[test]
fn encoded_payload_matches_wire_bytes_and_tx_events() {
    use geekcom_core::{encode, Payload};
    let (engine, mut peer, _) = setup();
    let cases = [
        ("中文", false, "none", "中文".as_bytes()),
        ("中文", false, "lf", "中文\n".as_bytes()),
        ("中文", false, "cr", "中文\r".as_bytes()),
        ("中文", false, "crlf", "中文\r\n".as_bytes()),
        ("00FF0D0A", true, "none", &[0, 255, 13, 10][..]),
        ("00 FF 0D 0A", true, "crlf", &[0, 255, 13, 10][..]),
    ];
    let mut submitted = 0;
    for (text, hex, ending, expected) in cases {
        let bytes = encode(&Payload {
            text: text.into(),
            hex,
            ending: ending.into(),
        })
        .unwrap();
        engine.send(bytes).unwrap();
        let mut got = vec![0; expected.len()];
        peer.read_exact(&mut got).unwrap();
        assert_eq!(got, expected);
        submitted += expected.len() as u64;
        let batch = engine.poll();
        assert_eq!(batch.status.tx, submitted);
        let logged: Vec<_> = batch
            .events
            .into_iter()
            .filter(|e| e.direction == "TX")
            .flat_map(|e| e.data)
            .collect();
        assert_eq!(logged, expected);
    }
}
