import {
  memo,
  useCallback,
  useLayoutEffect,
  useRef,
  type RefObject,
} from "react";
import { useVirtualizer } from "@tanstack/react-virtual";
import { ascii, hex, time, type LogRow, type View } from "./model";

const LogContent = memo(function LogContent({
  row,
  view,
  timestamps,
}: {
  row: LogRow;
  view: View;
  timestamps: boolean;
}) {
  return (
    <>
      {timestamps && <span className="timestamp">{time(row.timestamp)}</span>}
      <span className="direction">RX</span>
      <span className="row-content">
        {view === "text" ? row.text : hex(row.data)}
      </span>
      {view === "dual" && (
        <span className="ascii-column">{ascii(row.data)}</span>
      )}
    </>
  );
});

export const LogRows = memo(function LogRows({
  rows,
  view,
  timestamps,
  selected,
  onSelect,
  scrollRef,
  follow,
  active,
}: {
  rows: LogRow[];
  view: View;
  timestamps: boolean;
  selected: number | null;
  onSelect: (id: number) => void;
  scrollRef: RefObject<HTMLDivElement | null>;
  follow: boolean;
  active: boolean;
}) {
  const getItemKey = useCallback((index: number) => rows[index].id, [rows]);
  const virtualizer = useVirtualizer({
    count: rows.length,
    getScrollElement: () => scrollRef.current,
    getItemKey,
    estimateSize: () => 48,
    overscan: 8,
    // Stable event keys also preserve the reading anchor when old rows expire.
    anchorTo: "end",
    followOnAppend: follow,
    enabled: active,
    paddingStart: 7,
    paddingEnd: 7,
  });
  const previousRows = useRef(rows);
  useLayoutEffect(() => {
    // The library retains measured sizes by key. Discard expired keys together
    // with the bounded raw history, otherwise long sessions grow this cache.
    const first = rows[0]?.id ?? Infinity;
    for (const old of previousRows.current) {
      if (old.id >= first) break;
      virtualizer.itemSizeCache.delete(old.id);
    }
    previousRows.current = rows;
  }, [rows, virtualizer]);
  useLayoutEffect(() => {
    virtualizer.measure();
  }, [view, timestamps, virtualizer]);
  useLayoutEffect(() => {
    const element = scrollRef.current;
    if (!element) return;
    let width = element.clientWidth;
    const observer = new ResizeObserver(() => {
      if (element.clientWidth !== width) {
        width = element.clientWidth;
        virtualizer.measure();
        if (follow) virtualizer.scrollToEnd();
      }
    });
    observer.observe(element);
    return () => observer.disconnect();
  }, [scrollRef, virtualizer, follow]);
  useLayoutEffect(() => {
    if (follow) virtualizer.scrollToEnd();
  }, [rows, view, timestamps, follow, virtualizer]);

  return (
    <div className="log-list" style={{ height: virtualizer.getTotalSize() }}>
      {virtualizer.getVirtualItems().map((item) => {
        const row = rows[item.index];
        return (
          <div
            key={item.key}
            ref={virtualizer.measureElement}
            data-index={item.index}
            data-event-id={row.id}
            className={selected === row.id ? "log-row highlighted" : "log-row"}
            style={{ transform: `translateY(${item.start}px)` }}
            onClick={() => onSelect(row.id)}
          >
            <LogContent row={row} view={view} timestamps={timestamps} />
          </div>
        );
      })}
    </div>
  );
});
