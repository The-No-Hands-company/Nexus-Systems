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
 * File: PianoRoll.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */

#include "PianoRoll.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include "Timeline.h"

PianoRoll::PianoRoll(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(400);
    
    // Apply professional dark theme styling  
    setStyleSheet("PianoRoll { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2a2a2a, stop:1 #1a1a1a); "
                  "border: 1px solid #404040; "
                  "} "
                  "QScrollBar:vertical { "
                  "border: none; "
                  "background: #2a2a2a; "
                  "width: 12px; "
                  "margin: 0px; "
                  "} "
                  "QScrollBar::handle:vertical { "
                  "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #404040, stop:1 #555555); "
                  "min-height: 20px; "
                  "border-radius: 6px; "
                  "} "
                  "QScrollBar::handle:vertical:hover { "
                  "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0078d4, stop:1 #40a6ff); "
                  "} "
                  "QScrollBar:horizontal { "
                  "border: none; "
                  "background: #2a2a2a; "
                  "height: 12px; "
                  "margin: 0px; "
                  "} "
                  "QScrollBar::handle:horizontal { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #404040, stop:1 #555555); "
                  "min-width: 20px; "
                  "border-radius: 6px; "
                  "} "
                  "QScrollBar::handle:horizontal:hover { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0078d4, stop:1 #40a6ff); "
                  "} "
                  "QScrollBar::add-line, QScrollBar::sub-line { "
                  "border: none; "
                  "background: none; "
                  "}");
}

PianoRoll::~PianoRoll() {}

void PianoRoll::setMidiClip(std::shared_ptr<dawg::core::MidiClip> clip) {
    m_clip = clip;
    update();
}

void PianoRoll::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(26, 26, 26)); // Dark background instead of white
    
    // Draw grid with professional dark theme colors
    for (int i = 0; i < 12; ++i) {
        int y = 20 + i * 15;
        painter.setPen(QColor(64, 64, 64)); // Dark gray grid lines instead of light gray
        painter.drawLine(0, y, width(), y);
        painter.setPen(QColor(255, 255, 255)); // White text instead of black
        painter.drawText(5, y+12, QString("%1").arg(72-i)); // MIDI note number
    }
    if (!m_clip) return;
    // Draw notes
    int idx = 0;
    for (const auto& note : m_clip->events) {
        int x = static_cast<int>(note.time * 100);
        int y = 20 + (72-note.data1) * 15; // data1 = note number
        QRect r(x, y, 40, 14);
        painter.setBrush(idx == m_selectedNoteIdx ? QColor(220, 53, 69) : QColor(0, 120, 212)); // Professional red/blue colors
        painter.drawRect(r);
        painter.setPen(QColor(255, 255, 255)); // White text
        painter.drawText(r, Qt::AlignCenter, QString("%1").arg(note.data1));
        idx++;
    }
}

void PianoRoll::mousePressEvent(QMouseEvent* event) {
    if (!m_clip) return;
    int idx = 0;
    for (const auto& note : m_clip->events) {
        int x = static_cast<int>(note.time * 100);
        int y = 20 + (72-note.data1) * 15;
        QRect r(x, y, 40, 14);
        QRect leftEdge(x-5, y, 10, 14);
        QRect rightEdge(x+40-5, y, 10, 14);
        if (leftEdge.contains(event->pos())) {
            m_selectedNoteIdx = idx;
            m_resizing = true;
            m_resizeEdge = -1;
            m_dragStartPos = event->pos();
            update();
            return;
        } else if (rightEdge.contains(event->pos())) {
            m_selectedNoteIdx = idx;
            m_resizing = true;
            m_resizeEdge = 1;
            m_dragStartPos = event->pos();
            update();
            return;
        } else if (r.contains(event->pos())) {
            m_selectedNoteIdx = idx;
            m_dragging = true;
            m_dragStartPos = event->pos();
            update();
            return;
        }
        idx++;
    }
    m_selectedNoteIdx = -1;
    update();
}

void PianoRoll::mouseMoveEvent(QMouseEvent* event) {
    if (!m_clip) return;
    if (m_selectedNoteIdx < 0 || m_selectedNoteIdx >= (int)m_clip->events.size()) return;
    auto& note = m_clip->events[m_selectedNoteIdx];
    if (m_dragging) {
        int dx = event->pos().x() - m_dragStartPos.x();
        note.time += dx / 100.0;
        if (note.time < 0) note.time = 0;
        m_dragStartPos = event->pos();
        update();
    } else if (m_resizing) {
        int dx = event->pos().x() - m_dragStartPos.x();
        // For MIDI, resizing could mean changing note length (data2 = velocity, but could use for length)
        // Here, just move note end (not implemented in backend yet)
        m_dragStartPos = event->pos();
        update();
    }
}

void PianoRoll::mouseReleaseEvent(QMouseEvent* event) {
    m_dragging = false;
    m_resizing = false;
    m_resizeEdge = 0;
}

