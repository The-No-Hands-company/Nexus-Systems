"""GUI stdlib module for NexusLang.

Provides cross-platform windowing, font rendering, canvas drawing, and a minimal
widget toolkit backed by Python's built-in tkinter.  The module degrades
gracefully when no graphical display is available (CI, headless servers, etc.) by
returning None / -1 sentinel values instead of raising exceptions.

Architecture
------------
All graphical objects (windows, canvases, widgets, fonts) are identified by
integer handles.  This is the same handle-based API style used by the image_utils
and audio modules so NexusLang code can pass lightweight integers across function
boundaries without dealing with Python-object lifetimes.

Features
--------
Windowing System:
  window_create / window_show / window_hide / window_destroy
  window_set_title / window_set_size / window_get_size
  window_get_position / window_set_position
  window_set_background / window_fullscreen
  window_minimize / window_maximize / window_is_open
  window_update / window_mainloop / window_screenshot

Canvas / 2D Drawing:
  canvas_create / canvas_clear / canvas_update
  canvas_draw_line / canvas_draw_rect / canvas_draw_oval
  canvas_draw_polygon / canvas_draw_text / canvas_draw_arc
  canvas_draw_image_from_file
  canvas_delete_item / canvas_move_item / canvas_item_coords
  canvas_tag_raise / canvas_tag_lower
  canvas_set_background / canvas_resize

Font System:
  font_create / font_destroy
  font_families / font_list
  font_measure / font_metrics

Widget Toolkit:
  widget_label / widget_button / widget_entry / widget_checkbox
  widget_listbox / widget_combobox / widget_slider / widget_progressbar
  widget_text_area / widget_frame / widget_separator
  widget_get_text / widget_set_text
  widget_get_value / widget_set_value
  widget_get_checked / widget_set_checked
  widget_get_selected / widget_set_items
  widget_move / widget_resize / widget_destroy
  widget_set_enabled / widget_set_visible / widget_set_tooltip
  widget_configure

Event Handling:
  event_bind / event_unbind
  event_poll / event_wait_ms
  gui_after / gui_quit

Example usage in NexusLang:
    set win to window_create with "My App" and 800 and 600
    window_set_background with win and "#1e1e2e"
    set canvas to canvas_create with win and 800 and 600
    set item to canvas_draw_text with canvas and 400 and 300 and "Hello, NLPL" and "#ffffff" and "Arial" and 24
    window_show with win
    window_mainloop with win
"""

import sys
import threading
import queue
from typing import Any, Dict, List, Optional, Tuple, Callable

from ...runtime.runtime import Runtime

# ---------------------------------------------------------------------------
# Tkinter import (graceful degradation when no display available)
# ---------------------------------------------------------------------------

_tk_available: bool = False
_tk = None
_ttk = None
_font_module = None

def _probe_display() -> bool:
    """Return True only when a real graphical display exists and tkinter loads."""
    import os
    # On Linux/macOS, DISPLAY (X11) or WAYLAND_DISPLAY must be set.
    # On Windows, skip this check entirely.
    if sys.platform != "win32":
        if not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
            return False
    try:
        import tkinter as _tk_local
        # Only create the probe window when we know a display is present.
        root = _tk_local.Tk()
        root.withdraw()
        root.destroy()
        return True
    except Exception:
        return False

try:
    import tkinter as _tk
    import tkinter.ttk as _ttk
    import tkinter.font as _font_module
    _tk_available = _probe_display()
except Exception:
    _tk_available = False
    _tk = None
    _ttk = None
    _font_module = None

# ---------------------------------------------------------------------------
# Internal storage
# ---------------------------------------------------------------------------

_windows: Dict[int, Any] = {}       # id -> Tk / Toplevel instance
_canvases: Dict[int, Any] = {}      # id -> Canvas widget
_fonts: Dict[int, Any] = {}         # id -> font.Font instance
_widgets: Dict[int, Any] = {}       # id -> tk widget instance
_widget_meta: Dict[int, Dict] = {}  # id -> meta dict (type, window_id, etc.)
_event_queues: Dict[int, queue.Queue] = {}  # window_id -> event queue

_counter = 0


def _next_id() -> int:
    global _counter
    _counter += 1
    return _counter


def _unavailable(fn_name: str):
    """Return None with a warning when tkinter isn't usable."""
    return None


# ---------------------------------------------------------------------------
# Color helpers
# ---------------------------------------------------------------------------

def _coerce_color(color) -> str:
    """Accept '#rrggbb', 'red', etc.  Pass through strings; convert list/tuple."""
    if color is None:
        return ""
    if isinstance(color, str):
        return color
    if isinstance(color, (list, tuple)) and len(color) >= 3:
        r, g, b = int(color[0]), int(color[1]), int(color[2])
        return f"#{r:02x}{g:02x}{b:02x}"
    return str(color)


# ===========================================================================
# Windowing System
# ===========================================================================


def window_create(title: str = "NLPL Window", width: int = 800, height: int = 600) -> Optional[int]:
    """Create a new top-level window.  Returns integer handle or None."""
    if not _tk_available:
        return None
    try:
        win_id = _next_id()
        if not _windows:
            # First window: use the implicit Tk root
            root = _tk.Tk()
            root.title(str(title))
            root.geometry(f"{int(width)}x{int(height)}")
            root.withdraw()  # Hidden until window_show is called
            _windows[win_id] = root
        else:
            tl = _tk.Toplevel()
            tl.title(str(title))
            tl.geometry(f"{int(width)}x{int(height)}")
            tl.withdraw()
            _windows[win_id] = tl
        _event_queues[win_id] = queue.Queue()
        return win_id
    except Exception:
        return None


def window_show(window_id: int) -> bool:
    """Make a window visible.  Returns True on success."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].deiconify()
        return True
    except Exception:
        return False


def window_focus(window_id: int) -> bool:
    """Request input focus and raise the window to the front."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        win = _windows[window_id]
        win.deiconify()
        try:
            win.lift()
        except Exception:
            pass
        try:
            win.focus_force()
        except Exception:
            pass
        return True
    except Exception:
        return False


def window_set_topmost(window_id: int, topmost: bool = True) -> bool:
    """Set or clear always-on-top state for a window."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].attributes("-topmost", bool(topmost))
        return True
    except Exception:
        return False


def window_hide(window_id: int) -> bool:
    """Hide a window without destroying it."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].withdraw()
        return True
    except Exception:
        return False


def window_destroy(window_id: int) -> bool:
    """Destroy a window and free its resources."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].destroy()
        del _windows[window_id]
        _event_queues.pop(window_id, None)
        return True
    except Exception:
        return False


def window_is_open(window_id: int) -> bool:
    """Return True if the window handle is still valid."""
    if not _tk_available:
        return False
    return window_id in _windows


def window_set_title(window_id: int, title: str) -> bool:
    """Change the window title bar text."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].title(str(title))
        return True
    except Exception:
        return False


def window_set_size(window_id: int, width: int, height: int) -> bool:
    """Resize the window client area."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].geometry(f"{int(width)}x{int(height)}")
        return True
    except Exception:
        return False


def window_get_size(window_id: int) -> Optional[Dict]:
    """Return {'width': w, 'height': h} or None."""
    if not _tk_available or window_id not in _windows:
        return None
    try:
        win = _windows[window_id]
        win.update_idletasks()
        return {"width": win.winfo_width(), "height": win.winfo_height()}
    except Exception:
        return None


def window_set_position(window_id: int, x: int, y: int) -> bool:
    """Move the window to screen position (x, y)."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        win = _windows[window_id]
        geo = win.geometry()
        # Keep size, change position
        size_part = geo.split("+")[0] if "+" in geo else geo
        win.geometry(f"{size_part}+{int(x)}+{int(y)}")
        return True
    except Exception:
        return False


def window_get_position(window_id: int) -> Optional[Dict]:
    """Return {'x': x, 'y': y} screen position or None."""
    if not _tk_available or window_id not in _windows:
        return None
    try:
        win = _windows[window_id]
        win.update_idletasks()
        return {"x": win.winfo_x(), "y": win.winfo_y()}
    except Exception:
        return None


def window_set_background(window_id: int, color: str) -> bool:
    """Set the window background colour."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].configure(background=_coerce_color(color))
        return True
    except Exception:
        return False


def window_fullscreen(window_id: int, fullscreen: bool = True) -> bool:
    """Enter or leave fullscreen mode."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].attributes("-fullscreen", bool(fullscreen))
        return True
    except Exception:
        return False


def window_minimize(window_id: int) -> bool:
    """Minimize (iconify) the window."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].iconify()
        return True
    except Exception:
        return False


def window_maximize(window_id: int) -> bool:
    """Maximize the window (platform-dependent state)."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        win = _windows[window_id]
        if sys.platform == "win32":
            win.state("zoomed")
        else:
            win.attributes("-zoomed", True)
        return True
    except Exception:
        return False


def window_update(window_id: int) -> bool:
    """Process pending events for one window without blocking."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].update()
        return True
    except Exception:
        return False


def window_mainloop(window_id: int) -> None:
    """Start the blocking event loop for a window."""
    if not _tk_available or window_id not in _windows:
        return
    try:
        _windows[window_id].mainloop()
    except Exception:
        pass


def window_screenshot(window_id: int, file_path: str) -> bool:
    """Save a screenshot of the window to a PNG file.
    Requires the Pillow package (ImageGrab); returns False if unavailable."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        from PIL import ImageGrab  # type: ignore
        win = _windows[window_id]
        win.update_idletasks()
        x = win.winfo_rootx()
        y = win.winfo_rooty()
        w = win.winfo_width()
        h = win.winfo_height()
        img = ImageGrab.grab(bbox=(x, y, x + w, y + h))
        img.save(str(file_path))
        return True
    except Exception:
        return False


def window_set_resizable(window_id: int, resizable_x: bool = True, resizable_y: bool = True) -> bool:
    """Control whether the user can resize the window."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].resizable(bool(resizable_x), bool(resizable_y))
        return True
    except Exception:
        return False


def window_set_min_size(window_id: int, min_width: int, min_height: int) -> bool:
    """Set the minimum window size."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].minsize(int(min_width), int(min_height))
        return True
    except Exception:
        return False


def window_set_max_size(window_id: int, max_width: int, max_height: int) -> bool:
    """Set the maximum window size."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].maxsize(int(max_width), int(max_height))
        return True
    except Exception:
        return False


def window_set_icon(window_id: int, icon_path: str) -> bool:
    """Set window icon from a .ico / .png file."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].iconbitmap(str(icon_path))
        return True
    except Exception:
        return False


def window_set_alpha(window_id: int, alpha: float) -> bool:
    """Set window transparency (0.0 fully transparent, 1.0 opaque)."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].attributes("-alpha", max(0.0, min(1.0, float(alpha))))
        return True
    except Exception:
        return False


def window_get_screen_size() -> Dict:
    """Return {'width': w, 'height': h} of the primary screen."""
    if not _tk_available:
        return {"width": 0, "height": 0}
    try:
        # Create a temporary root if none exists
        if _windows:
            root = next(iter(_windows.values()))
        else:
            root = _tk.Tk()
            root.withdraw()
            sw, sh = root.winfo_screenwidth(), root.winfo_screenheight()
            root.destroy()
            return {"width": sw, "height": sh}
        return {"width": root.winfo_screenwidth(), "height": root.winfo_screenheight()}
    except Exception:
        return {"width": 0, "height": 0}


def gui_quit() -> None:
    """Quit the main tkinter event loop."""
    if not _tk_available:
        return
    try:
        if _windows:
            next(iter(_windows.values())).quit()
    except Exception:
        pass


# ===========================================================================
# Canvas / 2D Drawing
# ===========================================================================


def canvas_create(window_id: int, width: int = 400, height: int = 300,
                  background: str = "#ffffff") -> Optional[int]:
    """Create a canvas widget inside a window.  Returns canvas handle or None."""
    if not _tk_available or window_id not in _windows:
        return None
    try:
        canvas_id = _next_id()
        c = _tk.Canvas(_windows[window_id],
                       width=int(width), height=int(height),
                       bg=_coerce_color(background), highlightthickness=0)
        c.pack(fill=_tk.BOTH, expand=True)
        _canvases[canvas_id] = c
        return canvas_id
    except Exception:
        return None


def canvas_clear(canvas_id: int) -> bool:
    """Delete all items from the canvas."""
    if not _tk_available or canvas_id not in _canvases:
        return False
    try:
        _canvases[canvas_id].delete("all")
        return True
    except Exception:
        return False


def canvas_update(canvas_id: int) -> bool:
    """Force a redraw of the canvas."""
    if not _tk_available or canvas_id not in _canvases:
        return False
    try:
        _canvases[canvas_id].update()
        return True
    except Exception:
        return False


def canvas_set_background(canvas_id: int, color: str) -> bool:
    """Change the canvas background colour."""
    if not _tk_available or canvas_id not in _canvases:
        return False
    try:
        _canvases[canvas_id].configure(bg=_coerce_color(color))
        return True
    except Exception:
        return False


def canvas_resize(canvas_id: int, width: int, height: int) -> bool:
    """Resize the canvas."""
    if not _tk_available or canvas_id not in _canvases:
        return False
    try:
        _canvases[canvas_id].configure(width=int(width), height=int(height))
        return True
    except Exception:
        return False


def canvas_draw_line(canvas_id: int, x1: float, y1: float, x2: float, y2: float,
                     color: str = "#000000", width: float = 1.0,
                     dash: Optional[List] = None, arrow: str = "") -> Optional[int]:
    """Draw a line segment.  Returns item ID or None."""
    if not _tk_available or canvas_id not in _canvases:
        return None
    try:
        opts = {"fill": _coerce_color(color), "width": float(width)}
        if dash:
            opts["dash"] = tuple(int(d) for d in dash)
        if arrow in ("first", "last", "both"):
            opts["arrow"] = arrow
        item = _canvases[canvas_id].create_line(
            float(x1), float(y1), float(x2), float(y2), **opts)
        return item
    except Exception:
        return None


def canvas_draw_rect(canvas_id: int, x1: float, y1: float, x2: float, y2: float,
                     fill: str = "", outline: str = "#000000",
                     width: float = 1.0) -> Optional[int]:
    """Draw a filled/outlined rectangle.  Returns item ID or None."""
    if not _tk_available or canvas_id not in _canvases:
        return None
    try:
        item = _canvases[canvas_id].create_rectangle(
            float(x1), float(y1), float(x2), float(y2),
            fill=_coerce_color(fill) if fill else "",
            outline=_coerce_color(outline),
            width=float(width))
        return item
    except Exception:
        return None


def canvas_draw_oval(canvas_id: int, x1: float, y1: float, x2: float, y2: float,
                     fill: str = "", outline: str = "#000000",
                     width: float = 1.0) -> Optional[int]:
    """Draw an ellipse/circle inscribed in the bounding box.  Returns item ID."""
    if not _tk_available or canvas_id not in _canvases:
        return None
    try:
        item = _canvases[canvas_id].create_oval(
            float(x1), float(y1), float(x2), float(y2),
            fill=_coerce_color(fill) if fill else "",
            outline=_coerce_color(outline),
            width=float(width))
        return item
    except Exception:
        return None


def canvas_draw_arc(canvas_id: int, x1: float, y1: float, x2: float, y2: float,
                    start: float = 0.0, extent: float = 90.0,
                    fill: str = "", outline: str = "#000000",
                    style: str = "arc") -> Optional[int]:
    """Draw an arc, chord, or pie slice.  style: 'arc'|'chord'|'pieslice'."""
    if not _tk_available or canvas_id not in _canvases:
        return None
    try:
        valid_styles = {"arc", "chord", "pieslice"}
        s = style if style in valid_styles else "arc"
        item = _canvases[canvas_id].create_arc(
            float(x1), float(y1), float(x2), float(y2),
            start=float(start), extent=float(extent),
            fill=_coerce_color(fill) if fill else "",
            outline=_coerce_color(outline),
            style=s)
        return item
    except Exception:
        return None


def canvas_draw_polygon(canvas_id: int, points: List,
                        fill: str = "#cccccc", outline: str = "#000000",
                        width: float = 1.0) -> Optional[int]:
    """Draw a filled polygon.  points is [x0,y0, x1,y1, ...] or [(x,y), ...]."""
    if not _tk_available or canvas_id not in _canvases:
        return None
    try:
        # Flatten nested list
        flat: List[float] = []
        for p in points:
            if isinstance(p, (list, tuple)):
                flat.extend(float(v) for v in p)
            else:
                flat.append(float(p))
        item = _canvases[canvas_id].create_polygon(
            *flat,
            fill=_coerce_color(fill) if fill else "",
            outline=_coerce_color(outline),
            width=float(width))
        return item
    except Exception:
        return None


def canvas_draw_text(canvas_id: int, x: float, y: float, text: str,
                     color: str = "#000000", font_family: str = "Arial",
                     font_size: int = 12, bold: bool = False,
                     italic: bool = False, anchor: str = "center") -> Optional[int]:
    """Draw text on the canvas.  Returns item ID or None."""
    if not _tk_available or canvas_id not in _canvases:
        return None
    try:
        style_parts = []
        if bold:
            style_parts.append("bold")
        if italic:
            style_parts.append("italic")
        font_spec = (str(font_family), int(font_size)) + tuple(style_parts)
        item = _canvases[canvas_id].create_text(
            float(x), float(y), text=str(text),
            fill=_coerce_color(color), font=font_spec, anchor=anchor)
        return item
    except Exception:
        return None


def canvas_draw_image_from_file(canvas_id: int, x: float, y: float,
                                 file_path: str, anchor: str = "nw") -> Optional[int]:
    """Draw an image from a file onto the canvas.  Requires PIL or tkinter PhotoImage."""
    if not _tk_available or canvas_id not in _canvases:
        return None
    try:
        # Try PIL first for broad format support
        try:
            from PIL import Image, ImageTk  # type: ignore
            pil_img = Image.open(str(file_path))
            photo = ImageTk.PhotoImage(pil_img)
        except ImportError:
            photo = _tk.PhotoImage(file=str(file_path))
        # Keep a reference to prevent garbage collection
        c = _canvases[canvas_id]
        if not hasattr(c, "_image_refs"):
            c._image_refs = []
        c._image_refs.append(photo)
        item = c.create_image(float(x), float(y), image=photo, anchor=anchor)
        return item
    except Exception:
        return None


def canvas_delete_item(canvas_id: int, item_id: int) -> bool:
    """Remove a specific item from the canvas by its item ID."""
    if not _tk_available or canvas_id not in _canvases:
        return False
    try:
        _canvases[canvas_id].delete(item_id)
        return True
    except Exception:
        return False


def canvas_move_item(canvas_id: int, item_id: int, dx: float, dy: float) -> bool:
    """Move a canvas item by (dx, dy) pixels."""
    if not _tk_available or canvas_id not in _canvases:
        return False
    try:
        _canvases[canvas_id].move(item_id, float(dx), float(dy))
        return True
    except Exception:
        return False


def canvas_item_coords(canvas_id: int, item_id: int) -> Optional[List]:
    """Return the bounding coordinates of a canvas item."""
    if not _tk_available or canvas_id not in _canvases:
        return None
    try:
        coords = _canvases[canvas_id].coords(item_id)
        return list(coords)
    except Exception:
        return None


def canvas_tag_raise(canvas_id: int, item_id: int) -> bool:
    """Raise a canvas item to the top of the display stack."""
    if not _tk_available or canvas_id not in _canvases:
        return False
    try:
        _canvases[canvas_id].tag_raise(item_id)
        return True
    except Exception:
        return False


def canvas_tag_lower(canvas_id: int, item_id: int) -> bool:
    """Lower a canvas item to the bottom of the display stack."""
    if not _tk_available or canvas_id not in _canvases:
        return False
    try:
        _canvases[canvas_id].tag_lower(item_id)
        return True
    except Exception:
        return False


def canvas_configure_item(canvas_id: int, item_id: int, **kwargs) -> bool:
    """Reconfigure an existing canvas item (e.g. change fill, width, text)."""
    if not _tk_available or canvas_id not in _canvases:
        return False
    try:
        _canvases[canvas_id].itemconfigure(item_id, **kwargs)
        return True
    except Exception:
        return False


# ===========================================================================
# Font System
# ===========================================================================


def font_families() -> List[str]:
    """Return a sorted list of all available font family names."""
    if not _tk_available or _font_module is None:
        return []
    try:
        return sorted(_font_module.families())
    except Exception:
        return []


def font_list() -> List[str]:
    """Alias for font_families()."""
    return font_families()


def font_create(family: str = "Arial", size: int = 12,
                bold: bool = False, italic: bool = False,
                underline: bool = False) -> Optional[int]:
    """Create a named font object.  Returns font handle or None."""
    if not _tk_available or _font_module is None:
        return None
    try:
        font_id = _next_id()
        weight = "bold" if bold else "normal"
        slant = "italic" if italic else "roman"
        f = _font_module.Font(family=str(family), size=int(size),
                               weight=weight, slant=slant,
                               underline=int(underline))
        _fonts[font_id] = f
        return font_id
    except Exception:
        return None


def font_destroy(font_id: int) -> bool:
    """Release a font object."""
    if font_id not in _fonts:
        return False
    try:
        del _fonts[font_id]
        return True
    except Exception:
        return False


def font_measure(font_id: int, text: str) -> int:
    """Return the pixel width needed to render text in the given font."""
    if not _tk_available or font_id not in _fonts:
        return 0
    try:
        return _fonts[font_id].measure(str(text))
    except Exception:
        return 0


def font_metrics(font_id: int) -> Dict:
    """Return {'ascent', 'descent', 'linespace', 'fixed'} for the font."""
    if not _tk_available or font_id not in _fonts:
        return {"ascent": 0, "descent": 0, "linespace": 0, "fixed": False}
    try:
        m = _fonts[font_id].metrics()
        return {
            "ascent": m.get("ascent", 0),
            "descent": m.get("descent", 0),
            "linespace": m.get("linespace", 0),
            "fixed": bool(m.get("fixed", 0)),
        }
    except Exception:
        return {"ascent": 0, "descent": 0, "linespace": 0, "fixed": False}


def font_actual(font_id: int) -> Dict:
    """Return a dict of the font's actual (resolved) properties."""
    if not _tk_available or font_id not in _fonts:
        return {}
    try:
        return dict(_fonts[font_id].actual())
    except Exception:
        return {}


def font_configure(font_id: int, family: Optional[str] = None,
                   size: Optional[int] = None,
                   bold: Optional[bool] = None,
                   italic: Optional[bool] = None) -> bool:
    """Modify an existing font object's properties."""
    if not _tk_available or font_id not in _fonts:
        return False
    try:
        opts = {}
        if family is not None:
            opts["family"] = str(family)
        if size is not None:
            opts["size"] = int(size)
        if bold is not None:
            opts["weight"] = "bold" if bold else "normal"
        if italic is not None:
            opts["slant"] = "italic" if italic else "roman"
        if opts:
            _fonts[font_id].configure(**opts)
        return True
    except Exception:
        return False


# ===========================================================================
# Widget Toolkit
# ===========================================================================

def _get_window(window_id: int):
    return _windows.get(window_id)


def widget_label(window_id: int, text: str = "", x: int = 0, y: int = 0,
                 color: str = "#000000", bg: str = "", font_family: str = "Arial",
                 font_size: int = 12) -> Optional[int]:
    """Place a label widget.  Returns widget handle or None."""
    if not _tk_available or window_id not in _windows:
        return None
    try:
        widget_id = _next_id()
        opts = {"text": str(text), "fg": _coerce_color(color),
                "font": (str(font_family), int(font_size))}
        if bg:
            opts["bg"] = _coerce_color(bg)
        lbl = _tk.Label(_windows[window_id], **opts)
        lbl.place(x=int(x), y=int(y))
        _widgets[widget_id] = lbl
        _widget_meta[widget_id] = {"type": "label", "window_id": window_id}
        return widget_id
    except Exception:
        return None


def widget_button(window_id: int, text: str = "OK", x: int = 0, y: int = 0,
                  width: int = 80, height: int = 30,
                  color: str = "#000000", bg: str = "#e0e0e0",
                  font_family: str = "Arial", font_size: int = 11,
                  command_name: str = "") -> Optional[int]:
    """Place a button widget.  Returns widget handle or None.

    When the button is clicked it enqueues a ``button_click`` event in the
    window's event queue (accessible via ``event_poll``).  The event dict
    contains ``type="button_click"``, ``widget_id``, ``text``, and
    ``command_name`` fields so NexusLang programs can distinguish which button
    was pressed without requiring Python-level callables.
    """
    if not _tk_available or window_id not in _windows:
        return None
    try:
        widget_id = _next_id()

        q = _event_queues.setdefault(window_id, queue.Queue())

        def _on_click(_wid=widget_id, _text=text, _cname=command_name, _q=q):
            _q.put({
                "type": "button_click",
                "widget_id": _wid,
                "text": _text,
                "command_name": _cname,
            })

        btn = _tk.Button(
            _windows[window_id],
            text=str(text), fg=_coerce_color(color), bg=_coerce_color(bg),
            font=(str(font_family), int(font_size)),
            width=int(width) // 8,  # tkinter width is in char units for buttons
            command=_on_click,
        )
        btn.place(x=int(x), y=int(y), width=int(width), height=int(height))
        _widgets[widget_id] = btn
        _widget_meta[widget_id] = {"type": "button", "window_id": window_id,
                                   "command_name": command_name}
        return widget_id
    except Exception:
        return None


def widget_entry(window_id: int, x: int = 0, y: int = 0, width: int = 120,
                 initial_text: str = "", font_family: str = "Arial",
                 font_size: int = 11, password: bool = False) -> Optional[int]:
    """Place a single-line text entry widget."""
    if not _tk_available or window_id not in _windows:
        return None
    try:
        widget_id = _next_id()
        var = _tk.StringVar(value=str(initial_text))
        opts = {"textvariable": var,
                "font": (str(font_family), int(font_size)),
                "width": int(width) // 8}
        if password:
            opts["show"] = "*"
        entry = _tk.Entry(_windows[window_id], **opts)
        entry.place(x=int(x), y=int(y), width=int(width))
        _widgets[widget_id] = entry
        _widget_meta[widget_id] = {"type": "entry", "window_id": window_id,
                                   "var": var}
        return widget_id
    except Exception:
        return None


def widget_checkbox(window_id: int, text: str = "", x: int = 0, y: int = 0,
                    checked: bool = False, color: str = "#000000",
                    font_family: str = "Arial", font_size: int = 11) -> Optional[int]:
    """Place a checkbox widget."""
    if not _tk_available or window_id not in _windows:
        return None
    try:
        widget_id = _next_id()
        var = _tk.BooleanVar(value=bool(checked))
        cb = _tk.Checkbutton(_windows[window_id], text=str(text),
                              variable=var, fg=_coerce_color(color),
                              font=(str(font_family), int(font_size)))
        cb.place(x=int(x), y=int(y))
        _widgets[widget_id] = cb
        _widget_meta[widget_id] = {"type": "checkbox", "window_id": window_id,
                                   "var": var}
        return widget_id
    except Exception:
        return None


def widget_listbox(window_id: int, x: int = 0, y: int = 0,
                   width: int = 120, height: int = 80,
                   items: Optional[List] = None,
                   font_family: str = "Arial", font_size: int = 11,
                   multi_select: bool = False) -> Optional[int]:
    """Place a list-box widget."""
    if not _tk_available or window_id not in _windows:
        return None
    try:
        widget_id = _next_id()
        select_mode = _tk.MULTIPLE if multi_select else _tk.SINGLE
        lb = _tk.Listbox(_windows[window_id],
                          selectmode=select_mode,
                          font=(str(font_family), int(font_size)))
        if items:
            for i, item in enumerate(items):
                lb.insert(i, str(item))
        lb.place(x=int(x), y=int(y), width=int(width), height=int(height))
        _widgets[widget_id] = lb
        _widget_meta[widget_id] = {"type": "listbox", "window_id": window_id}
        return widget_id
    except Exception:
        return None


def widget_slider(window_id: int, x: int = 0, y: int = 0,
                  width: int = 150, min_val: float = 0.0, max_val: float = 100.0,
                  initial_val: float = 0.0, orient: str = "horizontal") -> Optional[int]:
    """Place a slider (scale) widget."""
    if not _tk_available or window_id not in _windows:
        return None
    try:
        widget_id = _next_id()
        var = _tk.DoubleVar(value=float(initial_val))
        tk_orient = _tk.HORIZONTAL if orient == "horizontal" else _tk.VERTICAL
        scale = _tk.Scale(_windows[window_id], variable=var,
                           from_=float(min_val), to=float(max_val),
                           orient=tk_orient, length=int(width),
                           resolution=(max_val - min_val) / 100.0)
        scale.place(x=int(x), y=int(y))
        _widgets[widget_id] = scale
        _widget_meta[widget_id] = {"type": "slider", "window_id": window_id,
                                   "var": var}
        return widget_id
    except Exception:
        return None


def widget_progressbar(window_id: int, x: int = 0, y: int = 0,
                       width: int = 150, height: int = 20,
                       max_val: float = 100.0,
                       orient: str = "horizontal") -> Optional[int]:
    """Place a progress bar widget (uses ttk.Progressbar)."""
    if not _tk_available or _ttk is None or window_id not in _windows:
        return None
    try:
        widget_id = _next_id()
        var = _tk.DoubleVar(value=0.0)
        tk_orient = _tk.HORIZONTAL if orient == "horizontal" else _tk.VERTICAL
        pb = _ttk.Progressbar(_windows[window_id], variable=var,
                               maximum=float(max_val), orient=tk_orient,
                               length=int(width))
        pb.place(x=int(x), y=int(y), width=int(width), height=int(height))
        _widgets[widget_id] = pb
        _widget_meta[widget_id] = {"type": "progressbar", "window_id": window_id,
                                   "var": var}
        return widget_id
    except Exception:
        return None


def widget_text_area(window_id: int, x: int = 0, y: int = 0,
                     width: int = 200, height: int = 100,
                     initial_text: str = "",
                     font_family: str = "Courier", font_size: int = 11,
                     wrap: str = "word") -> Optional[int]:
    """Place a multi-line text area widget."""
    if not _tk_available or window_id not in _windows:
        return None
    try:
        widget_id = _next_id()
        valid_wraps = {"none": _tk.NONE, "char": _tk.CHAR, "word": _tk.WORD}
        w = valid_wraps.get(wrap, _tk.WORD)
        ta = _tk.Text(_windows[window_id],
                       font=(str(font_family), int(font_size)), wrap=w)
        if initial_text:
            ta.insert(_tk.END, str(initial_text))
        ta.place(x=int(x), y=int(y), width=int(width), height=int(height))
        _widgets[widget_id] = ta
        _widget_meta[widget_id] = {"type": "text_area", "window_id": window_id}
        return widget_id
    except Exception:
        return None


def widget_frame(window_id: int, x: int = 0, y: int = 0,
                 width: int = 200, height: int = 150,
                 bg: str = "#f0f0f0", relief: str = "flat") -> Optional[int]:
    """Place a frame widget (container for other widgets)."""
    if not _tk_available or window_id not in _windows:
        return None
    try:
        widget_id = _next_id()
        valid_relief = {"flat", "raised", "sunken", "ridge", "groove", "solid"}
        r = relief if relief in valid_relief else "flat"
        fr = _tk.Frame(_windows[window_id], bg=_coerce_color(bg), relief=r)
        fr.place(x=int(x), y=int(y), width=int(width), height=int(height))
        _widgets[widget_id] = fr
        _widget_meta[widget_id] = {"type": "frame", "window_id": window_id}
        return widget_id
    except Exception:
        return None


def widget_separator(window_id: int, x: int = 0, y: int = 0,
                     width: int = 200, orient: str = "horizontal") -> Optional[int]:
    """Place a horizontal or vertical separator line."""
    if not _tk_available or _ttk is None or window_id not in _windows:
        return None
    try:
        widget_id = _next_id()
        tk_orient = _tk.HORIZONTAL if orient == "horizontal" else _tk.VERTICAL
        sep = _ttk.Separator(_windows[window_id], orient=tk_orient)
        if orient == "horizontal":
            sep.place(x=int(x), y=int(y), width=int(width), height=2)
        else:
            sep.place(x=int(x), y=int(y), width=2, height=int(width))
        _widgets[widget_id] = sep
        _widget_meta[widget_id] = {"type": "separator", "window_id": window_id}
        return widget_id
    except Exception:
        return None


# ---------------------------------------------------------------------------
# Widget read/write accessors
# ---------------------------------------------------------------------------


def widget_get_text(widget_id: int) -> str:
    """Get the current text value from a label, entry, or text area widget."""
    if not _tk_available or widget_id not in _widgets:
        return ""
    try:
        w = _widgets[widget_id]
        meta = _widget_meta.get(widget_id, {})
        wtype = meta.get("type", "")
        if wtype == "text_area":
            return w.get("1.0", _tk.END).rstrip("\n")
        if wtype == "label":
            return w.cget("text")
        if wtype == "entry":
            v = meta.get("var")
            return v.get() if v else w.get()
        # Fallback: try .get()
        return str(w.get()) if hasattr(w, "get") else ""
    except Exception:
        return ""


def widget_set_text(widget_id: int, text: str) -> bool:
    """Set the text of a label, entry, or text area widget."""
    if not _tk_available or widget_id not in _widgets:
        return False
    try:
        w = _widgets[widget_id]
        meta = _widget_meta.get(widget_id, {})
        wtype = meta.get("type", "")
        if wtype == "text_area":
            w.delete("1.0", _tk.END)
            w.insert(_tk.END, str(text))
        elif wtype == "label":
            w.configure(text=str(text))
        elif wtype == "entry":
            v = meta.get("var")
            if v:
                v.set(str(text))
            else:
                w.delete(0, _tk.END)
                w.insert(0, str(text))
        else:
            w.configure(text=str(text))
        return True
    except Exception:
        return False


def widget_get_value(widget_id: int) -> Optional[float]:
    """Get the numeric value from a slider or progressbar widget."""
    if not _tk_available or widget_id not in _widgets:
        return None
    try:
        meta = _widget_meta.get(widget_id, {})
        var = meta.get("var")
        if var:
            return float(var.get())
        return float(_widgets[widget_id].get())
    except Exception:
        return None


def widget_set_value(widget_id: int, value: float) -> bool:
    """Set the numeric value of a slider or progressbar widget."""
    if not _tk_available or widget_id not in _widgets:
        return False
    try:
        meta = _widget_meta.get(widget_id, {})
        var = meta.get("var")
        if var:
            var.set(float(value))
        else:
            _widgets[widget_id].set(float(value))
        return True
    except Exception:
        return False


def widget_get_checked(widget_id: int) -> bool:
    """Return the checked state of a checkbox widget."""
    if not _tk_available or widget_id not in _widgets:
        return False
    try:
        meta = _widget_meta.get(widget_id, {})
        var = meta.get("var")
        if var:
            return bool(var.get())
        return False
    except Exception:
        return False


def widget_set_checked(widget_id: int, checked: bool) -> bool:
    """Set the checked state of a checkbox widget."""
    if not _tk_available or widget_id not in _widgets:
        return False
    try:
        meta = _widget_meta.get(widget_id, {})
        var = meta.get("var")
        if var:
            var.set(bool(checked))
            return True
        return False
    except Exception:
        return False


def widget_get_selected(widget_id: int) -> Optional[List]:
    """Return the currently selected items in a listbox."""
    if not _tk_available or widget_id not in _widgets:
        return None
    try:
        lb = _widgets[widget_id]
        indices = lb.curselection()
        return [lb.get(i) for i in indices]
    except Exception:
        return []


def widget_set_items(widget_id: int, items: List) -> bool:
    """Replace all items in a listbox."""
    if not _tk_available or widget_id not in _widgets:
        return False
    try:
        lb = _widgets[widget_id]
        lb.delete(0, _tk.END)
        for item in items:
            lb.insert(_tk.END, str(item))
        return True
    except Exception:
        return False


def widget_move(widget_id: int, x: int, y: int) -> bool:
    """Move a widget to a new absolute position."""
    if not _tk_available or widget_id not in _widgets:
        return False
    try:
        _widgets[widget_id].place(x=int(x), y=int(y))
        return True
    except Exception:
        return False


def widget_resize(widget_id: int, width: int, height: int) -> bool:
    """Resize a widget."""
    if not _tk_available or widget_id not in _widgets:
        return False
    try:
        _widgets[widget_id].place(width=int(width), height=int(height))
        return True
    except Exception:
        return False


def widget_destroy(widget_id: int) -> bool:
    """Destroy a widget and remove it from internal storage."""
    if not _tk_available or widget_id not in _widgets:
        return False
    try:
        _widgets[widget_id].destroy()
        del _widgets[widget_id]
        _widget_meta.pop(widget_id, None)
        return True
    except Exception:
        return False


def widget_set_enabled(widget_id: int, enabled: bool) -> bool:
    """Enable or disable a widget."""
    if not _tk_available or widget_id not in _widgets:
        return False
    try:
        state = _tk.NORMAL if enabled else _tk.DISABLED
        _widgets[widget_id].configure(state=state)
        return True
    except Exception:
        return False


def widget_set_visible(widget_id: int, visible: bool) -> bool:
    """Show or hide a widget without destroying it."""
    if not _tk_available or widget_id not in _widgets:
        return False
    try:
        if visible:
            _widgets[widget_id].place_configure()
        else:
            _widgets[widget_id].place_forget()
        return True
    except Exception:
        return False


def widget_set_tooltip(widget_id: int, text: str) -> bool:
    """Attach a simple tooltip to a widget (shows on hover after brief delay)."""
    if not _tk_available or widget_id not in _widgets:
        return False
    try:
        widget = _widgets[widget_id]
        tooltip_label = [None]

        def _on_enter(event):
            tt = _tk.Toplevel(widget)
            tt.overrideredirect(True)
            tt.geometry(f"+{event.x_root + 12}+{event.y_root + 12}")
            lbl = _tk.Label(tt, text=str(text), background="#ffffe0",
                             relief="solid", borderwidth=1, font=("Arial", 9))
            lbl.pack()
            tooltip_label[0] = tt

        def _on_leave(event):
            if tooltip_label[0]:
                tooltip_label[0].destroy()
                tooltip_label[0] = None

        widget.bind("<Enter>", _on_enter)
        widget.bind("<Leave>", _on_leave)
        return True
    except Exception:
        return False


def widget_configure(widget_id: int, **kwargs) -> bool:
    """Pass arbitrary tkinter configure keyword arguments to a widget."""
    if not _tk_available or widget_id not in _widgets:
        return False
    try:
        _widgets[widget_id].configure(**kwargs)
        return True
    except Exception:
        return False


# ===========================================================================
# Event Handling
# ===========================================================================


def event_bind(window_id: int, event_name: str,
               handler_name: str = "") -> Optional[int]:
    """Bind a tkinter event string to a handler.
    Returns a binding ID (str converted to int hash) or None."""
    if not _tk_available or window_id not in _windows:
        return None
    try:
        q = _event_queues.setdefault(window_id, queue.Queue())

        def _cb(ev, _ename=event_name):
            q.put({
                "type": _ename,
                "x": getattr(ev, "x", 0),
                "y": getattr(ev, "y", 0),
                "key": getattr(ev, "keysym", ""),
                "button": getattr(ev, "num", 0),
                "delta": getattr(ev, "delta", 0),
                "handler": handler_name,
            })

        _windows[window_id].bind(str(event_name), _cb)
        # Return a simple opaque ID
        bind_id = _next_id()
        return bind_id
    except Exception:
        return None


def event_unbind(window_id: int, event_name: str) -> bool:
    """Remove a binding from a window."""
    if not _tk_available or window_id not in _windows:
        return False
    try:
        _windows[window_id].unbind(str(event_name))
        return True
    except Exception:
        return False


def event_poll(window_id: int) -> List[Dict]:
    """Return all pending events for a window without blocking."""
    if not _tk_available:
        return []
    q = _event_queues.get(window_id)
    if q is None:
        return []
    events = []
    while not q.empty():
        try:
            events.append(q.get_nowait())
        except queue.Empty:
            break
    return events


def event_wait_ms(window_id: int, timeout_ms: int = 1000) -> Optional[Dict]:
    """Wait up to timeout_ms for the next event for a window."""
    if not _tk_available:
        return None
    q = _event_queues.get(window_id)
    if q is None:
        return None
    try:
        return q.get(timeout=timeout_ms / 1000.0)
    except queue.Empty:
        return None


def gui_after(window_id: int, delay_ms: int, callback_name: str = "") -> Optional[int]:
    """Schedule a callback after delay_ms milliseconds.  Returns after-id or None."""
    if not _tk_available or window_id not in _windows:
        return None
    try:
        q = _event_queues.setdefault(window_id, queue.Queue())

        def _cb():
            q.put({"type": "after", "callback": callback_name})

        after_id = _windows[window_id].after(int(delay_ms), _cb)
        return after_id
    except Exception:
        return None


# ===========================================================================
# Introspection helpers
# ===========================================================================


def gui_is_available() -> bool:
    """Return True if a graphical display is available."""
    return _tk_available


def gui_window_count() -> int:
    """Return the number of open windows."""
    return len(_windows)


def gui_canvas_count() -> int:
    """Return the number of active canvas objects."""
    return len(_canvases)


def gui_widget_count() -> int:
    """Return the number of active widget objects."""
    return len(_widgets)


def gui_font_count() -> int:
    """Return the number of active font objects."""
    return len(_fonts)


# ===========================================================================
# Registration
# ===========================================================================


def register_gui_functions(runtime: Runtime) -> None:
    """Register all GUI functions with the NexusLang runtime."""

    # --- Windowing ---
    runtime.register_function("window_create", window_create)
    runtime.register_function("window_show", window_show)
    runtime.register_function("window_focus", window_focus)
    runtime.register_function("window_hide", window_hide)
    runtime.register_function("window_destroy", window_destroy)
    runtime.register_function("window_is_open", window_is_open)
    runtime.register_function("window_set_title", window_set_title)
    runtime.register_function("window_set_size", window_set_size)
    runtime.register_function("window_get_size", window_get_size)
    runtime.register_function("window_set_position", window_set_position)
    runtime.register_function("window_get_position", window_get_position)
    runtime.register_function("window_set_background", window_set_background)
    runtime.register_function("window_fullscreen", window_fullscreen)
    runtime.register_function("window_minimize", window_minimize)
    runtime.register_function("window_maximize", window_maximize)
    runtime.register_function("window_update", window_update)
    runtime.register_function("window_mainloop", window_mainloop)
    runtime.register_function("window_screenshot", window_screenshot)
    runtime.register_function("window_set_resizable", window_set_resizable)
    runtime.register_function("window_set_min_size", window_set_min_size)
    runtime.register_function("window_set_max_size", window_set_max_size)
    runtime.register_function("window_set_icon", window_set_icon)
    runtime.register_function("window_set_alpha", window_set_alpha)
    runtime.register_function("window_set_topmost", window_set_topmost)
    runtime.register_function("window_get_screen_size", window_get_screen_size)
    runtime.register_function("gui_quit", gui_quit)

    # --- Canvas / 2D Drawing ---
    runtime.register_function("canvas_create", canvas_create)
    runtime.register_function("canvas_clear", canvas_clear)
    runtime.register_function("canvas_update", canvas_update)
    runtime.register_function("canvas_set_background", canvas_set_background)
    runtime.register_function("canvas_resize", canvas_resize)
    runtime.register_function("canvas_draw_line", canvas_draw_line)
    runtime.register_function("canvas_draw_rect", canvas_draw_rect)
    runtime.register_function("canvas_draw_oval", canvas_draw_oval)
    runtime.register_function("canvas_draw_arc", canvas_draw_arc)
    runtime.register_function("canvas_draw_polygon", canvas_draw_polygon)
    runtime.register_function("canvas_draw_text", canvas_draw_text)
    runtime.register_function("canvas_draw_image_from_file", canvas_draw_image_from_file)
    runtime.register_function("canvas_delete_item", canvas_delete_item)
    runtime.register_function("canvas_move_item", canvas_move_item)
    runtime.register_function("canvas_item_coords", canvas_item_coords)
    runtime.register_function("canvas_tag_raise", canvas_tag_raise)
    runtime.register_function("canvas_tag_lower", canvas_tag_lower)
    runtime.register_function("canvas_configure_item", canvas_configure_item)

    # --- Font System ---
    runtime.register_function("font_families", font_families)
    runtime.register_function("font_list", font_list)
    runtime.register_function("font_create", font_create)
    runtime.register_function("font_destroy", font_destroy)
    runtime.register_function("font_measure", font_measure)
    runtime.register_function("font_metrics", font_metrics)
    runtime.register_function("font_actual", font_actual)
    runtime.register_function("font_configure", font_configure)

    # --- Widget Toolkit ---
    runtime.register_function("widget_label", widget_label)
    runtime.register_function("widget_button", widget_button)
    runtime.register_function("widget_entry", widget_entry)
    runtime.register_function("widget_checkbox", widget_checkbox)
    runtime.register_function("widget_listbox", widget_listbox)
    runtime.register_function("widget_slider", widget_slider)
    runtime.register_function("widget_progressbar", widget_progressbar)
    runtime.register_function("widget_text_area", widget_text_area)
    runtime.register_function("widget_frame", widget_frame)
    runtime.register_function("widget_separator", widget_separator)
    runtime.register_function("widget_get_text", widget_get_text)
    runtime.register_function("widget_set_text", widget_set_text)
    runtime.register_function("widget_get_value", widget_get_value)
    runtime.register_function("widget_set_value", widget_set_value)
    runtime.register_function("widget_get_checked", widget_get_checked)
    runtime.register_function("widget_set_checked", widget_set_checked)
    runtime.register_function("widget_get_selected", widget_get_selected)
    runtime.register_function("widget_set_items", widget_set_items)
    runtime.register_function("widget_move", widget_move)
    runtime.register_function("widget_resize", widget_resize)
    runtime.register_function("widget_destroy", widget_destroy)
    runtime.register_function("widget_set_enabled", widget_set_enabled)
    runtime.register_function("widget_set_visible", widget_set_visible)
    runtime.register_function("widget_set_tooltip", widget_set_tooltip)
    runtime.register_function("widget_configure", widget_configure)

    # --- Events ---
    runtime.register_function("event_bind", event_bind)
    runtime.register_function("event_unbind", event_unbind)
    runtime.register_function("event_poll", event_poll)
    runtime.register_function("event_wait_ms", event_wait_ms)
    runtime.register_function("gui_after", gui_after)

    # --- Introspection ---
    runtime.register_function("gui_is_available", gui_is_available)
    runtime.register_function("gui_window_count", gui_window_count)
    runtime.register_function("gui_canvas_count", gui_canvas_count)
    runtime.register_function("gui_widget_count", gui_widget_count)
    runtime.register_function("gui_font_count", gui_font_count)
