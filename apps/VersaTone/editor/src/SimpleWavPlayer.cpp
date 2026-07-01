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
 * File: SimpleWavPlayer.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#include "SimpleWavPlayer.h"
#include <QAudioSink>
#include <QAudioFormat>
#include <QTimer>
#include <QFile>
#include <QDebug>
#include <QtGlobal>
#include <cstring>

SimpleWavPlayer::SimpleWavPlayer(QObject* parent)
    : QObject(parent)
    , m_audioSink(nullptr)
    , m_audioDevice(nullptr)
    , m_playbackTimer(new QTimer(this))
    , m_volume(0.7f)
    , m_isPlaying(false)
    , m_sampleRate(44100)
    , m_channels(2)
    , m_bitsPerSample(16)
    , m_dataSize(0)
{
    m_playbackTimer->setSingleShot(true);
    connect(m_playbackTimer, &QTimer::timeout, this, &SimpleWavPlayer::onPlaybackTimeout);
}

SimpleWavPlayer::~SimpleWavPlayer() {
    stop();
    if (m_audioSink) {
        delete m_audioSink;
    }
}

bool SimpleWavPlayer::loadFile(const QString& filePath) {
    stop();
    
    if (!parseWavFile(filePath)) {
        qDebug() << "Failed to parse WAV file:" << filePath;
        return false;
    }
    
    m_currentFile = filePath;
    setupAudioFormat();
    
    return true;
}

bool SimpleWavPlayer::parseWavFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open file:" << filePath;
        return false;
    }
    
    // Read RIFF header
    char riffHeader[12];
    if (file.read(riffHeader, 12) != 12) {
        qDebug() << "Cannot read RIFF header";
        return false;
    }
    
    // Verify RIFF and WAVE
    if (strncmp(riffHeader, "RIFF", 4) != 0 || strncmp(riffHeader + 8, "WAVE", 4) != 0) {
        qDebug() << "Not a valid WAV file (RIFF/WAVE check failed)";
        return false;
    }
    
    // Find fmt chunk
    bool fmtFound = false;
    while (!fmtFound && !file.atEnd()) {
        char chunkId[4];
        if (file.read(chunkId, 4) != 4) break;
        
        quint32 chunkSize;
        if (file.read(reinterpret_cast<char*>(&chunkSize), 4) != 4) break;
        
        if (strncmp(chunkId, "fmt ", 4) == 0) {
            // Read format chunk
            if (chunkSize < 16) {
                qDebug() << "Format chunk too small";
                return false;
            }
            
            struct {
                quint16 audioFormat;
                quint16 channels;
                quint32 sampleRate;
                quint32 byteRate;
                quint16 blockAlign;
                quint16 bitsPerSample;
            } fmtData;
            
            if (file.read(reinterpret_cast<char*>(&fmtData), 16) != 16) {
                qDebug() << "Cannot read format data";
                return false;
            }
            
            // Support multiple audio formats
            if (fmtData.audioFormat != 1 && fmtData.audioFormat != 3 && fmtData.audioFormat != 65534) {
                qDebug() << "Unsupported audio format:" << fmtData.audioFormat << "(supported: 1=PCM, 3=IEEE_FLOAT, 65534=EXTENSIBLE)";
                return false;
            }
            
            // Store audio properties
            m_sampleRate = fmtData.sampleRate;
            m_channels = fmtData.channels;
            m_bitsPerSample = fmtData.bitsPerSample;
            m_audioFormat = fmtData.audioFormat; // Store the original format
            
            // Skip any extra bytes in fmt chunk
            if (chunkSize > 16) {
                file.seek(file.pos() + (chunkSize - 16));
            }
            
            fmtFound = true;
        } else {
            // Skip this chunk
            file.seek(file.pos() + chunkSize);
        }
    }
    
    if (!fmtFound) {
        qDebug() << "Format chunk not found";
        return false;
    }
    
    // Find data chunk
    file.seek(12); // Reset to after RIFF header
    bool dataFound = false;
    while (!dataFound && !file.atEnd()) {
        char chunkId[4];
        if (file.read(chunkId, 4) != 4) break;
        
        quint32 chunkSize;
        if (file.read(reinterpret_cast<char*>(&chunkSize), 4) != 4) break;
        
        if (strncmp(chunkId, "data", 4) == 0) {
            // Found data chunk
            m_dataSize = chunkSize;
            m_audioData = file.read(chunkSize);
            
            if (m_audioData.size() != static_cast<int>(chunkSize)) {
                qDebug() << "Could not read all audio data. Expected:" << chunkSize << "Got:" << m_audioData.size();
                return false;
            }
            
            dataFound = true;
        } else {
            // Skip this chunk
            file.seek(file.pos() + chunkSize);
        }
    }
    
    if (!dataFound) {
        qDebug() << "Data chunk not found";
        return false;
    }
    
    qDebug() << "Successfully parsed WAV:" << filePath;
    qDebug() << "Audio format:" << m_audioFormat << "(1=PCM, 3=IEEE_FLOAT)";
    qDebug() << "Sample rate:" << m_sampleRate << "Channels:" << m_channels << "Bits:" << m_bitsPerSample << "Data size:" << m_dataSize;
    return true;
}

void SimpleWavPlayer::setupAudioFormat() {
    m_qtAudioFormat.setSampleRate(m_sampleRate);
    m_qtAudioFormat.setChannelCount(m_channels);
    
    // Set sample format based on audio format and bits per sample
    if (m_audioFormat == 3) {
        // IEEE float format
        if (m_bitsPerSample == 32) {
            m_qtAudioFormat.setSampleFormat(QAudioFormat::Float);
        } else {
            qDebug() << "Unsupported IEEE float bits per sample:" << m_bitsPerSample;
            return;
        }
    } else {
        // PCM format
        if (m_bitsPerSample == 16) {
            m_qtAudioFormat.setSampleFormat(QAudioFormat::Int16);
        } else if (m_bitsPerSample == 8) {
            m_qtAudioFormat.setSampleFormat(QAudioFormat::UInt8);
        } else if (m_bitsPerSample == 24) {
            m_qtAudioFormat.setSampleFormat(QAudioFormat::Int32); // Qt doesn't have Int24, use Int32
        } else if (m_bitsPerSample == 32) {
            m_qtAudioFormat.setSampleFormat(QAudioFormat::Int32);
        } else {
            qDebug() << "Unsupported PCM bits per sample:" << m_bitsPerSample;
            return;
        }
    }
    
    // Clean up previous audio sink
    if (m_audioSink) {
        delete m_audioSink;
        m_audioSink = nullptr;
    }
    
    // Create new audio sink
    m_audioSink = new QAudioSink(m_qtAudioFormat, this);
    m_audioSink->setVolume(m_volume);
    
    connect(m_audioSink, &QAudioSink::stateChanged, this, &SimpleWavPlayer::onStateChanged);
}

void SimpleWavPlayer::play() {
    if (m_audioData.isEmpty() || !m_audioSink) {
        return;
    }
    
    if (m_isPlaying) {
        stop();
    }
    
    // Create audio buffer
    if (m_audioDevice) {
        delete m_audioDevice;
    }
    m_audioDevice = new SimpleAudioBuffer(m_audioData, this);
    m_audioDevice->open(QIODevice::ReadOnly);
    
    // Start playback
    m_audioSink->start(m_audioDevice);
    m_isPlaying = true;
    
    // Calculate playback duration and set timer
    double durationSeconds = static_cast<double>(m_dataSize) / (m_sampleRate * m_channels * (m_bitsPerSample / 8));
    m_playbackTimer->start(static_cast<int>(durationSeconds * 1000) + 100); // Add small buffer
    
    emit playbackStarted();
}

void SimpleWavPlayer::stop() {
    if (m_audioSink && m_isPlaying) {
        m_audioSink->stop();
    }
    
    if (m_audioDevice) {
        m_audioDevice->close();
        delete m_audioDevice;
        m_audioDevice = nullptr;
    }
    
    m_playbackTimer->stop();
    m_isPlaying = false;
    
    emit playbackStopped();
}

void SimpleWavPlayer::setVolume(float volume) {
    m_volume = qBound(0.0f, volume, 1.0f);
    if (m_audioSink) {
        m_audioSink->setVolume(m_volume);
    }
}

bool SimpleWavPlayer::isPlaying() const {
    return m_isPlaying && m_audioSink && m_audioSink->state() == QAudio::ActiveState;
}

void SimpleWavPlayer::onStateChanged() {
    if (m_audioSink && m_audioSink->state() == QAudio::IdleState && m_isPlaying) {
        // Playback finished
        stop();
        emit playbackFinished();
    }
}

void SimpleWavPlayer::onPlaybackTimeout() {
    if (m_isPlaying) {
        stop();
        emit playbackFinished();
    }
}

// SimpleAudioBuffer Implementation
SimpleAudioBuffer::SimpleAudioBuffer(const QByteArray& data, QObject* parent)
    : QIODevice(parent)
    , m_data(data)
    , m_position(0)
{
}

qint64 SimpleAudioBuffer::readData(char* data, qint64 maxlen) {
    if (m_position >= m_data.size()) {
        return 0; // End of data
    }
    
    qint64 bytesToRead = qMin(maxlen, static_cast<qint64>(m_data.size() - m_position));
    memcpy(data, m_data.constData() + m_position, bytesToRead);
    m_position += bytesToRead;
    
    return bytesToRead;
}

qint64 SimpleAudioBuffer::writeData(const char*, qint64) {
    return -1; // Read-only
}

qint64 SimpleAudioBuffer::bytesAvailable() const {
    return m_data.size() - m_position + QIODevice::bytesAvailable();
}

bool SimpleAudioBuffer::reset() {
    m_position = 0;
    return QIODevice::reset();
}

