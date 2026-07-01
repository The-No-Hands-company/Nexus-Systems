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
 * File: MixerView.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */

#include "MixerView.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include "Mixer.h"

MixerView::MixerView(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(300);
    
    // Apply professional dark theme styling
    setStyleSheet("MixerView { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2a2a2a, stop:1 #1a1a1a); "
                  "border: 1px solid #404040; "
                  "} "
                  "QSlider::groove:vertical { "
                  "border: 1px solid #555555; "
                  "width: 8px; "
                  "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2a2a2a, stop:1 #404040); "
                  "margin: 2px 0; "
                  "border-radius: 4px; "
                  "} "
                  "QSlider::handle:vertical { "
                  "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0078d4, stop:1 #005a9e); "
                  "border: 1px solid #005a9e; "
                  "height: 18px; "
                  "margin: 0 -2px; "
                  "border-radius: 3px; "
                  "} "
                  "QSlider::handle:vertical:hover { "
                  "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #40a6ff, stop:1 #0078d4); "
                  "} "
                  "QPushButton { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #404040, stop:1 #2a2a2a); "
                  "border: 1px solid #555555; "
                  "border-radius: 4px; "
                  "color: #ffffff; "
                  "font-weight: bold; "
                  "padding: 4px 8px; "
                  "} "
                  "QPushButton:hover { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #505050, stop:1 #404040); "
                  "border: 1px solid #0078d4; "
                  "} "
                  "QPushButton:pressed, QPushButton:checked { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0078d4, stop:1 #005a9e); "
                  "} "
                  "QLabel { "
                  "color: #ffffff; "
                  "font-family: 'Segoe UI', Arial, sans-serif; "
                  "font-size: 11px; "
                  "}");
}

MixerView::~MixerView() {}

void MixerView::setMixer(std::shared_ptr<dawg::dsp::Mixer> mixer) {
    m_mixer = mixer;
    update();
}

void MixerView::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::lightGray);
    if (!m_mixer) return;
    int x = 20;
    int idx = 0;
    for (const auto& track : m_mixer->tracks) {
        QRect trackRect(x, 20, 80, 180);
        painter.setBrush(Qt::white);
        painter.drawRect(trackRect);
        painter.drawText(x+10, 40, QString::fromStdString(track->name));
        // Volume fader
        int faderY = 150 - static_cast<int>(track->volume * 100);
        QRect faderRect(x+30, faderY, 20, 40);
        painter.setBrush(Qt::blue);
        painter.drawRect(faderRect);
        painter.drawText(x+30, faderY-5, "Vol");
        // Pan knob
        int panX = x+30 + static_cast<int>(track->pan * 20);
        QRect panRect(panX, 120, 20, 20);
        painter.setBrush(Qt::green);
        painter.drawEllipse(panRect);
        painter.drawText(x+30, 115, "Pan");
        // Mute/Solo buttons
        QRect muteRect(x+10, 170, 25, 20);
        QRect soloRect(x+45, 170, 25, 20);
        painter.setBrush(track->mute ? Qt::red : Qt::gray);
        painter.drawRect(muteRect);
        painter.drawText(muteRect, Qt::AlignCenter, "M");
        painter.setBrush(track->solo ? Qt::yellow : Qt::gray);
        painter.drawRect(soloRect);
        painter.drawText(soloRect, Qt::AlignCenter, "S");
        x += 100;
        idx++;
    }
}

void MixerView::mousePressEvent(QMouseEvent* event) {
    if (!m_mixer) return;
    int x = 20;
    int idx = 0;
    for (const auto& track : m_mixer->tracks) {
        QRect faderRect(x+30, 150-static_cast<int>(track->volume*100), 20, 40);
        QRect panRect(x+30+static_cast<int>(track->pan*20), 120, 20, 20);
        QRect muteRect(x+10, 170, 25, 20);
        QRect soloRect(x+45, 170, 25, 20);
        if (faderRect.contains(event->pos())) {
            m_draggingFader = idx;
            m_selectedTrackIdx = idx;
            return;
        } else if (panRect.contains(event->pos())) {
            m_draggingPan = idx;
            m_selectedTrackIdx = idx;
            return;
        } else if (muteRect.contains(event->pos())) {
            track->mute = !track->mute;
            update();
            return;
        } else if (soloRect.contains(event->pos())) {
            track->solo = !track->solo;
            update();
            return;
        }
        x += 100;
        idx++;
    }
}

void MixerView::mouseMoveEvent(QMouseEvent* event) {
    if (!m_mixer) return;
    if (m_draggingFader >= 0 && m_draggingFader < (int)m_mixer->tracks.size()) {
        auto& track = m_mixer->tracks[m_draggingFader];
        int y = event->pos().y();
        float vol = (150-y)/100.0f;
        if (vol < 0.0f) vol = 0.0f;
        if (vol > 1.0f) vol = 1.0f;
        track->volume = vol;
        update();
    } else if (m_draggingPan >= 0 && m_draggingPan < (int)m_mixer->tracks.size()) {
        auto& track = m_mixer->tracks[m_draggingPan];
        int x = event->pos().x();
        float pan = (x-50)/20.0f;
        if (pan < -1.0f) pan = -1.0f;
        if (pan > 1.0f) pan = 1.0f;
        track->pan = pan;
        update();
    }
}

void MixerView::mouseReleaseEvent(QMouseEvent* event) {
    m_draggingFader = -1;
    m_draggingPan = -1;
}

