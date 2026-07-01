#ifndef WAVEFORMPREVIEW_H
#define WAVEFORMPREVIEW_H

#include <QWidget>
#include <QAudioFormat>
#include <vector>
#include <QColor>
#include <QTimer>
#include <QString>

namespace daq {
namespace editor {

class WaveformPreview : public QWidget
{
    Q_OBJECT
public:
    explicit WaveformPreview(QWidget* parent = nullptr);
    ~WaveformPreview();

// Set audio data to display
     void setAudioData(const std::vector<float>& samples, const QAudioFormat& format);
     void clearWaveform();
     void loadAudioFile(const QString& filePath);
     void setPlaybackPosition(qreal position);

    signals:
        void waveformLoaded(const QString& filePath);
        void positionClicked(qreal position);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    qreal m_sampleRate = 44100.0;
    int m_channels = 2;
    QTimer* m_updateTimer = nullptr;
    QString m_currentFilePath;

    // Waveform display properties
    QVector<double> m_waveformData;
    QColor m_backgroundColor = Qt::black;
    QColor m_waveformColor = Qt::green;
    QColor m_textColor = Qt::white;
    QColor m_playbackLineColor = Qt::red;

    // Playback state
    double m_playbackPosition = 0.0;
    double m_duration = 0.0;
    bool m_isPlaying = false;
    bool m_isMousePressed = false;

    void generateWaveformData(const QString& filePath);
    void drawWaveform(QPainter& painter);
    void drawPlaybackLine(QPainter& painter);
    void drawTimeLabels(QPainter& painter);
    void updatePlaybackLine();
};
} // namespace editor
} // namespace daq

#endif // WAVEFORMPREVIEW_H