import { useEffect, useId, useRef, useState } from "react";
import { ChevronDown } from "lucide-react";

type Props = {
  value: string;
  ports: string[];
  disabled: boolean;
  onChange: (value: string, selected: boolean) => void;
};

// An editable combobox with an explicit popup: native datalist suggestions
// are not consistently exposed by the Linux WebKit desktop runtime.
export function PortComboBox({ value, ports, disabled, onChange }: Props) {
  const [open, setOpen] = useState(false);
  const [query, setQuery] = useState("");
  const [active, setActive] = useState(-1);
  const root = useRef<HTMLDivElement>(null);
  const input = useRef<HTMLInputElement>(null);
  const listId = useId();
  const visible = ports.filter((port) =>
    port.toLowerCase().includes(query.toLowerCase()),
  );
  const expanded = open && !disabled;

  useEffect(() => {
    if (!expanded) return;
    const outside = (event: PointerEvent) => {
      if (!root.current?.contains(event.target as Node)) setOpen(false);
    };
    document.addEventListener("pointerdown", outside);
    return () => document.removeEventListener("pointerdown", outside);
  }, [expanded]);
  useEffect(() => {
    if (disabled) setOpen(false);
  }, [disabled]);
  useEffect(() => {
    if (expanded && active >= 0)
      document
        .getElementById(`${listId}-${active}`)
        ?.scrollIntoView({ block: "nearest" });
  }, [active, expanded, listId]);

  const select = (port: string) => {
    onChange(port, true);
    setOpen(false);
    setQuery("");
    input.current?.focus();
  };
  return (
    <div
      className="port-combobox"
      ref={root}
      onBlur={(event) => {
        if (!event.currentTarget.contains(event.relatedTarget as Node))
          setOpen(false);
      }}
    >
      <input
        ref={input}
        aria-label="串口设备"
        role="combobox"
        aria-expanded={expanded}
        aria-controls={expanded ? listId : undefined}
        aria-autocomplete="list"
        aria-activedescendant={
          expanded && active >= 0 && active < visible.length
            ? `${listId}-${active}`
            : undefined
        }
        autoComplete="off"
        spellCheck={false}
        placeholder="选择或输入串口"
        disabled={disabled}
        value={value}
        onChange={(event) => {
          onChange(event.target.value, false);
          setQuery(event.target.value);
          setActive(-1);
          setOpen(true);
        }}
        onKeyDown={(event) => {
          if (event.nativeEvent.isComposing) return;
          if (event.key === "ArrowDown" || event.key === "ArrowUp") {
            event.preventDefault();
            if (!expanded) {
              setQuery("");
              setOpen(true);
              setActive(
                ports.length
                  ? event.key === "ArrowDown"
                    ? 0
                    : ports.length - 1
                  : -1,
              );
            } else {
              setActive((i) => {
                if (!visible.length) return -1;
                if (i < 0)
                  return event.key === "ArrowDown" ? 0 : visible.length - 1;
                return (
                  (i + (event.key === "ArrowDown" ? 1 : -1) + visible.length) %
                  visible.length
                );
              });
            }
          } else if (event.key === "Enter" && expanded) {
            event.preventDefault();
            if (active >= 0 && active < visible.length) select(visible[active]);
            else setOpen(false);
          } else if (event.key === "Escape") {
            event.preventDefault();
            setOpen(false);
          } else if (event.key === "Tab") setOpen(false);
        }}
      />
      <button
        type="button"
        className="port-toggle"
        aria-label="展开串口列表"
        aria-expanded={expanded}
        aria-controls={expanded ? listId : undefined}
        disabled={disabled}
        tabIndex={-1}
        onMouseDown={(event) => event.preventDefault()}
        onClick={() => {
          input.current?.focus();
          setQuery("");
          setActive(ports.indexOf(value));
          setOpen(!expanded);
        }}
      >
        <ChevronDown size={14} />
      </button>
      {expanded && (
        <ul
          className="port-dropdown"
          role="listbox"
          aria-label="串口列表"
          id={listId}
        >
          {visible.map((port, index) => (
            <li
              key={port}
              id={`${listId}-${index}`}
              role="option"
              aria-selected={active === index}
              className={
                active === index ? "port-option active" : "port-option"
              }
              onPointerDown={(event) => event.preventDefault()}
              onClick={() => select(port)}
            >
              {port}
            </li>
          ))}
          {!visible.length && (
            <li className="port-empty" role="presentation">
              {ports.length
                ? "无匹配端口，可直接输入"
                : "未发现串口，可直接输入"}
            </li>
          )}
        </ul>
      )}
    </div>
  );
}
