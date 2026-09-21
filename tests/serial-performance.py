"""Linux PTY benchmark. Measures text availability, not physical screen paint.
Python stdlib only. See docs/performance-test-plan.md for scope and commands.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import pty
import select
import shutil
import subprocess
import threading
import time
import urllib.request
import urllib.error

ROOT = Path(__file__).resolve().parents[1]


def summary(values):
    values = sorted(values)
    if not values:
        return {"count": 0}
    return {"count": len(values), "p50": values[int((len(values)-1)*.50)],
            "p95": values[int((len(values)-1)*.95)], "max": values[-1]}


class WebDriver:
    def __init__(self, binary, log):
        self.session = None
        native = os.environ.get("WEBKIT_WEBDRIVER", shutil.which("WebKitWebDriver"))
        if not native:
            raise RuntimeError("Set WEBKIT_WEBDRIVER")
        self.process = subprocess.Popen(["tauri-driver", "--port", "4449", "--native-port", "4450",
                                         "--native-driver", native], stdout=log, stderr=log)
        try:
            for _ in range(100):
                try:
                    self.request("GET", "/status")
                    break
                except (OSError, ValueError):
                    time.sleep(.1)
            result = self.request("POST", "/session", {"capabilities": {"alwaysMatch": {
                "tauri:options": {"application": str(binary)}}}})
            self.session = result["sessionId"]
        except Exception:
            self.close()
            raise

    def request(self, method, path, payload=None):
        request = urllib.request.Request("http://127.0.0.1:4449" + path, method=method,
            data=None if payload is None else json.dumps(payload).encode(),
            headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(request, timeout=20) as response:
                value = json.load(response)["value"]
        except urllib.error.HTTPError as exc:
            raise RuntimeError(exc.read().decode()) from exc
        if isinstance(value, dict) and "error" in value:
            raise RuntimeError(value)
        return value

    def js(self, script, *args):
        return self.request("POST", f"/session/{self.session}/execute/sync",
                            {"script": script, "args": args})

    def wait(self, script):
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if self.js(script):
                return
            time.sleep(.05)
        raise RuntimeError("Timed out: " + script + " UI: " + self.js("return document.body.innerText.slice(-2000)"))

    def close(self):
        try:
            if self.session:
                self.request("DELETE", f"/session/{self.session}")
        finally:
            self.process.terminate()
            try:
                self.process.wait(5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()


def feed(fd, duration, hz, size, result, stopped):
    start = time.monotonic()
    result.update(sent_bytes=0, sent_samples=0, blocked_writes=0)
    os.set_blocking(fd, False)
    for i in range(int(duration * hz)):
        if stopped.wait(max(0, start + i / hz - time.monotonic())):
            break
        packet = f"P{i:08d}:{int(time.time()*1000):013d}".encode().ljust(size-2, b".") + b"\r\n"
        offset = 0
        while offset < len(packet) and not stopped.is_set():
            try:
                n = os.write(fd, packet[offset:])
                offset += n
                result["sent_bytes"] += n
            except BlockingIOError:
                result["blocked_writes"] += 1
                if time.monotonic() - start > duration + 5:
                    result["error"] = "PTY writer backpressure exceeded 5s"
                    return
                select.select([], [fd], [], .01)
            except OSError as exc:
                result["error"] = str(exc)
                return
        result["sent_samples"] += 1
    result["elapsed_s"] = time.monotonic() - start


def resources(pid):
    """RSS sum includes shared-page double counting and automation descendants."""
    pending, seen, rss, ticks = [pid], set(), 0, 0
    while pending:
        child = pending.pop()
        if child in seen:
            continue
        seen.add(child)
        try:
            stat = Path(f"/proc/{child}/stat").read_text().rsplit(")", 1)[1].split()
            ticks += int(stat[11]) + int(stat[12])
            rss += int(stat[21]) * os.sysconf("SC_PAGE_SIZE")
            for children in Path(f"/proc/{child}/task").glob("*/children"):
                pending.extend(map(int, children.read_text().split()))
        except (OSError, ValueError, IndexError):
            pass
    return {"rss_mib": rss/1024/1024, "cpu_s": ticks/os.sysconf("SC_CLK_TCK")}


def run(args, implementation, mode):
    master, slave = pty.openpty()
    port = os.ttyname(slave)
    process = driver = thread = None
    stopped = threading.Event()
    sent = {}
    log_path = args.output / f"{implementation}-{mode}.log"
    with log_path.open("w") as log:
        try:
            if implementation == "qt":
                process = subprocess.Popen([str(args.qt_binary), port, mode, str(args.duration + 4)],
                                           stdout=subprocess.PIPE, stderr=log, text=True)
                if not select.select([process.stdout], [], [], 20)[0] or process.stdout.readline().strip() != "READY":
                    raise RuntimeError("Qt benchmark did not become ready")
            else:
                driver = WebDriver(args.tauri_binary, log)
                driver.wait("return !!document.querySelector('.connection')")
                driver.js("localStorage.setItem('connection',JSON.stringify({port:arguments[0],baud:115200,dataBits:8,parity:'none',stopBits:'1'}));localStorage.setItem('manualPort','true');localStorage.setItem('sidebar','true')", port)
                driver.request("POST", f"/session/{driver.session}/refresh", {})
                driver.wait("return !!document.querySelector('[aria-label=\"串口设备\"]') && !document.querySelector('[aria-label=\"串口设备\"]').disabled")
                driver.js("document.querySelector('.tabs button:nth-child('+arguments[0]+')').click()", 2 if mode == "terminal" else 1)
                if args.tauri_no_timestamps:
                    driver.js("const label=[...document.querySelectorAll('label')].find(e=>e.textContent.trim()==='时间戳'); const box=label.querySelector('input');if(box.checked)box.click()")
                # Startup enumeration can begin after the input first appears
                # enabled. Check and click the actual button in one JS task;
                # clicking a temporarily disabled button is otherwise ignored.
                driver.wait("const b=document.querySelector('.connection');if(!b || b.disabled)return false;b.click();return true")
                driver.wait("return document.querySelector('.connection').textContent.includes('断开')")
                # Same 16ms observation cadence as Qt. No screenshots or WebDriver polling during load.
                driver.js("""
                const seen=new Set(); const p=window.perf={samples:[],timer_gaps_ms:[]};
                let last=performance.now();
                p.timer=setInterval(()=>{
                  const tick=performance.now();p.timer_gaps_ms.push(tick-last);last=tick;
                  const root=document.querySelector(arguments[0]==='terminal'?'.xterm-rows':'.log-surface');
                  let text='';
                  for(let node=root?.lastElementChild;node && text.length<8192;node=node.previousElementSibling) text=node.textContent+text;
                  text=text.slice(-8192);const now=Date.now();
                  for(const m of text.matchAll(/P([0-9]{8}):([0-9]{13})/g)) {
                    if(!seen.has(m[1])) {seen.add(m[1]);p.samples.push({id:Number(m[1]),latency_ms:now-Number(m[2])});}
                  }
                },16);
                """, mode)
            os.close(slave)
            slave = None
            thread = threading.Thread(target=feed, args=(master, args.duration, args.hz, args.size, sent, stopped))
            thread.start()
            # Keep the GUI undisturbed while measuring.
            resource_samples = []
            control_samples = []
            next_control = time.monotonic() + 2
            while thread.is_alive():
                if args.exercise_controls and driver and time.monotonic() >= next_control:
                    was_open = driver.js("return !!document.querySelector('.sidebar')")
                    before = time.monotonic()
                    driver.js("document.querySelector('[aria-label=\"切换配置区域\"]').click()")
                    driver.wait("return " + ("!" if was_open else "!!") + "document.querySelector('.sidebar')")
                    control_samples.append((time.monotonic() - before) * 1000)
                    next_control = time.monotonic() + 2
                point = resources(process.pid if process else driver.process.pid)
                point["elapsed_s"] = len(resource_samples)
                resource_samples.append(point)
                thread.join(1)
            if implementation == "qt":
                stdout, _ = process.communicate(timeout=15)
                if process.returncode:
                    raise RuntimeError(f"Qt exited {process.returncode}")
                raw = json.loads(stdout.strip().splitlines()[-1])
            else:
                time.sleep(2)
                raw = driver.js("clearInterval(window.perf.timer);return {samples:perf.samples,timer_gaps_ms:perf.timer_gaps_ms,status:document.querySelector('.statusbar').textContent}")
                before = time.monotonic()
                driver.js("document.querySelector('.connection').click()")
                driver.wait("return document.querySelector('.connection').textContent.includes('连接串口')")
                raw["disconnect_observed_ms"] = (time.monotonic()-before)*1000
            raw["resources"] = resource_samples
            raw["control_response_ms"] = control_samples
            result = {"implementation": implementation, "mode": mode, "input": sent,
                "latency_ms": summary([s["latency_ms"] for s in raw["samples"]]),
                "timer_gap_ms": summary(raw["timer_gaps_ms"]),
                "observed_samples": len(raw["samples"]), "raw": raw}
            if control_samples:
                result["control_response_ms"] = summary(control_samples)
            result["rss_mib"] = summary([r["rss_mib"] for r in resource_samples])
            if len(resource_samples) > 1:
                result["cpu_core_percent_approx"] = 100 * (resource_samples[-1]["cpu_s"]-resource_samples[0]["cpu_s"]) / (len(resource_samples)-1)
            for label, low, high in [("early", 0, .25), ("late", .75, 1)]:
                result[label+"_latency_ms"] = summary([s["latency_ms"] for s in raw["samples"]
                    if low * sent["sent_samples"] <= s["id"] < high * sent["sent_samples"]])
            # Missing view markers do not by themselves prove loss on the wire.
            result["unobserved_samples"] = sent["sent_samples"] - len(raw["samples"])
            return result
        finally:
            stopped.set()
            if thread:
                thread.join(2)
            try:
                if driver:
                    driver.close()
            finally:
                if process and process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
                os.close(master)
                if slave is not None:
                    os.close(slave)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--duration", type=int, default=30)
    p.add_argument("--hz", type=int, default=100)
    p.add_argument("--size", type=int, default=96)
    p.add_argument("--implementation", choices=["qt", "tauri", "both"], default="both")
    p.add_argument("--mode", choices=["debug", "terminal", "both"], default="both")
    p.add_argument("--qt-binary", type=Path, default=Path('/tmp/geekcom-perf-release/GeekCOMPerf'))
    p.add_argument("--tauri-binary", type=Path, default=ROOT/'src-tauri/target/release/geekcom')
    p.add_argument("--exercise-controls", action="store_true", help="Toggle sidebar during RX; separate from untouched latency baseline")
    p.add_argument("--tauri-no-timestamps", action="store_true", help="Disable Tauri debug timestamps for a controlled comparison")
    p.add_argument("--output", type=Path, default=ROOT/'benchmark-results/performance')
    args = p.parse_args()
    if not 1 <= args.duration <= 3600 or not 1 <= args.hz <= 2000 or not 26 <= args.size <= 4096:
        p.error("duration 1..3600, hz 1..2000, size 26..4096")
    if args.exercise_controls and args.implementation != "tauri":
        p.error("--exercise-controls requires --implementation tauri")
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"environment": platform.platform(), "parameters": {k:str(v) for k,v in vars(args).items()},
        "started_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "binary_sha256": {name: hashlib.sha256(path.read_bytes()).hexdigest()
            for name,path in [("qt",args.qt_binary),("tauri",args.tauri_binary)] if path.is_file()},
        "results": []}
    for implementation in (["qt", "tauri"] if args.implementation == "both" else [args.implementation]):
        for mode in (["debug", "terminal"] if args.mode == "both" else [args.mode]):
            print(f"RUN {implementation} {mode}", flush=True)
            try:
                result = run(args, implementation, mode)
            except Exception as exc:
                result = {"implementation": implementation, "mode": mode, "error": str(exc)}
            report["results"].append(result)
            (args.output/'results.json').write_text(json.dumps(report, indent=2))
            print(json.dumps({k:v for k,v in result.items() if k != 'raw'}), flush=True)
    if any('error' in r or r.get('input',{}).get('error') or r.get('observed_samples',0)==0 for r in report['results']):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
