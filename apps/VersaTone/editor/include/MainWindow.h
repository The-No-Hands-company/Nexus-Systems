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
 * File: MainWindow.h
 * Purpose: Header file for audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#pragma once
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QSplitter>
#include <QWidget>
#include <memory>

#include "TrackView.h"
#include "MixerView.h"
#include "PianoRoll.h"
#include "TransportBar.h"
#include "FileBrowser.h"
#include "PluginBrowser.h"
#include "PatternSequencerWidget.h"

class BasicAudioEngine;
class QAudioSink;
class QIODevice;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onSampleSelected(const QString& filePath);
    void onLibraryAdded(const QString& folderPath);
    void onSampleDroppedOnStep(const QString& filePath, int step, int track);

private:
    void setupAudioOutput();
    
    TrackView* trackView;
    MixerView* mixerView;
    PianoRoll* pianoRoll;
    TransportBar* transportBar;
    FileBrowser* fileBrowser;
    PluginBrowser* pluginBrowser;
    PatternSequencerWidget* patternSequencer;
    std::unique_ptr<BasicAudioEngine> audioEngine;
    QAudioSink* audioSink;
    QIODevice* audioDevice;
};

