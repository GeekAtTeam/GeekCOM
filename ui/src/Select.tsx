import { useEffect, useId, useRef, useState } from "react";
import { ChevronDown } from "lucide-react";

export type SelectOption<T extends string | number> = {
  value: T;
  label: string;
};

type Props<T extends string | number> = {
  value: T;
  options: SelectOption<T>[];
  disabled?: boolean;
  "aria-label": string;
  onChange: (value: T) => void;
};

export function Select<T extends string | number>({
  value,
  options,
  disabled = false,
  "aria-label": label,
  onChange,
}: Props<T>) {
  const [open, setOpen] = useState(false);
  const [active, setActive] = useState(-1);
  const root = useRef<HTMLDivElement>(null);
  const trigger = useRef<HTMLButtonElement>(null);
  const listId = useId();
  const expanded = open && !disabled;
  const selected = options.find((option) => option.value === value);
  const selectedIndex = options.findIndex((option) => option.value === value);

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

  const pick = (next: T) => {
    onChange(next);
    setOpen(false);
    trigger.current?.focus();
  };

  return (
    <div
      className="menu-select"
      ref={root}
      onBlur={(event) => {
        if (!event.currentTarget.contains(event.relatedTarget as Node))
          setOpen(false);
      }}
    >
      <button
        ref={trigger}
        type="button"
        className="menu-select-trigger"
        aria-label={label}
        aria-haspopup="listbox"
        aria-expanded={expanded}
        aria-controls={expanded ? listId : undefined}
        disabled={disabled}
        onClick={() => {
          setActive(selectedIndex);
          setOpen(!expanded);
        }}
        onKeyDown={(event) => {
          if (event.nativeEvent.isComposing) return;
          if (event.key === "ArrowDown" || event.key === "ArrowUp") {
            event.preventDefault();
            if (!expanded) {
              setOpen(true);
              setActive(
                options.length
                  ? event.key === "ArrowDown"
                    ? Math.max(selectedIndex, 0)
                    : selectedIndex >= 0
                      ? selectedIndex
                      : options.length - 1
                  : -1,
              );
            } else {
              setActive((i) => {
                if (!options.length) return -1;
                if (i < 0)
                  return event.key === "ArrowDown" ? 0 : options.length - 1;
                return (
                  (i + (event.key === "ArrowDown" ? 1 : -1) + options.length) %
                  options.length
                );
              });
            }
          } else if (event.key === "Enter" || event.key === " ") {
            if (expanded) {
              event.preventDefault();
              if (active >= 0 && active < options.length)
                pick(options[active].value);
              else setOpen(false);
            }
          } else if (event.key === "Escape") {
            event.preventDefault();
            setOpen(false);
          }
        }}
      >
        <span>{selected?.label ?? String(value)}</span>
        <ChevronDown size={14} />
      </button>
      {expanded && (
        <ul
          className="menu-select-dropdown"
          role="listbox"
          aria-label={label}
          id={listId}
        >
          {options.map((option, index) => (
            <li
              key={String(option.value)}
              id={`${listId}-${index}`}
              role="option"
              aria-selected={option.value === value}
              className={[
                "menu-select-option",
                option.value === value ? "chosen" : "",
                active === index ? "active" : "",
              ]
                .filter(Boolean)
                .join(" ")}
              onPointerDown={(event) => event.preventDefault()}
              onClick={() => pick(option.value)}
              onMouseEnter={() => setActive(index)}
            >
              {option.label}
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
