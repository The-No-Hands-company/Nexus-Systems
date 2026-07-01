#ifndef PATTERNSEQUENCER_H
#define PATTERNSEQUENCER_H

#include <cstdint>
#include <vector>
#include <memory>
#include <QString>
#include <QColor>

namespace daq {
namespace editor {

/**
 * @brief Represents a single step in a pattern sequencer
 */
struct Step {
    bool enabled = false;
    int velocity = 100; // 0-127
    int pan = 0; // -100 (left) to 100 (right)
    QString samplePath; // Path to sample file for this step
    
    Step() = default;
    Step(bool enabled, int velocity, int pan, const QString& samplePath = "")
        : enabled(enabled), velocity(velocity), pan(pan), samplePath(samplePath) {}
};

/**
 * @brief Represents a pattern containing multiple tracks/steps
 */
class Pattern {
public:
    Pattern(int trackCount = 4, int stepCount = 16);
    ~Pattern();
    
    // Getters
    int getTrackCount() const { return m_trackCount; }
    int getStepCount() const { return m_stepCount; }
    const Step& getStep(int track, int step) const;
    Step& getStep(int track, int step);
    
    // Setters
    void setStep(int track, int step, const Step& stepValue);
    void setTrackCount(int trackCount);
    void setStepCount(int stepCount);
    
    // Utility
    void clear();
    
private:
    int m_trackCount;
    int m_stepCount;
    std::vector<std::vector<Step>> m_steps; // [track][step]
};

/**
 * @brief PatternSequencer handles pattern-based sequencing (like FL Studio step sequencer)
 */
class PatternSequencer
{
public:
    PatternSequencer();
    ~PatternSequencer();

    // Sequencer controls
    void setTempo(float bpm);
    float getTempo() const;

    void setTimeSignature(int numerator, int denominator);
    void getTimeSignature(int& numerator, int& denominator) const;

    // Pattern management
    void setPatternLength(int steps);
    int getPatternLength() const;

    void setStepEnabled(int track, int step, bool enabled);
    bool isStepEnabled(int track, int step) const;

    void setPattern(std::shared_ptr<Pattern> pattern);
    std::shared_ptr<Pattern> getPattern() const;
    std::shared_ptr<Pattern> createPattern(int trackCount = 4, int stepCount = 16);

    void clear();

private:
    float m_tempo = 120.0f; // BPM
    int m_timeSignatureNumerator = 4;
    int m_timeSignatureDenominator = 4;
    int m_patternLength = 16; // Steps per pattern
    std::shared_ptr<Pattern> m_pattern;
};

} // namespace editor
} // namespace daq

#endif // PATTERNSEQUENCER_H