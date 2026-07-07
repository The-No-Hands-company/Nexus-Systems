#pragma once

#include <nexus/utility/audio/audio_buffer_wrapper.h>
#include <vector>
#include <unordered_map>

namespace nexus::utility::audio {

/**
 * @brief Simple audio mixer for combining multiple audio sources.
 */
class AudioMixerSimple {
public:
    struct Channel {
        AudioBufferWrapper::AudioBuffer buffer;
        float volume = 1.0f;
        bool loop = false;
        size_t playPosition = 0;
        bool playing = false;
    };

    /**
     * @brief Adds a channel.
     */
    size_t addChannel(const AudioBufferWrapper::AudioBuffer& buffer, 
                     float volume = 1.0f, bool loop = false) {
        size_t id = nextChannelId_++;
        Channel channel;
        channel.buffer = buffer;
        channel.volume = volume;
        channel.loop = loop;
        channel.playing = true;
        channels_[id] = channel;
        return id;
    }

    /**
     * @brief Removes a channel.
     */
    void removeChannel(size_t id) {
        channels_.erase(id);
    }

    /**
     * @brief Sets channel volume.
     */
    void setChannelVolume(size_t id, float volume) {
        auto it = channels_.find(id);
        if (it != channels_.end()) {
            it->second.volume = volume;
        }
    }

    /**
     * @brief Pauses/resumes channel.
     */
    void setChannelPlaying(size_t id, bool playing) {
        auto it = channels_.find(id);
        if (it != channels_.end()) {
            it->second.playing = playing;
        }
    }

    /**
     * @brief Mixes all channels into output buffer.
     */
    AudioBufferWrapper::AudioBuffer mix(size_t sampleCount, uint32_t sampleRate) {
        auto output = AudioBufferWrapper::create(sampleRate, 
            AudioBufferWrapper::Format::Stereo16, sampleCount);
        
        // Clear output
        std::fill(output.data.begin(), output.data.end(), 0);
        
        // Mix each channel
        for (auto& [id, channel] : channels_) {
            if (!channel.playing) continue;
            
            for (size_t i = 0; i < sampleCount * 4; i += 4) {
                if (channel.playPosition >= channel.buffer.data.size()) {
                    if (channel.loop) {
                        channel.playPosition = 0;
                    } else {
                        channel.playing = false;
                        break;
                    }
                }
                
                // Get sample from channel (simplified - assumes stereo16)
                if (channel.playPosition + 3 < channel.buffer.data.size()) {
                    int16_t left = channel.buffer.data[channel.playPosition] | 
                                  (channel.buffer.data[channel.playPosition + 1] << 8);
                    int16_t right = channel.buffer.data[channel.playPosition + 2] | 
                                   (channel.buffer.data[channel.playPosition + 3] << 8);
                    
                    // Apply volume
                    left = static_cast<int16_t>(left * channel.volume);
                    right = static_cast<int16_t>(right * channel.volume);
                    
                    // Mix into output
                    int16_t outLeft = output.data[i] | (output.data[i + 1] << 8);
                    int16_t outRight = output.data[i + 2] | (output.data[i + 3] << 8);
                    
                    outLeft = static_cast<int16_t>(std::clamp(static_cast<int>(outLeft) + static_cast<int>(left), -32768, 32767));
                    outRight = static_cast<int16_t>(std::clamp(static_cast<int>(outRight) + static_cast<int>(right), -32768, 32767));
                    
                    output.data[i] = outLeft & 0xFF;
                    output.data[i + 1] = (outLeft >> 8) & 0xFF;
                    output.data[i + 2] = outRight & 0xFF;
                    output.data[i + 3] = (outRight >> 8) & 0xFF;
                    
                    channel.playPosition += 4;
                }
            }
        }
        
        return output;
    }

    /**
     * @brief Gets active channel count.
     */
    size_t getActiveChannelCount() const {
        size_t count = 0;
        for (const auto& [id, channel] : channels_) {
            if (channel.playing) count++;
        }
        return count;
    }

    /**
     * @brief Clears all channels.
     */
    void clear() {
        channels_.clear();
    }

private:
    std::unordered_map<size_t, Channel> channels_;
    size_t nextChannelId_ = 0;
};

} // namespace nexus::utility::audio
