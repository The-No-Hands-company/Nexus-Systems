import { useCallback } from "react";
import { useEditorStore } from "../stores/useEditorStore";

export default function PhotoEditor() {
  const viewingPhoto = useEditorStore((s) => s.viewingPhoto);
  const filters = useEditorStore((s) => s.filters);
  const setFilter = useEditorStore((s) => s.setFilter);
  const resetFilters = useEditorStore((s) => s.resetFilters);
  const cropTool = useEditorStore((s) => s.cropTool);
  const setCropTool = useEditorStore((s) => s.setCropTool);
  const transform = useEditorStore((s) => s.transform);
  const setTransform = useEditorStore((s) => s.setTransform);

  if (!viewingPhoto) return null;

  const sliders: { key: keyof typeof filters; label: string; min: number; max: number }[] = [
    { key: "brightness", label: "Brightness", min: -1, max: 1 },
    { key: "contrast", label: "Contrast", min: -1, max: 1 },
    { key: "saturation", label: "Saturation", min: -1, max: 1 },
    { key: "blur", label: "Blur", min: 0, max: 10 },
    { key: "sharpen", label: "Sharpen", min: 0, max: 5 },
  ];

  return (
    <section aria-label={`Editing ${viewingPhoto.title || "photo"}`} className="w-full h-full flex">
      <div className="flex-1 flex items-center justify-center bg-zinc-950">
        <img
          src={viewingPhoto.url}
          alt={viewingPhoto.title}
          className="max-w-full max-h-full object-contain"
          style={{
            filter: `brightness(${1 + filters.brightness}) contrast(${1 + filters.contrast}) saturate(${1 + filters.saturation}) blur(${filters.blur}px)`,
            transform: `${transform.flipH ? "scaleX(-1)" : ""} ${transform.flipV ? "scaleY(-1)" : ""} rotate(${transform.rotation}deg)`,
          }}
        />
      </div>
      <div className="w-72 border-l border-zinc-800 bg-zinc-900 p-4 overflow-y-auto shrink-0">
        <div className="text-xs font-medium text-zinc-500 uppercase tracking-wider mb-3">
          Adjustments
        </div>
        <div className="space-y-3">
          {sliders.map((slider) => (
            <div key={slider.key}>
              <div className="flex justify-between text-xs text-zinc-400 mb-1">
                <span>{slider.label}</span>
                <span>{filters[slider.key].toFixed(2)}</span>
              </div>
              <input
                aria-label={slider.label}
                type="range"
                min={slider.min}
                max={slider.max}
                step={0.01}
                value={filters[slider.key]}
                onChange={(e) => setFilter(slider.key, parseFloat(e.target.value))}
                className="w-full h-2 bg-zinc-700 rounded appearance-none cursor-pointer accent-accent focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
              />
            </div>
          ))}
        </div>

        <div className="border-t border-zinc-800 my-4 pt-4">
          <div className="text-xs font-medium text-zinc-500 uppercase tracking-wider mb-3">
            Transform
          </div>
          <div className="grid grid-cols-2 gap-2 text-xs">
            <button
              type="button"
              onClick={() => setTransform({ flipH: !transform.flipH })}
              className={`min-h-[44px] px-3 py-1.5 rounded focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ${transform.flipH ? "bg-accent text-zinc-900" : "bg-zinc-800 text-zinc-300 hover:bg-zinc-700"}`}
            >
              Flip H
            </button>
            <button
              type="button"
              onClick={() => setTransform({ flipV: !transform.flipV })}
              className={`min-h-[44px] px-3 py-1.5 rounded focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ${transform.flipV ? "bg-accent text-zinc-900" : "bg-zinc-800 text-zinc-300 hover:bg-zinc-700"}`}
            >
              Flip V
            </button>
            <button
              type="button"
              onClick={() => setTransform({ rotation: ((transform.rotation + 90) % 360) })}
              className="min-h-[44px] px-3 py-1.5 rounded bg-zinc-800 text-zinc-300 hover:bg-zinc-700 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
            >
              Rotate 90°
            </button>
            <button
              type="button"
              onClick={() => setTransform({ rotation: 0 })}
              className="min-h-[44px] px-3 py-1.5 rounded bg-zinc-800 text-zinc-300 hover:bg-zinc-700 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
            >
              Reset Rot
            </button>
          </div>
        </div>

        <div className="border-t border-zinc-800 my-4 pt-4">
          <div className="text-xs font-medium text-zinc-500 uppercase tracking-wider mb-3">
            Crop
          </div>
          <div className="grid grid-cols-2 gap-2 text-xs">
            <button
              type="button"
              onClick={() => setCropTool({ active: !cropTool.active })}
              aria-pressed={cropTool.active}
              className={`min-h-[44px] px-3 py-1.5 rounded focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ${cropTool.active ? "bg-accent text-zinc-900" : "bg-zinc-800 text-zinc-300 hover:bg-zinc-700"}`}
            >
              {cropTool.active ? "Done" : "Crop"}
            </button>
            <button
              type="button"
              onClick={() => setCropTool({ active: false, x: 0, y: 0, width: 0, height: 0 })}
              className="min-h-[44px] px-3 py-1.5 rounded bg-zinc-800 text-zinc-300 hover:bg-zinc-700 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
            >
              Reset
            </button>
          </div>
        </div>

        <div className="border-t border-zinc-800 my-4 pt-4 flex gap-2 text-xs">
          <button
            type="button"
            onClick={resetFilters}
            className="flex-1 min-h-[44px] px-3 py-1.5 rounded bg-zinc-800 text-zinc-300 hover:bg-zinc-700 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
          >
            Reset All
          </button>
          <button type="button" className="flex-1 min-h-[44px] px-3 py-1.5 rounded bg-accent text-zinc-900 hover:bg-accent-hover focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent">
            Apply
          </button>
        </div>
      </div>
    </section>
  );
}
