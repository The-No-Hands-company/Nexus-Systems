import { useEffect, useState } from "react";

const rows: [string, string][] = [
  ["V", "Select / move"],
  ["H or Space", "Pan (hand)"],
  ["P", "Pen (freehand)"],
  ["R", "Rectangle"],
  ["E", "Ellipse"],
  ["L", "Line"],
  ["A", "Arrow"],
  ["T", "Text"],
  ["S", "Sticky note"],
  ["E", "Eraser (click toolbar; no hotkey)"],
  ["[ ]", "Send backward / bring forward"],
  ["Del / Backspace", "Delete selection"],
  ["Ctrl/Cmd + C / V / D", "Copy / Paste / Duplicate"],
  ["Ctrl/Cmd + Z / Shift+Z", "Undo / Redo"],
  ["Ctrl/Cmd + + / −", "Zoom in / out"],
  ["Ctrl/Cmd + 0", "Reset zoom & pan"],
  ["? or ⌘/", "Show shortcuts"],
];

export default function HelpOverlay() {
  const [open, setOpen] = useState(false);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const target = e.target as HTMLElement;
      if (target && (target.tagName === "INPUT" || target.tagName === "TEXTAREA")) return;
      if (e.key === "?") {
        e.preventDefault();
        setOpen((o) => !o);
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  return (
    <>
      <button
        onClick={() => setOpen(true)}
        title="Shortcuts (?)"
        className="w-7 h-7 rounded-md text-xs text-zinc-400 hover:bg-zinc-800 hover:text-zinc-200"
      >
        ?
      </button>
      {open && (
        <div
          className="fixed inset-0 z-50 flex items-center justify-center bg-black/60"
          onClick={() => setOpen(false)}
        >
          <div
            className="bg-zinc-900 border border-zinc-700 rounded-xl p-6 w-96 max-h-[80vh] overflow-y-auto shadow-2xl"
            onClick={(e) => e.stopPropagation()}
          >
            <div className="flex items-center justify-between mb-4">
              <h2 className="text-sm font-semibold text-zinc-200">Keyboard Shortcuts</h2>
              <button onClick={() => setOpen(false)} className="text-zinc-400 hover:text-white text-sm">✕</button>
            </div>
            <table className="w-full text-sm">
              <tbody>
                {rows.map(([k, desc]) => (
                  <tr key={k + desc} className="border-b border-zinc-800 last:border-0">
                    <td className="py-1.5 pr-4">
                      <kbd className="inline-block px-1.5 py-0.5 rounded bg-zinc-800 border border-zinc-700 text-xs text-zinc-200 font-mono whitespace-nowrap">
                        {k}
                      </kbd>
                    </td>
                    <td className="py-1.5 text-zinc-400">{desc}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </>
  );
}