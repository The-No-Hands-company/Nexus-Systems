/*
 * â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•— â–ˆâ–ˆâ•—    â–ˆâ–ˆâ•— â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•— 
 * â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘    â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•”â•â•â•â•â• 
 * â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘ â–ˆâ•— â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ–ˆâ•—
 * â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘â–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘
 * â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â•šâ–ˆâ–ˆâ–ˆâ•”â–ˆâ–ˆâ–ˆâ•”â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•
 * â•šâ•â•â•â•â•â• â•šâ•â•  â•šâ•â• â•šâ•â•â•â•šâ•â•â•  â•šâ•â•â•â•â•â• 
 * 
 * Digital Audio Workstation Engine
 * High-Performance C++ Audio Processing Framework
 * 
 * Copyright (c) 2025 The No-Hands Company
 * Licensed under MIT License
 * 
 * File: PatternSequencerWidget.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#include "PatternSequencerWidget.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QTimer>
#include <QGroupBox>
#include <QFrame>
#include <QScrollArea>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>

namespace daq {
namespace editor {

// Initialize static constants
const QStringList PatternSequencerWidget::DEFAULT_TRACK_NAMES = {
    "Kick", "Snare", "HiHat", "OpenHat", "Crash", "Ride", "Tom1", "Tom2",
    "Perc1", "Perc2", "Bass", "Lead", "Pad", "FX1", "FX2", "Voice"
};

// StepButton Implementation
StepButton::StepButton(int step, int track, QWidget* parent)
    : QPushButton(parent), m_stepIndex(step), m_trackIndex(track)
{
    setFixedSize(32, 32);
    setCheckable(true);
    updateAppearance();
    
    connect(this, &QPushButton::clicked, this, &StepButton::onClicked);
    // Note: QPushButton doesn't have doubleClicked signal, we'll handle it differently
}

void StepButton::setStep(const Step& step) {
    m_step = step;
    setChecked(step.enabled);
    updateAppearance();
}

Step StepButton::getStep() const {
    return m_step;
}

void StepButton::setCurrentStep(bool current) {
    if (m_isCurrentStep != current) {
        m_isCurrentStep = current;
        updateAppearance();
    }
}

void StepButton::setPlaying(bool playing) {
    if (m_isPlaying != playing) {
        m_isPlaying = playing;
        updateAppearance();
    }
}

void StepButton::onClicked() {
    m_step.enabled = !m_step.enabled;
    if (m_step.enabled && m_step.velocity == 0) {
        m_step.velocity = 100; // Default velocity
    }
    updateAppearance();
    emit stepChanged(m_stepIndex, m_trackIndex, m_step);
}

void StepButton::onDoubleClicked() {
    emit stepDoubleClicked(m_stepIndex, m_trackIndex);
}

void StepButton::updateAppearance() {
    QString style = "QPushButton { "
                   "border: 1px solid #3a3a3a; "
                   "border-radius: 3px; "
                   "font-size: 9px; "
                   "font-weight: bold; "
                   "font-family: 'Segoe UI', Arial, sans-serif; ";
    
    if (m_isPlaying && m_isCurrentStep) {
        // Bright orange for currently playing step
        style += "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                "stop:0 #ff8c42, stop:1 #ff6b35); "
                "color: white; "
                "border-color: #ffaa66; "
                "box-shadow: 0px 0px 8px rgba(255, 107, 53, 0.6); ";
    } else if (m_step.enabled) {
        // Velocity-based green gradient for active steps
        int intensity = (m_step.velocity * 180) / 127 + 75; // Scale to 75-255 range
        style += QString("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                         "stop:0 rgb(0, %1, 0), stop:1 rgb(0, %2, 0)); "
                         "color: white; "
                         "border-color: rgb(0, %3, 0); ").arg(intensity).arg(intensity-20).arg(intensity+20);
    } else if (m_isCurrentStep) {
        // Subtle highlight for current step position
        style += "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                "stop:0 #4a4a4a, stop:1 #3a3a3a); "
                "color: #cccccc; "
                "border-color: #666666; ";
    } else {
        // Default dark button
        style += "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                "stop:0 #2e2e2e, stop:1 #252525); "
                "color: #888888; "
                "border-color: #3a3a3a; ";
    }
    
    style += "} "
             "QPushButton:hover { "
             "border-color: #0078d4; "
             "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
             "stop:0 #3e3e3e, stop:1 #2e2e2e); "
             "} "
             "QPushButton:pressed { "
             "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
             "stop:0 #1e1e1e, stop:1 #2e2e2e); "
             "}";
    
    setStyleSheet(style);
    setText(m_step.enabled ? QString::number(m_step.velocity) : "");
}

// PatternTrackHeader Implementation
PatternTrackHeader::PatternTrackHeader(int trackIndex, QWidget* parent)
    : QWidget(parent), m_trackIndex(trackIndex)
{
    setFixedSize(140, 32);
    setAcceptDrops(true); // Enable drag and drop for track headers
    
    // Professional styling for track header
    setStyleSheet("QWidget { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                  "stop:0 #3a3a3a, stop:1 #2a2a2a); "
                  "border: 1px solid #555555; "
                  "border-radius: 3px; "
                  "}");
    
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 3, 4, 3);
    layout->setSpacing(3);
    
    // Track name with professional styling
    QStringList trackNames = {"Kick", "Snare", "HiHat", "OpenHat", "Crash", "Ride", "Tom1", "Tom2",
                             "Perc1", "Perc2", "Bass", "Lead", "Pad", "FX1", "FX2", "Voice"};
    m_nameLabel = new QLabel(trackNames[m_trackIndex % trackNames.size()]);
    m_nameLabel->setFixedWidth(45);
    m_nameLabel->setStyleSheet("QLabel { "
                              "color: #ffffff; "
                              "font-weight: bold; "
                              "font-size: 10px; "
                              "font-family: 'Segoe UI', Arial, sans-serif; "
                              "background: transparent; "
                              "border: none; "
                              "}");
    layout->addWidget(m_nameLabel);
    
    // Professional volume slider
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 127);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setFixedWidth(35);
    m_volumeSlider->setStyleSheet("QSlider::groove:horizontal { "
                                 "border: 1px solid #666; "
                                 "height: 4px; "
                                 "background: #222; "
                                 "border-radius: 2px; "
                                 "} "
                                 "QSlider::handle:horizontal { "
                                 "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                                 "stop:0 #0078d4, stop:1 #005a9e); "
                                 "border: 1px solid #333; "
                                 "width: 8px; "
                                 "height: 8px; "
                                 "border-radius: 4px; "
                                 "margin: -3px 0; "
                                 "} "
                                 "QSlider::handle:horizontal:hover { "
                                 "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                                 "stop:0 #4da6ff, stop:1 #0078d4); "
                                 "}");
    layout->addWidget(m_volumeSlider);
    
    // Compact pan slider
    m_panSlider = new QSlider(Qt::Horizontal);
    m_panSlider->setRange(-64, 64);
    m_panSlider->setValue(0);
    m_panSlider->setFixedWidth(25);
    m_panSlider->setStyleSheet(m_volumeSlider->styleSheet()); // Same style as volume
    layout->addWidget(m_panSlider);
    
    // Professional mute button
    m_muteButton = new QPushButton("M");
    m_muteButton->setFixedSize(18, 18);
    m_muteButton->setCheckable(true);
    m_muteButton->setStyleSheet("QPushButton { "
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
                               "}");
    layout->addWidget(m_muteButton);
    
    // Professional solo button  
    m_soloButton = new QPushButton("S");
    m_soloButton->setFixedSize(18, 18);
    m_soloButton->setCheckable(true);
    m_soloButton->setStyleSheet("QPushButton { "
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
                               "}");
    layout->addWidget(m_soloButton);
    
    // Piano roll button
    m_pianoRollButton = new QPushButton("â™ª");
    m_pianoRollButton->setFixedSize(18, 18);
    m_pianoRollButton->setStyleSheet("QPushButton { "
                                    "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                                    "stop:0 #4a4a4a, stop:1 #3a3a3a); "
                                    "border: 1px solid #555; "
                                    "border-radius: 2px; "
                                    "color: #cccccc; "
                                    "font-weight: bold; "
                                    "font-size: 11px; "
                                    "} "
                                    "QPushButton:hover { "
                                    "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                                    "stop:0 #0078d4, stop:1 #005a9e); "
                                    "color: white; "
                                    "border-color: #4da6ff; "
                                    "}");
    layout->addWidget(m_pianoRollButton);
    
    // Connect signals
    connect(m_muteButton, &QPushButton::toggled, [this](bool muted) {
        emit muteChanged(m_trackIndex, muted);
    });
    connect(m_soloButton, &QPushButton::toggled, [this](bool soloed) {
        emit soloChanged(m_trackIndex, soloed);
    });
    connect(m_volumeSlider, &QSlider::valueChanged, [this](int value) {
        emit volumeChanged(m_trackIndex, value / 127.0f);
    });
    connect(m_panSlider, &QSlider::valueChanged, [this](int value) {
        emit panChanged(m_trackIndex, value / 64.0f);
    });
    connect(m_pianoRollButton, &QPushButton::clicked, [this]() {
        emit pianoRollRequested(m_trackIndex);
    });
}

void PatternTrackHeader::setTrackName(const QString& name) {
    m_nameLabel->setText(name);
}

QString PatternTrackHeader::getTrackName() const {
    return m_nameLabel->text();
}

void PatternTrackHeader::setVolume(float volume) {
    m_volumeSlider->setValue((int)(volume * 127));
}

float PatternTrackHeader::getVolume() const {
    return m_volumeSlider->value() / 127.0f;
}

void PatternTrackHeader::setPan(float pan) {
    m_panSlider->setValue((int)(pan * 64));
}

float PatternTrackHeader::getPan() const {
    return m_panSlider->value() / 64.0f;
}

void PatternTrackHeader::setMuted(bool muted) {
    m_muteButton->setChecked(muted);
}

bool PatternTrackHeader::isMuted() const {
    return m_muteButton->isChecked();
}

void PatternTrackHeader::setSoloed(bool soloed) {
    m_soloButton->setChecked(soloed);
}

bool PatternTrackHeader::isSoloed() const {
    return m_soloButton->isChecked();
}

// ===== TRACK HEADER DRAG AND DROP IMPLEMENTATION =====

void PatternTrackHeader::dragEnterEvent(QDragEnterEvent* event) {
    // Check if the drag contains audio file data
    if (event->mimeData()->hasUrls() || 
        event->mimeData()->hasFormat("application/x-dawg-sample")) {
        event->acceptProposedAction();
        // Change appearance to show it's a valid drop target
        setStyleSheet("QWidget { "
                     "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                     "stop:0 #4a7c4a, stop:1 #3a6a3a); "
                     "border: 2px solid #5cb85c; "
                     "border-radius: 3px; "
                     "}");
    } else {
        event->ignore();
    }
}

void PatternTrackHeader::dragMoveEvent(QDragMoveEvent* event) {
    // Accept drag move events for audio files
    if (event->mimeData()->hasUrls() || 
        event->mimeData()->hasFormat("application/x-dawg-sample")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void PatternTrackHeader::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    QString filePath;
    
    // Extract file path from different MIME data formats
    if (mimeData->hasFormat("application/x-dawg-sample")) {
        filePath = QString::fromUtf8(mimeData->data("application/x-dawg-sample"));
    } else if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        if (!urls.isEmpty()) {
            filePath = urls.first().toLocalFile();
        }
    } else if (mimeData->hasText()) {
        filePath = mimeData->text();
    }
    
    // Reset appearance
    setStyleSheet("QWidget { "
                 "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                 "stop:0 #3a3a3a, stop:1 #2a2a2a); "
                 "border: 1px solid #555555; "
                 "border-radius: 3px; "
                 "}");
    
    if (!filePath.isEmpty()) {
        QFileInfo fileInfo(filePath);
        qDebug() << "ðŸŽ¯ Sample dropped on track" << m_trackIndex << ":" << fileInfo.fileName();
        
        // Update track name to show loaded sample
        QString sampleName = fileInfo.baseName();
        if (sampleName.length() > 8) {
            sampleName = sampleName.left(8) + "...";
        }
        m_nameLabel->setText(sampleName);
        m_nameLabel->setToolTip(QString("Sample: %1").arg(fileInfo.fileName()));
        
        // Emit signal for the pattern sequencer to handle
        emit sampleDroppedOnTrack(filePath, m_trackIndex);
        
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void PatternTrackHeader::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    
    // Add visual indicator if this track has a sample loaded
    // TODO: Draw a small indicator icon when sample is loaded
}

// ===== PATTERN SEQUENCER WIDGET IMPLEMENTATION =====

PatternSequencerWidget::PatternSequencerWidget(QWidget* parent)
    : QWidget(parent), m_sequencer(std::make_shared<PatternSequencer>()), m_trackStates(MAX_TRACKS)
{
    setAcceptDrops(true); // Enable drag and drop for pattern sequencer
    setupUI();
    connectSignals();
    
    // Initialize track names
    QStringList defaultTrackNames = {"Kick", "Snare", "HiHat", "OpenHat", "Crash", "Ride", "Tom1", "Tom2",
                                     "Perc1", "Perc2", "Bass", "Lead", "Pad", "FX1", "FX2", "Voice"};
    for (int i = 0; i < MAX_TRACKS; ++i) {
        if (i < defaultTrackNames.size()) {
            m_trackStates[i].name = defaultTrackNames[i];
        } else {
            m_trackStates[i].name = QString("Track %1").arg(i + 1);
        }
        m_trackStates[i].volume = 1.0f;
        m_trackStates[i].pan = 0.0f;
        m_trackStates[i].muted = false;
        m_trackStates[i].soloed = false;
        m_trackStates[i].samplePath = "";
    }
    
    // Create default pattern
    newPattern();
    
    // Setup playback timer
    m_playbackTimer = new QTimer(this);
    connect(m_playbackTimer, &QTimer::timeout, this, &PatternSequencerWidget::updatePlaybackPosition);
}

PatternSequencerWidget::~PatternSequencerWidget() = default;

void PatternSequencerWidget::setupUI() {
    // Professional dark theme for the main widget
    setStyleSheet("QWidget { "
                  "background-color: #1a1a1a; "
                  "color: #ffffff; "
                  "font-family: 'Segoe UI', Arial, sans-serif; "
                  "}");
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(12, 12, 12, 12);
    m_mainLayout->setSpacing(12);
    
    setupControls();
    setupStepGrid();
}

void PatternSequencerWidget::setupControls() {
    m_controlsLayout = new QHBoxLayout();
    m_controlsLayout->setSpacing(10);
    
    // Professional Pattern Group
    m_patternGroup = new QGroupBox("Pattern");
    m_patternGroup->setStyleSheet("QGroupBox { "
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
                                  "}");
    auto* patternLayout = new QHBoxLayout(m_patternGroup);
    patternLayout->setSpacing(6);
    
    // Removed pattern combo box as we only handle one pattern at a time
    // Instead, we'll just have the buttons
    
    // Professional buttons with enhanced styling
    QString buttonStyle = "QPushButton { "
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
    
    m_newButton = new QPushButton("New");
    m_newButton->setStyleSheet(buttonStyle);
    
    m_duplicateButton = new QPushButton("Dup");
    m_duplicateButton->setStyleSheet(buttonStyle);
    
    m_clearButton = new QPushButton("Clear");
    m_clearButton->setStyleSheet(buttonStyle);
    
    patternLayout->addWidget(m_newButton);
    patternLayout->addWidget(m_duplicateButton);
    patternLayout->addWidget(m_clearButton);
    
    // Professional Transport Controls
    QString playButtonStyle = "QPushButton { "
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
    
    QString stopButtonStyle = "QPushButton { "
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
    
    m_playButton = new QPushButton("â–¶ Play");
    m_playButton->setStyleSheet(playButtonStyle);
    
    m_stopButton = new QPushButton("â¹ Stop");
    m_stopButton->setStyleSheet(stopButtonStyle);
    
    // Professional Tempo and Swing Controls
    auto* tempoLabel = new QLabel("BPM:");
    tempoLabel->setStyleSheet("QLabel { color: #cccccc; font-weight: bold; font-size: 10px; }");
    
    m_tempoSpin = new QSpinBox();
    m_tempoSpin->setRange(60, 200);
    m_tempoSpin->setValue(120);
    m_tempoSpin->setStyleSheet("QSpinBox { "
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
                               "}");
    
    auto* swingLabel = new QLabel("Swing:");
    swingLabel->setStyleSheet("QLabel { color: #cccccc; font-weight: bold; font-size: 10px; }");
    
    m_swingSlider = new QSlider(Qt::Horizontal);
    m_swingSlider->setRange(0, 100);
    m_swingSlider->setValue(0);
    m_swingSlider->setFixedWidth(100);
    m_swingSlider->setStyleSheet("QSlider::groove:horizontal { "
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
                                 "}");
    
    m_swingLabel = new QLabel("0%");
    m_swingLabel->setStyleSheet("QLabel { color: #ffffff; font-weight: bold; font-size: 10px; min-width: 30px; }");
    
    // Add to layout with proper spacing
    m_controlsLayout->addWidget(m_patternGroup);
    m_controlsLayout->addSpacing(15);
    m_controlsLayout->addWidget(m_playButton);
    m_controlsLayout->addWidget(m_stopButton);
    m_controlsLayout->addSpacing(15);
    m_controlsLayout->addWidget(tempoLabel);
    m_controlsLayout->addWidget(m_tempoSpin);
    m_controlsLayout->addSpacing(10);
    m_controlsLayout->addWidget(swingLabel);
    m_controlsLayout->addWidget(m_swingSlider);
    m_controlsLayout->addWidget(m_swingLabel);
    m_controlsLayout->addStretch();
    
    m_mainLayout->addLayout(m_controlsLayout);
}

void PatternSequencerWidget::setupStepGrid() {
    // Professional scroll area for the step grid
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet("QScrollArea { "
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
                               "QScrollBar:horizontal { "
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
                               "}");
    
    // Professional grid container
    m_gridContainer = new QWidget();
    m_gridContainer->setStyleSheet("QWidget { background: #1e1e1e; }");
    m_stepGrid = new QGridLayout(m_gridContainer);
    m_stepGrid->setSpacing(2);
    m_stepGrid->setContentsMargins(8, 8, 8, 8);
    
    setupTrackHeaders();
    
    // Professional step number headers with enhanced styling
    for (int step = 0; step < STEPS_PER_PATTERN; ++step) {
        auto* stepLabel = new QLabel(QString::number(step + 1));
        stepLabel->setAlignment(Qt::AlignCenter);
        stepLabel->setFixedSize(32, 24);
        
        // Highlight beat positions (1, 5, 9, 13) differently
        if (step % 4 == 0) {
            stepLabel->setStyleSheet("QLabel { "
                                    "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                                    "stop:0 #0078d4, stop:1 #005a9e); "
                                    "border: 1px solid #4da6ff; "
                                    "border-radius: 3px; "
                                    "color: white; "
                                    "font-weight: bold; "
                                    "font-size: 10px; "
                                    "}");
        } else {
            stepLabel->setStyleSheet("QLabel { "
                                    "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                                    "stop:0 #444444, stop:1 #333333); "
                                    "border: 1px solid #555555; "
                                    "border-radius: 3px; "
                                    "color: #cccccc; "
                                    "font-weight: bold; "
                                    "font-size: 10px; "
                                    "}");
        }
        m_stepGrid->addWidget(stepLabel, 0, step + 1);
    }
    
    // Create professional step buttons
    m_stepButtons.resize(MAX_TRACKS);
    for (int track = 0; track < MAX_TRACKS; ++track) {
        m_stepButtons[track].resize(STEPS_PER_PATTERN);
        for (int step = 0; step < STEPS_PER_PATTERN; ++step) {
            auto* stepButton = new StepButton(step, track);
            stepButton->setFixedSize(32, 32); // Slightly larger for better feel
            m_stepGrid->addWidget(stepButton, track + 1, step + 1);
            m_stepButtons[track][step] = stepButton;
            
            connect(stepButton, &StepButton::stepChanged,
                    this, &PatternSequencerWidget::onStepChanged);
            connect(stepButton, &StepButton::stepDoubleClicked,
                    this, &PatternSequencerWidget::onStepDoubleClicked);
        }
    }
    
    m_scrollArea->setWidget(m_gridContainer);
    m_mainLayout->addWidget(m_scrollArea);
}

void PatternSequencerWidget::setupTrackHeaders() {
    m_trackHeaders.resize(MAX_TRACKS);
    for (int track = 0; track < MAX_TRACKS; ++track) {
        auto* trackHeader = new PatternTrackHeader(track);
        m_stepGrid->addWidget(trackHeader, track + 1, 0);
        m_trackHeaders[track] = trackHeader;
        
        // Connect signals
        connect(trackHeader, &PatternTrackHeader::muteChanged,
                this, [this](int track, bool muted) {
                    if (track >= 0 && track < MAX_TRACKS) {
                        m_trackStates[track].muted = muted;
                        emit patternChanged();
                    }
                });
        connect(trackHeader, &PatternTrackHeader::soloChanged,
                this, [this](int track, bool soloed) {
                    if (track >= 0 && track < MAX_TRACKS) {
                        m_trackStates[track].soloed = soloed;
                        emit patternChanged();
                    }
                });
        connect(trackHeader, &PatternTrackHeader::volumeChanged,
                this, [this](int track, float volume) {
                    if (track >= 0 && track < MAX_TRACKS) {
                        m_trackStates[track].volume = volume;
                        emit patternChanged();
                    }
                });
        connect(trackHeader, &PatternTrackHeader::pianoRollRequested,
                this, &PatternSequencerWidget::trackEditRequested);
        
        // Connect drag and drop signal
        connect(trackHeader, &PatternTrackHeader::sampleDroppedOnTrack,
                this, [this](const QString& filePath, int track) {
                    if (track >= 0 && track < MAX_TRACKS) {
                        QFileInfo fileInfo(filePath);
                        qDebug() << "ðŸŽµ Sample loaded into track" << track << ":" << fileInfo.fileName();
                        
                        // Update track name to show loaded sample
                        QString sampleName = fileInfo.baseName();
                        if (sampleName.length() > 8) {
                            sampleName = sampleName.left(8) + "...";
                        }
                        m_trackStates[track].name = sampleName;
                        m_trackStates[track].samplePath = filePath;
                        
                        // Activate the first step for immediate feedback
                        auto pattern = m_sequencer->getPattern();
                        if (pattern) {
                            // Check if the step is currently disabled
                            Step step = pattern->getStep(track, 0);
                            if (!step.enabled) {
                                step.enabled = true;
                                step.velocity = 127;
                                pattern->setStep(track, 0, step);
                                
                                // Update UI
                                if (track < (int)m_stepButtons.size() && 
                                    0 < (int)m_stepButtons[track].size()) {
                                    m_stepButtons[track][0]->setChecked(true);
                                }
                            }
                        }
                        
                        // Update the track header to reflect the new name
                        // We'll trigger an update of the track headers
                        updateTrackHeaders();
                        
                        emit patternChanged();
                        
                        // For now, emit the signal to let MainWindow handle it
                        emit sampleDroppedOnStep(filePath, 0, track); // Step 0 for track-level assignment
                    }
                });
    }
}

void PatternSequencerWidget::connectSignals() {
    // Pattern controls
    connect(m_newButton, &QPushButton::clicked, this, &PatternSequencerWidget::newPattern);
    connect(m_duplicateButton, &QPushButton::clicked, this, &PatternSequencerWidget::duplicateCurrentPattern);
    connect(m_clearButton, &QPushButton::clicked, this, &PatternSequencerWidget::clearCurrentPattern);
    
    // Transport controls
    connect(m_playButton, &QPushButton::clicked, [this]() {
        setPlaying(true);
        emit playbackRequested(true);
    });
    connect(m_stopButton, &QPushButton::clicked, [this]() {
        setPlaying(false);
        emit playbackRequested(false);
    });
    
    // Tempo and swing
    connect(m_tempoSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PatternSequencerWidget::onTempoChanged);
    connect(m_swingSlider, &QSlider::valueChanged,
            this, &PatternSequencerWidget::onSwingChanged);
}

void PatternSequencerWidget::setPatternSequencer(std::shared_ptr<PatternSequencer> sequencer) {
    m_sequencer = sequencer;
    updateStepGrid();
}

std::shared_ptr<PatternSequencer> PatternSequencerWidget::getPatternSequencer() const {
    return m_sequencer;
}

void PatternSequencerWidget::setAudioEngine(BasicAudioEngine* engine) {
    m_audioEngine = engine;
}

void PatternSequencerWidget::setPianoRoll(PianoRoll* pianoRoll) {
    m_pianoRoll = pianoRoll;
}

void PatternSequencerWidget::saveCurrentPattern() {
    // Pattern is automatically saved as we modify it
    emit patternChanged();
}

void PatternSequencerWidget::newPattern() {
    if (!m_sequencer) return;
    
    // Create new pattern
    auto pattern = std::make_unique<Pattern>(MAX_TRACKS, STEPS_PER_PATTERN);
    
    // Note: The Pattern class does not have a name field, so we don't set a name here.
    // We rely on m_trackStates for track names.
    
    // Set the pattern in the sequencer
    m_sequencer->setPattern(std::move(pattern));
    
    // Update the UI
    updateStepGrid();
    updateTrackHeaders();
    
    emit patternChanged();
}

void PatternSequencerWidget::duplicateCurrentPattern() {
    if (!m_sequencer) return;
    
    // Get the current pattern
    auto currentPattern = m_sequencer->getPattern();
    if (!currentPattern) return;
    
    // Create new pattern as a copy of the current one
    auto newPattern = std::make_unique<Pattern>(*currentPattern);
    
    // We could append " Copy" to the name if the pattern had a name, but it doesn't.
    // So we just duplicate.
    
    // Set the new pattern in the sequencer
    m_sequencer->setPattern(std::move(newPattern));
    
    // Update the UI
    updateStepGrid();
    updateTrackHeaders();
    
    emit patternChanged();
}

void PatternSequencerWidget::clearCurrentPattern() {
    if (!m_sequencer) return;
    
    // Clear the current pattern
    m_sequencer->clear();
    
    // Update the UI
    updateStepGrid();
    updateTrackHeaders();
    
    emit patternChanged();
}

void PatternSequencerWidget::setPlaying(bool playing) {
    if (m_isPlaying != playing) {
        m_isPlaying = playing;
        
        if (playing) {
            m_playbackTimer->start(100); // Update every 100ms
        } else {
            m_playbackTimer->stop();
            setCurrentStep(-1); // Clear current step highlight
        }
    }
}

void PatternSequencerWidget::setCurrentStep(int step) {
    // Clear previous current step
    if (m_currentStep >= 0 && m_currentStep < STEPS_PER_PATTERN) {
        for (int track = 0; track < MAX_TRACKS; ++track) {
            m_stepButtons[track][m_currentStep]->setCurrentStep(false);
        }
    }
    
    m_currentStep = step;
    
    // Set new current step
    if (m_currentStep >= 0 && m_currentStep < STEPS_PER_PATTERN) {
        for (int track = 0; track < MAX_TRACKS; ++track) {
            m_stepButtons[track][m_currentStep]->setCurrentStep(true);
            m_stepButtons[track][m_currentStep]->setPlaying(m_isPlaying);
        }
    }
}

void PatternSequencerWidget::updateStepGrid() {
    auto pattern = m_sequencer->getPattern();
    if (!pattern) return;
    
    for (int track = 0; track < MAX_TRACKS; ++track) {
        if (track < pattern->getTrackCount()) {
            for (int step = 0; step < STEPS_PER_PATTERN; ++step) {
                if (step < pattern->getStepCount()) {
                    m_stepButtons[track][step]->setStep(pattern->getStep(track, step));
                }
            }
        }
    }
}

void PatternSequencerWidget::updateTrackHeaders() {
    for (int track = 0; track < MAX_TRACKS; ++track) {
        if (track < (int)m_trackHeaders.size()) {
            auto* header = m_trackHeaders[track];
            header->setTrackName(m_trackStates[track].name);
            header->setVolume(m_trackStates[track].volume);
            header->setMuted(m_trackStates[track].muted);
            header->setSoloed(m_trackStates[track].soloed);
        }
    }
}

void PatternSequencerWidget::onStepChanged(int step, int track, const Step& stepData) {
    auto pattern = m_sequencer->getPattern();
    if (!pattern) return;
    
    if (track < 0 || track >= pattern->getTrackCount()) return;
    if (step < 0 || step >= pattern->getStepCount()) return;
    
    pattern->setStep(track, step, stepData);
    
    emit patternChanged();
}

void PatternSequencerWidget::onStepDoubleClicked(int step, int track) {
    // TODO: Open step editor or piano roll for this step
    emit trackEditRequested(track);
}

void PatternSequencerWidget::onTrackHeaderChanged() {
    // Track header changes are handled by individual signal connections
    emit patternChanged();
}



void PatternSequencerWidget::onTempoChanged(int bpm) {
    m_tempoSpin->setValue(bpm);
    emit tempoChanged(bpm);
}

void PatternSequencerWidget::onSwingChanged(int swing) {
    m_swingLabel->setText(QString::number(swing) + "%");
    // TODO: Apply swing to pattern playback
}

void PatternSequencerWidget::updatePlaybackPosition() {
    if (m_isPlaying && m_audioEngine) {
        // TODO: Get actual playback position from audio engine
        // For now, simulate step progression
        static int simulatedStep = 0;
        setCurrentStep(simulatedStep);
        simulatedStep = (simulatedStep + 1) % STEPS_PER_PATTERN;
    }
}

// ===== DRAG AND DROP IMPLEMENTATION =====

void PatternSequencerWidget::dragEnterEvent(QDragEnterEvent* event) {
    // Check if the drag contains audio file data
    if (event->mimeData()->hasUrls() || 
        event->mimeData()->hasFormat("application/x-dawg-sample")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void PatternSequencerWidget::dragMoveEvent(QDragMoveEvent* event) {
    // Accept drag move events for audio files
    if (event->mimeData()->hasUrls() || 
        event->mimeData()->hasFormat("application/x-dawg-sample")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void PatternSequencerWidget::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    QString filePath;
    
    // Extract file path from different MIME data formats
    if (mimeData->hasFormat("application/x-dawg-sample")) {
        filePath = QString::fromUtf8(mimeData->data("application/x-dawg-sample"));
    } else if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        if (!urls.isEmpty()) {
            filePath = urls.first().toLocalFile();
        }
    } else if (mimeData->hasText()) {
        filePath = mimeData->text();
    }
    
    if (!filePath.isEmpty()) {
        // Calculate which step and track the drop occurred on
        QPoint localPos = event->position().toPoint();
        
        // Simple calculation - we'll improve this later
        // Assume 16 steps across and up to 16 tracks down
        int stepWidth = width() / STEPS_PER_PATTERN;
        int trackHeight = height() / MAX_TRACKS;
        
        int step = localPos.x() / stepWidth;
        int track = localPos.y() / trackHeight;
        
        // Clamp to valid ranges
        step = qBound(0, step, STEPS_PER_PATTERN - 1);
        track = qBound(0, track, MAX_TRACKS - 1);
        
        QFileInfo fileInfo(filePath);
        qDebug() << "ðŸŽ¯ Sample dropped on Pattern Sequencer:";
        qDebug() << "   File:" << fileInfo.fileName();
        qDebug() << "   Step:" << step << "Track:" << track;
        qDebug() << "   Position:" << localPos;
        
        // Emit signal for other components to handle
        emit sampleDroppedOnStep(filePath, step, track);
        
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

} // namespace editor
} // namespace daq

