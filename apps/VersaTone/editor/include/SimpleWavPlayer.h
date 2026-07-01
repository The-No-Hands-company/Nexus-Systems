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
 * File: SimpleWavPlayer.h
 * Purpose: Header file for audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#pragma once
#include <QObject>
#include <QAudioSink>
#include <QAudioFormat>
#include <QIODevice>
#include <QByteArray>
#include <QString>

class QTimer;

class SimpleWavPlayer : public QObject {
    Q_OBJECT
    
public:
    explicit SimpleWavPlayer(QObject* parent = nullptr);
    ~SimpleWavPlayer();
    
    bool loadFile(const QString& filePath);
    void play();
    void stop();
    void setVolume(float volume); // 0.0 to 1.0
    
    bool isPlaying() const;
    QString currentFile() const { return m_currentFile; }
    
signals:
    void playbackFinished();
    void playbackStarted();
    void playbackStopped();
    
private slots:
    void onStateChanged();
    void onPlaybackTimeout();
    
private:
    struct WavHeader {
        char riff[4];           // "RIFF"
        uint32_t fileSize;      // File size - 8
        char wave[4];           // "WAVE"
        char fmt[4];            // "fmt "
        uint32_t fmtSize;       // Format chunk size
        uint16_t audioFormat;   // Audio format (1 = PCM)
        uint16_t channels;      // Number of channels
        uint32_t sampleRate;    // Sample rate
        uint32_t byteRate;      // Bytes per second
        uint16_t blockAlign;    // Block align
        uint16_t bitsPerSample; // Bits per sample
        char data[4];           // "data"
        uint32_t dataSize;      // Data size
    };
    
    bool parseWavFile(const QString& filePath);
    void setupAudioFormat();
    
    QAudioSink* m_audioSink;
    QAudioFormat m_qtAudioFormat;  // Qt audio format object
    QIODevice* m_audioDevice;
    QByteArray m_audioData;
    QTimer* m_playbackTimer;
    
    QString m_currentFile;
    float m_volume;
    bool m_isPlaying;
    
    // WAV file info
    uint32_t m_sampleRate;
    uint16_t m_channels;
    uint16_t m_bitsPerSample;
    uint16_t m_audioFormat;  // 1=PCM, 3=IEEE_FLOAT, etc.
    uint32_t m_dataSize;
};

class SimpleAudioBuffer : public QIODevice {
    Q_OBJECT
    
public:
    explicit SimpleAudioBuffer(const QByteArray& data, QObject* parent = nullptr);
    
    qint64 readData(char* data, qint64 maxlen) override;
    qint64 writeData(const char* data, qint64 len) override;
    
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override;
    
    bool reset() override;
    
private:
    QByteArray m_data;
    qint64 m_position;
};

