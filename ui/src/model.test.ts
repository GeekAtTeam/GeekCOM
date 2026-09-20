import { describe, expect, it } from "vitest";
import {
  appendRows,
  ascii,
  exportText,
  hex,
  terminalInput,
  type LogRow,
} from "./model";
const row = (id: number, data: number[], text = ""): LogRow => ({
  id,
  session: 1,
  data,
  text,
  timestamp: 0,
  direction: "RX",
});
describe("raw byte presentation", () => {
  it("keeps nonprintable and non-ASCII bytes intact in HEX", () => {
    expect(hex([0, 13, 10, 65, 255])).toBe("00 0D 0A 41 FF");
    expect(ascii([0, 13, 10, 65, 255])).toBe("...A.");
    expect(exportText([row(1, [65, 0, 255])], "dual", false)).toBe(
      "41 00 FF  |  A..\n",
    );
  });
  it("exports the selected representation", () => {
    expect(exportText([row(1, [65], "A")], "text", false)).toBe("A\n");
    expect(exportText([row(1, [65], "A")], "hex", false)).toBe("41\n");
  });
  it("bounds long sessions even with automatic clearing disabled", () => {
    const rows = Array.from({ length: 4001 }, (_, i) => row(i, [65], "A"));
    const result = appendRows([], rows, false);
    expect(result).toHaveLength(4000);
    expect(result[0].id).toBe(1);
    expect(
      appendRows(
        [row(1, Array(2 * 1024 * 1024).fill(0))],
        [row(2, [1])],
        false,
      ).map((r) => r.id),
    ).toEqual([2]);
  });
  it("automatically clears at the display threshold", () => {
    expect(
      appendRows([row(1, Array(17000).fill(0))], [row(2, [1])], true).map(
        (r) => r.id,
      ),
    ).toEqual([2]);
  });
});
describe("terminal input", () => {
  it("preserves escapes/control bytes and encodes UTF-8", () => {
    expect(terminalInput("\x1b[A", true)).toEqual([27, 91, 65]);
    expect(terminalInput("\x03", true)).toEqual([3]);
    expect(terminalInput("中", true)).toEqual([228, 184, 173]);
  });
  it("applies enter and backspace preferences", () => {
    expect(terminalInput("\r", true)).toEqual([13, 10]);
    expect(terminalInput("\r", false)).toEqual([13]);
    expect(terminalInput("\x7f", true)).toEqual([8]);
  });
});
