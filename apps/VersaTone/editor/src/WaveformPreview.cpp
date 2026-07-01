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
 * File: WaveformPreview.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#include "WaveformPreview.h"
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <cmath>

namespace daq {
namespace editor {

WaveformPreview::WaveformPreview(QWidget* parent)
    : QWidget(parent)
    , m_playbackPosition(0.0)
    , m_isPlaying(false)
    , m_sampleRate(44100)
    , m_channels(2)
    , m_duration(0.0)
    , m_backgroundColor(QColor(26, 26, 26))        // #1a1a1a
    , m_waveformColor(QColor(0, 120, 212))         // #0078d4
    , m_playbackLineColor(QColor(255, 193, 7))     // #ffc107
    , m_textColor(QColor(204, 204, 204))           // #cccccc
    , m_updateTimer(new QTimer(this))
    , m_isMousePressed(false)
{
    setMinimumHeight(120);
    setMaximumHeight(180);
    
    // Professional dark theme styling
    setStyleSheet(
        "WaveformPreview { "
        "    background-color: #1a1a1a; "
        "    border: 1px solid #404040; "
        "    border-radius: 6px; "
        "}"
    );
    
    // Setup update timer for smooth playback line animation
    m_updateTimer->setInterval(16); // ~60 FPS
    connect(m_updateTimer, &QTimer::timeout, this, &WaveformPreview::updatePlaybackLine);
    
    // Enable mouse tracking
    setMouseTracking(true);
}

WaveformPreview::~WaveformPreview()
{
    // m_updateTimer is automatically deleted because it's a child of this object
}

void WaveformPreview::loadAudioFile(const QString& filePath)
{
    if (filePath.isEmpty()) {
        clearWaveform();
        return;
    }
    
    m_currentFilePath = filePath;
    generateWaveformData(filePath);
    update(); // Trigger repaint
    
    emit waveformLoaded(filePath);
}

void WaveformPreview::clearWaveform()
{
    m_waveformData.clear();
    m_currentFilePath.clear();
    m_playbackPosition = 0.0;
    m_duration = 0.0;
    update();
}

void WaveformPreview::setPlaybackPosition(qreal position)
{
    m_playbackPosition = qMax(0.0, qMin(1.0, position));
    update();
}

void WaveformPreview::generateWaveformData(const QString& filePath)
{
    // ðŸŽµ SIMPLIFIED WAVEFORM GENERATION
    // For now, generate a mock waveform based on file name
    // In a real implementation, you'd parse the audio file
    
    m_waveformData.clear();
    
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.baseName().toLower();
    
    // Generate different waveform patterns based on file type/name
    int samples = 1000; // Number of waveform points
    m_waveformData.reserve(samples);
    
    for (int i = 0; i < samples; ++i) {
        float x = static_cast<float>(i) / samples;
        float amplitude = 0.0f;
        
        // Different patterns for different sample types
        if (fileName.contains("kick") || fileName.contains("drum")) {
            // Sharp attack, quick decay
            amplitude = std::exp(-x * 8.0f) * std::sin(x * 20.0f);
        } else if (fileName.contains("bass")) {
            // Low frequency wave
            amplitude = 0.8f * std::sin(x * 4.0f * M_PI) * std::exp(-x * 2.0f);
        } else if (fileName.contains("synth") || fileName.contains("lead")) {
            // Complex harmonic pattern
            amplitude = 0.6f * (std::sin(x * 12.0f * M_PI) + 0.3f * std::sin(x * 24.0f * M_PI));
        } else if (fileName.contains("ambient") || fileName.contains("pad")) {
            // Smooth, sustained wave
            amplitude = 0.7f * std::sin(x * 2.0f * M_PI) * (1.0f - x * 0.3f);
        } else {
            // Generic waveform
            amplitude = 0.5f * std::sin(x * 8.0f * M_PI) * std::exp(-x * 1.5f);
        }
        
        // Add some randomness for realism
        amplitude += 0.1f * (static_cast<float>(rand()) / RAND_MAX - 0.5f);
        amplitude = qMax(-1.0f, qMin(1.0f, amplitude));
        
        m_waveformData.append(amplitude);
    }
    
    // Estimate duration based on file type
    if (fileName.contains("loop")) {
        m_duration = 8.0; // 8 seconds for loops
    } else if (fileName.contains("shot") || fileName.contains("hit")) {
        m_duration = 1.0; // 1 second for one-shots
    } else {
        m_duration = 4.0; // 4 seconds default
    }
}

void WaveformPreview::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Fill background
    painter.fillRect(rect(), m_backgroundColor);
    
    if (!m_waveformData.isEmpty()) {
        drawWaveform(painter);
        drawPlaybackLine(painter);
    } else {
        // Draw placeholder text
        painter.setPen(m_textColor);
        painter.setFont(QFont("Arial", 10));
        painter.drawText(rect(), Qt::AlignCenter, "ðŸŽµ Select an audio file to see waveform");
    }
    
    drawTimeLabels(painter);
}

void WaveformPreview::drawWaveform(QPainter& painter)
{
    if (m_waveformData.isEmpty()) return;
    
    int w = width() - 20; // Margin
    int h = height() - 40; // Margin for time labels
    int centerY = height() / 2;
    
    painter.setPen(QPen(m_waveformColor, 1.5));
    
    // Draw waveform
    QPolygonF waveformPath;
    for (int i = 0; i < m_waveformData.size(); ++i) {
        float x = 10 + (static_cast<float>(i) / m_waveformData.size()) * w;
        float y = centerY - (m_waveformData[i] * h * 0.4f);
        
        if (i == 0) {
            waveformPath.append(QPointF(x, y));
        } else {
            waveformPath.append(QPointF(x, y));
        }
    }
    
    painter.drawPolyline(waveformPath);
    
    // Draw center line
    painter.setPen(QPen(m_textColor.darker(), 1));
    painter.drawLine(10, centerY, width() - 10, centerY);
}

void WaveformPreview::drawPlaybackLine(QPainter& painter)
{
    if (m_playbackPosition <= 0.0) return;
    
    int w = width() - 20;
    int x = 10 + static_cast<int>(m_playbackPosition * w);
    
    painter.setPen(QPen(m_playbackLineColor, 2));
    painter.drawLine(x, 10, x, height() - 20);
    
    // Draw playback position indicator
    painter.setBrush(m_playbackLineColor);
    painter.drawEllipse(x - 4, 6, 8, 8);
}

void WaveformPreview::drawTimeLabels(QPainter& painter)
{
    painter.setPen(m_textColor);
    painter.setFont(QFont("Arial", 8));
    
    // Draw time at start
    painter.drawText(10, height() - 5, "0:00");
    
    // Draw time at end
    if (m_duration > 0) {
        int minutes = static_cast<int>(m_duration) / 60;
        int seconds = static_cast<int>(m_duration) % 60;
        QString timeText = QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
        painter.drawText(width() - 40, height() - 5, timeText);
    }
    
    // Draw current playback time
    if (m_playbackPosition > 0 && m_duration > 0) {
        qreal currentTime = m_playbackPosition * m_duration;
        int minutes = static_cast<int>(currentTime) / 60;
        int seconds = static_cast<int>(currentTime) % 60;
        QString currentTimeText = QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
        
        int x = 10 + static_cast<int>(m_playbackPosition * (width() - 20));
        painter.drawText(x - 15, 15, currentTimeText);
    }
}

void WaveformPreview::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_waveformData.isEmpty()) {
        m_isMousePressed = true;
        
        // Calculate position from mouse click
        int w = width() - 20;
        int mouseX = event->pos().x() - 10;
        qreal position = qMax(0.0, qMin(1.0, static_cast<qreal>(mouseX) / w));
        
        setPlaybackPosition(position);
        emit positionClicked(position);
    }
    
    QWidget::mousePressEvent(event);
}

void WaveformPreview::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isMousePressed && !m_waveformData.isEmpty()) {
        // Calculate position from mouse drag
        int w = width() - 20;
        int mouseX = event->pos().x() - 10;
        qreal position = qMax(0.0, qMin(1.0, static_cast<qreal>(mouseX) / w));
        
        setPlaybackPosition(position);
        emit positionClicked(position);
    }
    
    QWidget::mouseMoveEvent(event);
}

void WaveformPreview::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update(); // Redraw waveform with new size
}

void WaveformPreview::updatePlaybackLine()
{
    if (m_isPlaying) {
        // This would be connected to actual audio playback
        // For now, just update the display
        update();
    }
}

} // namespace editor
} // namespace daq

