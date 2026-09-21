#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]
use geekcom_core::{Batch, Config, Engine, Payload, PortInfo, Status};
use serde::Serialize;
use std::{path::PathBuf, sync::Mutex};
use tauri::{Manager, State};
use tauri_plugin_dialog::DialogExt;

struct AppState {
    engine: Engine,
    selected_file: Mutex<Option<PathBuf>>,
}
#[derive(Serialize)]
struct SelectedFile {
    name: String,
    size: u64,
}
#[tauri::command]
async fn list_ports() -> Result<Vec<PortInfo>, String> {
    tauri::async_runtime::spawn_blocking(geekcom_core::ports)
        .await
        .map_err(|e| e.to_string())?
}
#[tauri::command]
fn preview(payload: Payload) -> Result<Vec<u8>, String> {
    geekcom_core::encode(&payload)
}
#[tauri::command]
fn poll(state: State<AppState>) -> Batch {
    state.engine.poll()
}
#[tauri::command]
fn dismiss_error(state: State<AppState>) {
    state.engine.dismiss_error();
}
#[tauri::command]
async fn connect_serial(config: Config, state: State<'_, AppState>) -> Result<Status, String> {
    let engine = state.engine.clone();
    tauri::async_runtime::spawn_blocking(move || {
        engine.open(config)?;
        Ok(engine.status())
    })
    .await
    .map_err(|e| e.to_string())?
}
#[tauri::command]
async fn disconnect_serial(state: State<'_, AppState>) -> Result<Status, String> {
    let engine = state.engine.clone();
    tauri::async_runtime::spawn_blocking(move || {
        engine.close()?;
        Ok(engine.status())
    })
    .await
    .map_err(|e| e.to_string())?
}
#[tauri::command]
async fn send(payload: Payload, state: State<'_, AppState>) -> Result<(), String> {
    let bytes = geekcom_core::encode(&payload)?;
    let engine = state.engine.clone();
    tauri::async_runtime::spawn_blocking(move || engine.send(bytes))
        .await
        .map_err(|e| e.to_string())?
}
#[tauri::command]
async fn send_bytes(data: Vec<u8>, state: State<'_, AppState>) -> Result<(), String> {
    let engine = state.engine.clone();
    tauri::async_runtime::spawn_blocking(move || engine.send(data))
        .await
        .map_err(|e| e.to_string())?
}
#[tauri::command]
async fn start_auto(
    payload: Payload,
    interval: u64,
    state: State<'_, AppState>,
) -> Result<(), String> {
    let bytes = geekcom_core::encode(&payload)?;
    let engine = state.engine.clone();
    tauri::async_runtime::spawn_blocking(move || engine.start_auto(bytes, interval))
        .await
        .map_err(|e| e.to_string())?
}
#[tauri::command]
async fn stop_task(state: State<'_, AppState>) -> Result<(), String> {
    let engine = state.engine.clone();
    tauri::async_runtime::spawn_blocking(move || engine.stop())
        .await
        .map_err(|e| e.to_string())?
}
#[tauri::command]
async fn reset_stats(state: State<'_, AppState>) -> Result<(), String> {
    let engine = state.engine.clone();
    tauri::async_runtime::spawn_blocking(move || engine.reset())
        .await
        .map_err(|e| e.to_string())?
}
#[tauri::command]
async fn choose_file(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
) -> Result<Option<SelectedFile>, String> {
    let picked = tauri::async_runtime::spawn_blocking(move || {
        app.dialog()
            .file()
            .set_title("选择发送文件")
            .blocking_pick_file()
    })
    .await
    .map_err(|e| e.to_string())?;
    let Some(file) = picked else { return Ok(None) };
    let path = file.into_path().map_err(|e| e.to_string())?;
    let metadata = std::fs::metadata(&path).map_err(|e| e.to_string())?;
    if !metadata.is_file() {
        return Err("请选择普通文件".into());
    }
    let result = SelectedFile {
        name: path
            .file_name()
            .unwrap_or_default()
            .to_string_lossy()
            .into_owned(),
        size: metadata.len(),
    };
    *state.selected_file.lock().unwrap() = Some(path);
    Ok(Some(result))
}
#[tauri::command]
async fn send_file(state: State<'_, AppState>) -> Result<(), String> {
    let path = state
        .selected_file
        .lock()
        .unwrap()
        .clone()
        .ok_or("请先选择文件")?;
    let engine = state.engine.clone();
    tauri::async_runtime::spawn_blocking(move || engine.send_file(path))
        .await
        .map_err(|e| e.to_string())?
}
#[tauri::command]
async fn save_receive(app: tauri::AppHandle, text: String) -> Result<bool, String> {
    tauri::async_runtime::spawn_blocking(move || {
        let Some(file) = app
            .dialog()
            .file()
            .set_title("保存接收数据")
            .set_file_name("geekcom-receive.txt")
            .add_filter("Text", &["txt"])
            .blocking_save_file()
        else {
            return Ok(false);
        };
        let path = file.into_path().map_err(|e| e.to_string())?;
        std::fs::write(path, text.as_bytes()).map_err(|e| e.to_string())?;
        Ok(true)
    })
    .await
    .map_err(|e| e.to_string())?
}
fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(AppState {
            engine: Engine::new(),
            selected_file: Mutex::new(None),
        })
        .invoke_handler(tauri::generate_handler![
            list_ports,
            preview,
            poll,
            dismiss_error,
            connect_serial,
            disconnect_serial,
            send,
            send_bytes,
            start_auto,
            stop_task,
            reset_stats,
            choose_file,
            send_file,
            save_receive
        ])
        .on_window_event(|window, event| {
            if matches!(event, tauri::WindowEvent::Destroyed) {
                let _ = window.state::<AppState>().engine.close();
            }
        })
        .run(tauri::generate_context!())
        .expect("Unable to start GeekCOM");
}
