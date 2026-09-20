"""Linux desktop smoke test: real Tauri IPC + OS pseudo-terminal, no hardware mocks.
Build with `npm run tauri build -- --debug --no-bundle` first.
Needs tauri-driver, WebKitWebDriver, Python 3 and a running graphical session.
"""
import base64
import json
import os
from pathlib import Path
import pty
import select
import shutil
import subprocess
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
NATIVE = os.environ.get("WEBKIT_WEBDRIVER", shutil.which("WebKitWebDriver"))
XDO = os.environ.get("XDOTOOL", shutil.which("xdotool"))
assert NATIVE, "Install webkit2gtk-driver or set WEBKIT_WEBDRIVER"
BINARY = Path(os.environ.get("GEEKCOM_BINARY", ROOT / "src-tauri/target/debug/geekcom"))
master, slave = pty.openpty()
port = os.ttyname(slave)
session = None
out = ROOT / "test-results"
out.mkdir(exist_ok=True)
log = (out / "native-driver.log").open("w")
driver = subprocess.Popen(["tauri-driver", "--port", "4447", "--native-port", "4448", "--native-driver", NATIVE], stdout=log, stderr=log)


def request(method, path, payload=None):
    data = None if payload is None else json.dumps(payload).encode()
    req = urllib.request.Request("http://127.0.0.1:4447" + path, data=data, method=method, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=30) as response:
        value = json.load(response)["value"]
    if isinstance(value, dict) and "error" in value:
        raise RuntimeError(value)
    return value


def js(script, *args):
    return request("POST", f"/session/{session}/execute/sync", {"script": script, "args": list(args)})


def wait_js(script):
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        if js(script):
            return
        time.sleep(0.05)
    raise AssertionError(f"Timed out: {script}\n{js('return document.body.innerText')}")


def click(selector):
    js("document.querySelector(arguments[0]).click()", selector)


def field(label, value):
    js("""const e=document.querySelector('[aria-label="'+arguments[0]+'"]');
    const p=e.tagName==='SELECT'?HTMLSelectElement.prototype:e.tagName==='TEXTAREA'?HTMLTextAreaElement.prototype:HTMLInputElement.prototype;
    Object.getOwnPropertyDescriptor(p,'value').set.call(e,arguments[1]);
    e.dispatchEvent(new Event(e.tagName==='SELECT'?'change':'input',{bubbles:true}));""", label, value)


def read_bytes(count):
    data = b""
    deadline = time.monotonic() + 4
    while len(data) < count and time.monotonic() < deadline:
        ready, _, _ = select.select([master], [], [], 0.2)
        if ready:
            data += os.read(master, count - len(data))
    assert len(data) == count, (count, data)
    return data


def file_dialog(title, path):
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        result = subprocess.run([XDO, "search", "--onlyvisible", "--name", title], capture_output=True, text=True)
        if result.returncode == 0:
            window = result.stdout.splitlines()[-1]
            subprocess.run([XDO, "windowactivate", "--sync", window], check=True)
            subprocess.run([XDO, "key", "--clearmodifiers", "ctrl+l"], check=True)
            subprocess.run([XDO, "key", "--clearmodifiers", "ctrl+a"], check=True)
            subprocess.run([XDO, "type", "--clearmodifiers", "--delay", "1", str(path)], check=True)
            subprocess.run([XDO, "key", "--clearmodifiers", "Return"], check=True)
            return
        time.sleep(0.1)
    raise AssertionError(f"Native file dialog did not appear: {title}")


try:
    for _ in range(100):
        try:
            request("GET", "/status")
            break
        except (OSError, ValueError):
            time.sleep(0.1)
    response = request("POST", "/session", {"capabilities": {"alwaysMatch": {"tauri:options": {"application": str(BINARY)}}}})
    session = response["sessionId"]
    wait_js("return !!document.querySelector('.connection')")
    field("外观", "dark")
    # Connection is initiated by the actual React button, not a mocked command.
    field("串口设备", port)
    click(".connection")
    wait_js("return document.querySelector('.connection').textContent.includes('断开')")
    os.close(slave)
    slave = None
    os.write(master, b"GeekCOM PTY ready\r\n\x00\xff")
    wait_js("return document.querySelector('.log-surface').textContent.includes('GeekCOM PTY ready')")
    click(".segmented button:nth-child(3)")
    wait_js("return document.querySelector('.ascii-column').textContent.includes('GeekCOM PTY ready')")
    # Split UTF-8 must retain its original bytes without replacement characters.
    os.write(master, b"\xe4\xb8")
    time.sleep(0.15)
    os.write(master, b"\xad")
    click(".segmented button:first-child")
    wait_js("return document.querySelector('.log-surface').textContent.includes('中')")
    assert '\ufffd' not in js("return [...document.querySelectorAll('.log-row')].slice(1).map(e=>e.textContent).join('')")
    field("发送数据", "AT")
    field("发送行尾", "crlf")
    wait_js("return document.querySelector('.send-preview').textContent.includes('41 54 0D 0A')")
    click(".send-actions .primary")
    assert read_bytes(4) == b"AT\r\n"
    # Illegal HEX must not reach the port.
    click(".send-heading input[type=checkbox]")
    field("发送数据", "0G")
    wait_js("return document.querySelector('.send-preview').classList.contains('invalid')")
    assert js("return document.querySelector('.send-actions .primary').disabled")
    field("发送数据", "00 FF 0A")
    wait_js("return !document.querySelector('.send-actions .primary').disabled")
    click(".send-actions .primary")
    assert read_bytes(3) == b"\x00\xff\x0a"
    field("周期发送间隔", "100")
    click(".periodic button")
    assert read_bytes(6) == b"\x00\xff\x0a" * 2
    wait_js("return document.querySelector('.periodic button').classList.contains('toggled')")
    click(".periodic button")
    wait_js("return !document.querySelector('.periodic button').classList.contains('toggled')")
    if XDO:
        # Drive the OS file picker too; no application filesystem shortcuts.
        source = out / f"native-{os.getpid()}.bin"
        payload = bytes(range(256)) * 16
        source.write_bytes(payload)
        click(".send-actions > .quiet")
        file_dialog("选择发送文件", source)
        wait_js("return document.querySelectorAll('.send-actions > .quiet').length === 2 && !document.querySelectorAll('.send-actions > .quiet')[1].disabled")
        click(".send-actions > .quiet:nth-child(2)")
        assert read_bytes(len(payload)) == payload
        wait_js("return !document.querySelector('.send-actions .primary').disabled")
        saved = out / f"native-{os.getpid()}.txt"
        click('[aria-label="保存接收数据"]')
        file_dialog("保存接收数据", saved)
        wait_js("return document.body.innerText.includes('接收数据已保存')")
        assert "GeekCOM PTY ready" in saved.read_text()
        source.unlink()
        saved.unlink()
        print("PASS: native file picker, binary file transfer, save dialog and exported content")
    else:
        print("SKIP: native file dialogs (install xdotool to enable)")
    click(".tabs button:nth-child(2)")
    assert js("return !document.querySelector('.receive-pane').hidden"), "Mode changed while connected"
    click(".connection")
    wait_js("return document.querySelector('.connection').textContent.includes('连接串口')")
    click(".tabs button:nth-child(2)")
    click(".connection")
    wait_js("return document.querySelector('.connection').textContent.includes('断开')")
    os.write(master, b"\x1b[31mTerminal OK\x1b[0m\r\n")
    # xterm generates device-status response only in terminal mode.
    os.write(master, b"\x1b[5n")
    assert read_bytes(4) == b"\x1b[0n"
    os.close(master)
    master = None
    wait_js("return document.querySelector('.connection').textContent.includes('连接串口')")
    wait_js("return document.querySelector('.message-bar').textContent.includes('接收失败') || document.querySelector('.message-bar').textContent.includes('断开')")
    click(".tabs button:first-child")
    click(".segmented button:nth-child(3)")
    image = request("GET", f"/session/{session}/screenshot")
    (out / "native-workbench.png").write_bytes(base64.b64decode(image))
    print("PASS: native UI + IPC + PTY: connect, RX, split UTF-8, HEX/ASCII, text/HEX TX, validation, periodic send/stop, mode guard, terminal response, unplug")
finally:
    if session:
        request("DELETE", f"/session/{session}")
    driver.terminate()
    try:
        driver.wait(timeout=5)
    except subprocess.TimeoutExpired:
        driver.kill()
    log.close()
    if slave is not None:
        os.close(slave)
    if master is not None:
        os.close(master)
