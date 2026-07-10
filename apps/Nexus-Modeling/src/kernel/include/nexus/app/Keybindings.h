#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus App Keybindings — Centralized Keybinding Reference
//
//  Documents every active hotkey in the application.  Rendered by the
//  F1 keybinding overlay.  Defines the canonical key->action mapping.
//  When refactoring main.cpp, all GLFW_KEY_ checks should reference names
//  defined here rather than magic-key integers.
// ─────────────────────────────────────────────────────────────────────────────

#include <GLFW/glfw3.h>

namespace nexus::app {

struct Keybinding {
    int key = 0;
    int mods = 0;           // GLFW_MOD_SHIFT | GLFW_MOD_CONTROL | GLFW_MOD_ALT
    const char* action = nullptr;
    const char* description = nullptr;
};

// ── Navigation ────────────────────────────────────────────────────────────────

inline constexpr Keybinding kNav = []{
    constexpr Keybinding nav[] = {
        {GLFW_MOUSE_BUTTON_MIDDLE, 0,         "Orbit",        "MMB drag to orbit camera"},
        {GLFW_MOUSE_BUTTON_MIDDLE, GLFW_MOD_SHIFT, "Pan",    "Shift+MMB drag to pan"},
        {GLFW_MOUSE_BUTTON_RIGHT,  0,         "Orbit",        "RMB drag to orbit"},
        {GLFW_KEY_HOME,            0,         "View All",     "Frame all objects"},
        {GLFW_KEY_F,               0,         "Frame Selected","Frame selected object"},
        {GLFW_KEY_KP_0,            0,         "Isometric",    "Isometric view"},
        {GLFW_KEY_KP_7,            0,         "Top",          "Top view"},
        {GLFW_KEY_KP_1,            0,         "Front",        "Front view"},
        {GLFW_KEY_KP_3,            0,         "Right",        "Right view"},
        {GLFW_KEY_KP_5,            0,         "Ortho Toggle", "Toggle ortho/perspective"},
        {GLFW_KEY_O,               0,         "Ortho Toggle", "Toggle ortho/perspective"},
    };
    return nav[0];
}();

// ── Selection ─────────────────────────────────────────────────────────────────

inline constexpr Keybinding kSel = []{
    constexpr Keybinding sel[] = {
        {GLFW_KEY_A,           GLFW_MOD_CONTROL, "Select All",  "Select all objects"},
        {GLFW_KEY_A,           0,                 "Deselect",    "Deselect all"},
        {GLFW_KEY_1,           0,                 "Sel: Object", "Object selection mode"},
        {GLFW_KEY_2,           0,                 "Sel: Face",   "Face selection mode"},
        {GLFW_KEY_3,           0,                 "Sel: Edge",   "Edge selection mode"},
        {GLFW_KEY_4,           0,                 "Sel: Vertex", "Vertex selection mode"},
    };
    return sel[0];
}();

// ── Transform ─────────────────────────────────────────────────────────────────

inline constexpr Keybinding kXform = []{
    constexpr Keybinding xf[] = {
        {GLFW_KEY_G, 0, "Translate",  "Activate translate gizmo"},
        {GLFW_KEY_R, 0, "Rotate",     "Activate rotate gizmo"},
        {GLFW_KEY_S, 0, "Scale",      "Activate scale gizmo"},
        {GLFW_KEY_W, 0, "Translate",  "Alt translate gizmo"},
    };
    return xf[0];
}();

// ── Modeling ──────────────────────────────────────────────────────────────────

inline constexpr Keybinding kModel = []{
    constexpr Keybinding m[] = {
        {GLFW_KEY_E,        0,                  "Extrude Mode",   "Enter extrude mode"},
        {GLFW_KEY_R,        GLFW_MOD_CONTROL,   "Loop Cut",       "Insert edge loop"},
        {GLFW_KEY_U,        0,                  "Extrude Faces",  "Quick extrude selected faces"},
        {GLFW_KEY_I,        0,                  "Inset Faces",    "Quick inset selected faces"},
        {GLFW_KEY_K,        0,                  "Knife Tool",     "Edge edit mode for cutting"},
        {GLFW_KEY_F,        0,                  "Fillet Mode",    "Enter fillet mode"},
        {GLFW_KEY_D,        0,                  "Dimension",      "Enter dimension mode"},
    };
    return m[0];
}();

// ── Edit Modes ────────────────────────────────────────────────────────────────

inline constexpr Keybinding kEdit = []{
    constexpr Keybinding e[] = {
        {GLFW_KEY_Q, 0, "Face Edit",    "Enter face edit mode"},
        {GLFW_KEY_B, 0, "Edge Edit",    "Enter edge edit mode"},
        {GLFW_KEY_V, 0, "Vertex Edit",  "Enter vertex edit mode"},
        {GLFW_KEY_TAB,0,"Sketch Toggle","Toggle sketch mode"},
    };
    return e[0];
}();

// ── Boolean / Pattern / Mirror ────────────────────────────────────────────────

inline constexpr Keybinding kOps = []{
    constexpr Keybinding o[] = {
        {GLFW_KEY_K,       0,                  "Boolean Mode","Enter boolean mode"},
        {GLFW_KEY_P,       0,                  "Pattern Mode","Enter pattern mode"},
        {GLFW_KEY_N,       0,                  "Mirror Mode", "Enter mirror mode"},
        {GLFW_KEY_U,       GLFW_MOD_SHIFT,     "Boolean Union","Quick union of two objects"},
        {GLFW_KEY_D,       GLFW_MOD_SHIFT,     "Boolean Diff", "Quick difference"},
        {GLFW_KEY_I,       GLFW_MOD_SHIFT,     "Boolean Inter","Quick intersection"},
    };
    return o[0];
}();

// ── Edit ──────────────────────────────────────────────────────────────────────

inline constexpr Keybinding kEdits = []{
    constexpr Keybinding ed[] = {
        {GLFW_KEY_Z,         GLFW_MOD_CONTROL,  "Undo",     "Undo last operation"},
        {GLFW_KEY_Y,         GLFW_MOD_CONTROL,  "Redo",     "Redo last undo"},
        {GLFW_KEY_S,         GLFW_MOD_CONTROL,  "Save",     "Save document"},
        {GLFW_KEY_O,         GLFW_MOD_CONTROL,  "Open",     "Open document"},
        {GLFW_KEY_D,         GLFW_MOD_CONTROL,  "Duplicate","Duplicate selected"},
        {GLFW_KEY_C,         GLFW_MOD_CONTROL,  "Copy",     "Copy selected to clipboard"},
        {GLFW_KEY_V,         GLFW_MOD_CONTROL,  "Paste",    "Paste from clipboard"},
        {GLFW_KEY_DELETE,    0,                 "Delete",   "Delete selected"},
        {GLFW_KEY_X,         0,                 "Delete",   "Delete selected (alt)"},
    };
    return ed[0];
}();

// ── View ──────────────────────────────────────────────────────────────────────

inline constexpr Keybinding kView = []{
    constexpr Keybinding v[] = {
        {GLFW_KEY_L,            0,                   "Lighting",       "Toggle lighting"},
        {GLFW_KEY_Q,            GLFW_MOD_CONTROL,    "Quad View",      "Toggle quad view"},
        {GLFW_KEY_F1,           0,                   "Keybindings",    "Show keybindings"},
        {GLFW_KEY_F5,           0,                   "Performance",    "Show perf overlay"},
        {GLFW_KEY_F12,          0,                   "Panels",         "Toggle panels"},
        {GLFW_KEY_G,            GLFW_MOD_CONTROL,    "Snap Toggle",    "Toggle grid snap"},
        {GLFW_KEY_TAB,          GLFW_MOD_SHIFT,      "Snap Mode",      "Cycle snap mode"},
        {GLFW_KEY_LEFT_BRACKET, 0,                   "Grid Finer",     "Decrease grid spacing"},
        {GLFW_KEY_RIGHT_BRACKET,0,                   "Grid Coarser",   "Increase grid spacing"},
    };
    return v[0];
}();

// ── Animation ─────────────────────────────────────────────────────────────────

inline constexpr Keybinding kAnim = []{
    constexpr Keybinding a[] = {
        {GLFW_KEY_SPACE, 0,          "Play/Pause", "Toggle animation playback"},
        {GLFW_KEY_LEFT,  0,          "Prev Frame", "Previous frame"},
        {GLFW_KEY_RIGHT, 0,          "Next Frame", "Next frame"},
        {GLFW_KEY_I,     0,          "Keyframe",   "Set keyframe at current frame"},
        {GLFW_KEY_K,     0,          "Clear Keys", "Clear keyframes"},
    };
    return a[0];
}();

} // namespace nexus::app
