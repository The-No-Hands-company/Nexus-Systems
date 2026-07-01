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
 * File: MainWindow.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */

#include "MainWindow.h"
#include "TrackView.h"
#include "MixerView.h"
#include "PianoRoll.h"
#include "TransportBar.h"
#include "FileBrowser.h"
#include "PluginBrowser.h"
#include "PatternSequencerWidget.h"
#include "audio/BasicAudioEngine.h"
#include <memory>
#include "Timeline.h"
#include <QWidget>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QStatusBar>
#include <QAudioSink>
#include <QAudioFormat>
#include <QIODevice>
#include <QTimer>
#include <iostream>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("DAWG - Professional Digital Audio Workstation");
    resize(1400, 900);

    // Apply professional dark theme to main window
    setStyleSheet("QMainWindow { "
                  "background-color: #1a1a1a; "
                  "color: #ffffff; "
                  "font-family: 'Segoe UI', Arial, sans-serif; "
                  "} "
                  "QWidget { "
                  "background-color: #1a1a1a; "
                  "color: #ffffff; "
                  "font-family: 'Segoe UI', Arial, sans-serif; "
                  "} "
                  "QTabWidget { "
                  "background-color: #2a2a2a; "
                  "border: 1px solid #404040; "
                  "} "
                  "QTabWidget::pane { "
                  "border: 1px solid #404040; "
                  "background-color: #2a2a2a; "
                  "} "
                  "QTabBar::tab { "
                  "background-color: #404040; "
                  "color: #ffffff; "
                  "padding: 8px 16px; "
                  "margin-right: 2px; "
                  "border-top-left-radius: 4px; "
                  "border-top-right-radius: 4px; "
                  "} "
                  "QTabBar::tab:selected { "
                  "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0078d4, stop:1 #005a9e); "
                  "color: #ffffff; "
                  "} "
                  "QTabBar::tab:hover:!selected { "
                  "background-color: #505050; "
                  "} "
                  "QSplitter::handle { "
                  "background-color: #404040; "
                  "width: 3px; "
                  "} "
                  "QSplitter::handle:hover { "
                  "background-color: #0078d4; "
                  "}");

    // Central widget and layout
    QWidget* central = new QWidget(this);
    central->setStyleSheet("QWidget { background-color: #1a1a1a; }");
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Transport bar at top
    transportBar = new TransportBar(this);
    mainLayout->addWidget(transportBar);

    // Splitter for main views
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);

    // Left: File browser
    fileBrowser = new FileBrowser(this);
    splitter->addWidget(fileBrowser);

    // Center: Tab widget for TrackView/PianoRoll/PatternSequencer
    QTabWidget* centerTabs = new QTabWidget(this);
    centerTabs->setStyleSheet("QTabWidget::pane { "
                             "border: 1px solid #404040; "
                             "background-color: #1a1a1a; "
                             "} "
                             "QTabWidget::tab-bar { "
                             "alignment: center; "
                             "}");
    trackView = new TrackView(this);
    pianoRoll = new PianoRoll(this);
    patternSequencer = new PatternSequencerWidget(this);
    centerTabs->addTab(trackView, "Tracks");
    centerTabs->addTab(pianoRoll, "Piano Roll");
    centerTabs->addTab(patternSequencer, "Pattern Sequencer");
    splitter->addWidget(centerTabs);

    // Right: Mixer and Plugin browser
    QTabWidget* rightTabs = new QTabWidget(this);
    mixerView = new MixerView(this);
    pluginBrowser = new PluginBrowser(this);
    rightTabs->addTab(mixerView, "Mixer");
    rightTabs->addTab(pluginBrowser, "Plugins");
    splitter->addWidget(rightTabs);

    splitter->setStretchFactor(1, 2); // Center wider
    mainLayout->addWidget(splitter);

    setCentralWidget(central);

    // --- Timeline backend integration ---
    auto timeline = std::make_shared<Timeline>();
    // Add some mock clips for demonstration
    auto audioClip = std::make_shared<AudioClip>();
    audioClip->startTime = 0.5;
    audioClip->length = 2.0;
    audioClip->audioFile = "test.wav";
    timeline->addClip(audioClip);
    auto midiClip = std::make_shared<MidiClip>();
    midiClip->startTime = 3.0;
    midiClip->length = 1.5;
    midiClip->midiFile = "test.mid";
    timeline->addClip(midiClip);
    trackView->setTimeline(timeline);

    // --- Audio engine integration ---
    audioEngine = std::make_unique<BasicAudioEngine>();
    
    // Connect FileBrowser signals for sample loading
    connect(fileBrowser, &FileBrowser::sampleSelected, this, &MainWindow::onSampleSelected);
    connect(fileBrowser, &FileBrowser::libraryAdded, this, &MainWindow::onLibraryAdded);
    audioEngine->init(44100, 2, 512);
    audioEngine->setTimeline(timeline);
    transportBar->setTransport(&audioEngine->transport(), audioEngine.get());
    
    // Setup audio output
    setupAudioOutput();
    
    // --- Pattern sequencer integration ---
    patternSequencer->setAudioEngine(audioEngine.get());
    patternSequencer->setPianoRoll(pianoRoll);
    audioEngine->setPatternSequencer(patternSequencer->getPatternSequencer());
    
    // Connect pattern sequencer signals
    connect(patternSequencer, &PatternSequencerWidget::playbackRequested, 
            [this](bool play) {
                if (play) {
                    audioEngine->start();
                } else {
                    audioEngine->stop();
                }
            });
    
    connect(patternSequencer, &PatternSequencerWidget::tempoChanged,
            [this](int bpm) {
                audioEngine->setTempo(bpm);
            });
    
    connect(patternSequencer, &PatternSequencerWidget::trackEditRequested,
            [this, centerTabs](int track) {
                // Switch to piano roll tab and edit the track
                centerTabs->setCurrentWidget(pianoRoll);
                // TODO: Set piano roll to edit specific track data
            });
    
    // Connect drag and drop signals
    connect(patternSequencer, &PatternSequencerWidget::sampleDroppedOnStep,
            this, &MainWindow::onSampleDroppedOnStep);
}

MainWindow::~MainWindow() {}

void MainWindow::onSampleSelected(const QString& filePath) {
    // Simple sample loading - display in status bar
    statusBar()->showMessage(QString("Sample selected: %1").arg(QFileInfo(filePath).fileName()), 3000);
    
    // TODO: Add sample to current track or create new audio clip
    // For now, we'll create a new audio clip in the timeline
    if (trackView) {
        // Create a new audio clip with the selected sample
        // This is a simplified version without the complex SampleLibrary
        auto timeline = std::make_shared<Timeline>();
        auto audioClip = std::make_shared<AudioClip>();
        audioClip->startTime = 0.0;
        audioClip->length = 2.0; // Default length
        audioClip->audioFile = filePath.toStdString();
        timeline->addClip(audioClip);
        trackView->setTimeline(timeline);
    }
}

void MainWindow::onLibraryAdded(const QString& folderPath) {
    // Simple library addition - display in status bar
    statusBar()->showMessage(QString("Library added: %1").arg(QFileInfo(folderPath).fileName()), 3000);
}

void MainWindow::onSampleDroppedOnStep(const QString& filePath, int step, int track) {
    QFileInfo fileInfo(filePath);
    QString message = QString("ðŸŽ¯ Sample dropped! %1 â†’ Step %2, Track %3")
        .arg(fileInfo.fileName())
        .arg(step + 1)  // Show 1-based step numbers to user
        .arg(track + 1); // Show 1-based track numbers to user
    
    statusBar()->showMessage(message, 5000);
    
    // TODO: Actually assign the sample to the pattern sequencer step
    // For now, just log the action
    qDebug() << "Sample assignment:";
    qDebug() << "  File:" << filePath;
    qDebug() << "  Step:" << step << "(1-based:" << step + 1 << ")";
    qDebug() << "  Track:" << track << "(1-based:" << track + 1 << ")";
}

void MainWindow::setupAudioOutput() {
    // Create QAudioFormat for 44.1kHz, 2 channels, float32
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);
    
    // Create audio sink
    audioSink = new QAudioSink(format, this);
    audioDevice = audioSink->start();
    
    // Set audio callback to bridge BasicAudioEngine to QAudioSink
    audioEngine->setCallback([](float* data, size_t frames, size_t channels, uint32_t sampleRate, void* userData) {
        MainWindow* self = static_cast<MainWindow*>(userData);
        if (self->audioDevice && self->audioDevice->isWritable()) {
            size_t bytesToWrite = frames * channels * sizeof(float);
            self->audioDevice->write(reinterpret_cast<const char*>(data), bytesToWrite);
        }
    }, this);
    
    std::cout << "Audio output setup complete - callback connected to QAudioSink" << std::endl;
}

