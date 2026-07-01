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
 * File: PatternSequencerWidget.h
 * Purpose: Header file for audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#pragma once
#include <QWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QScrollArea>
#include <QFrame>
#include <QGroupBox>
#include <QTimer>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <memory>
#include "PatternSequencer.h"

namespace daq {
namespace editor {

class BasicAudioEngine;
class PianoRoll;

/**
 * @brief Step button for pattern programming - mimics FL Studio step sequencer
 */
class StepButton : public QPushButton {
    Q_OBJECT
public:
    explicit StepButton(int step, int track, QWidget* parent = nullptr);
    
    void setStep(const Step& step);
    Step getStep() const;
    void setCurrentStep(bool current);
    void setPlaying(bool playing);

signals:
    void stepChanged(int step, int track, const Step& stepData);
    void stepDoubleClicked(int step, int track);

private slots:
    void onClicked();
    void onDoubleClicked();

private:
    void updateAppearance();
    
    int m_stepIndex;
    int m_trackIndex;
    Step m_step;
    bool m_isCurrentStep = false;
    bool m_isPlaying = false;
};

/**
 * @brief Track header with name, volume, pan, mute, solo controls
 */
class PatternTrackHeader : public QWidget {
    Q_OBJECT
public:
    explicit PatternTrackHeader(int trackIndex, QWidget* parent = nullptr);
    
    void setTrackName(const QString& name);
    QString getTrackName() const;
    void setVolume(float volume);
    float getVolume() const;
    void setPan(float pan);
    float getPan() const;
    void setMuted(bool muted);
    bool isMuted() const;
    void setSoloed(bool soloed);
    bool isSoloed() const;

signals:
    void trackNameChanged(int track, const QString& name);
    void volumeChanged(int track, float volume);
    void panChanged(int track, float pan);
    void muteChanged(int track, bool muted);
    void soloChanged(int track, bool soloed);
    void pianoRollRequested(int track);
    void sampleDroppedOnTrack(const QString& filePath, int track);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    int m_trackIndex;
    QLabel* m_nameLabel;
    QSlider* m_volumeSlider;
    QSlider* m_panSlider;
    QPushButton* m_muteButton;
    QPushButton* m_soloButton;
    QPushButton* m_pianoRollButton;
};

/**
 * @brief Main pattern sequencer widget - FL Studio style step sequencer
 */
class PatternSequencerWidget : public QWidget {
    Q_OBJECT
public:
    explicit PatternSequencerWidget(QWidget* parent = nullptr);
    ~PatternSequencerWidget();
    
    // Pattern management
    void setPatternSequencer(std::shared_ptr<PatternSequencer> sequencer);
    std::shared_ptr<PatternSequencer> getPatternSequencer() const;
    
    // Integration points
    void setAudioEngine(BasicAudioEngine* engine);
    void setPianoRoll(PianoRoll* pianoRoll);
    
    // Pattern operations
    void saveCurrentPattern();
    void newPattern();
    void duplicateCurrentPattern();
    void clearCurrentPattern();
    
    // Playback control
    void setPlaying(bool playing);
    void setCurrentStep(int step);

public slots:
    void onStepChanged(int step, int track, const Step& stepData);
    void onStepDoubleClicked(int step, int track);
    void onTrackHeaderChanged();
    void onTempoChanged(int bpm);
    void onSwingChanged(int swing);
    void updatePlaybackPosition();

signals:
    void patternChanged();
    void playbackRequested(bool play);
    void tempoChanged(int bpm);
    void trackEditRequested(int track);
    void sampleDroppedOnStep(const QString& filePath, int step, int track);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void setupUI();
    void setupStepGrid();
    void setupControls();
    void setupTrackHeaders();
    void updateStepGrid();
    void updateTrackHeaders();
    void connectSignals();
    
    // Core components
    std::shared_ptr<PatternSequencer> m_sequencer;
    BasicAudioEngine* m_audioEngine = nullptr;
    PianoRoll* m_pianoRoll = nullptr;
    
    // Current state
    int m_currentStep = -1;
    bool m_isPlaying = false;
    
    // Track state storage
    struct TrackState {
        QString name;
        bool muted = false;
        bool soloed = false;
        float volume = 1.0f; // 0.0 to 1.0
        float pan = 0.0f; // -1.0 to 1.0
        QString samplePath;
    };
    std::vector<TrackState> m_trackStates;
    
    // UI Layout
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_controlsLayout;
    QScrollArea* m_scrollArea;
    QWidget* m_gridContainer;
    QGridLayout* m_stepGrid;
    
    // Controls
    QGroupBox* m_patternGroup;
    QPushButton* m_newButton;
    QPushButton* m_duplicateButton;
    QPushButton* m_clearButton;
    QPushButton* m_playButton;
    QPushButton* m_stopButton;
    QSpinBox* m_tempoSpin;
    QSlider* m_swingSlider;
    QLabel* m_swingLabel;
    
    // Step grid
    std::vector<std::vector<StepButton*>> m_stepButtons; // [track][step]
    std::vector<PatternTrackHeader*> m_trackHeaders;
    
    // Playback
    QTimer* m_playbackTimer;
    
    // Constants
    static constexpr int MAX_TRACKS = 16;
    static constexpr int STEPS_PER_PATTERN = 16;
    static const QStringList DEFAULT_TRACK_NAMES;
};

} // namespace editor
} // namespace daq

