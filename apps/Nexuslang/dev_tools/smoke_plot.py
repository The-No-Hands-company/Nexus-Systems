"""Smoke test for the plot stdlib module."""
import sys
import os
import math
import importlib.util

# Import the plot module directly to avoid loading the full stdlib (with heavy
# optional deps like jsonschema/attrs that can be slow to initialise).
_HERE = os.path.dirname(os.path.abspath(__file__))
_PLOT_INIT = os.path.join(_HERE, "..", "src", "nlpl", "stdlib", "plot", "__init__.py")
_spec = importlib.util.spec_from_file_location("nexuslang.stdlib.plot", _PLOT_INIT)
_plot = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_plot)

_plot_linspace       = _plot._plot_linspace
_plot_arange         = _plot._plot_arange
_plot_normalize      = _plot._plot_normalize
_plot_scale          = _plot._plot_scale
_plot_stats          = _plot._plot_stats
_plot_color_scheme   = _plot._plot_color_scheme
_plot_line_text      = _plot._plot_line_text
_plot_bar_text       = _plot._plot_bar_text
_plot_scatter_text   = _plot._plot_scatter_text
_plot_hist_text      = _plot._plot_hist_text
_plot_line_svg       = _plot._plot_line_svg
_plot_scatter_svg    = _plot._plot_scatter_svg
_plot_bar_svg        = _plot._plot_bar_svg
_plot_hist_svg       = _plot._plot_hist_svg
_plot_pie_svg        = _plot._plot_pie_svg
_plot_multi_line_svg = _plot._plot_multi_line_svg
_plot_multi_bar_svg  = _plot._plot_multi_bar_svg
_plot_area_svg       = _plot._plot_area_svg
_plot_step_svg       = _plot._plot_step_svg
_plot_heatmap_svg    = _plot._plot_heatmap_svg
_plot_box_svg        = _plot._plot_box_svg
_plot_annotations_svg= _plot._plot_annotations_svg
_plot_save           = _plot._plot_save
_plot_to_html        = _plot._plot_to_html
_plot_combine_svg    = _plot._plot_combine_svg
_plot_svg_dimensions = _plot._plot_svg_dimensions

errors = []

def check(name, fn, *args, expect=None, **kwargs):
    try:
        result = fn(*args, **kwargs)
        if expect is not None:
            if result != expect:
                errors.append(f"FAIL {name}: expected {expect!r}, got {result!r}")
                return
        print(f"  OK  {name}: {str(result)[:80]}")
    except Exception as e:
        errors.append(f"FAIL {name}: {e}")

print("=== Data utilities ===")
ls = _plot_linspace(0, 1, 5)
assert ls == [0.0, 0.25, 0.5, 0.75, 1.0], f"linspace failed: {ls}"
print(f"  OK  plot_linspace(0,1,5): {ls}")

ar = _plot_arange(0, 1, 0.25)
assert abs(ar[0] - 0) < 1e-12 and len(ar) == 4, f"arange failed: {ar}"
print(f"  OK  plot_arange(0,1,0.25): {ar}")

norm = _plot_normalize([0, 5, 10])
assert norm == [0.0, 0.5, 1.0], f"normalize failed: {norm}"
print(f"  OK  plot_normalize: {norm}")

sc = _plot_scale([0, 5, 10], -1, 1)
assert abs(sc[0] - -1) < 1e-12 and abs(sc[2] - 1) < 1e-12, f"scale failed: {sc}"
print(f"  OK  plot_scale: {sc}")

st = _plot_stats([1, 2, 3, 4, 5])
assert st["count"] == 5 and abs(st["mean"] - 3.0) < 1e-12
print(f"  OK  plot_stats: mean={st['mean']}, std={st['std']:.4f}")

cs = _plot_color_scheme("default")
assert len(cs) == 10 and cs[0].startswith("#")
print(f"  OK  plot_color_scheme: {cs[:3]}")

print("\n=== ASCII charts ===")
x = list(range(10))
y = [v ** 2 for v in x]
check("plot_line_text", _plot_line_text, x, y, title="Square")
check("plot_bar_text", _plot_bar_text, ["A", "B", "C"], [10, 20, 15])
check("plot_scatter_text", _plot_scatter_text, x, y[:10])
check("plot_hist_text", _plot_hist_text, [1,2,2,3,3,3,4,4,5], bins=4)

print("\n=== SVG charts ===")
x2 = _plot_linspace(0, 2 * math.pi, 50)
y2 = [math.sin(v) for v in x2]
svg = _plot_line_svg(x2, y2, title="Sine Wave")
assert svg.startswith("<?xml"), "line_svg must start with XML declaration"
assert len(svg) > 100
dims = _plot_svg_dimensions(svg)
assert dims["width"] == 600 and dims["height"] == 400, f"dims wrong: {dims}"
print(f"  OK  plot_line_svg: {len(svg)} bytes, dims={dims}")

svg2 = _plot_scatter_svg(x2, y2, title="Scatter")
assert "<circle" in svg2
print(f"  OK  plot_scatter_svg: {len(svg2)} bytes")

svg3 = _plot_bar_svg(["Q1","Q2","Q3","Q4"], [100, 120, 90, 150], title="Revenue")
assert "<rect" in svg3
print(f"  OK  plot_bar_svg: {len(svg3)} bytes")

svg4 = _plot_hist_svg([1,1,2,2,2,3,3,4,5,5,5,5], bins=5, title="Histogram")
assert "<rect" in svg4
print(f"  OK  plot_hist_svg: {len(svg4)} bytes")

svg5 = _plot_pie_svg(["Python","Rust","Go","C++"], [40, 25, 20, 15], title="Languages")
assert "<path" in svg5
print(f"  OK  plot_pie_svg: {len(svg5)} bytes")

ml_series = [
    {"x": x2, "y": y2, "label": "sin"},
    {"x": x2, "y": [math.cos(v) for v in x2], "label": "cos"},
]
svg6 = _plot_multi_line_svg(ml_series, title="Sin/Cos")
assert svg6.count("<polyline") == 2
print(f"  OK  plot_multi_line_svg: {len(svg6)} bytes")

mb_cats = ["Jan","Feb","Mar","Apr"]
mb_series = [{"values":[10,12,8,15],"label":"A"},{"values":[6,9,11,7],"label":"B"}]
svg7 = _plot_multi_bar_svg(mb_cats, mb_series, title="Grouped Bars")
assert "<rect" in svg7
print(f"  OK  plot_multi_bar_svg: {len(svg7)} bytes")

svg8 = _plot_area_svg(x2, y2, title="Area")
assert "<polygon" in svg8
print(f"  OK  plot_area_svg: {len(svg8)} bytes")

svg9 = _plot_step_svg(list(range(8)), [0,1,1,2,2,3,3,4], title="Step")
assert "<polyline" in svg9
print(f"  OK  plot_step_svg: {len(svg9)} bytes")

mat = [[1,2,3],[4,5,6],[7,8,9]]
svg10 = _plot_heatmap_svg(mat, title="Heatmap", row_labels=["R1","R2","R3"], col_labels=["C1","C2","C3"])
assert "<rect" in svg10
print(f"  OK  plot_heatmap_svg: {len(svg10)} bytes")

import random
random.seed(42)
svg11 = _plot_box_svg([[random.gauss(0,1) for _ in range(100)],
                        [random.gauss(2,1.5) for _ in range(100)]],
                       labels=["A","B"], title="Box Plot")
assert "<rect" in svg11
print(f"  OK  plot_box_svg: {len(svg11)} bytes")

ann = _plot_annotations_svg(svg, [{"x":100,"y":100,"text":"note","arrow_to":(200,200)}])
assert "note" in ann
print(f"  OK  plot_annotations_svg: annotation injected")

print("\n=== SVG utilities ===")
html = _plot_to_html(svg, title="Test Chart")
assert "<!DOCTYPE html>" in html and "<svg" in html
print(f"  OK  plot_to_html: {len(html)} bytes")

combined = _plot_combine_svg([svg, svg2], cols=2)
assert combined.count('<g transform="translate') == 2
print(f"  OK  plot_combine_svg: {len(combined)} bytes")

import tempfile, os
with tempfile.NamedTemporaryFile(suffix=".svg", delete=False) as tf:
    path = tf.name
written = _plot_save(svg, path)
assert os.path.exists(written) and os.path.getsize(written) > 100
os.unlink(written)
print(f"  OK  plot_save: wrote and verified {written}")

print()
if errors:
    print("FAILURES:")
    for e in errors:
        print(" ", e)
    sys.exit(1)
else:
    print("ALL OK")
