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
    <div className="w-full h-full flex">
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
                type="range"
                min={slider.min}
                max={slider.max}
                step={0.01}
                value={filters[slider.key]}
                onChange={(e) => setFilter(slider.key, parseFloat(e.target.value))}
                className="w-full h-1 bg-zinc-700 rounded appearance-none cursor-pointer accent-blue-500"
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
              onClick={() => setTransform({ flipH: !transform.flipH })}
              className={`px-3 py-1.5 rounded ${transform.flipH ? "bg-blue-600 text-white" : "bg-zinc-800 text-zinc-300 hover:bg-zinc-700"}`}
            >
              Flip H
            </button>
            <button
              onClick={() => setTransform({ flipV: !transform.flipV })}
              className={`px-3 py-1.5 rounded ${transform.flipV ? "bg-blue-600 text-white" : "bg-zinc-800 text-zinc-300 hover:bg-zinc-700"}`}
            >
              Flip V
            </button>
            <button
              onClick={() => setTransform({ rotation: ((transform.rotation + 90) % 360) })}
              className="px-3 py-1.5 rounded bg-zinc-800 text-zinc-300 hover:bg-zinc-700"
            >
              Rotate 90°
            </button>
            <button
              onClick={() => setTransform({ rotation: 0 })}
              className="px-3 py-1.5 rounded bg-zinc-800 text-zinc-300 hover:bg-zinc-700"
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
              onClick={() => setCropTool({ active: !cropTool.active })}
              className={`px-3 py-1.5 rounded ${cropTool.active ? "bg-blue-600 text-white" : "bg-zinc-800 text-zinc-300 hover:bg-zinc-700"}`}
            >
              {cropTool.active ? "Done" : "Crop"}
            </button>
            <button
              onClick={() => setCropTool({ active: false, x: 0, y: 0, width: 0, height: 0 })}
              className="px-3 py-1.5 rounded bg-zinc-800 text-zinc-300 hover:bg-zinc-700"
            >
              Reset
            </button>
          </div>
        </div>

        <div className="border-t border-zinc-800 my-4 pt-4 flex gap-2 text-xs">
          <button
            onClick={resetFilters}
            className="flex-1 px-3 py-1.5 rounded bg-zinc-800 text-zinc-300 hover:bg-zinc-700"
          >
            Reset All
          </button>
          <button className="flex-1 px-3 py-1.5 rounded bg-blue-600 text-white hover:bg-blue-500">
            Apply
          </button>
        </div>
      </div>
    </div>
  );
}
