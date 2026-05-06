"""
Tests for src/nlpl/stdlib/gui/__init__.py

All tests run without a real graphical display.  The approach is:

  - For availability / graceful-degradation tests: patch `_tk_available = False`
    and assert functions return sentinel values (None, False, "", [], etc.).

  - For functional tests: patch `_tk_available = True`, inject a minimal tkinter
    façade via unittest.mock, and verify that the module calls the right tkinter
    primitives with the right arguments.

No tests import tkinter directly; they only interact with the NexusLang GUI module API.
"""

import sys
import types
import unittest
from unittest.mock import MagicMock, patch, call

# ---------------------------------------------------------------------------
# Helpers to build a minimal tkinter mock tree
# ---------------------------------------------------------------------------

def _make_tk_mock():
    """Return a mock that impersonates the tkinter module."""
    tk = MagicMock(name="tkinter")

    # Constants
    tk.NORMAL = "normal"
    tk.DISABLED = "disabled"
    tk.END = "end"
    tk.BOTH = "both"
    tk.HORIZONTAL = "horizontal"
    tk.VERTICAL = "vertical"
    tk.SINGLE = "single"
    tk.MULTIPLE = "multiple"
    tk.NONE = "none"
    tk.CHAR = "char"
    tk.WORD = "word"

    # Tk root
    root = MagicMock(name="Tk_instance")
    root.winfo_width.return_value = 800
    root.winfo_height.return_value = 600
    root.winfo_x.return_value = 100
    root.winfo_y.return_value = 50
    root.winfo_screenwidth.return_value = 1920
    root.winfo_screenheight.return_value = 1080
    root.geometry.return_value = "800x600+100+50"
    tk.Tk.return_value = root

    # Toplevel
    tl = MagicMock(name="Toplevel_instance")
    tl.winfo_width.return_value = 400
    tl.winfo_height.return_value = 300
    tl.winfo_x.return_value = 0
    tl.winfo_y.return_value = 0
    tl.geometry.return_value = "400x300+0+0"
    tk.Toplevel.return_value = tl

    # Canvas
    canvas = MagicMock(name="Canvas_instance")
    canvas.create_line.return_value = 1
    canvas.create_rectangle.return_value = 2
    canvas.create_oval.return_value = 3
    canvas.create_arc.return_value = 4
    canvas.create_polygon.return_value = 5
    canvas.create_text.return_value = 6
    canvas.create_image.return_value = 7
    canvas.coords.return_value = [10.0, 20.0, 110.0, 120.0]
    canvas.curselection.return_value = ()
    tk.Canvas.return_value = canvas

    # StringVar / BooleanVar / DoubleVar
    sv = MagicMock(name="StringVar")
    sv.get.return_value = "hello"
    bv = MagicMock(name="BooleanVar")
    bv.get.return_value = False
    dv = MagicMock(name="DoubleVar")
    dv.get.return_value = 42.0
    tk.StringVar.return_value = sv
    tk.BooleanVar.return_value = bv
    tk.DoubleVar.return_value = dv

    # Widgets
    lbl = MagicMock(name="Label")
    lbl.cget.return_value = "Test"
    tk.Label.return_value = lbl

    btn = MagicMock(name="Button")
    tk.Button.return_value = btn

    entry = MagicMock(name="Entry")
    entry.get.return_value = "entry_text"
    tk.Entry.return_value = entry

    cb = MagicMock(name="Checkbutton")
    tk.Checkbutton.return_value = cb

    lb = MagicMock(name="Listbox")
    lb.curselection.return_value = (0, 1)
    lb.get.side_effect = lambda i: f"item{i}"
    tk.Listbox.return_value = lb

    scale = MagicMock(name="Scale")
    scale.get.return_value = 50.0
    tk.Scale.return_value = scale

    text_widget = MagicMock(name="Text")
    text_widget.get.return_value = "area text"
    tk.Text.return_value = text_widget

    frame = MagicMock(name="Frame")
    tk.Frame.return_value = frame

    return tk, root, canvas


def _make_ttk_mock():
    """Return a minimal ttk module mock."""
    ttk = MagicMock(name="ttk")
    progressbar = MagicMock(name="Progressbar")
    ttk.Progressbar.return_value = progressbar
    sep = MagicMock(name="Separator")
    ttk.Separator.return_value = sep
    return ttk, progressbar, sep


def _make_font_mock():
    """Return a minimal tkinter.font module mock."""
    font_mod = MagicMock(name="tkinter.font")
    font_obj = MagicMock(name="Font_instance")
    font_obj.measure.return_value = 75
    font_obj.metrics.return_value = {"ascent": 12, "descent": 3,
                                      "linespace": 15, "fixed": 0}
    font_obj.actual.return_value = {"family": "Arial", "size": 12,
                                     "weight": "normal", "slant": "roman"}
    font_mod.Font.return_value = font_obj
    font_mod.families.return_value = ("Arial", "Courier", "Helvetica")
    return font_mod, font_obj


# ---------------------------------------------------------------------------
# Base helper: reset module-level storage between tests
# ---------------------------------------------------------------------------

import nexuslang.stdlib.gui as _gui_mod


def _reset():
    """Clear all in-module storage dictionaries."""
    _gui_mod._windows.clear()
    _gui_mod._canvases.clear()
    _gui_mod._fonts.clear()
    _gui_mod._widgets.clear()
    _gui_mod._widget_meta.clear()
    _gui_mod._event_queues.clear()
    _gui_mod._counter = 0


# ===========================================================================
# 1.  Availability checks
# ===========================================================================

class TestGuiAvailability(unittest.TestCase):
    """Tests for gui_is_available() and graceful degradation."""

    def test_is_available_returns_bool(self):
        result = _gui_mod.gui_is_available()
        self.assertIsInstance(result, bool)

    def test_unavailable_when_patched_false(self):
        with patch.object(_gui_mod, "_tk_available", False):
            self.assertFalse(_gui_mod.gui_is_available())

    def test_available_when_patched_true(self):
        with patch.object(_gui_mod, "_tk_available", True):
            self.assertTrue(_gui_mod.gui_is_available())

    def test_count_helpers_return_ints(self):
        self.assertIsInstance(_gui_mod.gui_window_count(), int)
        self.assertIsInstance(_gui_mod.gui_canvas_count(), int)
        self.assertIsInstance(_gui_mod.gui_widget_count(), int)
        self.assertIsInstance(_gui_mod.gui_font_count(), int)


# ===========================================================================
# 2.  color helper
# ===========================================================================

class TestCoerceColor(unittest.TestCase):
    def test_hex_string_passthrough(self):
        self.assertEqual(_gui_mod._coerce_color("#ff0000"), "#ff0000")

    def test_named_color_passthrough(self):
        self.assertEqual(_gui_mod._coerce_color("red"), "red")

    def test_rgb_tuple_to_hex(self):
        result = _gui_mod._coerce_color((255, 0, 0))
        self.assertEqual(result.lower(), "#ff0000")

    def test_rgb_list_to_hex(self):
        result = _gui_mod._coerce_color([0, 255, 0])
        self.assertEqual(result.lower(), "#00ff00")

    def test_none_returns_empty(self):
        self.assertEqual(_gui_mod._coerce_color(None), "")


# ===========================================================================
# 3.  Window functions — unavailable path
# ===========================================================================

class TestWindowFunctionsUnavailable(unittest.TestCase):
    """All window functions return safe sentinels when GUI is unavailable."""

    def setUp(self):
        _reset()
        self._patcher = patch.object(_gui_mod, "_tk_available", False)
        self._patcher.start()

    def tearDown(self):
        self._patcher.stop()
        _reset()

    def test_window_create_returns_none(self):
        self.assertIsNone(_gui_mod.window_create("Test", 800, 600))

    def test_window_show_false(self):
        self.assertFalse(_gui_mod.window_show(1))

    def test_window_hide_false(self):
        self.assertFalse(_gui_mod.window_hide(1))

    def test_window_destroy_false(self):
        self.assertFalse(_gui_mod.window_destroy(1))

    def test_window_is_open_false(self):
        self.assertFalse(_gui_mod.window_is_open(1))

    def test_window_set_title_false(self):
        self.assertFalse(_gui_mod.window_set_title(1, "x"))

    def test_window_set_size_false(self):
        self.assertFalse(_gui_mod.window_set_size(1, 100, 100))

    def test_window_get_size_none(self):
        self.assertIsNone(_gui_mod.window_get_size(1))

    def test_window_set_position_false(self):
        self.assertFalse(_gui_mod.window_set_position(1, 0, 0))

    def test_window_get_position_none(self):
        self.assertIsNone(_gui_mod.window_get_position(1))

    def test_window_set_background_false(self):
        self.assertFalse(_gui_mod.window_set_background(1, "#000"))

    def test_window_fullscreen_false(self):
        self.assertFalse(_gui_mod.window_fullscreen(1, True))

    def test_window_minimize_false(self):
        self.assertFalse(_gui_mod.window_minimize(1))

    def test_window_maximize_false(self):
        self.assertFalse(_gui_mod.window_maximize(1))

    def test_window_update_false(self):
        self.assertFalse(_gui_mod.window_update(1))

    def test_window_screenshot_false(self):
        self.assertFalse(_gui_mod.window_screenshot(1, "/tmp/x.png"))

    def test_window_get_screen_returns_zeros(self):
        result = _gui_mod.window_get_screen_size()
        self.assertEqual(result, {"width": 0, "height": 0})

    def test_gui_quit_noop(self):
        _gui_mod.gui_quit()  # must not raise


# ===========================================================================
# 4.  Window functions — available path (tkinter mocked)
# ===========================================================================

class TestWindowFunctionsAvailable(unittest.TestCase):
    def setUp(self):
        _reset()
        self._tk_mock, self._root_mock, _ = _make_tk_mock()
        self._patches = [
            patch.object(_gui_mod, "_tk_available", True),
            patch.object(_gui_mod, "_tk", self._tk_mock),
        ]
        for p in self._patches:
            p.start()

    def tearDown(self):
        for p in reversed(self._patches):
            p.stop()
        _reset()

    def test_window_create_returns_int(self):
        win_id = _gui_mod.window_create("Hello", 640, 480)
        self.assertIsInstance(win_id, int)
        self.assertGreater(win_id, 0)

    def test_window_create_stores_instance(self):
        win_id = _gui_mod.window_create("Hello", 640, 480)
        self.assertIn(win_id, _gui_mod._windows)

    def test_window_is_open_true_after_create(self):
        win_id = _gui_mod.window_create("Title", 400, 300)
        self.assertTrue(_gui_mod.window_is_open(win_id))

    def test_window_is_open_false_for_unknown(self):
        self.assertFalse(_gui_mod.window_is_open(99999))

    def test_window_show_calls_deiconify(self):
        win_id = _gui_mod.window_create("T", 100, 100)
        result = _gui_mod.window_show(win_id)
        self.assertTrue(result)
        _gui_mod._windows[win_id].deiconify.assert_called_once()

    def test_window_hide_calls_withdraw(self):
        win_id = _gui_mod.window_create("T", 100, 100)
        result = _gui_mod.window_hide(win_id)
        self.assertTrue(result)
        _gui_mod._windows[win_id].withdraw.assert_called()

    def test_window_destroy_removes_from_storage(self):
        win_id = _gui_mod.window_create("T", 100, 100)
        result = _gui_mod.window_destroy(win_id)
        self.assertTrue(result)
        self.assertNotIn(win_id, _gui_mod._windows)

    def test_window_set_title_calls_title_method(self):
        win_id = _gui_mod.window_create("T", 100, 100)
        _gui_mod.window_set_title(win_id, "New Title")
        _gui_mod._windows[win_id].title.assert_called_with("New Title")

    def test_window_set_size_calls_geometry(self):
        win_id = _gui_mod.window_create("T", 100, 100)
        _gui_mod.window_set_size(win_id, 1024, 768)
        _gui_mod._windows[win_id].geometry.assert_called_with("1024x768")

    def test_window_get_size_returns_dict(self):
        win_id = _gui_mod.window_create("T", 800, 600)
        size = _gui_mod.window_get_size(win_id)
        self.assertIsInstance(size, dict)
        self.assertIn("width", size)
        self.assertIn("height", size)

    def test_window_set_background_calls_configure(self):
        win_id = _gui_mod.window_create("T", 100, 100)
        _gui_mod.window_set_background(win_id, "#1e1e2e")
        _gui_mod._windows[win_id].configure.assert_called()

    def test_window_fullscreen_calls_attributes(self):
        win_id = _gui_mod.window_create("T", 100, 100)
        _gui_mod.window_fullscreen(win_id, True)
        _gui_mod._windows[win_id].attributes.assert_called_with("-fullscreen", True)

    def test_window_minimize_calls_iconify(self):
        win_id = _gui_mod.window_create("T", 100, 100)
        _gui_mod.window_minimize(win_id)
        _gui_mod._windows[win_id].iconify.assert_called_once()

    def test_window_update_calls_update(self):
        win_id = _gui_mod.window_create("T", 100, 100)
        result = _gui_mod.window_update(win_id)
        self.assertTrue(result)
        _gui_mod._windows[win_id].update.assert_called()

    def test_window_get_position_returns_dict(self):
        win_id = _gui_mod.window_create("T", 100, 100)
        pos = _gui_mod.window_get_position(win_id)
        self.assertIsInstance(pos, dict)
        self.assertIn("x", pos)
        self.assertIn("y", pos)

    def test_window_set_resizable(self):
        win_id = _gui_mod.window_create("T", 100, 100)
        result = _gui_mod.window_set_resizable(win_id, True, False)
        self.assertTrue(result)
        _gui_mod._windows[win_id].resizable.assert_called_with(True, False)

    def test_window_set_alpha(self):
        win_id = _gui_mod.window_create("T", 100, 100)
        result = _gui_mod.window_set_alpha(win_id, 0.8)
        self.assertTrue(result)
        _gui_mod._windows[win_id].attributes.assert_called()

    def test_second_window_uses_toplevel(self):
        _gui_mod.window_create("First", 400, 300)
        win_id2 = _gui_mod.window_create("Second", 400, 300)
        self.assertIn(win_id2, _gui_mod._windows)
        self._tk_mock.Toplevel.assert_called()

    def test_window_get_screen_size_returns_dict(self):
        _gui_mod.window_create("T", 100, 100)
        size = _gui_mod.window_get_screen_size()
        self.assertIsInstance(size, dict)
        self.assertIn("width", size)
        self.assertIn("height", size)


# ===========================================================================
# 5.  Canvas functions — unavailable path
# ===========================================================================

class TestCanvasFunctionsUnavailable(unittest.TestCase):
    def setUp(self):
        _reset()
        self._patcher = patch.object(_gui_mod, "_tk_available", False)
        self._patcher.start()

    def tearDown(self):
        self._patcher.stop()
        _reset()

    def test_canvas_create_none(self):
        self.assertIsNone(_gui_mod.canvas_create(1, 400, 300))

    def test_canvas_clear_false(self):
        self.assertFalse(_gui_mod.canvas_clear(1))

    def test_canvas_draw_line_none(self):
        self.assertIsNone(_gui_mod.canvas_draw_line(1, 0, 0, 100, 100))

    def test_canvas_draw_rect_none(self):
        self.assertIsNone(_gui_mod.canvas_draw_rect(1, 0, 0, 100, 100))

    def test_canvas_draw_oval_none(self):
        self.assertIsNone(_gui_mod.canvas_draw_oval(1, 0, 0, 100, 100))

    def test_canvas_draw_text_none(self):
        self.assertIsNone(_gui_mod.canvas_draw_text(1, 50, 50, "hi"))

    def test_canvas_move_item_false(self):
        self.assertFalse(_gui_mod.canvas_move_item(1, 2, 5, 5))

    def test_canvas_delete_item_false(self):
        self.assertFalse(_gui_mod.canvas_delete_item(1, 2))

    def test_canvas_item_coords_none(self):
        self.assertIsNone(_gui_mod.canvas_item_coords(1, 2))


# ===========================================================================
# 6.  Canvas functions — available path
# ===========================================================================

class TestCanvasFunctionsAvailable(unittest.TestCase):
    def setUp(self):
        _reset()
        self._tk_mock, self._root_mock, self._canvas_mock = _make_tk_mock()
        self._patches = [
            patch.object(_gui_mod, "_tk_available", True),
            patch.object(_gui_mod, "_tk", self._tk_mock),
        ]
        for p in self._patches:
            p.start()
        self._win_id = _gui_mod.window_create("T", 400, 300)

    def tearDown(self):
        for p in reversed(self._patches):
            p.stop()
        _reset()

    def test_canvas_create_returns_int(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        self.assertIsInstance(cid, int)
        self.assertIn(cid, _gui_mod._canvases)

    def test_canvas_clear_calls_delete_all(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        result = _gui_mod.canvas_clear(cid)
        self.assertTrue(result)
        _gui_mod._canvases[cid].delete.assert_called_with("all")

    def test_canvas_draw_line_returns_int(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_line(cid, 0, 0, 100, 100, "#ff0000", 2.0)
        self.assertIsNotNone(item)

    def test_canvas_draw_rect_returns_int(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_rect(cid, 10, 10, 90, 90, "#blue")
        self.assertIsNotNone(item)

    def test_canvas_draw_oval_returns_int(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_oval(cid, 10, 10, 90, 90)
        self.assertIsNotNone(item)

    def test_canvas_draw_arc_returns_int(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_arc(cid, 10, 10, 90, 90, 0, 180, style="pieslice")
        self.assertIsNotNone(item)

    def test_canvas_draw_polygon_flat_list(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_polygon(cid, [0, 0, 50, 100, 100, 0], "#aaaaaa")
        self.assertIsNotNone(item)

    def test_canvas_draw_polygon_nested_list(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_polygon(cid, [(0, 0), (50, 100), (100, 0)])
        self.assertIsNotNone(item)

    def test_canvas_draw_text_returns_int(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_text(cid, 200, 150, "NexusLang", "#ffffff", "Arial", 24)
        self.assertIsNotNone(item)

    def test_canvas_draw_text_bold_italic(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_text(cid, 200, 150, "Bold!", bold=True, italic=True)
        self.assertIsNotNone(item)

    def test_canvas_delete_item(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_line(cid, 0, 0, 10, 10)
        result = _gui_mod.canvas_delete_item(cid, item)
        self.assertTrue(result)
        _gui_mod._canvases[cid].delete.assert_called_with(item)

    def test_canvas_move_item(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_rect(cid, 0, 0, 50, 50)
        result = _gui_mod.canvas_move_item(cid, item, 10.0, 20.0)
        self.assertTrue(result)
        _gui_mod._canvases[cid].move.assert_called_with(item, 10.0, 20.0)

    def test_canvas_item_coords_returns_list(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_rect(cid, 0, 0, 50, 50)
        coords = _gui_mod.canvas_item_coords(cid, item)
        self.assertIsInstance(coords, list)

    def test_canvas_tag_raise(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_rect(cid, 0, 0, 50, 50)
        result = _gui_mod.canvas_tag_raise(cid, item)
        self.assertTrue(result)
        _gui_mod._canvases[cid].tag_raise.assert_called_with(item)

    def test_canvas_tag_lower(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_rect(cid, 0, 0, 50, 50)
        result = _gui_mod.canvas_tag_lower(cid, item)
        self.assertTrue(result)
        _gui_mod._canvases[cid].tag_lower.assert_called_with(item)

    def test_canvas_set_background(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        result = _gui_mod.canvas_set_background(cid, "#000000")
        self.assertTrue(result)

    def test_canvas_resize(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        result = _gui_mod.canvas_resize(cid, 800, 600)
        self.assertTrue(result)

    def test_canvas_update(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        result = _gui_mod.canvas_update(cid)
        self.assertTrue(result)

    def test_canvas_draw_line_with_dash(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_line(cid, 0, 0, 100, 100, dash=[4, 2])
        self.assertIsNotNone(item)

    def test_canvas_draw_line_with_arrow(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_line(cid, 0, 0, 100, 100, arrow="last")
        self.assertIsNotNone(item)

    def test_canvas_configure_item(self):
        cid = _gui_mod.canvas_create(self._win_id, 400, 300)
        item = _gui_mod.canvas_draw_rect(cid, 0, 0, 50, 50)
        result = _gui_mod.canvas_configure_item(cid, item, fill="blue")
        self.assertTrue(result)
        _gui_mod._canvases[cid].itemconfigure.assert_called_with(item, fill="blue")


# ===========================================================================
# 7.  Font functions — unavailable path
# ===========================================================================

class TestFontFunctionsUnavailable(unittest.TestCase):
    def setUp(self):
        _reset()
        self._patches = [
            patch.object(_gui_mod, "_tk_available", False),
            patch.object(_gui_mod, "_font_module", None),
        ]
        for p in self._patches:
            p.start()

    def tearDown(self):
        for p in reversed(self._patches):
            p.stop()
        _reset()

    def test_font_families_returns_empty_list(self):
        self.assertEqual(_gui_mod.font_families(), [])

    def test_font_list_returns_empty(self):
        self.assertEqual(_gui_mod.font_list(), [])

    def test_font_create_returns_none(self):
        self.assertIsNone(_gui_mod.font_create("Arial", 12))

    def test_font_destroy_false(self):
        self.assertFalse(_gui_mod.font_destroy(1))

    def test_font_measure_returns_zero(self):
        self.assertEqual(_gui_mod.font_measure(1, "hello"), 0)

    def test_font_metrics_returns_zeros(self):
        m = _gui_mod.font_metrics(1)
        self.assertEqual(m["ascent"], 0)
        self.assertEqual(m["descent"], 0)
        self.assertEqual(m["linespace"], 0)
        self.assertFalse(m["fixed"])


# ===========================================================================
# 8.  Font functions — available path
# ===========================================================================

class TestFontFunctionsAvailable(unittest.TestCase):
    def setUp(self):
        _reset()
        self._font_mod_mock, self._font_obj_mock = _make_font_mock()
        self._patches = [
            patch.object(_gui_mod, "_tk_available", True),
            patch.object(_gui_mod, "_font_module", self._font_mod_mock),
        ]
        for p in self._patches:
            p.start()

    def tearDown(self):
        for p in reversed(self._patches):
            p.stop()
        _reset()

    def test_font_families_returns_sorted_list(self):
        families = _gui_mod.font_families()
        self.assertIsInstance(families, list)
        self.assertEqual(families, sorted(families))

    def test_font_create_returns_int(self):
        fid = _gui_mod.font_create("Arial", 14, bold=True)
        self.assertIsInstance(fid, int)
        self.assertIn(fid, _gui_mod._fonts)

    def test_font_create_normal_variant(self):
        fid = _gui_mod.font_create("Courier", 10, bold=False, italic=False)
        self.assertIsNotNone(fid)
        self._font_mod_mock.Font.assert_called_with(
            family="Courier", size=10, weight="normal", slant="roman", underline=0)

    def test_font_create_bold_italic(self):
        fid = _gui_mod.font_create("Helvetica", 12, bold=True, italic=True)
        self.assertIsNotNone(fid)
        self._font_mod_mock.Font.assert_called_with(
            family="Helvetica", size=12, weight="bold", slant="italic", underline=0)

    def test_font_measure_returns_int(self):
        fid = _gui_mod.font_create("Arial", 12)
        width = _gui_mod.font_measure(fid, "hello world")
        self.assertIsInstance(width, int)
        self.assertGreater(width, 0)

    def test_font_metrics_returns_dict(self):
        fid = _gui_mod.font_create("Arial", 12)
        m = _gui_mod.font_metrics(fid)
        self.assertIn("ascent", m)
        self.assertIn("descent", m)
        self.assertIn("linespace", m)
        self.assertIn("fixed", m)

    def test_font_actual_returns_dict(self):
        fid = _gui_mod.font_create("Arial", 12)
        actual = _gui_mod.font_actual(fid)
        self.assertIsInstance(actual, dict)

    def test_font_destroy_removes_from_storage(self):
        fid = _gui_mod.font_create("Arial", 12)
        result = _gui_mod.font_destroy(fid)
        self.assertTrue(result)
        self.assertNotIn(fid, _gui_mod._fonts)

    def test_font_destroy_nonexistent_false(self):
        result = _gui_mod.font_destroy(99999)
        self.assertFalse(result)

    def test_font_configure_changes_properties(self):
        fid = _gui_mod.font_create("Arial", 12)
        result = _gui_mod.font_configure(fid, family="Courier", size=16)
        self.assertTrue(result)
        _gui_mod._fonts[fid].configure.assert_called()


# ===========================================================================
# 9.  Widget functions — unavailable path
# ===========================================================================

class TestWidgetFunctionsUnavailable(unittest.TestCase):
    def setUp(self):
        _reset()
        self._patches = [
            patch.object(_gui_mod, "_tk_available", False),
            patch.object(_gui_mod, "_ttk", None),
        ]
        for p in self._patches:
            p.start()

    def tearDown(self):
        for p in reversed(self._patches):
            p.stop()
        _reset()

    def test_widget_label_none(self):
        self.assertIsNone(_gui_mod.widget_label(1, "text"))

    def test_widget_button_none(self):
        self.assertIsNone(_gui_mod.widget_button(1, "OK"))

    def test_widget_entry_none(self):
        self.assertIsNone(_gui_mod.widget_entry(1))

    def test_widget_checkbox_none(self):
        self.assertIsNone(_gui_mod.widget_checkbox(1, "Check"))

    def test_widget_listbox_none(self):
        self.assertIsNone(_gui_mod.widget_listbox(1))

    def test_widget_slider_none(self):
        self.assertIsNone(_gui_mod.widget_slider(1))

    def test_widget_text_area_none(self):
        self.assertIsNone(_gui_mod.widget_text_area(1))

    def test_widget_frame_none(self):
        self.assertIsNone(_gui_mod.widget_frame(1))

    def test_widget_get_text_empty(self):
        self.assertEqual(_gui_mod.widget_get_text(1), "")

    def test_widget_set_text_false(self):
        self.assertFalse(_gui_mod.widget_set_text(1, "x"))

    def test_widget_get_value_none(self):
        self.assertIsNone(_gui_mod.widget_get_value(1))

    def test_widget_set_value_false(self):
        self.assertFalse(_gui_mod.widget_set_value(1, 50))

    def test_widget_get_checked_false(self):
        self.assertFalse(_gui_mod.widget_get_checked(1))

    def test_widget_set_checked_false(self):
        self.assertFalse(_gui_mod.widget_set_checked(1, True))

    def test_widget_get_selected_none(self):
        self.assertIsNone(_gui_mod.widget_get_selected(1))

    def test_widget_set_items_false(self):
        self.assertFalse(_gui_mod.widget_set_items(1, ["a", "b"]))

    def test_widget_move_false(self):
        self.assertFalse(_gui_mod.widget_move(1, 10, 10))

    def test_widget_destroy_false(self):
        self.assertFalse(_gui_mod.widget_destroy(1))

    def test_widget_set_enabled_false(self):
        self.assertFalse(_gui_mod.widget_set_enabled(1, True))

    def test_widget_set_visible_false(self):
        self.assertFalse(_gui_mod.widget_set_visible(1, True))


# ===========================================================================
# 10. Widget functions — available path
# ===========================================================================

class TestWidgetFunctionsAvailable(unittest.TestCase):
    def setUp(self):
        _reset()
        self._tk_mock, self._root_mock, _ = _make_tk_mock()
        self._ttk_mock, self._pb_mock, self._sep_mock = _make_ttk_mock()
        self._patches = [
            patch.object(_gui_mod, "_tk_available", True),
            patch.object(_gui_mod, "_tk", self._tk_mock),
            patch.object(_gui_mod, "_ttk", self._ttk_mock),
        ]
        for p in self._patches:
            p.start()
        self._win_id = _gui_mod.window_create("T", 400, 300)

    def tearDown(self):
        for p in reversed(self._patches):
            p.stop()
        _reset()

    def test_widget_label_returns_id(self):
        wid = _gui_mod.widget_label(self._win_id, "Hello", 10, 10)
        self.assertIsInstance(wid, int)
        self.assertIn(wid, _gui_mod._widgets)

    def test_widget_button_returns_id(self):
        wid = _gui_mod.widget_button(self._win_id, "Click Me", 10, 10)
        self.assertIsInstance(wid, int)
        self.assertIn(wid, _gui_mod._widgets)

    def test_widget_entry_returns_id(self):
        wid = _gui_mod.widget_entry(self._win_id, 0, 0, 120, "default")
        self.assertIsInstance(wid, int)
        self.assertIn(wid, _gui_mod._widgets)

    def test_widget_checkbox_returns_id(self):
        wid = _gui_mod.widget_checkbox(self._win_id, "Enable feature", 0, 0)
        self.assertIsInstance(wid, int)

    def test_widget_listbox_returns_id(self):
        wid = _gui_mod.widget_listbox(self._win_id, 0, 0, 120, 80,
                                       items=["alpha", "beta", "gamma"])
        self.assertIsInstance(wid, int)

    def test_widget_slider_returns_id(self):
        wid = _gui_mod.widget_slider(self._win_id, 0, 0, 150, 0, 100, 50)
        self.assertIsInstance(wid, int)

    def test_widget_progressbar_returns_id(self):
        wid = _gui_mod.widget_progressbar(self._win_id, 0, 0, 150, 20)
        self.assertIsInstance(wid, int)

    def test_widget_text_area_returns_id(self):
        wid = _gui_mod.widget_text_area(self._win_id, 0, 0, 200, 100, "init text")
        self.assertIsInstance(wid, int)

    def test_widget_frame_returns_id(self):
        wid = _gui_mod.widget_frame(self._win_id, 0, 0, 200, 150)
        self.assertIsInstance(wid, int)

    def test_widget_separator_returns_id(self):
        wid = _gui_mod.widget_separator(self._win_id, 0, 100, 200)
        self.assertIsInstance(wid, int)

    def test_widget_label_type_meta(self):
        wid = _gui_mod.widget_label(self._win_id, "hi")
        self.assertEqual(_gui_mod._widget_meta[wid]["type"], "label")

    def test_widget_entry_type_meta(self):
        wid = _gui_mod.widget_entry(self._win_id)
        self.assertEqual(_gui_mod._widget_meta[wid]["type"], "entry")

    def test_widget_checkbox_type_meta(self):
        wid = _gui_mod.widget_checkbox(self._win_id, "cb")
        self.assertEqual(_gui_mod._widget_meta[wid]["type"], "checkbox")

    def test_widget_get_text_label(self):
        wid = _gui_mod.widget_label(self._win_id, "Label text")
        text = _gui_mod.widget_get_text(wid)
        self.assertIsInstance(text, str)

    def test_widget_set_text_label(self):
        wid = _gui_mod.widget_label(self._win_id, "old")
        result = _gui_mod.widget_set_text(wid, "new")
        self.assertTrue(result)
        _gui_mod._widgets[wid].configure.assert_called_with(text="new")

    def test_widget_get_text_entry_uses_var(self):
        wid = _gui_mod.widget_entry(self._win_id, initial_text="hello")
        _ = _gui_mod.widget_get_text(wid)
        # var.get() should be called
        _gui_mod._widget_meta[wid]["var"].get.assert_called()

    def test_widget_set_text_entry_sets_var(self):
        wid = _gui_mod.widget_entry(self._win_id)
        result = _gui_mod.widget_set_text(wid, "world")
        self.assertTrue(result)
        _gui_mod._widget_meta[wid]["var"].set.assert_called_with("world")

    def test_widget_get_checked_uses_var(self):
        wid = _gui_mod.widget_checkbox(self._win_id, "cb")
        _ = _gui_mod.widget_get_checked(wid)
        _gui_mod._widget_meta[wid]["var"].get.assert_called()

    def test_widget_set_checked_sets_var(self):
        wid = _gui_mod.widget_checkbox(self._win_id, "cb")
        result = _gui_mod.widget_set_checked(wid, True)
        self.assertTrue(result)
        _gui_mod._widget_meta[wid]["var"].set.assert_called_with(True)

    def test_widget_get_value_slider(self):
        wid = _gui_mod.widget_slider(self._win_id, 0, 0, 150, 0, 100, 50)
        val = _gui_mod.widget_get_value(wid)
        self.assertIsNotNone(val)

    def test_widget_set_value_slider(self):
        wid = _gui_mod.widget_slider(self._win_id, 0, 0, 150, 0, 100, 50)
        result = _gui_mod.widget_set_value(wid, 75.0)
        self.assertTrue(result)
        _gui_mod._widget_meta[wid]["var"].set.assert_called_with(75.0)

    def test_widget_get_selected_returns_list(self):
        wid = _gui_mod.widget_listbox(self._win_id, items=["a", "b", "c"])
        selected = _gui_mod.widget_get_selected(wid)
        self.assertIsInstance(selected, list)

    def test_widget_set_items_clears_and_repopulates(self):
        wid = _gui_mod.widget_listbox(self._win_id, items=["old"])
        result = _gui_mod.widget_set_items(wid, ["new1", "new2"])
        self.assertTrue(result)
        _gui_mod._widgets[wid].delete.assert_called()

    def test_widget_move_calls_place(self):
        wid = _gui_mod.widget_label(self._win_id, "lbl")
        result = _gui_mod.widget_move(wid, 50, 80)
        self.assertTrue(result)
        _gui_mod._widgets[wid].place.assert_called_with(x=50, y=80)

    def test_widget_resize_calls_place(self):
        wid = _gui_mod.widget_label(self._win_id, "lbl")
        result = _gui_mod.widget_resize(wid, 200, 40)
        self.assertTrue(result)
        _gui_mod._widgets[wid].place.assert_called_with(width=200, height=40)

    def test_widget_destroy_removes_from_storage(self):
        wid = _gui_mod.widget_label(self._win_id, "bye")
        result = _gui_mod.widget_destroy(wid)
        self.assertTrue(result)
        self.assertNotIn(wid, _gui_mod._widgets)
        self.assertNotIn(wid, _gui_mod._widget_meta)

    def test_widget_set_enabled_normal(self):
        wid = _gui_mod.widget_button(self._win_id, "btn")
        result = _gui_mod.widget_set_enabled(wid, True)
        self.assertTrue(result)
        _gui_mod._widgets[wid].configure.assert_called_with(state=self._tk_mock.NORMAL)

    def test_widget_set_enabled_disabled(self):
        wid = _gui_mod.widget_button(self._win_id, "btn")
        result = _gui_mod.widget_set_enabled(wid, False)
        self.assertTrue(result)
        _gui_mod._widgets[wid].configure.assert_called_with(state=self._tk_mock.DISABLED)

    def test_widget_set_visible_show(self):
        wid = _gui_mod.widget_label(self._win_id, "lbl")
        result = _gui_mod.widget_set_visible(wid, True)
        self.assertTrue(result)
        _gui_mod._widgets[wid].place_configure.assert_called_once()

    def test_widget_set_visible_hide(self):
        wid = _gui_mod.widget_label(self._win_id, "lbl")
        result = _gui_mod.widget_set_visible(wid, False)
        self.assertTrue(result)
        _gui_mod._widgets[wid].place_forget.assert_called_once()

    def test_widget_configure_calls_through(self):
        wid = _gui_mod.widget_label(self._win_id, "lbl")
        result = _gui_mod.widget_configure(wid, fg="#ff0000", font=("Arial", 14))
        self.assertTrue(result)
        _gui_mod._widgets[wid].configure.assert_called_with(
            fg="#ff0000", font=("Arial", 14))

    def test_widget_tooltip_binds_enter_leave(self):
        wid = _gui_mod.widget_label(self._win_id, "hover me")
        result = _gui_mod.widget_set_tooltip(wid, "This is a tooltip")
        self.assertTrue(result)
        bind_calls = _gui_mod._widgets[wid].bind.call_args_list
        bound_events = [c.args[0] for c in bind_calls]
        self.assertIn("<Enter>", bound_events)
        self.assertIn("<Leave>", bound_events)

    def test_widget_listbox_multi_select(self):
        wid = _gui_mod.widget_listbox(self._win_id, multi_select=True)
        self.assertIsInstance(wid, int)
        self._tk_mock.Listbox.assert_called()

    def test_widget_entry_password_mode(self):
        wid = _gui_mod.widget_entry(self._win_id, password=True)
        self.assertIsInstance(wid, int)
        # Verify Entry was created with show="*"
        call_kwargs = self._tk_mock.Entry.call_args
        entry_opts = call_kwargs[1] if call_kwargs[1] else {}
        # show="*" should have been passed
        self.assertEqual(entry_opts.get("show"), "*")


# ===========================================================================
# 11. Event functions
# ===========================================================================

class TestEventFunctions(unittest.TestCase):
    def setUp(self):
        _reset()
        self._tk_mock, self._root_mock, _ = _make_tk_mock()
        self._patches = [
            patch.object(_gui_mod, "_tk_available", True),
            patch.object(_gui_mod, "_tk", self._tk_mock),
        ]
        for p in self._patches:
            p.start()
        self._win_id = _gui_mod.window_create("T", 400, 300)

    def tearDown(self):
        for p in reversed(self._patches):
            p.stop()
        _reset()

    def test_event_bind_returns_id(self):
        bid = _gui_mod.event_bind(self._win_id, "<Button-1>")
        self.assertIsNotNone(bid)

    def test_event_bind_calls_window_bind(self):
        _gui_mod.event_bind(self._win_id, "<KeyPress>", "on_key")
        _gui_mod._windows[self._win_id].bind.assert_called()

    def test_event_unbind_returns_true(self):
        _gui_mod.event_bind(self._win_id, "<Button-1>")
        result = _gui_mod.event_unbind(self._win_id, "<Button-1>")
        self.assertTrue(result)

    def test_event_poll_returns_list(self):
        events = _gui_mod.event_poll(self._win_id)
        self.assertIsInstance(events, list)

    def test_event_poll_empty_initially(self):
        events = _gui_mod.event_poll(self._win_id)
        self.assertEqual(events, [])

    def test_event_wait_ms_timeout_returns_none(self):
        result = _gui_mod.event_wait_ms(self._win_id, 1)
        self.assertIsNone(result)

    def test_gui_after_returns_id(self):
        after_id = _gui_mod.gui_after(self._win_id, 100, "on_timer")
        self.assertIsNotNone(after_id)

    def test_event_bind_unavailable(self):
        with patch.object(_gui_mod, "_tk_available", False):
            self.assertIsNone(_gui_mod.event_bind(1, "<Button-1>"))

    def test_event_poll_unavailable(self):
        with patch.object(_gui_mod, "_tk_available", False):
            self.assertEqual(_gui_mod.event_poll(1), [])


# ===========================================================================
# 12. Registration
# ===========================================================================

class TestRegistration(unittest.TestCase):
    def test_register_gui_functions_registers_all_core_functions(self):
        runtime = MagicMock()
        _gui_mod.register_gui_functions(runtime)
        registered = {c.args[0] for c in runtime.register_function.call_args_list}

        required = {
            "window_create", "window_show", "window_hide", "window_destroy",
            "window_is_open", "window_set_title", "window_set_size",
            "window_get_size", "window_set_position", "window_get_position",
            "window_set_background", "window_fullscreen", "window_minimize",
            "window_maximize", "window_update", "window_mainloop",
            "window_screenshot", "window_set_resizable", "window_set_alpha",
            "window_get_screen_size", "gui_quit",
            "canvas_create", "canvas_clear", "canvas_update",
            "canvas_draw_line", "canvas_draw_rect", "canvas_draw_oval",
            "canvas_draw_arc", "canvas_draw_polygon", "canvas_draw_text",
            "canvas_delete_item", "canvas_move_item",
            "font_families", "font_create", "font_destroy",
            "font_measure", "font_metrics",
            "widget_label", "widget_button", "widget_entry", "widget_checkbox",
            "widget_listbox", "widget_slider", "widget_text_area",
            "widget_get_text", "widget_set_text",
            "widget_get_value", "widget_set_value",
            "widget_get_checked", "widget_set_checked",
            "widget_destroy", "widget_set_enabled", "widget_set_visible",
            "event_bind", "event_unbind", "event_poll",
            "gui_is_available", "gui_window_count", "gui_canvas_count",
            "gui_widget_count", "gui_font_count",
        }

        missing = required - registered
        self.assertEqual(missing, set(),
                         msg=f"Functions not registered: {sorted(missing)}")

    def test_register_gui_functions_total_count(self):
        runtime = MagicMock()
        _gui_mod.register_gui_functions(runtime)
        count = runtime.register_function.call_count
        self.assertGreaterEqual(count, 60,
                                msg=f"Expected >= 60 registrations, got {count}")


# ===========================================================================
# 13. Introspection helpers
# ===========================================================================

class TestIntrospectionHelpers(unittest.TestCase):
    def setUp(self):
        _reset()

    def tearDown(self):
        _reset()

    def test_gui_window_count_zero_initially(self):
        self.assertEqual(_gui_mod.gui_window_count(), 0)

    def test_gui_canvas_count_zero_initially(self):
        self.assertEqual(_gui_mod.gui_canvas_count(), 0)

    def test_gui_widget_count_zero_initially(self):
        self.assertEqual(_gui_mod.gui_widget_count(), 0)

    def test_gui_font_count_zero_initially(self):
        self.assertEqual(_gui_mod.gui_font_count(), 0)

    def test_counts_reflect_storage(self):
        _gui_mod._windows[1] = MagicMock()
        _gui_mod._windows[2] = MagicMock()
        _gui_mod._canvases[10] = MagicMock()
        self.assertEqual(_gui_mod.gui_window_count(), 2)
        self.assertEqual(_gui_mod.gui_canvas_count(), 1)


# ===========================================================================
# 14. Edge-case & boundary tests
# ===========================================================================

class TestEdgeCases(unittest.TestCase):
    def setUp(self):
        _reset()

    def tearDown(self):
        _reset()

    def test_destroy_nonexistent_window(self):
        with patch.object(_gui_mod, "_tk_available", True):
            self.assertFalse(_gui_mod.window_destroy(99999))

    def test_canvas_operations_on_missing_canvas(self):
        with patch.object(_gui_mod, "_tk_available", True):
            self.assertFalse(_gui_mod.canvas_clear(99999))
            self.assertFalse(_gui_mod.canvas_delete_item(99999, 1))
            self.assertIsNone(_gui_mod.canvas_draw_line(99999, 0, 0, 1, 1))
            self.assertIsNone(_gui_mod.canvas_item_coords(99999, 1))

    def test_widget_operations_on_missing_widget(self):
        with patch.object(_gui_mod, "_tk_available", True):
            self.assertEqual(_gui_mod.widget_get_text(99999), "")
            self.assertFalse(_gui_mod.widget_set_text(99999, "x"))
            self.assertFalse(_gui_mod.widget_destroy(99999))
            self.assertIsNone(_gui_mod.widget_get_value(99999))

    def test_font_operations_on_missing_font(self):
        with patch.object(_gui_mod, "_tk_available", True):
            self.assertEqual(_gui_mod.font_measure(99999, "text"), 0)
            m = _gui_mod.font_metrics(99999)
            self.assertEqual(m["ascent"], 0)

    def test_coerce_color_short_tuple(self):
        # Single-element tuple should not crash
        result = _gui_mod._coerce_color((128,))
        self.assertIsInstance(result, str)

    def test_coerce_color_arbitrary_string(self):
        self.assertEqual(_gui_mod._coerce_color("cornflowerblue"), "cornflowerblue")

    def test_polygon_empty_points_list(self):
        tk_mock, _, _ = _make_tk_mock()
        with patch.object(_gui_mod, "_tk_available", True), \
             patch.object(_gui_mod, "_tk", tk_mock):
            win_id = _gui_mod.window_create("T", 100, 100)
            cid = _gui_mod.canvas_create(win_id, 100, 100)
            # Empty polygon list should not crash
            item = _gui_mod.canvas_draw_polygon(cid, [])
            # create_polygon was called with no coords — result may be a mock value
            self.assertIsNotNone(item)

    def test_window_create_handles_tk_exception(self):
        bad_tk = MagicMock()
        bad_tk.Tk.side_effect = Exception("No display")
        with patch.object(_gui_mod, "_tk_available", True), \
             patch.object(_gui_mod, "_tk", bad_tk):
            result = _gui_mod.window_create("T", 100, 100)
            self.assertIsNone(result)

    def test_canvas_draw_arc_invalid_style_defaults_to_arc(self):
        tk_mock, _, canvas_mock = _make_tk_mock()
        with patch.object(_gui_mod, "_tk_available", True), \
             patch.object(_gui_mod, "_tk", tk_mock):
            win_id = _gui_mod.window_create("T", 100, 100)
            cid = _gui_mod.canvas_create(win_id, 100, 100)
            _gui_mod.canvas_draw_arc(cid, 0, 0, 100, 100, style="invalid_style")
            # style kwarg passed to create_arc should be "arc"
            call_kwargs = _gui_mod._canvases[cid].create_arc.call_args[1]
            self.assertEqual(call_kwargs.get("style"), "arc")


if __name__ == "__main__":
    unittest.main()
