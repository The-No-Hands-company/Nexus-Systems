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
 * File: TrackView.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#include "TrackView.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>

TrackView::TrackView(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(300);
    setAcceptDrops(true); // Enable drag and drop for timeline tracks
    
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

void TrackView::setTimeline(std::shared_ptr<dawg::core::Timeline> timeline) {
    m_timeline = timeline;
    update();
}

void TrackView::addClip(std::shared_ptr<dawg::core::Clip> clip) {
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
    for (const auto& clip : m_timeline->clips) {
        int x = static_cast<int>(clip->startTime * 100);
        int w = static_cast<int>(clip->length * 100);
        QRect r(x, y, w, 30);
        if (r.contains(event->pos())) {
            m_selectedClipIdx = idx;
            m_dragging = true;
            m_dragStartPos = event->pos();
            m_originalClipPos = clip->startTime;
            update();
            return;
        }
        y += 40;
        idx++;
    }
    m_selectedClipIdx = -1;
    update();
}

void TrackView::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Delete:
            removeSelectedClip();
            break;
        case Qt::Key_D:
            if (event->modifiers() & Qt::ControlModifier) {
                duplicateSelectedClip();
            }
            break;
    }
    QWidget::keyPressEvent(event);
}

void TrackView::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging && m_selectedClipIdx >= 0 && m_timeline) {
        QPoint delta = event->pos() - m_dragStartPos;
        double timeDelta = delta.x() / 100.0; // Scale factor
        auto& clip = m_timeline->clips[m_selectedClipIdx];
        clip->startTime = m_originalClipPos + timeDelta;
        // Clamp to positive time
        if (clip->startTime < 0) {
            clip->startTime = 0;
        }
        update();
    }
}

void TrackView::mouseReleaseEvent(QMouseEvent* event) {
    m_dragging = false;
}

