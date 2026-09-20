import { useEffect, useRef, useImperativeHandle, forwardRef } from "react";
import { Terminal } from "@xterm/xterm";
import { FitAddon } from "@xterm/addon-fit";
import "@xterm/xterm/css/xterm.css";
export type TerminalHandle = {
  write: (data: Uint8Array) => void;
  clear: () => void;
};
export const TerminalView = forwardRef<
  TerminalHandle,
  { active: boolean; onInput: (text: string) => void }
>(({ active, onInput }, ref) => {
  const host = useRef<HTMLDivElement>(null);
  const terminal = useRef<Terminal | null>(null);
  const input = useRef(onInput);
  input.current = onInput;
  useImperativeHandle(
    ref,
    () => ({
      write: (d) => terminal.current?.write(d),
      clear: () => terminal.current?.reset(),
    }),
    [],
  );
  useEffect(() => {
    const term = new Terminal({
      cursorBlink: true,
      fontFamily: '"JetBrains Mono", "SFMono-Regular", Consolas, monospace',
      fontSize: 13,
      scrollback: 5000,
      theme: {
        background: "#111419",
        foreground: "#d7e0e9",
        cursor: "#ee6b67",
        selectionBackground: "#374555",
      },
    });
    const fit = new FitAddon();
    term.loadAddon(fit);
    term.open(host.current!);
    terminal.current = term;
    const d = term.onData((s) => input.current(s));
    const resize = new ResizeObserver(() => {
      if (host.current?.clientWidth) fit.fit();
    });
    resize.observe(host.current!);
    return () => {
      resize.disconnect();
      d.dispose();
      term.dispose();
      terminal.current = null;
    };
  }, []);
  useEffect(() => {
    if (active) terminal.current?.focus();
  }, [active]);
  return <div ref={host} className="terminal-host" aria-label="串口终端" />;
});
