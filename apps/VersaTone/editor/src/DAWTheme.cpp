/*
 * â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•— â–ˆâ–ˆâ•—    â–ˆâ–ˆâ•— â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•— 
 * â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘    â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•”â•â•â•â•â• 
 * â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘ â–ˆâ•— â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ–ˆâ•—
 * â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘â–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘
 * â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â•šâ–ˆâ–ˆâ–ˆâ•”â–ˆâ–ˆâ–ˆâ•”â•â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•
 * â•šâ•â•â•â•â•â• â•šâ•â•  â•šâ•â• â•šâ•â•â•â•šâ•â•â•  â•šâ•â•â•â•â•â• 
 * 
 * Digital Audio Workstation Engine
 * High-Performance C++ Audio Processing Framework
 * 
 * Copyright (c) 2025 The No-Hands Company
 * Licensed under MIT License
 * 
 * File: DAWTheme.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#include "DAWTheme.h"

namespace daq {
namespace editor:

// Color Constants
const QString DAWTheme::PRIMARY_DARK = "#1a1a1a";
const QString DAWTheme::SECONDARY_DARK = "#2a2a2a";
const QString DAWTheme::ACCENT_BLUE = "#0078d4";
const QString DAWTheme::ACCENT_GREEN = "#4CAF50";
const QString DAWTheme::ACCENT_RED = "#F44336";
const QString DAWTheme::ACCENT_ORANGE = "#ff6b35";
const QString DAWTheme::ACCENT_YELLOW = "#ffeb3b";
const QString DAWTheme::TEXT_PRIMARY = "#ffffff";
const QString DAWTheme::TEXT_SECONDARY = "#cccccc";
const QString DAWTheme::BORDER_COLOR = "#555555";
const QString DAWTheme::HOVER_COLOR = "#0078d4";

// Main Window Style
const QString DAWTheme::MAIN_WINDOW_STYLE = 
    "QMainWindow { "
        "background-color: #1a1a1a; "
        "color: #ffffff; "
        "font-family: 'Segoe UI', Arial, sans-serif; "
    "} "
    "QWidget { "
        "background-color: #1a1a1a; "
        "color: #ffffff; "
        "font-family: 'Segoe UI', Arial, sans-serif; "
    "}";

// Button Styles
const QString DAWTheme::BUTTON_STYLE = 
    "QPushButton { "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #4a4a4a, stop:1 #3a3a3a); "
        "border: 1px solid #555555; "
        "border-radius: 4px; "
        "color: #ffffff; "
        "font-weight: bold; "
        "font-size: 10px; "
        "padding: 6px 12px; "
        "min-width: 50px; "
    "} "
    "QPushButton:hover { "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #0078d4, stop:1 #005a9e); "
        "border-color: #4da6ff; "
    "} "
    "QPushButton:pressed { "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #005a9e, stop:1 #004080); "
    "}";

const QString DAWTheme::PLAY_BUTTON_STYLE = 
    "QPushButton { "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #4CAF50, stop:1 #388E3C); "
        "border: 1px solid #66BB6A; "
        "border-radius: 4px; "
        "color: white; "
        "font-weight: bold; "
        "font-size: 12px; "
        "padding: 8px 16px; "
    "} "
    "QPushButton:hover { "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #66BB6A, stop:1 #4CAF50); "
    "} "
    "QPushButton:pressed { "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #388E3C, stop:1 #2E7D32); "
    "}";

const QString DAWTheme::STOP_BUTTON_STYLE = 
    "QPushButton { "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #F44336, stop:1 #D32F2F); "
        "border: 1px solid #EF5350; "
        "border-radius: 4px; "
        "color: white; "
        "font-weight: bold; "
        "font-size: 12px; "
        "padding: 8px 16px; "
    "} "
    "QPushButton:hover { "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #EF5350, stop:1 #F44336); "
    "} "
    "QPushButton:pressed { "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #D32F2F, stop:1 #B71C1C); "
    "}";

// Tab Widget Style
const QString DAWTheme::TAB_WIDGET_STYLE = 
    "QTabWidget::pane { "
        "border: 2px solid #444444; "
        "border-radius: 6px; "
        "background: #1e1e1e; "
        "top: -1px; "
    "} "
    "QTabWidget::tab-bar { "
        "left: 5px; "
    "} "
    "QTabBar::tab { "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #3a3a3a, stop:1 #2a2a2a); "
        "border: 1px solid #555555; "
        "border-bottom-color: #444444; "
        "border-top-left-radius: 6px; "
        "border-top-right-radius: 6px; "
        "min-width: 8ex; "
        "padding: 8px 16px; "
        "color: #cccccc; "
        "font-weight: bold; "
        "font-size: 11px; "
    "} "
    "QTabBar::tab:selected { "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #0078d4, stop:1 #005a9e); "
        "border-color: #4da6ff; "
        "border-bottom-color: #1e1e1e; "
        "color: white; "
    "} "
    "QTabBar::tab:hover { "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #4a4a4a, stop:1 #3a3a3a); "
        "border-color: #0078d4; "
    "}";

// Group Box Style
const QString DAWTheme::GROUP_BOX_STYLE = 
    "QGroupBox { "
        "font-weight: bold; "
        "font-size: 11px; "
        "color: #ffffff; "
        "border: 2px solid #555555; "
        "border-radius: 6px; "
        "margin-top: 8px; "
        "padding-top: 4px; "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #2a2a2a, stop:1 #1e1e1e); "
    "} "
    "QGroupBox::title { "
        "subcontrol-origin: margin; "
        "left: 10px; "
        "padding: 0 5px 0 5px; "
    "}";

// Slider Style  
const QString DAWTheme::SLIDER_STYLE = 
    "QSlider::groove:horizontal { "
        "border: 1px solid #666; "
        "height: 6px; "
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "stop:0 #222, stop:1 #444); "
        "border-radius: 3px; "
    "} "
    "QSlider::handle:horizontal { "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #0078d4, stop:1 #005a9e); "
        "border: 1px solid #333; "
        "width: 12px; "
        "height: 12px; "
        "border-radius: 6px; "
        "margin: -4px 0; "
    "} "
    "QSlider::handle:horizontal:hover { "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #4da6ff, stop:1 #0078d4); "
    "}";

// SpinBox Style
const QString DAWTheme::SPINBOX_STYLE = 
    "QSpinBox { "
        "border: 1px solid #555555; "
        "border-radius: 3px; "
        "padding: 4px; "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #3a3a3a, stop:1 #2a2a2a); "
        "color: #ffffff; "
        "font-size: 10px; "
        "min-width: 50px; "
    "} "
    "QSpinBox:hover { "
        "border-color: #0078d4; "
    "}";

// ComboBox Style
const QString DAWTheme::COMBOBOX_STYLE = 
    "QComboBox { "
        "border: 1px solid #555555; "
        "border-radius: 3px; "
        "padding: 4px 8px; "
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #3a3a3a, stop:1 #2a2a2a); "
        "color: #ffffff; "
        "font-size: 10px; "
    "} "
    "QComboBox:hover { "
        "border-color: #0078d4; "
    "} "
    "QComboBox::drop-down { "
        "border: none; "
        "width: 20px; "
    "} "
    "QComboBox QAbstractItemView { "
        "background: #2a2a2a; "
        "border: 1px solid #555555; "
        "color: #ffffff; "
        "selection-background-color: #0078d4; "
    "}";

// ScrollArea Style
const QString DAWTheme::SCROLLAREA_STYLE = 
    "QScrollArea { "
        "background: #1a1a1a; "
        "border: 2px solid #444444; "
        "border-radius: 6px; "
    "} "
    "QScrollBar:vertical { "
        "background: #2a2a2a; "
        "width: 12px; "
        "border-radius: 6px; "
    "} "
    "QScrollBar::handle:vertical { "
        "background: #555555; "
        "border-radius: 6px; "
        "min-height: 20px; "
    "} "
    "QScrollBar::handle:vertical:hover { "
        "background: #666666; "
    "} "
    "QScrollBar::horizontal { "
        "background: #2a2a2a; "
        "height: 12px; "
        "border-radius: 6px; "
    "} "
    "QScrollBar::handle:horizontal { "
        "background: #555555; "
        "border-radius: 6px; "
        "min-width: 20px; "
    "} "
    "QScrollBar::handle:horizontal:hover { "
        "background: #666666; "
    "}";

// Splitter Style
const QString DAWTheme::SPLITTER_STYLE = 
    "QSplitter::handle { "
        "background: #444444; "
        "border: 1px solid #555555; "
    "} "
    "QSplitter::handle:horizontal { "
        "width: 3px; "
    "} "
    "QSplitter::handle:vertical { "
        "height: 3px; "
    "} "
    "QSplitter::handle:pressed { "
        "background: #0078d4; "
    "}";

// Helper Functions
QString DAWTheme::getButtonStyle(const QString& baseColor, const QString& hoverColor) {
    return QString("QPushButton { "
                   "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                   "stop:0 %1, stop:1 %2); "
                   "border: 1px solid #555555; "
                   "border-radius: 4px; "
                   "color: #ffffff; "
                   "font-weight: bold; "
                   "font-size: 10px; "
                   "padding: 6px 12px; "
                   "} "
                   "QPushButton:hover { "
                       "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                       "stop:0 %3, stop:1 %4); "
                       "border-color: #4da6ff; "
                   "}").arg(baseColor).arg(baseColor).arg(hoverColor).arg(hoverColor);
}

QString DAWTheme::getGradientBackground(const QString& color1, const QString& color2) {
    return QString("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2);").arg(color1).arg(color2);
}

QString DAWTheme::getMuteButtonStyle() {
    return "QPushButton { "
           "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
           "stop:0 #4a4a4a, stop:1 #3a3a3a); "
           "border: 1px solid #555; "
           "border-radius: 2px; "
           "color: #cccccc; "
           "font-weight: bold; "
           "font-size: 9px; "
           "} "
           "QPushButton:checked { "
               "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
               "stop:0 #ff6b6b, stop:1 #ff5252); "
               "color: white; "
               "border-color: #ff8a80; "
           "} "
           "QPushButton:hover { "
               "border-color: #0078d4; "
           "}";
}

QString DAWTheme::getSoloButtonStyle() {
    return "QPushButton { "
           "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
           "stop:0 #4a4a4a, stop:1 #3a3a3a); "
           "border: 1px solid #555; "
           "border-radius: 2px; "
           "color: #cccccc; "
           "font-weight: bold; "
           "font-size: 9px; "
           "} "
           "QPushButton:checked { "
               "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
               "stop:0 #ffeb3b, stop:1 #ffc107); "
               "color: #333; "
               "border-color: #fff176; "
           "} "
           "QPushButton:hover { "
               "border-color: #0078d4; "
           "}";
}

} // namespace editor
} // namespace daq

QString DAWTheme::getGradientBackground(const QString& color1, const QString& color2) {
    return QString("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2);").arg(color1).arg(color2);
}

QString DAWTheme::getMuteButtonStyle() {
    return "QPushButton { "
           "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
           "stop:0 #4a4a4a, stop:1 #3a3a3a); "
           "border: 1px solid #555; "
           "border-radius: 2px; "
           "color: #cccccc; "
           "font-weight: bold; "
           "font-size: 9px; "
           "} "
           "QPushButton:checked { "
           "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
           "stop:0 #ff6b6b, stop:1 #ff5252); "
           "color: white; "
           "border-color: #ff8a80; "
           "} "
           "QPushButton:hover { "
           "border-color: #0078d4; "
           "}";
}

QString DAWTheme::getSoloButtonStyle() {
    return "QPushButton { "
           "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
           "stop:0 #4a4a4a, stop:1 #3a3a3a); "
           "border: 1px solid #555; "
           "border-radius: 2px; "
           "color: #cccccc; "
           "font-weight: bold; "
           "font-size: 9px; "
           "} "
           "QPushButton:checked { "
           "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
           "stop:0 #ffeb3b, stop:1 #ffc107); "
           "color: #333; "
           "border-color: #fff176; "
           "} "
           "QPushButton:hover { "
           "border-color: #0078d4; "
           "}";
}

