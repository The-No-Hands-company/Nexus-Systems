#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace nexus::utility::audio {

/**
 * @brief Audio buffer wrapper for PCM audio data.
 */
class AudioBufferWrapper {
public:
    enum class Format {
        Mono8,
        Mono16,
        Stereo8,
        Stereo16
    };

    struct AudioBuffer {
        std::vector<uint8_t> data;
        uint32_t sampleRate;
        Format format;
        
        size_t getSampleCount() const {
            size_t bytesPerSample = getBytesPerSample();
            return bytesPerSample > 0 ? data.size() / bytesPerSample : 0;
        }
        
        size_t getChannelCount() const {
            return (format == Format::Stereo8 || format == Format::Stereo16) ? 2 : 1;
        }
        
        size_t getBytesPerSample() const {
            switch (format) {
                case Format::Mono8: return 1;
                case Format::Mono16: return 2;
                case Format::Stereo8: return 2;
                case Format::Stereo16: return 4;
            }
            return 0;
        }
        
        float getDuration() const {
            return static_cast<float>(getSampleCount()) / sampleRate;
        }
    };

    /**
     * @brief Creates empty buffer.
     */
    static AudioBuffer create(uint32_t sampleRate, Format format, size_t sampleCount) {
        AudioBuffer buffer;
        buffer.sampleRate = sampleRate;
        buffer.format = format;
        
        size_t bytesPerSample = 0;
        switch (format) {
            case Format::Mono8: bytesPerSample = 1; break;
            case Format::Mono16: bytesPerSample = 2; break;
            case Format::Stereo8: bytesPerSample = 2; break;
            case Format::Stereo16: bytesPerSample = 4; break;
        }
        
        buffer.data.resize(sampleCount * bytesPerSample);
        return buffer;
    }

    /**
     * @brief Generates sine wave.
     */
    static AudioBuffer generateSineWave(float frequency, float duration, 
                                       uint32_t sampleRate = 44100) {
        size_t sampleCount = static_cast<size_t>(duration * sampleRate);
        AudioBuffer buffer = create(sampleRate, Format::Mono16, sampleCount);
        
        for (size_t i = 0; i < sampleCount; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            float value = std::sin(2.0f * M_PI * frequency * t);
            int16_t sample = static_cast<int16_t>(value * 32767);
            
            buffer.data[i * 2] = sample & 0xFF;
            buffer.data[i * 2 + 1] = (sample >> 8) & 0xFF;
        }
        
        return buffer;
    }

    /**
     * @brief Applies volume/gain.
     */
    static void applyGain(AudioBuffer& buffer, float gain) {
        if (buffer.format == Format::Mono16 || buffer.format == Format::Stereo16) {
            for (size_t i = 0; i < buffer.data.size(); i += 2) {
                int16_t sample = buffer.data[i] | (buffer.data[i + 1] << 8);
                sample = static_cast<int16_t>(std::clamp(sample * gain, -32768.0f, 32767.0f));
                buffer.data[i] = sample & 0xFF;
                buffer.data[i + 1] = (sample >> 8) & 0xFF;
            }
        }
    }

    /**
     * @brief Mixes two buffers.
     */
    static AudioBuffer mix(const AudioBuffer& a, const AudioBuffer& b, float mixRatio = 0.5f) {
        if (a.format != b.format || a.sampleRate != b.sampleRate) {
            return a; // Incompatible
        }
        
        size_t minSize = std::min(a.data.size(), b.data.size());
        AudioBuffer result = a;
        result.data.resize(minSize);
        
        if (a.format == Format::Mono16 || a.format == Format::Stereo16) {
            for (size_t i = 0; i < minSize; i += 2) {
                int16_t sampleA = a.data[i] | (a.data[i + 1] << 8);
                int16_t sampleB = b.data[i] | (b.data[i + 1] << 8);
                int16_t mixed = static_cast<int16_t>(sampleA * (1.0f - mixRatio) + sampleB * mixRatio);
                result.data[i] = mixed & 0xFF;
                result.data[i + 1] = (mixed >> 8) & 0xFF;
            }
        }
        
        return result;
    }

    /**
     * @brief Converts stereo to mono.
     */
    static AudioBuffer stereoToMono(const AudioBuffer& stereo) {
        if (stereo.format != Format::Stereo16) return stereo;
        
        AudioBuffer mono = create(stereo.sampleRate, Format::Mono16, 
                                 stereo.getSampleCount() / 2);
        
        for (size_t i = 0; i < stereo.data.size(); i += 4) {
            int16_t left = stereo.data[i] | (stereo.data[i + 1] << 8);
            int16_t right = stereo.data[i + 2] | (stereo.data[i + 3] << 8);
            int16_t avg = (left + right) / 2;
            
            size_t outIdx = i / 2;
            mono.data[outIdx] = avg & 0xFF;
            mono.data[outIdx + 1] = (avg >> 8) & 0xFF;
        }
        
        return mono;
    }
};

} // namespace nexus::utility::audio
