#include "dawg/core/AudioBuffer.h"
#include <algorithm>
#include <cstring>

namespace dawg {
namespace core {

AudioBuffer::AudioBuffer(uint16_t channels, uint32_t samples)
    : m_channels(channels), m_samples(samples) {
    m_data = std::make_unique<float[]>(channels * samples);
    clear();
}

AudioBuffer::~AudioBuffer() = default;

AudioBuffer::AudioBuffer(AudioBuffer&& other) noexcept
    : m_channels(other.m_channels)
    , m_samples(other.m_samples)
    , m_timestamp(other.m_timestamp)
    , m_data(std::move(other.m_data)) {
    other.m_channels = 0;
    other.m_samples = 0;
    other.m_timestamp = 0;
}

AudioBuffer& AudioBuffer::operator=(AudioBuffer&& other) noexcept {
    if (this != &other) {
        m_channels = other.m_channels;
        m_samples = other.m_samples;
        m_timestamp = other.m_timestamp;
        m_data = std::move(other.m_data);
        
        other.m_channels = 0;
        other.m_samples = 0;
        other.m_timestamp = 0;
    }
    return *this;
}

float* AudioBuffer::getChannelData(uint16_t channel) noexcept {
    if (channel >= m_channels || !m_data) return nullptr;
    return m_data.get() + (channel * m_samples);
}

const float* AudioBuffer::getChannelData(uint16_t channel) const noexcept {
    if (channel >= m_channels || !m_data) return nullptr;
    return m_data.get() + (channel * m_samples);
}

float& AudioBuffer::sample(uint16_t channel, uint32_t index) noexcept {
    static float dummy = 0.0f;
    if (channel >= m_channels || index >= m_samples || !m_data) {
        return dummy;
    }
    return m_data[channel * m_samples + index];
}

const float& AudioBuffer::sample(uint16_t channel, uint32_t index) const noexcept {
    static const float dummy = 0.0f;
    if (channel >= m_channels || index >= m_samples || !m_data) {
        return dummy;
    }
    return m_data[channel * m_samples + index];
}

void AudioBuffer::clear() noexcept {
    if (m_data) {
        std::memset(m_data.get(), 0, m_channels * m_samples * sizeof(float));
    }
}

void AudioBuffer::copyFrom(const AudioBuffer& source) noexcept {
    if (!m_data || !source.m_data) return;
    
    uint16_t minChannels = std::min(m_channels, source.m_channels);
    uint32_t minSamples = std::min(m_samples, source.m_samples);
    
    for (uint16_t ch = 0; ch < minChannels; ++ch) {
        std::memcpy(getChannelData(ch), source.getChannelData(ch), 
                   minSamples * sizeof(float));
    }
}

void AudioBuffer::addFrom(const AudioBuffer& source, float gain) noexcept {
    if (!m_data || !source.m_data) return;
    
    uint16_t minChannels = std::min(m_channels, source.m_channels);
    uint32_t minSamples = std::min(m_samples, source.m_samples);
    
    for (uint16_t ch = 0; ch < minChannels; ++ch) {
        float* dest = getChannelData(ch);
        const float* src = source.getChannelData(ch);
        
        for (uint32_t i = 0; i < minSamples; ++i) {
            dest[i] += src[i] * gain;
        }
    }
}

void AudioBuffer::applyGain(float gain) noexcept {
    if (!m_data) return;
    
    for (uint16_t ch = 0; ch < m_channels; ++ch) {
        float* data = getChannelData(ch);
        for (uint32_t i = 0; i < m_samples; ++i) {
            data[i] *= gain;
        }
    }
}

uint16_t AudioBuffer::getChannelCount() const noexcept {
    return m_channels;
}

uint32_t AudioBuffer::getSampleCount() const noexcept {
    return m_samples;
}

float AudioBuffer::getSample(uint16_t channel, uint32_t frame) const noexcept {
    if (channel >= m_channels || frame >= m_samples || !m_data) return 0.0f;
    return m_data[channel * m_samples + frame];
}

void AudioBuffer::setSample(uint16_t channel, uint32_t frame, float value) noexcept {
    if (channel >= m_channels || frame >= m_samples || !m_data) return;
    m_data[channel * m_samples + frame] = value;
}

void AudioBuffer::copyToInterleaved(float* dest, uint32_t numFrames) const noexcept {
    if (!dest || !m_data) return;
    
    uint32_t framesToCopy = std::min(numFrames, m_samples);
    
    for (uint32_t frame = 0; frame < framesToCopy; ++frame) {
        for (uint16_t channel = 0; channel < m_channels; ++channel) {
            dest[frame * m_channels + channel] = getSample(channel, frame);
        }
    }
}

} // namespace core
} // namespace dawg
