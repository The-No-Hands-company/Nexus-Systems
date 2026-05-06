"""
NLPL Standard Library - Plotting and Visualization Module

Pure-Python implementation: zero external dependencies.
Provides two output modes:
  - ASCII/text charts for terminal display
  - SVG chart generation for file output / embedding in HTML

Registered functions (26 total):
  Data utilities (6):
    plot_linspace, plot_arange, plot_normalize, plot_scale,
    plot_stats, plot_color_scheme

  ASCII / terminal charts (4):
    plot_line_text, plot_bar_text, plot_scatter_text, plot_hist_text

  SVG primitives & utilities (4):
    plot_save, plot_to_html, plot_combine_svg, plot_svg_dimensions

  SVG single-series charts (5):
    plot_line_svg, plot_scatter_svg, plot_bar_svg,
    plot_hist_svg, plot_pie_svg

  SVG multi-series charts (2):
    plot_multi_line_svg, plot_multi_bar_svg

  SVG extras (5):
    plot_area_svg, plot_step_svg, plot_heatmap_svg,
    plot_box_svg, plot_annotations_svg
"""

import math
import colorsys


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def _to_float_list(seq):
    """Convert a sequence to a list of floats, raising on bad values."""
    out = []
    for v in seq:
        try:
            out.append(float(v))
        except (TypeError, ValueError) as exc:
            raise TypeError(f"plot: expected numeric value, got {type(v).__name__}: {v!r}") from exc
    return out


def _check_equal_length(x, y, fname="plot"):
    if len(x) != len(y):
        raise ValueError(f"{fname}: x and y must have equal length ({len(x)} vs {len(y)})")


def _safe_range(values):
    """Return (min, max) of values; raises if empty."""
    if not values:
        raise ValueError("plot: cannot compute range of empty sequence")
    lo, hi = values[0], values[0]
    for v in values[1:]:
        if v < lo:
            lo = v
        if v > hi:
            hi = v
    return lo, hi


def _lerp(a, b, t):
    return a + (b - a) * t


def _xml_escape(text):
    return (str(text)
            .replace("&", "&amp;")
            .replace("<", "&lt;")
            .replace(">", "&gt;")
            .replace('"', "&quot;"))


# Pre-defined color palettes (colorblind-friendly where possible)
_PALETTES = {
    "default":    ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728",
                   "#9467bd", "#8c564b", "#e377c2", "#7f7f7f",
                   "#bcbd22", "#17becf"],
    "pastel":     ["#aec6cf", "#ffb347", "#b5ead7", "#ff9cac",
                   "#c9a0e8", "#f8c8a0", "#f4c2c2", "#c5e3f7",
                   "#d4edac", "#a0d0e8"],
    "dark":       ["#003f5c", "#2f4b7c", "#665191", "#a05195",
                   "#d45087", "#f95d6a", "#ff7c43", "#ffa600",
                   "#7ec8a0", "#44a8b3"],
    "grayscale":  ["#000000", "#222222", "#444444", "#666666",
                   "#888888", "#aaaaaa", "#cccccc", "#eeeeee",
                   "#bbbbbb", "#333333"],
    "warm":       ["#e63946", "#f4a261", "#e9c46a", "#f3722c",
                   "#90be6d", "#43aa8b", "#4d908e", "#577590",
                   "#277da1", "#c77dff"],
}

_ASCII_CHARS = " .:-=+*#%@"


# ---------------------------------------------------------------------------
# Data utilities
# ---------------------------------------------------------------------------

def _plot_linspace(start, stop, n=50):
    """Return n evenly-spaced floats from start to stop (inclusive)."""
    start, stop = float(start), float(stop)
    n = int(n)
    if n < 1:
        raise ValueError("plot_linspace: n must be >= 1")
    if n == 1:
        return [start]
    step = (stop - start) / (n - 1)
    return [start + i * step for i in range(n)]


def _plot_arange(start, stop, step=1.0):
    """Return floats from start up to (but not including) stop with given step."""
    start, stop, step = float(start), float(stop), float(step)
    if step == 0:
        raise ValueError("plot_arange: step cannot be zero")
    result = []
    x = start
    if step > 0:
        while x < stop - 1e-12 * abs(step):
            result.append(x)
            x += step
    else:
        while x > stop + 1e-12 * abs(step):
            result.append(x)
            x += step
    return result


def _plot_normalize(data):
    """Normalize data list to [0.0, 1.0]."""
    data = _to_float_list(data)
    lo, hi = _safe_range(data)
    span = hi - lo
    if span == 0:
        return [0.5] * len(data)
    return [(v - lo) / span for v in data]


def _plot_scale(data, min_val, max_val):
    """Scale data list to [min_val, max_val]."""
    data = _to_float_list(data)
    min_val, max_val = float(min_val), float(max_val)
    norm = _plot_normalize(data)
    return [_lerp(min_val, max_val, t) for t in norm]


def _plot_stats(data):
    """Return a dict with min, max, mean, std, median, count for data."""
    data = _to_float_list(data)
    n = len(data)
    if n == 0:
        return {"count": 0, "min": None, "max": None,
                "mean": None, "std": None, "median": None}
    lo, hi = _safe_range(data)
    mean = sum(data) / n
    variance = sum((v - mean) ** 2 for v in data) / n
    std = math.sqrt(variance)
    sorted_data = sorted(data)
    if n % 2 == 1:
        median = sorted_data[n // 2]
    else:
        median = (sorted_data[n // 2 - 1] + sorted_data[n // 2]) / 2.0
    return {
        "count": n, "min": lo, "max": hi,
        "mean": mean, "std": std, "median": median,
    }


def _plot_color_scheme(name="default"):
    """Return the named color palette as a list of hex strings."""
    name = str(name).lower()
    if name not in _PALETTES:
        available = ", ".join(_PALETTES.keys())
        raise ValueError(f"plot_color_scheme: unknown palette '{name}'. Available: {available}")
    return list(_PALETTES[name])


# ---------------------------------------------------------------------------
# ASCII / terminal charts
# ---------------------------------------------------------------------------

def _make_ascii_canvas(width, height):
    """Create a 2-D character grid filled with spaces."""
    return [[" "] * width for _ in range(height)]


def _canvas_to_str(canvas):
    return "\n".join("".join(row) for row in canvas)


def _plot_line_text(x, y, width=60, height=20, title=""):
    """
    Render a line chart as an ASCII string.

    Parameters
    ----------
    x, y   : numeric sequences of equal length
    width  : character width of the plot area
    height : character height of the plot area
    title  : optional title printed above the plot

    Returns
    -------
    str
    """
    x = _to_float_list(x)
    y = _to_float_list(y)
    _check_equal_length(x, y, "plot_line_text")
    width, height = max(int(width), 10), max(int(height), 5)

    x_lo, x_hi = _safe_range(x)
    y_lo, y_hi = _safe_range(y)
    x_span = x_hi - x_lo or 1.0
    y_span = y_hi - y_lo or 1.0

    canvas = _make_ascii_canvas(width, height)

    # Plot points
    prev_col = None
    prev_row = None
    for xi, yi in zip(x, y):
        col = int((xi - x_lo) / x_span * (width - 1))
        row = height - 1 - int((yi - y_lo) / y_span * (height - 1))
        col = max(0, min(col, width - 1))
        row = max(0, min(row, height - 1))
        canvas[row][col] = "*"
        # Simple line interpolation between consecutive points
        if prev_col is not None:
            steps = max(abs(col - prev_col), abs(row - prev_row))
            for s in range(1, steps):
                t = s / steps
                ic = int(_lerp(prev_col, col, t))
                ir = int(_lerp(prev_row, row, t))
                if 0 <= ir < height and 0 <= ic < width:
                    if canvas[ir][ic] == " ":
                        canvas[ir][ic] = "."
        prev_col, prev_row = col, row

    lines = []
    if title:
        lines.append(title.center(width + 6))
    for i, row in enumerate(canvas):
        y_val = y_hi - i * y_span / (height - 1) if height > 1 else y_lo
        prefix = f"{y_val:6.2f}|"
        lines.append(prefix + "".join(row))
    # X-axis labels
    x_axis = " " * 7 + f"{x_lo:.2f}" + " " * (width - 12) + f"{x_hi:.2f}"
    lines.append(" " * 7 + "-" * width)
    lines.append(x_axis)
    return "\n".join(lines)


def _plot_bar_text(labels, values, width=60, height=20, title=""):
    """
    Render a horizontal bar chart as an ASCII string.

    Parameters
    ----------
    labels : sequence of label strings
    values : sequence of numeric values
    width  : total character width
    height : ignored (auto-computed from number of bars)
    title  : optional title

    Returns
    -------
    str
    """
    labels = [str(lb) for lb in labels]
    values = _to_float_list(values)
    _check_equal_length(labels, values, "plot_bar_text")
    width = max(int(width), 20)

    v_max = max(abs(v) for v in values) if values else 1.0
    if v_max == 0:
        v_max = 1.0

    bar_area = width - 2  # leave 2 chars for label
    max_label = max(len(lb) for lb in labels) if labels else 4
    max_label = min(max_label, 20)
    bar_width = width - max_label - 8  # room for label, value, bar
    bar_width = max(bar_width, 10)

    lines = []
    if title:
        lines.append(title)
        lines.append("-" * width)
    for lb, v in zip(labels, values):
        bar_len = int(abs(v) / v_max * bar_width)
        bar_str = "#" * bar_len
        sign = "-" if v < 0 else " "
        line = f"{lb[:max_label]:<{max_label}} |{sign}{bar_str:<{bar_width}} {v:>8.3g}"
        lines.append(line)
    return "\n".join(lines)


def _plot_scatter_text(x, y, width=60, height=20, title=""):
    """
    Render a scatter plot as an ASCII string.

    Parameters
    ----------
    x, y   : numeric sequences of equal length
    width  : character width
    height : character height
    title  : optional title

    Returns
    -------
    str
    """
    x = _to_float_list(x)
    y = _to_float_list(y)
    _check_equal_length(x, y, "plot_scatter_text")
    width, height = max(int(width), 10), max(int(height), 5)

    x_lo, x_hi = _safe_range(x)
    y_lo, y_hi = _safe_range(y)
    x_span = x_hi - x_lo or 1.0
    y_span = y_hi - y_lo or 1.0

    canvas = _make_ascii_canvas(width, height)
    for xi, yi in zip(x, y):
        col = int((xi - x_lo) / x_span * (width - 1))
        row = height - 1 - int((yi - y_lo) / y_span * (height - 1))
        col = max(0, min(col, width - 1))
        row = max(0, min(row, height - 1))
        canvas[row][col] = "o"

    lines = []
    if title:
        lines.append(title.center(width + 6))
    for i, row in enumerate(canvas):
        y_val = y_hi - i * y_span / (height - 1) if height > 1 else y_lo
        lines.append(f"{y_val:6.2f}|" + "".join(row))
    x_axis = " " * 7 + f"{x_lo:.2f}" + " " * (width - 12) + f"{x_hi:.2f}"
    lines.append(" " * 7 + "-" * width)
    lines.append(x_axis)
    return "\n".join(lines)


def _compute_histogram_bins(data, bins):
    """Return (bin_edges, counts) for data."""
    data = sorted(data)
    lo, hi = data[0], data[-1]
    span = hi - lo
    if span == 0:
        # All values identical — put every value in the first bin; remaining empty
        edges = [lo + i for i in range(bins + 1)]
        counts = [len(data)] + [0] * (bins - 1)
        return edges, counts
    edges = [lo + i * span / bins for i in range(bins + 1)]
    counts = [0] * bins
    for v in data:
        idx = int((v - lo) / span * bins)
        if idx >= bins:
            idx = bins - 1
        counts[idx] += 1
    return edges, counts


def _plot_hist_text(data, bins=10, width=60, height=20, title=""):
    """
    Render a histogram as an ASCII string.

    Parameters
    ----------
    data   : numeric sequence
    bins   : number of bins
    width  : character width
    height : character height
    title  : optional title

    Returns
    -------
    str
    """
    data = _to_float_list(data)
    bins = max(int(bins), 2)
    if not data:
        return "(empty)"
    edges, counts = _compute_histogram_bins(data, bins)
    labels = [f"[{edges[i]:.2g},{edges[i+1]:.2g})" for i in range(bins)]
    return _plot_bar_text(labels, counts, width=width, height=height, title=title)


# ---------------------------------------------------------------------------
# SVG helpers
# ---------------------------------------------------------------------------

_SVG_HEADER = (
    '<?xml version="1.0" encoding="UTF-8"?>\n'
    '<svg xmlns="http://www.w3.org/2000/svg" '
    'xmlns:xlink="http://www.w3.org/1999/xlink" '
    'width="{w}" height="{h}" viewBox="0 0 {w} {h}">\n'
)

_FONT = "font-family=\"'Segoe UI', Arial, sans-serif\""


def _svg_open(w, h):
    return _SVG_HEADER.format(w=w, h=h)


def _svg_close():
    return "</svg>\n"


def _svg_rect(x, y, w, h, fill="#ffffff", stroke="none", stroke_width=1, rx=0):
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" '
            f'fill="{fill}" stroke="{stroke}" stroke-width="{stroke_width}" rx="{rx}"/>\n')


def _svg_text(x, y, text, size=12, color="#333333", anchor="middle",
              bold=False, rotate=None):
    weight = "bold" if bold else "normal"
    transform = f' transform="rotate({rotate},{x},{y})"' if rotate else ""
    return (f'<text x="{x}" y="{y}" {_FONT} font-size="{size}" '
            f'fill="{color}" text-anchor="{anchor}" font-weight="{weight}"'
            f'{transform}>{_xml_escape(text)}</text>\n')


def _svg_line(x1, y1, x2, y2, color="#cccccc", width=1, dash=""):
    da = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" '
            f'stroke="{color}" stroke-width="{width}"{da}/>\n')


def _svg_polyline(points, color="#1f77b4", width=2, fill="none"):
    pts = " ".join(f"{x:.2f},{y:.2f}" for x, y in points)
    return (f'<polyline points="{pts}" stroke="{color}" '
            f'stroke-width="{width}" fill="{fill}"/>\n')


def _svg_polygon(points, fill="#1f77b4", stroke="none", stroke_width=1, opacity=1.0):
    pts = " ".join(f"{x:.2f},{y:.2f}" for x, y in points)
    return (f'<polygon points="{pts}" fill="{fill}" stroke="{stroke}" '
            f'stroke-width="{stroke_width}" opacity="{opacity}"/>\n')


def _svg_circle(cx, cy, r, fill="#1f77b4", stroke="white", stroke_width=1, opacity=1.0):
    return (f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{r}" '
            f'fill="{fill}" stroke="{stroke}" stroke-width="{stroke_width}" '
            f'opacity="{opacity}"/>\n')


def _svg_path(d, fill="none", stroke="#1f77b4", stroke_width=2, opacity=1.0):
    return (f'<path d="{d}" fill="{fill}" stroke="{stroke}" '
            f'stroke-width="{stroke_width}" opacity="{opacity}"/>\n')


def _draw_axes(buf, lx, ly, rx, ry, x_data, y_lo, y_hi, x_tick_labels=None, n_y_ticks=5):
    """Draw axis lines, tick marks, and labels; return updated buffer."""
    # Main axes
    buf += _svg_line(lx, ly, lx, ry, color="#888888", width=1)
    buf += _svg_line(lx, ry, rx, ry, color="#888888", width=1)

    # Grid + Y ticks
    for i in range(n_y_ticks + 1):
        t = i / n_y_ticks
        val = y_lo + t * (y_hi - y_lo)
        gy = ry - t * (ry - ly)
        buf += _svg_line(lx, gy, rx, gy, color="#eeeeee", width=1)
        buf += _svg_text(lx - 6, gy + 4, f"{val:.3g}", size=10, color="#666666", anchor="end")

    # X ticks
    if x_tick_labels:
        step = max(1, len(x_tick_labels) // 10)
        n = len(x_tick_labels)
        for i, lb in enumerate(x_tick_labels):
            if i % step != 0 and i != n - 1:
                continue
            gx = lx + (i / max(n - 1, 1)) * (rx - lx)
            buf += _svg_line(gx, ry, gx, ry + 4, color="#888888", width=1)
            buf += _svg_text(gx, ry + 16, str(lb), size=10, color="#666666")

    return buf


# ---------------------------------------------------------------------------
# SVG single-series charts
# ---------------------------------------------------------------------------

def _plot_line_svg(x, y, title="", width=600, height=400,
                   color=None, line_width=2, show_points=False):
    """
    Generate an SVG line chart.

    Parameters
    ----------
    x, y        : numeric sequences of equal length
    title       : chart title string
    width       : SVG pixel width
    height      : SVG pixel height
    color       : line color (hex string, default palette color)
    line_width  : stroke width in pixels
    show_points : whether to draw circles at each data point

    Returns
    -------
    str  (SVG markup)
    """
    x = _to_float_list(x)
    y = _to_float_list(y)
    _check_equal_length(x, y, "plot_line_svg")
    color = color or _PALETTES["default"][0]
    width, height = int(width), int(height)

    margin = {"top": 40, "right": 30, "bottom": 50, "left": 60}
    lx = margin["left"]
    rx = width - margin["right"]
    ly = margin["top"]
    ry = height - margin["bottom"]

    x_lo, x_hi = _safe_range(x)
    y_lo, y_hi = _safe_range(y)
    x_span = x_hi - x_lo or 1.0
    y_span = y_hi - y_lo or 1.0
    # Add a small padding to y range
    pad = y_span * 0.05
    y_lo -= pad
    y_hi += pad
    y_span = y_hi - y_lo

    def tx(v):
        return lx + (v - x_lo) / x_span * (rx - lx)

    def ty(v):
        return ry - (v - y_lo) / y_span * (ry - ly)

    buf = _svg_open(width, height)
    buf += _svg_rect(0, 0, width, height, fill="#ffffff")
    if title:
        buf += _svg_text(width / 2, 24, title, size=16, bold=True)

    # Gridlines (Y only already drawn; add X grid)
    n_x_grid = min(10, len(x))
    for i in range(n_x_grid + 1):
        gx = lx + i / n_x_grid * (rx - lx)
        buf += _svg_line(gx, ly, gx, ry, color="#eeeeee", width=1)

    # Y gridlines + ticks
    n_y = 5
    for i in range(n_y + 1):
        t = i / n_y
        val = y_lo + t * y_span
        gy = ry - t * (ry - ly)
        buf += _svg_line(lx, gy, rx, gy, color="#dddddd", width=1)
        buf += _svg_text(lx - 8, gy + 4, f"{val:.3g}", size=10, color="#555555", anchor="end")

    # X tick labels
    n_x = min(6, len(x))
    for i in range(n_x + 1):
        xi_val = x_lo + (i / n_x) * x_span
        gx = tx(xi_val)
        buf += _svg_text(gx, ry + 18, f"{xi_val:.3g}", size=10, color="#555555")

    # Axes
    buf += _svg_line(lx, ry, rx, ry, color="#888888", width=1)
    buf += _svg_line(lx, ly, lx, ry, color="#888888", width=1)

    # Line
    points = [(tx(xi), ty(yi)) for xi, yi in zip(x, y)]
    buf += _svg_polyline(points, color=color, width=line_width)

    # Optional points
    if show_points:
        for px, py in points:
            buf += _svg_circle(px, py, 3, fill=color, stroke="white", stroke_width=1)

    buf += _svg_close()
    return buf


def _plot_scatter_svg(x, y, title="", width=600, height=400,
                      color=None, point_size=5, opacity=0.8):
    """
    Generate an SVG scatter plot.

    Parameters
    ----------
    x, y        : numeric sequences of equal length
    title       : chart title
    width       : SVG pixel width
    height      : SVG pixel height
    color       : point color (hex string)
    point_size  : circle radius in pixels
    opacity     : point opacity (0.0 – 1.0)

    Returns
    -------
    str  (SVG markup)
    """
    x = _to_float_list(x)
    y = _to_float_list(y)
    _check_equal_length(x, y, "plot_scatter_svg")
    color = color or _PALETTES["default"][0]
    width, height = int(width), int(height)

    margin = {"top": 40, "right": 30, "bottom": 50, "left": 60}
    lx, rx = margin["left"], width - margin["right"]
    ly, ry = margin["top"], height - margin["bottom"]

    x_lo, x_hi = _safe_range(x)
    y_lo, y_hi = _safe_range(y)
    x_span = x_hi - x_lo or 1.0
    y_span = y_hi - y_lo or 1.0
    pad_x = x_span * 0.05
    pad_y = y_span * 0.05
    x_lo -= pad_x; x_hi += pad_x; x_span = x_hi - x_lo
    y_lo -= pad_y; y_hi += pad_y; y_span = y_hi - y_lo

    def tx(v):
        return lx + (v - x_lo) / x_span * (rx - lx)

    def ty(v):
        return ry - (v - y_lo) / y_span * (ry - ly)

    buf = _svg_open(width, height)
    buf += _svg_rect(0, 0, width, height, fill="#ffffff")
    if title:
        buf += _svg_text(width / 2, 24, title, size=16, bold=True)

    # Grid
    for i in range(6):
        gx = lx + i / 5 * (rx - lx)
        buf += _svg_line(gx, ly, gx, ry, color="#eeeeee", width=1)
        gy = ly + i / 5 * (ry - ly)
        buf += _svg_line(lx, gy, rx, gy, color="#eeeeee", width=1)

    # Axis labels
    for i in range(6):
        xi_val = x_lo + i / 5 * x_span
        buf += _svg_text(tx(xi_val), ry + 18, f"{xi_val:.3g}", size=10, color="#555555")
        yi_val = y_lo + (1 - i / 5) * y_span
        buf += _svg_text(lx - 8, ly + i / 5 * (ry - ly) + 4,
                         f"{yi_val:.3g}", size=10, color="#555555", anchor="end")

    # Axes
    buf += _svg_line(lx, ry, rx, ry, color="#888888", width=1)
    buf += _svg_line(lx, ly, lx, ry, color="#888888", width=1)

    # Points
    for xi, yi in zip(x, y):
        buf += _svg_circle(tx(xi), ty(yi), point_size, fill=color,
                           stroke="white", stroke_width=1, opacity=opacity)

    buf += _svg_close()
    return buf


def _plot_bar_svg(labels, values, title="", width=600, height=400,
                  colors=None, bar_gap=0.2, horizontal=False):
    """
    Generate an SVG bar chart.

    Parameters
    ----------
    labels     : sequence of string labels
    values     : sequence of numeric values
    title      : chart title
    width      : SVG pixel width
    height     : SVG pixel height
    colors     : list of colors (cycles if shorter than values)
    bar_gap    : fraction of bar slot left as gap (0–1)
    horizontal : if True, draw horizontal bars

    Returns
    -------
    str  (SVG markup)
    """
    labels = [str(lb) for lb in labels]
    values = _to_float_list(values)
    _check_equal_length(labels, values, "plot_bar_svg")
    colors = list(colors) if colors else _PALETTES["default"]
    width, height = int(width), int(height)

    margin = {"top": 40, "right": 30, "bottom": 60, "left": 60}
    lx, rx = margin["left"], width - margin["right"]
    ly, ry = margin["top"], height - margin["bottom"]

    n = len(values)
    v_lo = min(0.0, min(values))
    v_hi = max(0.0, max(values)) if values else 1.0
    v_span = v_hi - v_lo or 1.0
    pad = v_span * 0.05
    v_hi += pad
    v_span = v_hi - v_lo

    def scale_val(v):
        return (v - v_lo) / v_span

    buf = _svg_open(width, height)
    buf += _svg_rect(0, 0, width, height, fill="#ffffff")
    if title:
        buf += _svg_text(width / 2, 24, title, size=16, bold=True)

    if not horizontal:
        slot_w = (rx - lx) / max(n, 1)
        bar_w = slot_w * (1 - bar_gap)
        pad_w = slot_w * bar_gap / 2
        zero_y = ry - scale_val(0) * (ry - ly)

        # Y grid
        for i in range(6):
            t = i / 5
            val = v_lo + t * v_span
            gy = ry - t * (ry - ly)
            buf += _svg_line(lx, gy, rx, gy, color="#dddddd", width=1)
            buf += _svg_text(lx - 8, gy + 4, f"{val:.3g}", size=10, color="#555555", anchor="end")

        # Axes
        buf += _svg_line(lx, ry, rx, ry, color="#888888", width=1)
        buf += _svg_line(lx, ly, lx, ry, color="#888888", width=1)
        buf += _svg_line(lx, zero_y, rx, zero_y, color="#888888", width=1, dash="")

        for i, (lb, v) in enumerate(zip(labels, values)):
            c = colors[i % len(colors)]
            bx = lx + i * slot_w + pad_w
            bar_h = abs(v) / v_span * (ry - ly)
            by = zero_y - (scale_val(v) - scale_val(0)) * (ry - ly)
            by = min(zero_y, by)
            bar_h = abs(scale_val(v) - scale_val(0)) * (ry - ly)
            buf += _svg_rect(bx, by, bar_w, bar_h, fill=c, stroke="white", stroke_width=1, rx=2)
            buf += _svg_text(bx + bar_w / 2, ry + 18, lb, size=10, color="#444444")

    else:
        slot_h = (ry - ly) / max(n, 1)
        bar_h = slot_h * (1 - bar_gap)
        pad_h = slot_h * bar_gap / 2
        zero_x = lx + scale_val(0) * (rx - lx)

        # X grid
        for i in range(6):
            t = i / 5
            val = v_lo + t * v_span
            gx = lx + t * (rx - lx)
            buf += _svg_line(gx, ly, gx, ry, color="#dddddd", width=1)
            buf += _svg_text(gx, ry + 18, f"{val:.3g}", size=10, color="#555555")

        buf += _svg_line(lx, ry, rx, ry, color="#888888", width=1)
        buf += _svg_line(lx, ly, lx, ry, color="#888888", width=1)
        buf += _svg_line(zero_x, ly, zero_x, ry, color="#888888", width=1)

        for i, (lb, v) in enumerate(zip(labels, values)):
            c = colors[i % len(colors)]
            by = ly + i * slot_h + pad_h
            bar_width_ = abs(scale_val(v) - scale_val(0)) * (rx - lx)
            bx = min(zero_x, zero_x + (v / v_span) * (rx - lx))
            buf += _svg_rect(bx, by, bar_width_, bar_h, fill=c, stroke="white", stroke_width=1, rx=2)
            buf += _svg_text(lx - 6, by + bar_h / 2 + 4, lb, size=10, color="#444444", anchor="end")

    buf += _svg_close()
    return buf


def _plot_hist_svg(data, bins=10, title="", width=600, height=400,
                   color=None):
    """
    Generate an SVG histogram.

    Parameters
    ----------
    data   : numeric sequence
    bins   : number of bins
    title  : chart title
    width  : SVG pixel width
    height : SVG pixel height
    color  : bar fill color (hex string)

    Returns
    -------
    str  (SVG markup)
    """
    data = _to_float_list(data)
    bins = max(int(bins), 2)
    color = color or _PALETTES["default"][0]
    if not data:
        return _svg_open(width, height) + _svg_text(width / 2, height / 2, "(empty)") + _svg_close()
    edges, counts = _compute_histogram_bins(data, bins)
    labels = [f"{edges[i]:.3g}" for i in range(bins)]
    return _plot_bar_svg(labels, counts, title=title, width=width, height=height, colors=[color])


def _plot_pie_svg(labels, values, title="", width=500, height=500, colors=None):
    """
    Generate an SVG pie chart.

    Parameters
    ----------
    labels : sequence of string labels
    values : numeric sequence (must all be non-negative)
    title  : chart title
    width  : SVG pixel width
    height : SVG pixel height
    colors : list of colors (cycles if shorter)

    Returns
    -------
    str  (SVG markup)
    """
    labels = [str(lb) for lb in labels]
    values = _to_float_list(values)
    _check_equal_length(labels, values, "plot_pie_svg")
    if any(v < 0 for v in values):
        raise ValueError("plot_pie_svg: all values must be non-negative")
    colors = list(colors) if colors else _PALETTES["default"]
    width, height = int(width), int(height)

    total = sum(values)
    if total == 0:
        return _svg_open(width, height) + _svg_text(width / 2, height / 2, "(all zeros)") + _svg_close()

    legend_w = 150
    chart_w = width - legend_w
    cx = chart_w / 2
    cy = height / 2
    r = min(chart_w, height) / 2 * 0.85

    buf = _svg_open(width, height)
    buf += _svg_rect(0, 0, width, height, fill="#ffffff")
    if title:
        buf += _svg_text(width / 2, 24, title, size=16, bold=True)

    angle = -math.pi / 2  # start at top
    for i, (lb, v) in enumerate(zip(labels, values)):
        sweep = 2 * math.pi * v / total
        x1 = cx + r * math.cos(angle)
        y1 = cy + r * math.sin(angle)
        angle2 = angle + sweep
        x2 = cx + r * math.cos(angle2)
        y2 = cy + r * math.sin(angle2)
        large = 1 if sweep > math.pi else 0
        c = colors[i % len(colors)]
        d = (f"M {cx:.2f},{cy:.2f} "
             f"L {x1:.2f},{y1:.2f} "
             f"A {r:.2f},{r:.2f} 0 {large},1 {x2:.2f},{y2:.2f} Z")
        buf += _svg_path(d, fill=c, stroke="white", stroke_width=2)

        # Label inside slice (if slice is big enough)
        mid_angle = angle + sweep / 2
        lx = cx + r * 0.65 * math.cos(mid_angle)
        ly = cy + r * 0.65 * math.sin(mid_angle)
        pct = v / total * 100
        if pct > 5:
            buf += _svg_text(lx, ly + 4, f"{pct:.1f}%", size=11, color="#ffffff")

        # Legend
        legend_y = 50 + i * 22
        buf += _svg_rect(chart_w + 10, legend_y - 10, 14, 14, fill=c, rx=2)
        buf += _svg_text(chart_w + 28, legend_y + 2,
                         f"{lb[:18]} ({v:.3g})", size=11, color="#444444", anchor="start")

        angle = angle2

    buf += _svg_close()
    return buf


# ---------------------------------------------------------------------------
# SVG multi-series charts
# ---------------------------------------------------------------------------

def _plot_multi_line_svg(series, title="", width=700, height=450,
                         colors=None, line_width=2, show_points=False):
    """
    Generate an SVG multi-line chart.

    Parameters
    ----------
    series      : list of dicts with keys 'x', 'y', and optionally 'label'
                  e.g. [{'x': [...], 'y': [...], 'label': 'Series A'}, ...]
    title       : chart title
    width       : SVG pixel width
    height      : SVG pixel height
    colors      : list of colors (cycles)
    line_width  : stroke width
    show_points : draw circles at data points

    Returns
    -------
    str  (SVG markup)
    """
    colors = list(colors) if colors else _PALETTES["default"]
    width, height = int(width), int(height)

    # Validate and flatten
    parsed = []
    for s in series:
        sx = _to_float_list(s["x"])
        sy = _to_float_list(s["y"])
        _check_equal_length(sx, sy, "plot_multi_line_svg")
        parsed.append((sx, sy, str(s.get("label", ""))))

    if not parsed:
        return _svg_open(width, height) + _svg_close()

    all_x = [v for sx, sy, _ in parsed for v in sx]
    all_y = [v for sx, sy, _ in parsed for v in sy]
    x_lo, x_hi = _safe_range(all_x)
    y_lo, y_hi = _safe_range(all_y)
    x_span = x_hi - x_lo or 1.0
    y_span = y_hi - y_lo or 1.0
    pad = y_span * 0.05
    y_lo -= pad; y_hi += pad; y_span = y_hi - y_lo

    has_labels = any(lb for _, _, lb in parsed)
    legend_w = 140 if has_labels else 0

    margin = {"top": 40, "right": 30 + legend_w, "bottom": 50, "left": 60}
    lx, rx = margin["left"], width - margin["right"]
    ly, ry = margin["top"], height - margin["bottom"]

    def tx(v):
        return lx + (v - x_lo) / x_span * (rx - lx)

    def ty(v):
        return ry - (v - y_lo) / y_span * (ry - ly)

    buf = _svg_open(width, height)
    buf += _svg_rect(0, 0, width, height, fill="#ffffff")
    if title:
        buf += _svg_text(width / 2, 24, title, size=16, bold=True)

    # Grid
    for i in range(6):
        gx = lx + i / 5 * (rx - lx)
        buf += _svg_line(gx, ly, gx, ry, color="#eeeeee", width=1)
        gy = ly + i / 5 * (ry - ly)
        val = y_hi - i / 5 * y_span
        buf += _svg_line(lx, gy, rx, gy, color="#eeeeee", width=1)
        buf += _svg_text(lx - 8, gy + 4, f"{val:.3g}", size=10, color="#555555", anchor="end")

    # X ticks
    for i in range(6):
        xi_val = x_lo + i / 5 * x_span
        buf += _svg_text(tx(xi_val), ry + 18, f"{xi_val:.3g}", size=10, color="#555555")

    buf += _svg_line(lx, ry, rx, ry, color="#888888", width=1)
    buf += _svg_line(lx, ly, lx, ry, color="#888888", width=1)

    # Series
    for i, (sx, sy, lb) in enumerate(parsed):
        c = colors[i % len(colors)]
        points = [(tx(xi), ty(yi)) for xi, yi in zip(sx, sy)]
        buf += _svg_polyline(points, color=c, width=line_width)
        if show_points:
            for px, py in points:
                buf += _svg_circle(px, py, 3, fill=c, stroke="white", stroke_width=1)

    # Legend
    if has_labels:
        legend_x = width - legend_w + 10
        for i, (_, _, lb) in enumerate(parsed):
            c = colors[i % len(colors)]
            ly_ = 50 + i * 22
            buf += _svg_line(legend_x, ly_, legend_x + 20, ly_, color=c, width=3)
            buf += _svg_text(legend_x + 26, ly_ + 4, lb[:18], size=11,
                             color="#444444", anchor="start")

    buf += _svg_close()
    return buf


def _plot_multi_bar_svg(categories, series, title="", width=700, height=450,
                        colors=None, bar_gap=0.15, group_gap=0.25):
    """
    Generate an SVG grouped bar chart.

    Parameters
    ----------
    categories : list of category label strings
    series     : list of dicts with keys 'values' and optionally 'label'
                 e.g. [{'values': [1, 2, 3], 'label': 'A'}, ...]
    title      : chart title
    width      : SVG pixel width
    height     : SVG pixel height
    colors     : list of colors
    bar_gap    : gap between bars within a group (fraction of bar slot)
    group_gap  : gap between groups (fraction of group slot)

    Returns
    -------
    str  (SVG markup)
    """
    categories = [str(c) for c in categories]
    colors = list(colors) if colors else _PALETTES["default"]
    width, height = int(width), int(height)

    parsed = []
    for s in series:
        vals = _to_float_list(s["values"])
        parsed.append((vals, str(s.get("label", ""))))

    n_groups = len(categories)
    n_series = len(parsed)
    if n_groups == 0 or n_series == 0:
        return _svg_open(width, height) + _svg_close()

    all_vals = [v for vals, _ in parsed for v in vals]
    v_lo = min(0.0, min(all_vals))
    v_hi = max(0.0, max(all_vals)) if all_vals else 1.0
    v_span = v_hi - v_lo or 1.0
    v_hi += v_span * 0.05
    v_span = v_hi - v_lo

    has_labels = any(lb for _, lb in parsed)
    legend_w = 140 if has_labels else 0

    margin = {"top": 40, "right": 30 + legend_w, "bottom": 60, "left": 60}
    lx, rx = margin["left"], width - margin["right"]
    ly, ry = margin["top"], height - margin["bottom"]

    zero_y = ry - (-v_lo / v_span) * (ry - ly)
    group_w = (rx - lx) / n_groups
    inner_w = group_w * (1 - group_gap)
    bar_w = inner_w / n_series * (1 - bar_gap)

    buf = _svg_open(width, height)
    buf += _svg_rect(0, 0, width, height, fill="#ffffff")
    if title:
        buf += _svg_text(width / 2, 24, title, size=16, bold=True)

    for i in range(6):
        t = i / 5
        val = v_lo + t * v_span
        gy = ry - t * (ry - ly)
        buf += _svg_line(lx, gy, rx, gy, color="#dddddd", width=1)
        buf += _svg_text(lx - 8, gy + 4, f"{val:.3g}", size=10, color="#555555", anchor="end")

    buf += _svg_line(lx, ry, rx, ry, color="#888888", width=1)
    buf += _svg_line(lx, ly, lx, ry, color="#888888", width=1)
    buf += _svg_line(lx, zero_y, rx, zero_y, color="#555555", width=1)

    for gi, cat in enumerate(categories):
        gx_start = lx + gi * group_w + group_w * group_gap / 2
        cat_cx = lx + gi * group_w + group_w / 2
        buf += _svg_text(cat_cx, ry + 18, cat, size=10, color="#444444")

        for si, (vals, _) in enumerate(parsed):
            if gi >= len(vals):
                continue
            v = vals[gi]
            c = colors[si % len(colors)]
            bx = gx_start + si * (inner_w / n_series) + bar_gap * inner_w / (2 * n_series)
            bar_h = abs(v / v_span) * (ry - ly)
            by = zero_y - (v / v_span) * (ry - ly) if v >= 0 else zero_y
            by = min(zero_y, by)
            buf += _svg_rect(bx, by, bar_w, bar_h, fill=c, stroke="white", stroke_width=1, rx=2)

    if has_labels:
        legend_x = width - legend_w + 10
        for i, (_, lb) in enumerate(parsed):
            c = colors[i % len(colors)]
            ly_ = 50 + i * 22
            buf += _svg_rect(legend_x, ly_ - 10, 14, 14, fill=c, rx=2)
            buf += _svg_text(legend_x + 18, ly_ + 2, lb[:18], size=11,
                             color="#444444", anchor="start")

    buf += _svg_close()
    return buf


# ---------------------------------------------------------------------------
# SVG extras
# ---------------------------------------------------------------------------

def _plot_area_svg(x, y, title="", width=600, height=400,
                   color=None, opacity=0.5):
    """
    Generate an SVG filled area chart (line + shaded area beneath).

    Parameters
    ----------
    x, y    : numeric sequences of equal length
    title   : chart title
    width   : SVG pixel width
    height  : SVG pixel height
    color   : fill/line color (hex)
    opacity : fill opacity (0.0 – 1.0)

    Returns
    -------
    str  (SVG markup)
    """
    x = _to_float_list(x)
    y = _to_float_list(y)
    _check_equal_length(x, y, "plot_area_svg")
    color = color or _PALETTES["default"][0]
    width, height = int(width), int(height)

    margin = {"top": 40, "right": 30, "bottom": 50, "left": 60}
    lx, rx = margin["left"], width - margin["right"]
    ly, ry = margin["top"], height - margin["bottom"]

    x_lo, x_hi = _safe_range(x)
    y_lo, y_hi = _safe_range(y)
    x_span = x_hi - x_lo or 1.0
    y_span = y_hi - y_lo or 1.0
    pad = y_span * 0.05
    y_lo -= pad; y_hi += pad; y_span = y_hi - y_lo

    def tx(v):
        return lx + (v - x_lo) / x_span * (rx - lx)

    def ty(v):
        return ry - (v - y_lo) / y_span * (ry - ly)

    buf = _svg_open(width, height)
    buf += _svg_rect(0, 0, width, height, fill="#ffffff")
    if title:
        buf += _svg_text(width / 2, 24, title, size=16, bold=True)

    # Grid
    for i in range(6):
        gy = ly + i / 5 * (ry - ly)
        val = y_hi - i / 5 * y_span
        buf += _svg_line(lx, gy, rx, gy, color="#eeeeee", width=1)
        buf += _svg_text(lx - 8, gy + 4, f"{val:.3g}", size=10, color="#555555", anchor="end")
    for i in range(6):
        xi_val = x_lo + i / 5 * x_span
        buf += _svg_text(tx(xi_val), ry + 18, f"{xi_val:.3g}", size=10, color="#555555")

    # Filled polygon (area)
    pts = [(tx(x[0]), ry)] + [(tx(xi), ty(yi)) for xi, yi in zip(x, y)] + [(tx(x[-1]), ry)]
    buf += _svg_polygon(pts, fill=color, stroke="none", opacity=opacity)

    # Line on top
    line_pts = [(tx(xi), ty(yi)) for xi, yi in zip(x, y)]
    buf += _svg_polyline(line_pts, color=color, width=2)

    buf += _svg_line(lx, ry, rx, ry, color="#888888", width=1)
    buf += _svg_line(lx, ly, lx, ry, color="#888888", width=1)
    buf += _svg_close()
    return buf


def _plot_step_svg(x, y, title="", width=600, height=400, color=None):
    """
    Generate an SVG step chart (staircase line).

    Parameters
    ----------
    x, y   : numeric sequences of equal length
    title  : chart title
    width  : SVG pixel width
    height : SVG pixel height
    color  : line color (hex)

    Returns
    -------
    str  (SVG markup)
    """
    x = _to_float_list(x)
    y = _to_float_list(y)
    _check_equal_length(x, y, "plot_step_svg")
    color = color or _PALETTES["default"][2]
    width, height = int(width), int(height)

    margin = {"top": 40, "right": 30, "bottom": 50, "left": 60}
    lx, rx = margin["left"], width - margin["right"]
    ly, ry = margin["top"], height - margin["bottom"]

    x_lo, x_hi = _safe_range(x)
    y_lo, y_hi = _safe_range(y)
    x_span = x_hi - x_lo or 1.0
    y_span = y_hi - y_lo or 1.0
    pad = y_span * 0.05
    y_lo -= pad; y_hi += pad; y_span = y_hi - y_lo

    def tx(v):
        return lx + (v - x_lo) / x_span * (rx - lx)

    def ty(v):
        return ry - (v - y_lo) / y_span * (ry - ly)

    # Build step-function point list
    step_pts = []
    for i, (xi, yi) in enumerate(zip(x, y)):
        step_pts.append((tx(xi), ty(yi)))
        if i + 1 < len(x):
            step_pts.append((tx(x[i + 1]), ty(yi)))

    buf = _svg_open(width, height)
    buf += _svg_rect(0, 0, width, height, fill="#ffffff")
    if title:
        buf += _svg_text(width / 2, 24, title, size=16, bold=True)

    for i in range(6):
        gy = ly + i / 5 * (ry - ly)
        val = y_hi - i / 5 * y_span
        buf += _svg_line(lx, gy, rx, gy, color="#eeeeee", width=1)
        buf += _svg_text(lx - 8, gy + 4, f"{val:.3g}", size=10, color="#555555", anchor="end")
    for i in range(6):
        xi_val = x_lo + i / 5 * x_span
        buf += _svg_text(tx(xi_val), ry + 18, f"{xi_val:.3g}", size=10, color="#555555")

    buf += _svg_line(lx, ry, rx, ry, color="#888888", width=1)
    buf += _svg_line(lx, ly, lx, ry, color="#888888", width=1)
    buf += _svg_polyline(step_pts, color=color, width=2)
    buf += _svg_close()
    return buf


def _plot_heatmap_svg(matrix, title="", width=600, height=500,
                      row_labels=None, col_labels=None, color_lo="#ffffff",
                      color_hi="#1f77b4"):
    """
    Generate an SVG heatmap from a 2-D numeric matrix.

    Parameters
    ----------
    matrix     : list of lists (rows x cols) of numeric values
    title      : chart title
    width      : SVG pixel width
    height     : SVG pixel height
    row_labels : optional list of row label strings
    col_labels : optional list of column label strings
    color_lo   : hex color for minimum values
    color_hi   : hex color for maximum values

    Returns
    -------
    str  (SVG markup)
    """
    rows = len(matrix)
    if rows == 0:
        return _svg_open(width, height) + _svg_close()
    cols = len(matrix[0])

    # Flatten and find range
    flat = [float(matrix[r][c]) for r in range(rows) for c in range(cols)]
    lo, hi = _safe_range(flat)
    span = hi - lo or 1.0

    row_labels = list(row_labels) if row_labels else [str(i) for i in range(rows)]
    col_labels = list(col_labels) if col_labels else [str(j) for j in range(cols)]

    # Parse hex colors for interpolation
    def hex_to_rgb(h):
        h = h.lstrip("#")
        return tuple(int(h[i:i+2], 16) for i in (0, 2, 4))

    def rgb_to_hex(r, g, b):
        return f"#{int(r):02x}{int(g):02x}{int(b):02x}"

    lo_rgb = hex_to_rgb(color_lo)
    hi_rgb = hex_to_rgb(color_hi)

    margin = {"top": 60, "right": 40, "bottom": 40, "left": 80}
    px = margin["left"]
    py = margin["top"]
    pw = width - margin["left"] - margin["right"]
    ph = height - margin["top"] - margin["bottom"]
    cell_w = pw / cols
    cell_h = ph / rows

    buf = _svg_open(width, height)
    buf += _svg_rect(0, 0, width, height, fill="#ffffff")
    if title:
        buf += _svg_text(width / 2, 24, title, size=16, bold=True)

    for r in range(rows):
        for c in range(cols):
            t = (float(matrix[r][c]) - lo) / span
            rgb = tuple(_lerp(lo_rgb[i], hi_rgb[i], t) for i in range(3))
            fc = rgb_to_hex(*rgb)
            cx = px + c * cell_w
            cy = py + r * cell_h
            buf += _svg_rect(cx, cy, cell_w, cell_h, fill=fc, stroke="white", stroke_width=1)
            # Value label inside cell
            if cell_w > 25 and cell_h > 18:
                buf += _svg_text(cx + cell_w / 2, cy + cell_h / 2 + 4,
                                 f"{float(matrix[r][c]):.2g}", size=9, color="#333333")

    # Col labels (top)
    for c, lb in enumerate(col_labels):
        buf += _svg_text(px + (c + 0.5) * cell_w, py - 8, lb[:10],
                         size=10, color="#444444")

    # Row labels (left)
    for r, lb in enumerate(row_labels):
        buf += _svg_text(px - 6, py + (r + 0.5) * cell_h + 4, lb[:12],
                         size=10, color="#444444", anchor="end")

    buf += _svg_close()
    return buf


def _plot_box_svg(data_groups, labels=None, title="", width=600, height=400,
                  colors=None):
    """
    Generate an SVG box-and-whisker plot.

    Parameters
    ----------
    data_groups : list of numeric sequences (one per group)
    labels      : optional list of group label strings
    title       : chart title
    width       : SVG pixel width
    height      : SVG pixel height
    colors      : list of fill colors

    Returns
    -------
    str  (SVG markup)
    """
    groups = [_to_float_list(g) for g in data_groups]
    n = len(groups)
    if n == 0:
        return _svg_open(width, height) + _svg_close()
    colors = list(colors) if colors else _PALETTES["default"]
    labels = list(labels) if labels else [str(i + 1) for i in range(n)]
    width, height = int(width), int(height)

    # Compute box stats per group
    def _box_stats(vals):
        s = sorted(vals)
        n = len(s)
        if n == 0:
            return None
        q1 = s[max(0, int(n * 0.25))]
        median = s[n // 2]
        q3 = s[min(n - 1, int(n * 0.75))]
        iqr = q3 - q1
        lo_w = max(s[0], q1 - 1.5 * iqr)
        hi_w = min(s[-1], q3 + 1.5 * iqr)
        outliers = [v for v in s if v < lo_w or v > hi_w]
        return {"q1": q1, "median": median, "q3": q3,
                "lo_w": lo_w, "hi_w": hi_w, "outliers": outliers,
                "min": s[0], "max": s[-1]}

    stats = [_box_stats(g) for g in groups if g]

    all_vals = [v for g in groups for v in g]
    y_lo, y_hi = _safe_range(all_vals)
    y_span = y_hi - y_lo or 1.0
    pad = y_span * 0.1
    y_lo -= pad; y_hi += pad; y_span = y_hi - y_lo

    margin = {"top": 40, "right": 30, "bottom": 50, "left": 60}
    lx, rx = margin["left"], width - margin["right"]
    ly, ry = margin["top"], height - margin["bottom"]

    def ty(v):
        return ry - (v - y_lo) / y_span * (ry - ly)

    slot_w = (rx - lx) / n
    box_w = slot_w * 0.5

    buf = _svg_open(width, height)
    buf += _svg_rect(0, 0, width, height, fill="#ffffff")
    if title:
        buf += _svg_text(width / 2, 24, title, size=16, bold=True)

    for i in range(6):
        t = i / 5
        val = y_lo + t * y_span
        gy = ry - t * (ry - ly)
        buf += _svg_line(lx, gy, rx, gy, color="#dddddd", width=1)
        buf += _svg_text(lx - 8, gy + 4, f"{val:.3g}", size=10, color="#555555", anchor="end")

    buf += _svg_line(lx, ry, rx, ry, color="#888888", width=1)
    buf += _svg_line(lx, ly, lx, ry, color="#888888", width=1)

    for i, (st, lb) in enumerate(zip(stats, labels)):
        if st is None:
            continue
        c = colors[i % len(colors)]
        cx = lx + (i + 0.5) * slot_w
        bx = cx - box_w / 2

        # Whiskers
        buf += _svg_line(cx, ty(st["lo_w"]), cx, ty(st["q1"]), color="#555555", width=1)
        buf += _svg_line(cx, ty(st["q3"]), cx, ty(st["hi_w"]), color="#555555", width=1)
        buf += _svg_line(bx, ty(st["lo_w"]), bx + box_w, ty(st["lo_w"]), color="#555555", width=1)
        buf += _svg_line(bx, ty(st["hi_w"]), bx + box_w, ty(st["hi_w"]), color="#555555", width=1)

        # Box
        box_top = ty(st["q3"])
        box_bot = ty(st["q1"])
        buf += _svg_rect(bx, box_top, box_w, box_bot - box_top,
                         fill=c, stroke="#555555", stroke_width=1, rx=2)

        # Median line
        buf += _svg_line(bx, ty(st["median"]), bx + box_w, ty(st["median"]),
                         color="#333333", width=2)

        # Outliers
        for ov in st["outliers"]:
            buf += _svg_circle(cx, ty(ov), 3, fill=c, stroke="#555555", stroke_width=1)

        buf += _svg_text(cx, ry + 18, lb, size=10, color="#444444")

    buf += _svg_close()
    return buf


def _plot_annotations_svg(base_svg, annotations):
    """
    Add text/arrow annotations to an existing SVG string.

    Parameters
    ----------
    base_svg    : SVG string to annotate
    annotations : list of dicts, each with keys:
                    'x', 'y'        - position in SVG pixels
                    'text'          - annotation label
                    'color'         - optional text color (default '#d62728')
                    'arrow_to'      - optional (x2, y2) tuple to draw arrow toward

    Returns
    -------
    str  (SVG markup with annotations inserted before </svg>)
    """
    inserts = []
    for ann in annotations:
        ax = float(ann["x"])
        ay = float(ann["y"])
        text = str(ann["text"])
        color = ann.get("color", "#d62728")
        arrow = ann.get("arrow_to")

        if arrow:
            ax2, ay2 = float(arrow[0]), float(arrow[1])
            inserts.append(_svg_line(ax, ay, ax2, ay2, color=color, width=1, dash="4,2"))
            inserts.append(_svg_circle(ax2, ay2, 3, fill=color, stroke="none"))

        inserts.append(_svg_text(ax, ay, text, size=11, color=color, anchor="start"))

    joined = "".join(inserts)
    return base_svg.replace("</svg>", joined + "</svg>")


# ---------------------------------------------------------------------------
# SVG utilities
# ---------------------------------------------------------------------------

def _plot_save(svg_content, path):
    """
    Save SVG content string to a file.

    Parameters
    ----------
    svg_content : str  (SVG markup)
    path        : file path to write

    Returns
    -------
    str  (absolute path written)
    """
    import os
    path = str(path)
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True) if os.path.dirname(path) else None
    with open(path, "w", encoding="utf-8") as f:
        f.write(svg_content)
    return os.path.abspath(path)


def _plot_to_html(svg_content, title="Chart"):
    """
    Wrap an SVG string in a minimal self-contained HTML document.

    Parameters
    ----------
    svg_content : str  (SVG markup)
    title       : HTML page title

    Returns
    -------
    str  (HTML markup)
    """
    title = _xml_escape(title)
    return (
        "<!DOCTYPE html>\n<html>\n<head>\n"
        f"<meta charset='UTF-8'><title>{title}</title>\n"
        "<style>body{margin:0;display:flex;justify-content:center;"
        "align-items:center;min-height:100vh;background:#f5f5f5;}</style>\n"
        f"</head>\n<body>\n{svg_content}\n</body>\n</html>\n"
    )


def _plot_combine_svg(svgs, cols=2, padding=20, bg_color="#f8f8f8"):
    """
    Combine multiple SVG strings into a single grid layout SVG.

    Parameters
    ----------
    svgs      : list of SVG strings
    cols      : number of columns in the grid
    padding   : pixels of padding between charts
    bg_color  : background color of the combined canvas

    Returns
    -------
    str  (SVG markup)
    """
    import re
    if not svgs:
        return ""

    widths, heights = [], []
    for svg in svgs:
        wm = re.search(r'<svg[^>]*width="(\d+)"', svg)
        hm = re.search(r'<svg[^>]*height="(\d+)"', svg)
        widths.append(int(wm.group(1)) if wm else 600)
        heights.append(int(hm.group(1)) if hm else 400)

    rows = math.ceil(len(svgs) / cols)
    cell_w = max(widths) + padding
    cell_h = max(heights) + padding
    total_w = cols * cell_w + padding
    total_h = rows * cell_h + padding

    buf = _svg_open(total_w, total_h)
    buf += _svg_rect(0, 0, total_w, total_h, fill=bg_color)

    for i, svg in enumerate(svgs):
        r, c = divmod(i, cols)
        ox = c * cell_w + padding
        oy = r * cell_h + padding
        # Extract inner SVG content and wrap in a <g> with translate transform
        inner = re.sub(r'<\?xml[^?]*\?>', '', svg)
        inner = re.sub(r'<svg[^>]*>', '', inner)
        inner = inner.replace('</svg>', '').strip()
        buf += f'<g transform="translate({ox},{oy})">\n{inner}\n</g>\n'

    buf += _svg_close()
    return buf


def _plot_svg_dimensions(svg_content):
    """
    Extract the width and height (in pixels) from an SVG string.

    Parameters
    ----------
    svg_content : str  (SVG markup)

    Returns
    -------
    dict  with keys 'width' and 'height' as integers
    """
    import re
    wm = re.search(r'<svg[^>]*width="(\d+)"', svg_content)
    hm = re.search(r'<svg[^>]*height="(\d+)"', svg_content)
    return {
        "width": int(wm.group(1)) if wm else 0,
        "height": int(hm.group(1)) if hm else 0,
    }


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_plot_functions(runtime) -> None:
    """Register all plot functions with the NexusLang runtime."""

    _fns = {
        # Data utilities
        "plot_linspace":        _plot_linspace,
        "plot_arange":          _plot_arange,
        "plot_normalize":       _plot_normalize,
        "plot_scale":           _plot_scale,
        "plot_stats":           _plot_stats,
        "plot_color_scheme":    _plot_color_scheme,

        # ASCII / terminal charts
        "plot_line_text":       _plot_line_text,
        "plot_bar_text":        _plot_bar_text,
        "plot_scatter_text":    _plot_scatter_text,
        "plot_hist_text":       _plot_hist_text,

        # SVG utilities
        "plot_save":            _plot_save,
        "plot_to_html":         _plot_to_html,
        "plot_combine_svg":     _plot_combine_svg,
        "plot_svg_dimensions":  _plot_svg_dimensions,

        # SVG single-series charts
        "plot_line_svg":        _plot_line_svg,
        "plot_scatter_svg":     _plot_scatter_svg,
        "plot_bar_svg":         _plot_bar_svg,
        "plot_hist_svg":        _plot_hist_svg,
        "plot_pie_svg":         _plot_pie_svg,

        # SVG multi-series charts
        "plot_multi_line_svg":  _plot_multi_line_svg,
        "plot_multi_bar_svg":   _plot_multi_bar_svg,

        # SVG extras
        "plot_area_svg":        _plot_area_svg,
        "plot_step_svg":        _plot_step_svg,
        "plot_heatmap_svg":     _plot_heatmap_svg,
        "plot_box_svg":         _plot_box_svg,
        "plot_annotations_svg": _plot_annotations_svg,
    }

    for name, fn in _fns.items():
        runtime.register_function(name, fn)
