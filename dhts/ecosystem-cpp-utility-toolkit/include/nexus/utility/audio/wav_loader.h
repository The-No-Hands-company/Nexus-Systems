#pragma once

#include <nexus/utility/audio/audio_buffer_wrapper.h>
#include <fstream>
#include <cstring>

namespace nexus::utility::audio {

/**
 * @brief WAV file loader.
 */
class WavLoader {
public:
    struct WavHeader {
        char riff[4];           // "RIFF"
        uint32_t fileSize;
        char wave[4];           // "WAVE"
        char fmt[4];            // "fmt "
        uint32_t fmtSize;
        uint16_t audioFormat;   // 1 = PCM
        uint16_t numChannels;
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample;
        char data[4];           // "data"
        uint32_t dataSize;
    };

    /**
     * @brief Loads WAV file.
     */
    static AudioBufferWrapper::AudioBuffer loadFromFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return {};
        }
        
        // Read header
        WavHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));
        
        // Validate
        if (std::strncmp(header.riff, "RIFF", 4) != 0 ||
            std::strncmp(header.wave, "WAVE", 4) != 0) {
            return {};
        }
        
        // Determine format
        AudioBufferWrapper::Format format;
        if (header.numChannels == 1 && header.bitsPerSample == 8) {
            format = AudioBufferWrapper::Format::Mono8;
        } else if (header.numChannels == 1 && header.bitsPerSample == 16) {
            format = AudioBufferWrapper::Format::Mono16;
        } else if (header.numChannels == 2 && header.bitsPerSample == 8) {
            format = AudioBufferWrapper::Format::Stereo8;
        } else if (header.numChannels == 2 && header.bitsPerSample == 16) {
            format = AudioBufferWrapper::Format::Stereo16;
        } else {
            return {}; // Unsupported format
        }
        
        // Read audio data
        AudioBufferWrapper::AudioBuffer buffer;
        buffer.sampleRate = header.sampleRate;
        buffer.format = format;
        buffer.data.resize(header.dataSize);
        file.read(reinterpret_cast<char*>(buffer.data.data()), header.dataSize);
        
        return buffer;
    }

    /**
     * @brief Saves WAV file.
     */
    static bool saveToFile(const std::string& filename, 
                          const AudioBufferWrapper::AudioBuffer& buffer) {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        WavHeader header;
        std::memcpy(header.riff, "RIFF", 4);
        std::memcpy(header.wave, "WAVE", 4);
        std::memcpy(header.fmt, "fmt ", 4);
        std::memcpy(header.data, "data", 4);
        
        header.fmtSize = 16;
        header.audioFormat = 1; // PCM
        header.numChannels = buffer.getChannelCount();
        header.sampleRate = buffer.sampleRate;
        
        switch (buffer.format) {
            case AudioBufferWrapper::Format::Mono8:
            case AudioBufferWrapper::Format::Stereo8:
                header.bitsPerSample = 8;
                break;
            case AudioBufferWrapper::Format::Mono16:
            case AudioBufferWrapper::Format::Stereo16:
                header.bitsPerSample = 16;
                break;
        }
        
        header.byteRate = buffer.sampleRate * header.numChannels * (header.bitsPerSample / 8);
        header.blockAlign = header.numChannels * (header.bitsPerSample / 8);
        header.dataSize = buffer.data.size();
        header.fileSize = 36 + header.dataSize;
        
        file.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
        file.write(reinterpret_cast<const char*>(buffer.data.data()), buffer.data.size());
        
        return true;
    }
};

} // namespace nexus::utility::audio
