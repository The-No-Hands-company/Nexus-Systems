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
 * File: TrackView_backup.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */

#include "TrackView.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>

TrackView::TrackView(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(300);
    
    // Apply professional dark theme styling
    setStyleSheet("TrackView { "
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
                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
                  "border: none; "
                  "background: none; "
                  "}");
}

TrackView::~TrackView() {}
{
    setMinimumHeight(300);
    
    // Apply professional dark theme styling
    setStyleSheet("TrackView { "
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
                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
                  "border: none; "
                  "background: none; "
                  "}");
}

TrackView::~TrackView() {}


void TrackView::setTimeline(std::shared_ptr<Timeline> timeline) {
    m_timeline = timeline;
    update();
}

void TrackView::addClip(std::shared_ptr<Clip> clip) {
    if (m_timeline) {
        m_timeline->addClip(clip);
        update();
    }
}

void TrackView::removeSelectedClip() {
    if (m_timeline && m_selectedClipIdx >= 0 && m_selectedClipIdx < (int)m_timeline->clips.size()) {
        auto clip = m_timeline->clips[m_selectedClipIdx];
        m_timeline->removeClip(clip);
        m_selectedClipIdx = -1;
        update();
    }
}

void TrackView::duplicateSelectedClip() {
    if (m_timeline && m_selectedClipIdx >= 0 && m_selectedClipIdx < (int)m_timeline->clips.size()) {
        auto orig = m_timeline->clips[m_selectedClipIdx];
        // Shallow copy for demo; deep copy recommended for real use
        std::shared_ptr<Clip> copy = orig; // TODO: implement deep copy
        copy->startTime += 1.0;
        m_timeline->addClip(copy);
        update();
    }
}

void TrackView::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(26, 26, 26)); // Dark background instead of white
    if (!m_timeline) return;

    int y = 20;
    int idx = 0;
    for (const auto& clip : m_timeline->clips) {
        int x = static_cast<int>(clip->startTime * 100); // simple scale
        int w = static_cast<int>(clip->length * 100);
        QRect r(x, y, w, 30);
        if (idx == m_selectedClipIdx) {
            painter.setBrush(QColor(220, 53, 69)); // Professional red for selection
        } else {
            painter.setBrush(clip->type() == "audio" ? QColor(0, 188, 212) : QColor(255, 193, 7)); // Professional cyan/yellow
        }
        painter.drawRect(r);
        painter.setPen(QColor(255, 255, 255)); // White text
        painter.drawText(r, Qt::AlignCenter, QString::fromStdString(clip->type()));
        y += 40;
        idx++;
    }
}

void TrackView::mousePressEvent(QMouseEvent* event) {
    if (!m_timeline) return;
    int y = 20;
    int idx = 0;
    bool multiSelect = event->modifiers() & Qt::ControlModifier;
    for (const auto& clip : m_timeline->clips) {
        int x = static_cast<int>(clip->startTime * 100);
        int w = static_cast<int>(clip->length * 100);
        QRect r(x, y, w, 30);
        QRect leftEdge(x-5, y, 10, 30);
        QRect rightEdge(x+w-5, y, 10, 30);
        if (leftEdge.contains(event->pos())) {
            m_selectedClipIdx = idx;
            m_resizing = true;
            m_resizeEdge = -1;
            m_dragStartPos = event->pos();
            update();
            return;
        } else if (rightEdge.contains(event->pos())) {
            m_selectedClipIdx = idx;
            m_resizing = true;
            m_resizeEdge = 1;
            m_dragStartPos = event->pos();
            update();
            return;
        } else if (r.contains(event->pos())) {
            m_selectedClipIdx = idx;
            m_dragging = true;
            m_dragStartPos = event->pos();
            if (multiSelect) {
                m_multiSelectedClipIdxs.push_back(idx);
            } else {
                m_multiSelectedClipIdxs.clear();
            }
            update();
            return;
        }
        y += 40;
        idx++;
    }
    m_selectedClipIdx = -1;
    m_multiSelectedClipIdxs.clear();
    update();
}
void TrackView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete) {
        removeSelectedClip();
    } else if (event->key() == Qt::Key_D && event->modifiers() & Qt::ControlModifier) {
        duplicateSelectedClip();
    }
}

void TrackView::mouseMoveEvent(QMouseEvent* event) {
    if (!m_timeline) return;
    if (m_selectedClipIdx < 0 || m_selectedClipIdx >= (int)m_timeline->clips.size()) return;
    auto& clip = m_timeline->clips[m_selectedClipIdx];
    if (m_dragging) {
        int dx = event->pos().x() - m_dragStartPos.x();
        clip->startTime += dx / 100.0; // scale back
        if (clip->startTime < 0) clip->startTime = 0;
        m_dragStartPos = event->pos();
        update();
    } else if (m_resizing) {
        int dx = event->pos().x() - m_dragStartPos.x();
        if (m_resizeEdge == -1) {
            // left edge
            clip->startTime += dx / 100.0;
            clip->length -= dx / 100.0;
            if (clip->startTime < 0) clip->startTime = 0;
            if (clip->length < 0.1) clip->length = 0.1;
        } else if (m_resizeEdge == 1) {
            // right edge
            clip->length += dx / 100.0;
            if (clip->length < 0.1) clip->length = 0.1;
        }
        m_dragStartPos = event->pos();
        update();
    }
}

void TrackView::mouseReleaseEvent(QMouseEvent* event) {
    m_dragging = false;
    m_resizing = false;
    m_resizeEdge = 0;
}

