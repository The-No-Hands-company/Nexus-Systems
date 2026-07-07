#pragma once

#include <cmath>
#include <algorithm>

namespace nexus::utility::audio {

/**
 * @brief Volume control utilities with fade and normalization.
 */
class VolumeController {
public:
    /**
     * @brief Converts linear volume to decibels.
     */
    static float linearToDb(float linear) {
        if (linear <= 0.0f) return -100.0f;
        return 20.0f * std::log10(linear);
    }

    /**
     * @brief Converts decibels to linear volume.
     */
    static float dbToLinear(float db) {
        return std::pow(10.0f, db / 20.0f);
    }

    /**
     * @brief Applies fade in.
     */
    static float fadeIn(float t, float duration) {
        if (duration <= 0) return 1.0f;
        return std::clamp(t / duration, 0.0f, 1.0f);
    }

    /**
     * @brief Applies fade out.
     */
    static float fadeOut(float t, float duration) {
        if (duration <= 0) return 0.0f;
        return std::clamp(1.0f - (t / duration), 0.0f, 1.0f);
    }

    /**
     * @brief Smooth fade (S-curve).
     */
    static float smoothFade(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    /**
     * @brief Crossfade between two volumes.
     */
    static std::pair<float, float> crossfade(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        float fadeOutVol = std::cos(t * M_PI * 0.5f);
        float fadeInVol = std::sin(t * M_PI * 0.5f);
        return {fadeOutVol, fadeInVol};
    }

    /**
     * @brief Applies compression (simple).
     */
    static float compress(float input, float threshold, float ratio) {
        float absInput = std::abs(input);
        if (absInput > threshold) {
            float excess = absInput - threshold;
            float compressed = threshold + (excess / ratio);
            return (input >= 0) ? compressed : -compressed;
        }
        return input;
    }

    /**
     * @brief Applies limiter (hard clipping prevention).
     */
    static float limit(float input, float ceiling = 1.0f) {
        return std::clamp(input, -ceiling, ceiling);
    }

    /**
     * @brief Normalizes audio to peak.
     */
    static float normalizeGain(float currentPeak, float targetPeak = 1.0f) {
        if (currentPeak <= 0.0f) return 1.0f;
        return targetPeak / currentPeak;
    }

    /**
     * @brief Applies envelope (ADSR).
     */
    struct ADSR {
        float attack = 0.1f;
        float decay = 0.1f;
        float sustain = 0.7f;
        float release = 0.2f;
        
        float getAmplitude(float time, float noteLength) const {
            if (time < attack) {
                // Attack phase
                return time / attack;
            } else if (time < attack + decay) {
                // Decay phase
                float t = (time - attack) / decay;
                return 1.0f - t * (1.0f - sustain);
            } else if (time < noteLength) {
                // Sustain phase
                return sustain;
            } else if (time < noteLength + release) {
                // Release phase
                float t = (time - noteLength) / release;
                return sustain * (1.0f - t);
            }
            return 0.0f;
        }
    };
};

} // namespace nexus::utility::audio
