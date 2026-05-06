"""
Tests for the plot stdlib module.

Covers all 27 registered functions:
  Data utilities     : plot_linspace, plot_arange, plot_normalize, plot_scale,
                       plot_stats, plot_color_scheme
  ASCII charts       : plot_line_text, plot_bar_text, plot_scatter_text,
                       plot_hist_text
  SVG utilities      : plot_save, plot_to_html, plot_combine_svg,
                       plot_svg_dimensions
  SVG single-series  : plot_line_svg, plot_scatter_svg, plot_bar_svg,
                       plot_hist_svg, plot_pie_svg
  SVG multi-series   : plot_multi_line_svg, plot_multi_bar_svg
  SVG extras         : plot_area_svg, plot_step_svg, plot_heatmap_svg,
                       plot_box_svg, plot_annotations_svg
"""

import importlib.util
import math
import os
import re
import tempfile

import pytest

# ---------------------------------------------------------------------------
# Fixture: import the plot module directly to avoid slow optional deps
# ---------------------------------------------------------------------------


@pytest.fixture(scope="module")
def plot():
    _here = os.path.dirname(os.path.abspath(__file__))
    _init = os.path.join(_here, "..", "..", "..", "src", "nlpl", "stdlib", "plot", "__init__.py")
    spec = importlib.util.spec_from_file_location("nexuslang.stdlib.plot", _init)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


# ---------------------------------------------------------------------------
# Helper
# ---------------------------------------------------------------------------


def linspace(start, stop, n):
    if n == 1:
        return [float(start)]
    step = (stop - start) / (n - 1)
    return [start + i * step for i in range(n)]


# ===========================================================================
# Data utilities
# ===========================================================================


class TestPlotLinspace:
    def test_five_points(self, plot):
        res = plot._plot_linspace(0, 1, 5)
        assert res == [0.0, 0.25, 0.5, 0.75, 1.0]

    def test_endpoints_preserved(self, plot):
        res = plot._plot_linspace(-3, 7, 11)
        assert abs(res[0] - -3.0) < 1e-12
        assert abs(res[-1] - 7.0) < 1e-12

    def test_single_point(self, plot):
        res = plot._plot_linspace(5, 10, 1)
        assert res == [5.0]

    def test_returns_list(self, plot):
        assert isinstance(plot._plot_linspace(0, 10, 4), list)

    def test_length(self, plot):
        assert len(plot._plot_linspace(0, 100, 50)) == 50

    def test_negative_range(self, plot):
        res = plot._plot_linspace(-1, 0, 3)
        assert abs(res[0] - -1.0) < 1e-12
        assert abs(res[1] - -0.5) < 1e-12
        assert abs(res[2] - 0.0) < 1e-12

    def test_zero_n_raises(self, plot):
        with pytest.raises(ValueError, match="n must be >= 1"):
            plot._plot_linspace(0, 1, 0)


class TestPlotArange:
    def test_basic(self, plot):
        res = plot._plot_arange(0, 1, 0.25)
        assert len(res) == 4
        assert abs(res[0] - 0.0) < 1e-12
        assert abs(res[-1] - 0.75) < 1e-12

    def test_integer_step(self, plot):
        res = plot._plot_arange(0, 5, 1)
        assert res == [0.0, 1.0, 2.0, 3.0, 4.0]

    def test_negative_step(self, plot):
        res = plot._plot_arange(5, 0, -1)
        assert res[0] == 5.0 and res[-1] == 1.0

    def test_zero_step_raises(self, plot):
        with pytest.raises(ValueError, match="step cannot be zero"):
            plot._plot_arange(0, 10, 0)

    def test_empty_when_reversed_positive_step(self, plot):
        res = plot._plot_arange(5, 0, 1)
        assert res == []


class TestPlotNormalize:
    def test_range_zero_to_one(self, plot):
        res = plot._plot_normalize([0, 5, 10])
        assert res == [0.0, 0.5, 1.0]

    def test_all_same_returns_half(self, plot):
        res = plot._plot_normalize([7, 7, 7])
        assert res == [0.5, 0.5, 0.5]

    def test_negative_values(self, plot):
        res = plot._plot_normalize([-10, 0, 10])
        assert res == [0.0, 0.5, 1.0]

    def test_single_element(self, plot):
        res = plot._plot_normalize([42])
        assert res == [0.5]

    def test_float_input(self, plot):
        res = plot._plot_normalize([0.1, 0.2, 0.3])
        assert abs(res[0] - 0.0) < 1e-12
        assert abs(res[-1] - 1.0) < 1e-12

    def test_returns_list(self, plot):
        assert isinstance(plot._plot_normalize([1, 2, 3]), list)


class TestPlotScale:
    def test_scale_to_minus_one_to_one(self, plot):
        res = plot._plot_scale([0, 5, 10], -1, 1)
        assert abs(res[0] - -1.0) < 1e-12
        assert abs(res[2] - 1.0) < 1e-12

    def test_scale_to_0_100(self, plot):
        res = plot._plot_scale([0, 0.5, 1.0], 0, 100)
        assert abs(res[-1] - 100.0) < 1e-12

    def test_constant_data(self, plot):
        res = plot._plot_scale([3, 3, 3], 10, 20)
        assert all(abs(v - 15.0) < 1e-12 for v in res)

    def test_length_preserved(self, plot):
        data = list(range(50))
        assert len(plot._plot_scale(data, 0, 1)) == 50


class TestPlotStats:
    def test_integers(self, plot):
        st = plot._plot_stats([1, 2, 3, 4, 5])
        assert st["count"] == 5
        assert abs(st["mean"] - 3.0) < 1e-12
        assert abs(st["min"] - 1.0) < 1e-12
        assert abs(st["max"] - 5.0) < 1e-12

    def test_std(self, plot):
        st = plot._plot_stats([2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0])
        assert abs(st["std"] - 2.0) < 1e-9

    def test_median_odd(self, plot):
        st = plot._plot_stats([3, 1, 2])
        assert abs(st["median"] - 2.0) < 1e-12

    def test_median_even(self, plot):
        st = plot._plot_stats([1, 2, 3, 4])
        assert abs(st["median"] - 2.5) < 1e-12

    def test_empty_returns_none_fields(self, plot):
        st = plot._plot_stats([])
        assert st["count"] == 0
        assert st["mean"] is None

    def test_single_element(self, plot):
        st = plot._plot_stats([99])
        assert st["min"] == 99 and st["max"] == 99 and st["std"] == 0.0


class TestPlotColorScheme:
    def test_default(self, plot):
        cs = plot._plot_color_scheme("default")
        assert len(cs) == 10
        assert all(c.startswith("#") for c in cs)

    def test_pastel(self, plot):
        cs = plot._plot_color_scheme("pastel")
        assert len(cs) == 10

    def test_dark(self, plot):
        cs = plot._plot_color_scheme("dark")
        assert isinstance(cs, list)

    def test_unknown_palette_raises(self, plot):
        with pytest.raises(ValueError, match="unknown palette"):
            plot._plot_color_scheme("neon_unicorn")

    def test_returns_copy(self, plot):
        cs1 = plot._plot_color_scheme("default")
        cs1[0] = "#000000"
        cs2 = plot._plot_color_scheme("default")
        assert cs2[0] != "#000000"


# ===========================================================================
# ASCII / terminal charts
# ===========================================================================


class TestPlotLineText:
    def test_returns_string(self, plot):
        res = plot._plot_line_text([1, 2, 3], [1, 4, 9])
        assert isinstance(res, str)

    def test_contains_asterisk_points(self, plot):
        res = plot._plot_line_text([0, 1], [0, 1])
        assert "*" in res

    def test_title_in_output(self, plot):
        res = plot._plot_line_text([0, 1], [0, 1], title="MyTitle")
        assert "MyTitle" in res

    def test_x_axis_labels(self, plot):
        res = plot._plot_line_text([0, 10], [0, 10])
        assert "0.00" in res
        assert "10.00" in res

    def test_unequal_lengths_raises(self, plot):
        with pytest.raises(ValueError):
            plot._plot_line_text([1, 2], [1])

    def test_constant_y(self, plot):
        res = plot._plot_line_text([0, 1, 2], [5, 5, 5])
        assert isinstance(res, str)

    def test_custom_size(self, plot):
        res = plot._plot_line_text([0, 1], [0, 1], width=30, height=10)
        assert isinstance(res, str)


class TestPlotBarText:
    def test_returns_string(self, plot):
        res = plot._plot_bar_text(["A", "B"], [10, 20])
        assert isinstance(res, str)

    def test_label_in_output(self, plot):
        res = plot._plot_bar_text(["Alpha", "Beta"], [1, 2])
        assert "Alpha" in res

    def test_bar_character_present(self, plot):
        res = plot._plot_bar_text(["X"], [100])
        assert "#" in res

    def test_title_in_output(self, plot):
        res = plot._plot_bar_text(["A"], [1], title="Revenue")
        assert "Revenue" in res

    def test_negative_values(self, plot):
        res = plot._plot_bar_text(["A", "B"], [-5, 5])
        assert "-" in res

    def test_unequal_lengths_raises(self, plot):
        with pytest.raises(ValueError):
            plot._plot_bar_text(["A", "B", "C"], [1, 2])


class TestPlotScatterText:
    def test_returns_string(self, plot):
        assert isinstance(plot._plot_scatter_text([1, 2], [1, 2]), str)

    def test_point_character_present(self, plot):
        res = plot._plot_scatter_text([0, 5, 10], [0, 25, 100])
        assert "o" in res

    def test_unequal_lengths_raises(self, plot):
        with pytest.raises(ValueError):
            plot._plot_scatter_text([1, 2], [1])

    def test_title_in_output(self, plot):
        res = plot._plot_scatter_text([0], [0], title="ScatterTitle")
        assert "ScatterTitle" in res


class TestPlotHistText:
    def test_returns_string(self, plot):
        assert isinstance(plot._plot_hist_text([1, 2, 2, 3, 3, 3]), str)

    def test_bar_present(self, plot):
        res = plot._plot_hist_text([1, 1, 2, 3, 3, 3, 4, 5], bins=4)
        assert "#" in res

    def test_empty_returns_string(self, plot):
        res = plot._plot_hist_text([])
        assert isinstance(res, str)

    def test_single_bin(self, plot):
        res = plot._plot_hist_text([5, 5, 5, 5], bins=2)
        assert isinstance(res, str)


# ===========================================================================
# SVG single-series charts
# ===========================================================================


def _svg_has(svg, tag):
    return f"<{tag}" in svg


def _svg_is_valid_xml(svg):
    """True if SVG starts with proper declarations and has closing tag."""
    return svg.startswith("<?xml") and "</svg>" in svg


class TestPlotLineSvg:
    def test_is_valid_xml(self, plot):
        svg = plot._plot_line_svg([0, 1, 2], [0, 1, 4])
        assert _svg_is_valid_xml(svg)

    def test_contains_polyline(self, plot):
        svg = plot._plot_line_svg([0, 1, 2], [0, 1, 4])
        assert _svg_has(svg, "polyline")

    def test_default_dimensions(self, plot):
        svg = plot._plot_line_svg([0, 1], [0, 1])
        dims = plot._plot_svg_dimensions(svg)
        assert dims["width"] == 600 and dims["height"] == 400

    def test_custom_dimensions(self, plot):
        svg = plot._plot_line_svg([0, 1], [0, 1], width=800, height=500)
        dims = plot._plot_svg_dimensions(svg)
        assert dims["width"] == 800 and dims["height"] == 500

    def test_title_in_svg(self, plot):
        svg = plot._plot_line_svg([0, 1], [0, 1], title="Hello")
        assert "Hello" in svg

    def test_show_points_adds_circles(self, plot):
        svg_no_pts = plot._plot_line_svg([0, 1, 2], [0, 1, 2], show_points=False)
        svg_pts = plot._plot_line_svg([0, 1, 2], [0, 1, 2], show_points=True)
        assert svg_pts.count("<circle") > svg_no_pts.count("<circle")

    def test_custom_color(self, plot):
        svg = plot._plot_line_svg([0, 1], [0, 1], color="#ff0000")
        assert "#ff0000" in svg

    def test_unequal_lengths_raises(self, plot):
        with pytest.raises(ValueError):
            plot._plot_line_svg([0, 1, 2], [0, 1])


class TestPlotScatterSvg:
    def test_is_valid_xml(self, plot):
        assert _svg_is_valid_xml(plot._plot_scatter_svg([1, 2, 3], [4, 5, 6]))

    def test_contains_circles(self, plot):
        svg = plot._plot_scatter_svg([1, 2, 3], [4, 5, 6])
        assert _svg_has(svg, "circle")

    def test_circle_count_matches_data(self, plot):
        n = 20
        svg = plot._plot_scatter_svg(list(range(n)), list(range(n)))
        assert svg.count("<circle") == n

    def test_custom_point_size(self, plot):
        svg = plot._plot_scatter_svg([0], [0], point_size=10)
        assert 'r="10"' in svg

    def test_unequal_lengths_raises(self, plot):
        with pytest.raises(ValueError):
            plot._plot_scatter_svg([1, 2], [1])


class TestPlotBarSvg:
    def test_is_valid_xml(self, plot):
        assert _svg_is_valid_xml(plot._plot_bar_svg(["A", "B"], [1, 2]))

    def test_contains_rects(self, plot):
        svg = plot._plot_bar_svg(["A", "B", "C"], [10, 20, 15])
        assert _svg_has(svg, "rect")

    def test_title_present(self, plot):
        svg = plot._plot_bar_svg(["A"], [1], title="Sales")
        assert "Sales" in svg

    def test_horizontal_mode(self, plot):
        svg = plot._plot_bar_svg(["A", "B"], [10, 20], horizontal=True)
        assert _svg_has(svg, "rect")
        assert _svg_is_valid_xml(svg)

    def test_negative_values(self, plot):
        svg = plot._plot_bar_svg(["A", "B"], [-10, 10])
        assert _svg_is_valid_xml(svg)

    def test_unequal_lengths_raises(self, plot):
        with pytest.raises(ValueError):
            plot._plot_bar_svg(["A", "B"], [1])


class TestPlotHistSvg:
    def test_is_valid_xml(self, plot):
        assert _svg_is_valid_xml(plot._plot_hist_svg([1, 2, 2, 3, 3, 3]))

    def test_empty_data(self, plot):
        svg = plot._plot_hist_svg([])
        assert _svg_is_valid_xml(svg)

    def test_bins_respected(self, plot):
        svg = plot._plot_hist_svg(list(range(100)), bins=10)
        assert isinstance(svg, str) and len(svg) > 0

    def test_custom_color(self, plot):
        svg = plot._plot_hist_svg([1, 2, 3], color="#abcdef")
        assert "#abcdef" in svg


class TestPlotPieSvg:
    def test_is_valid_xml(self, plot):
        assert _svg_is_valid_xml(plot._plot_pie_svg(["A", "B"], [30, 70]))

    def test_contains_path(self, plot):
        svg = plot._plot_pie_svg(["A", "B", "C"], [40, 35, 25])
        assert _svg_has(svg, "path")

    def test_path_count_matches_slices(self, plot):
        svg = plot._plot_pie_svg(["X", "Y", "Z"], [10, 20, 30])
        assert svg.count("<path") == 3

    def test_percentage_labels_large_slice(self, plot):
        svg = plot._plot_pie_svg(["Big", "Small"], [90, 10])
        # The 90% slice should show a percentage
        assert "%" in svg

    def test_negative_values_raise(self, plot):
        with pytest.raises(ValueError, match="non-negative"):
            plot._plot_pie_svg(["A", "B"], [-1, 2])

    def test_all_zero(self, plot):
        svg = plot._plot_pie_svg(["A", "B"], [0, 0])
        assert _svg_is_valid_xml(svg)

    def test_unequal_lengths_raises(self, plot):
        with pytest.raises(ValueError):
            plot._plot_pie_svg(["A", "B", "C"], [1, 2])


# ===========================================================================
# SVG multi-series charts
# ===========================================================================


class TestPlotMultiLineSvg:
    def _make_series(self, n=2):
        x = linspace(0, 2 * math.pi, 30)
        return [
            {"x": x, "y": [math.sin(v + i) for v in x], "label": f"S{i}"}
            for i in range(n)
        ]

    def test_is_valid_xml(self, plot):
        assert _svg_is_valid_xml(plot._plot_multi_line_svg(self._make_series()))

    def test_polyline_count_matches_series(self, plot):
        svg = plot._plot_multi_line_svg(self._make_series(3))
        assert svg.count("<polyline") == 3

    def test_legend_labels_present(self, plot):
        svg = plot._plot_multi_line_svg(self._make_series(2))
        assert "S0" in svg and "S1" in svg

    def test_empty_series(self, plot):
        svg = plot._plot_multi_line_svg([])
        assert _svg_is_valid_xml(svg)

    def test_unequal_xy_raises(self, plot):
        with pytest.raises(ValueError):
            plot._plot_multi_line_svg([{"x": [0, 1], "y": [0], "label": "bad"}])


class TestPlotMultiBarSvg:
    def _make_grouped(self):
        return (
            ["Jan", "Feb", "Mar"],
            [{"values": [10, 12, 8], "label": "A"},
             {"values": [6, 9, 11], "label": "B"}],
        )

    def test_is_valid_xml(self, plot):
        cats, series = self._make_grouped()
        assert _svg_is_valid_xml(plot._plot_multi_bar_svg(cats, series))

    def test_contains_rects(self, plot):
        cats, series = self._make_grouped()
        svg = plot._plot_multi_bar_svg(cats, series)
        assert _svg_has(svg, "rect")

    def test_legend_labels(self, plot):
        cats, series = self._make_grouped()
        svg = plot._plot_multi_bar_svg(cats, series)
        assert "A" in svg and "B" in svg

    def test_empty_inputs(self, plot):
        svg = plot._plot_multi_bar_svg([], [])
        assert _svg_is_valid_xml(svg)


# ===========================================================================
# SVG extras
# ===========================================================================


class TestPlotAreaSvg:
    def test_is_valid_xml(self, plot):
        x = linspace(0, 1, 10)
        assert _svg_is_valid_xml(plot._plot_area_svg(x, x))

    def test_contains_polygon(self, plot):
        x = linspace(0, 1, 10)
        y = [v * v for v in x]
        svg = plot._plot_area_svg(x, y)
        assert _svg_has(svg, "polygon")

    def test_contains_polyline(self, plot):
        x = linspace(0, 1, 10)
        svg = plot._plot_area_svg(x, x)
        assert _svg_has(svg, "polyline")

    def test_custom_opacity(self, plot):
        x = [0, 1]
        svg = plot._plot_area_svg(x, x, opacity=0.3)
        assert "0.3" in svg

    def test_unequal_lengths_raises(self, plot):
        with pytest.raises(ValueError):
            plot._plot_area_svg([0, 1], [0])


class TestPlotStepSvg:
    def test_is_valid_xml(self, plot):
        assert _svg_is_valid_xml(
            plot._plot_step_svg([0, 1, 2, 3], [0, 1, 1, 2])
        )

    def test_contains_polyline(self, plot):
        svg = plot._plot_step_svg([0, 1, 2], [5, 10, 7])
        assert _svg_has(svg, "polyline")

    def test_step_point_count_is_doubled(self, plot):
        x = [0, 1, 2, 3]
        y = [0, 1, 1, 2]
        svg = plot._plot_step_svg(x, y)
        # 4 points generate 7 step-function vertices (4 pts + 3 horizontal steps)
        pts_match = re.search(r'points="([^"]+)"', svg)
        assert pts_match is not None
        pts = pts_match.group(1).strip().split()
        assert len(pts) == 2 * len(x) - 1

    def test_unequal_lengths_raises(self, plot):
        with pytest.raises(ValueError):
            plot._plot_step_svg([0, 1], [0])


class TestPlotHeatmapSvg:
    def test_is_valid_xml(self, plot):
        m = [[1, 2], [3, 4]]
        assert _svg_is_valid_xml(plot._plot_heatmap_svg(m))

    def test_rects_match_cells(self, plot):
        rows, cols = 3, 4
        m = [[r * cols + c for c in range(cols)] for r in range(rows)]
        svg = plot._plot_heatmap_svg(m)
        # Each cell + outer background = rows*cols + 1 rects
        assert svg.count("<rect") >= rows * cols

    def test_row_labels(self, plot):
        m = [[1, 2], [3, 4]]
        svg = plot._plot_heatmap_svg(m, row_labels=["Row0", "Row1"])
        assert "Row0" in svg

    def test_col_labels(self, plot):
        m = [[1, 2], [3, 4]]
        svg = plot._plot_heatmap_svg(m, col_labels=["Col0", "Col1"])
        assert "Col0" in svg

    def test_empty_matrix(self, plot):
        svg = plot._plot_heatmap_svg([])
        assert _svg_is_valid_xml(svg)


class TestPlotBoxSvg:
    def _make_data(self, n=100, seed=42):
        import random
        rng = random.Random(seed)
        return [[rng.gauss(0, 1) for _ in range(n)],
                [rng.gauss(3, 0.5) for _ in range(n)]]

    def test_is_valid_xml(self, plot):
        assert _svg_is_valid_xml(plot._plot_box_svg(self._make_data()))

    def test_contains_rects(self, plot):
        svg = plot._plot_box_svg(self._make_data())
        assert _svg_has(svg, "rect")

    def test_median_lines_present(self, plot):
        svg = plot._plot_box_svg(self._make_data())
        assert _svg_has(svg, "line")

    def test_labels_in_output(self, plot):
        svg = plot._plot_box_svg(self._make_data(), labels=["Control", "Treatment"])
        assert "Control" in svg and "Treatment" in svg

    def test_empty_groups(self, plot):
        svg = plot._plot_box_svg([])
        assert _svg_is_valid_xml(svg)


class TestPlotAnnotationsSvg:
    def test_annotation_text_injected(self, plot):
        base = plot._plot_line_svg([0, 1], [0, 1])
        ann = plot._plot_annotations_svg(base, [{"x": 50, "y": 50, "text": "Important"}])
        assert "Important" in ann

    def test_arrow_adds_line_and_circle(self, plot):
        base = plot._plot_line_svg([0, 1], [0, 1])
        ann = plot._plot_annotations_svg(
            base,
            [{"x": 50, "y": 50, "text": "Note", "arrow_to": (200, 200)}]
        )
        # An arrow line and a filled circle should be added
        assert ann.count("<line") > base.count("<line")
        assert ann.count("<circle") > base.count("<circle")

    def test_multiple_annotations(self, plot):
        base = plot._plot_line_svg([0, 1], [0, 1])
        anns = [
            {"x": 10, "y": 10, "text": "First"},
            {"x": 20, "y": 20, "text": "Second"},
        ]
        result = plot._plot_annotations_svg(base, anns)
        assert "First" in result and "Second" in result

    def test_result_is_valid_xml(self, plot):
        base = plot._plot_line_svg([0, 1], [0, 1])
        result = plot._plot_annotations_svg(base, [{"x": 0, "y": 0, "text": "hi"}])
        assert _svg_is_valid_xml(result)

    def test_custom_color(self, plot):
        base = plot._plot_line_svg([0, 1], [0, 1])
        result = plot._plot_annotations_svg(
            base, [{"x": 0, "y": 0, "text": "c", "color": "#123456"}]
        )
        assert "#123456" in result


# ===========================================================================
# SVG utilities
# ===========================================================================


class TestPlotSave:
    def test_writes_file(self, plot):
        svg = plot._plot_line_svg([0, 1], [0, 1])
        with tempfile.NamedTemporaryFile(suffix=".svg", delete=False) as tf:
            path = tf.name
        try:
            written = plot._plot_save(svg, path)
            assert os.path.exists(written)
            assert os.path.getsize(written) > 100
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_returns_absolute_path(self, plot):
        svg = plot._plot_line_svg([0, 1], [0, 1])
        with tempfile.NamedTemporaryFile(suffix=".svg", delete=False) as tf:
            path = tf.name
        try:
            written = plot._plot_save(svg, path)
            assert os.path.isabs(written)
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_file_content_matches(self, plot):
        svg = plot._plot_line_svg([0, 1], [0, 1])
        with tempfile.NamedTemporaryFile(suffix=".svg", delete=False) as tf:
            path = tf.name
        try:
            plot._plot_save(svg, path)
            with open(path, encoding="utf-8") as f:
                content = f.read()
            assert content == svg
        finally:
            if os.path.exists(path):
                os.unlink(path)


class TestPlotToHtml:
    def test_doctype_present(self, plot):
        svg = plot._plot_line_svg([0, 1], [0, 1])
        html = plot._plot_to_html(svg)
        assert "<!DOCTYPE html>" in html

    def test_svg_embedded(self, plot):
        svg = plot._plot_line_svg([0, 1], [0, 1])
        html = plot._plot_to_html(svg)
        assert "<svg" in html

    def test_title_in_head(self, plot):
        svg = plot._plot_line_svg([0, 1], [0, 1])
        html = plot._plot_to_html(svg, title="My Chart")
        assert "My Chart" in html

    def test_html_body_tags(self, plot):
        svg = plot._plot_line_svg([0, 1], [0, 1])
        html = plot._plot_to_html(svg)
        assert "<body>" in html and "</body>" in html


class TestPlotCombineSvg:
    def _two_svgs(self, plot):
        s1 = plot._plot_line_svg([0, 1], [0, 1])
        s2 = plot._plot_scatter_svg([0, 1], [0, 1])
        return [s1, s2]

    def test_is_valid_xml(self, plot):
        assert _svg_is_valid_xml(plot._plot_combine_svg(self._two_svgs(plot)))

    def test_group_count_matches_charts(self, plot):
        svgs = self._two_svgs(plot)
        combined = plot._plot_combine_svg(svgs)
        assert combined.count('<g transform="translate') == 2

    def test_empty_list(self, plot):
        result = plot._plot_combine_svg([])
        assert result == ""

    def test_custom_cols(self, plot):
        svgs = [plot._plot_line_svg([0, 1], [0, 1]) for _ in range(4)]
        combined = plot._plot_combine_svg(svgs, cols=4)
        assert combined.count('<g transform="translate') == 4


class TestPlotSvgDimensions:
    def test_default_line_chart(self, plot):
        svg = plot._plot_line_svg([0, 1], [0, 1])
        dims = plot._plot_svg_dimensions(svg)
        assert dims == {"width": 600, "height": 400}

    def test_custom_size(self, plot):
        svg = plot._plot_bar_svg(["A"], [1], width=800, height=600)
        dims = plot._plot_svg_dimensions(svg)
        assert dims == {"width": 800, "height": 600}

    def test_empty_string(self, plot):
        dims = plot._plot_svg_dimensions("")
        assert dims["width"] == 0 and dims["height"] == 0


# ===========================================================================
# Registration
# ===========================================================================


class TestRegistration:
    def test_register_plot_functions_callable(self, plot):
        assert callable(plot.register_plot_functions)

    def test_all_functions_registered(self, plot):
        class FakeRuntime:
            def __init__(self):
                self.registered = {}

            def register_function(self, name, fn):
                self.registered[name] = fn

        rt = FakeRuntime()
        plot.register_plot_functions(rt)
        expected = {
            "plot_linspace", "plot_arange", "plot_normalize", "plot_scale",
            "plot_stats", "plot_color_scheme",
            "plot_line_text", "plot_bar_text", "plot_scatter_text", "plot_hist_text",
            "plot_save", "plot_to_html", "plot_combine_svg", "plot_svg_dimensions",
            "plot_line_svg", "plot_scatter_svg", "plot_bar_svg",
            "plot_hist_svg", "plot_pie_svg",
            "plot_multi_line_svg", "plot_multi_bar_svg",
            "plot_area_svg", "plot_step_svg", "plot_heatmap_svg",
            "plot_box_svg", "plot_annotations_svg",
        }
        assert expected.issubset(rt.registered.keys())

    def test_registered_functions_callable(self, plot):
        class FakeRuntime:
            def __init__(self):
                self.registered = {}

            def register_function(self, name, fn):
                self.registered[name] = fn

        rt = FakeRuntime()
        plot.register_plot_functions(rt)
        for name, fn in rt.registered.items():
            assert callable(fn), f"{name} is not callable"
