export type Config = {
  port: string;
  baud: number;
  dataBits: number;
  parity: string;
  stopBits: string;
};
export type Status = {
  session: number;
  connected: boolean;
  port: string;
  rx: number;
  tx: number;
  autoRunning: boolean;
  autoCount: number;
  fileRunning: boolean;
  fileSent: number;
  fileSize: number;
  error: string | null;
  dropped: number;
};
export type SerialEvent = {
  session: number;
  id: number;
  timestamp: number;
  direction: "RX" | "TX";
  data: number[];
};
export type Batch = { status: Status; events: SerialEvent[] };
export type Port = { name: string; description: string };
export type View = "text" | "hex" | "dual";
export type LogRow = SerialEvent & { text: string };
export const emptyStatus: Status = {
  session: 0,
  connected: false,
  port: "",
  rx: 0,
  tx: 0,
  autoRunning: false,
  autoCount: 0,
  fileRunning: false,
  fileSent: 0,
  fileSize: 0,
  error: null,
  dropped: 0,
};
const statusKeys = Object.keys(emptyStatus) as (keyof Status)[];
export const sameStatus = (a: Status, b: Status) =>
  statusKeys.every((key) => a[key] === b[key]);

export const rates = [
  1200, 2400, 4800, 9600, 19200, 38400, 57600, 100000, 115200, 230400, 460800,
  921600, 1500000,
];
export const hex = (bytes: number[]) =>
  bytes.map((b) => b.toString(16).padStart(2, "0").toUpperCase()).join(" ");
export const ascii = (bytes: number[]) =>
  bytes
    .map((b) => (b >= 32 && b < 127 ? String.fromCharCode(b) : "."))
    .join("");
export const time = (ts: number) =>
  new Date(ts).toLocaleTimeString("zh-CN", { hour12: false }) +
  "." +
  String(ts % 1000).padStart(3, "0");
export function exportText(
  rows: LogRow[],
  view: View,
  timestamps: boolean,
): string {
  return rows
    .map(
      (r) =>
        `${timestamps ? `[${time(r.timestamp)}] ` : ""}${view === "text" ? r.text : view === "hex" ? hex(r.data) : `${hex(r.data)}  |  ${ascii(r.data)}`}\n`,
    )
    .join("");
}
// Bound both raw and rendered data. Auto-clear follows the original 50k display
// threshold; the unconditional cap protects sessions when auto-clear is off.
export function appendRows(
  old: LogRow[],
  incoming: LogRow[],
  autoClear: boolean,
): LogRow[] {
  let rows = [...old, ...incoming];
  if (
    autoClear &&
    old.reduce((n, r) => n + Math.max(r.text.length, r.data.length * 3), 0) >
      50_000
  )
    rows = [...incoming];
  let bytes = rows.reduce((n, r) => n + r.data.length, 0);
  let start = 0;
  while (
    (bytes > 2 * 1024 * 1024 || rows.length - start > 4000) &&
    start < rows.length
  )
    bytes -= rows[start++].data.length;
  return rows.slice(start);
}
export function terminalInput(input: string, crlf: boolean): number[] {
  return [
    ...new TextEncoder().encode(
      input === "\r"
        ? crlf
          ? "\r\n"
          : "\r"
        : input === "\x7f"
          ? "\x08"
          : input,
    ),
  ];
}
