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
 * File: TransportBar.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */


#include "TransportBar.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include "audio/Transport.h"
#include "audio/BasicAudioEngine.h"

TransportBar::TransportBar(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(60);
    
    // Apply professional dark theme styling
    setStyleSheet("TransportBar { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2a2a2a, stop:1 #1a1a1a); "
                  "border-bottom: 2px solid #404040; "
                  "}"
                  "QPushButton { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #404040, stop:1 #2a2a2a); "
                  "border: 1px solid #555555; "
                  "border-radius: 6px; "
                  "color: #ffffff; "
                  "font-weight: bold; "
                  "padding: 8px 16px; "
                  "margin: 4px; "
                  "} "
                  "QPushButton:hover { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #505050, stop:1 #404040); "
                  "border: 1px solid #0078d4; "
                  "} "
                  "QPushButton:pressed { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0078d4, stop:1 #005a9e); "
                  "} "
                  "QLabel { "
                  "color: #ffffff; "
                  "font-family: 'Segoe UI', Arial, sans-serif; "
                  "font-size: 12px; "
                  "}");
}

TransportBar::~TransportBar() {}

void TransportBar::setTransport(Transport* transport, BasicAudioEngine* engine) {
    m_transport = transport;
    m_engine = engine;
    update();
}

void TransportBar::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::darkGray);
    // Play/Stop button
    QRect playRect(10, 10, 30, 30);
    painter.setBrush(m_playing ? Qt::red : Qt::green);
    painter.drawEllipse(playRect);
    painter.drawText(playRect, Qt::AlignCenter, m_playing ? "Stop" : "Play");
    // Loop button
    QRect loopRect(50, 10, 30, 30);
    painter.setBrush((m_loopEnd > m_loopStart) ? Qt::yellow : Qt::gray);
    painter.drawEllipse(loopRect);
    painter.drawText(loopRect, Qt::AlignCenter, "Loop");
    // Seek bar
    m_seekBarRect = QRect(100, 10, 300, 20);
    painter.setBrush(Qt::black);
    painter.drawRect(m_seekBarRect);
    // Draw loop region on seek bar
    if (m_loopEnd > m_loopStart) {
        int x1 = m_seekBarRect.left() + (int)((m_loopStart / 10.0) * m_seekBarRect.width());
        int x2 = m_seekBarRect.left() + (int)((m_loopEnd / 10.0) * m_seekBarRect.width());
        QRect loopRegion(x1, m_seekBarRect.top(), x2-x1, m_seekBarRect.height());
        painter.setBrush(QColor(255,255,0,128));
        painter.drawRect(loopRegion);
    }
    // Draw seek position
    int seekX = m_seekBarRect.left() + (int)((m_seekPos / 10.0) * m_seekBarRect.width());
    painter.setBrush(Qt::red);
    painter.drawRect(seekX-2, m_seekBarRect.top(), 4, m_seekBarRect.height());
    // Tempo/time signature (stub)
    painter.setPen(Qt::white);
    painter.drawText(420, 30, "Tempo: 120");
    painter.drawText(520, 30, "Time Sig: 4/4");
}

void TransportBar::mousePressEvent(QMouseEvent* event) {
    QRect playRect(10, 10, 30, 30);
    QRect loopRect(50, 10, 30, 30);
    if (playRect.contains(event->pos())) {
        m_playing = !m_playing;
        if (m_transport && m_engine) {
            if (m_playing) {
                m_transport->play();
                m_engine->start();
            } else {
                m_transport->stop();
                m_engine->stop();
            }
        }
        update();
        return;
    } else if (loopRect.contains(event->pos())) {
        // Toggle loop region (demo: set to fixed region or clear)
        if (m_loopEnd > m_loopStart) {
            m_loopStart = 0.0;
            m_loopEnd = 0.0;
        } else {
            m_loopStart = 2.0;
            m_loopEnd = 6.0;
        }
        if (m_engine && m_engine->transport().isPlaying()) {
            // Update engine's timeline loop points if playing
            // (Assume timeline is accessible via engine)
            auto timeline = m_engine->transport().isPlaying() ? m_engine->transport().getSamplePosition() : 0;
        }
        update();
        return;
    } else if (m_seekBarRect.contains(event->pos())) {
        // Seek to position
        double frac = (event->pos().x() - m_seekBarRect.left()) / (double)m_seekBarRect.width();
        m_seekPos = frac * 10.0; // 0-10 seconds demo
        if (m_transport) {
            uint64_t sampleFrame = (uint64_t)(m_seekPos * 44100.0);
            m_transport->seek(sampleFrame);
        }
        update();
        return;
    }
}

