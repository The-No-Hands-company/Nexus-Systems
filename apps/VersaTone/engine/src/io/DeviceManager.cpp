#include "dawg/io/DeviceManager.h"
#include "dawg/core/AudioBuffer.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <comdef.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winmm.lib")
#endif

namespace dawg {
namespace io {

/**
 * @brief DeviceManager implementation for Windows WASAPI
 */
class DeviceManager::Impl {
public:
    Impl() : isRunning(false), cpuLoad(0.0), xruns(0) {
#ifdef _WIN32
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif
    }
    
    ~Impl() {
        closeStream();
#ifdef _WIN32
        if (audioEvent) {
            CloseHandle(audioEvent);
            audioEvent = nullptr;
        }
        CoUninitialize();
#endif
    }
    
    std::vector<AudioDevice> getOutputDevices() const {
        std::vector<AudioDevice> devices;
        
#ifdef _WIN32
        IMMDeviceEnumerator* enumerator = nullptr;
        IMMDeviceCollection* collection = nullptr;
        
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                     CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                     (void**)&enumerator);
        
        if (SUCCEEDED(hr)) {
            hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
            
            if (SUCCEEDED(hr)) {
                UINT count;
                collection->GetCount(&count);
                
                for (UINT i = 0; i < count; i++) {
                    IMMDevice* device = nullptr;
                    if (SUCCEEDED(collection->Item(i, &device))) {
                        AudioDevice audioDevice = getDeviceInfo(device, false);
                        if (!audioDevice.name.empty()) {
                            devices.push_back(audioDevice);
                        }
                        device->Release();
                    }
                }
                collection->Release();
            }
            enumerator->Release();
        }
#else
        // Add Linux/Mac implementations here
        AudioDevice defaultDevice;
        defaultDevice.name = "Default Audio Device";
        defaultDevice.id = "default";
        defaultDevice.maxOutputChannels = 2;
        defaultDevice.supportedSampleRates = {44100, 48000, 96000};
        defaultDevice.supportedBufferSizes = {128, 256, 512, 1024};
        devices.push_back(defaultDevice);
#endif
        
        return devices;
    }
    
    bool openStream(const AudioStreamConfig& config, AudioCallback callback) {
        this->callback = callback;
        this->config = config;
        
#ifdef _WIN32
        return openWASAPIStream();
#else
        // Fallback implementation
        std::cout << "Audio stream opened (fallback mode)\n";
        return true;
#endif
    }
    
    bool startStream() {
        if (!isRunning.exchange(true)) {
#ifdef _WIN32
            if (audioClient) {
                HRESULT hr = audioClient->Start();
                if (FAILED(hr)) {
                    std::cerr << "Failed to start WASAPI stream. HRESULT: 0x" << std::hex << hr << std::endl;
                    isRunning = false;
                    return false;
                }
                std::cout << "WASAPI stream started successfully" << std::endl;
            }
#endif
            audioThread = std::thread(&Impl::audioThreadFunc, this);
            return true;
        }
        return false;
    }
    
    bool stopStream() {
        if (isRunning.exchange(false)) {
            if (audioThread.joinable()) {
                audioThread.join();
            }
            return true;
        }
        return false;
    }
    
    bool closeStream() {
        stopStream();
#ifdef _WIN32
        if (audioClient) {
            audioClient->Stop();
            audioClient->Release();
            audioClient = nullptr;
        }
        if (renderClient) {
            renderClient->Release();
            renderClient = nullptr;
        }
        if (audioEvent) {
            CloseHandle(audioEvent);
            audioEvent = nullptr;
        }
#endif
        return true;
    }
    
    double getCpuLoad() const { return cpuLoad; }
    uint32_t getXruns() const { return xruns; }
    bool isStreamRunning() const { return isRunning; }
    
private:
#ifdef _WIN32
    IAudioClient* audioClient = nullptr;
    IAudioRenderClient* renderClient = nullptr;
    HANDLE audioEvent = nullptr;
    
    AudioDevice getDeviceInfo(IMMDevice* device, bool isInput) const {
        AudioDevice audioDevice;
        
        LPWSTR deviceId = nullptr;
        if (SUCCEEDED(device->GetId(&deviceId))) {
            audioDevice.id = wstringToString(deviceId);
            CoTaskMemFree(deviceId);
        }
        
        IPropertyStore* props = nullptr;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
            PROPVARIANT varName;
            PropVariantInit(&varName);
            
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName))) {
                audioDevice.name = wstringToString(varName.pwszVal);
            }
            
            PropVariantClear(&varName);
            props->Release();
        }
        
        audioDevice.driver = "WASAPI";
        audioDevice.maxOutputChannels = isInput ? 0 : 8;
        audioDevice.maxInputChannels = isInput ? 8 : 0;
        audioDevice.supportedSampleRates = {44100, 48000, 88200, 96000, 176400, 192000};
        audioDevice.supportedBufferSizes = {64, 128, 256, 512, 1024, 2048};
        audioDevice.isExclusive = true;
        
        return audioDevice;
    }
    
    bool openWASAPIStream() {
        IMMDeviceEnumerator* enumerator = nullptr;
        IMMDevice* device = nullptr;
        
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                     CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                     (void**)&enumerator);
        
        if (FAILED(hr)) {
            std::cerr << "Failed to create device enumerator. HRESULT: 0x" << std::hex << hr << std::endl;
            return false;
        }
        
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (FAILED(hr)) {
            std::cerr << "Failed to get default audio endpoint. HRESULT: 0x" << std::hex << hr << std::endl;
            enumerator->Release();
            return false;
        }
        
        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audioClient);
        if (FAILED(hr)) {
            std::cerr << "Failed to activate audio client. HRESULT: 0x" << std::hex << hr << std::endl;
            device->Release();
            enumerator->Release();
            return false;
        }
        
        // Get the device's native format first
        WAVEFORMATEX* deviceFormat = nullptr;
        hr = audioClient->GetMixFormat(&deviceFormat);
        if (FAILED(hr)) {
            std::cerr << "Failed to get device mix format. HRESULT: 0x" << std::hex << hr << std::endl;
            device->Release();
            enumerator->Release();
            return false;
        }
        
        std::cout << "Device native format: " << deviceFormat->nSamplesPerSec << " Hz, " 
                  << deviceFormat->nChannels << " channels, " << deviceFormat->wBitsPerSample << " bits" << std::endl;
        
        // Try shared mode first (more reliable)
        REFERENCE_TIME bufferDuration = (REFERENCE_TIME)((double)config.bufferSize / config.sampleRate * 10000000.0);
        
        // Use the device's native format for shared mode
        hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                   AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                   bufferDuration, 0, deviceFormat, nullptr);
        
        if (FAILED(hr)) {
            std::cerr << "Failed to initialize in shared mode. HRESULT: 0x" << std::hex << hr << std::endl;
            
            // Try exclusive mode if shared mode fails
            WAVEFORMATEX format = {};
            format.wFormatTag = WAVE_FORMAT_PCM;  // Use PCM instead of float for compatibility
            format.nChannels = config.outputChannels;
            format.nSamplesPerSec = config.sampleRate;
            format.wBitsPerSample = 16;  // Use 16-bit for better compatibility
            format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
            format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
            format.cbSize = 0;
            
            hr = audioClient->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                       AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                       bufferDuration, bufferDuration, &format, nullptr);
            
            if (FAILED(hr)) {
                std::cerr << "Failed to initialize in exclusive mode. HRESULT: 0x" << std::hex << hr << std::endl;
                CoTaskMemFree(deviceFormat);
                device->Release();
                enumerator->Release();
                return false;
            } else {
                std::cout << "Successfully initialized in exclusive mode" << std::endl;
            }
        } else {
            std::cout << "Successfully initialized in shared mode" << std::endl;
        }
        
        // Create event for callback
        audioEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (audioEvent == nullptr) {
            std::cerr << "Failed to create audio event" << std::endl;
            CoTaskMemFree(deviceFormat);
            device->Release();
            enumerator->Release();
            return false;
        }
        
        hr = audioClient->SetEventHandle(audioEvent);
        if (FAILED(hr)) {
            std::cerr << "Failed to set event handle. HRESULT: 0x" << std::hex << hr << std::endl;
            CloseHandle(audioEvent);
            CoTaskMemFree(deviceFormat);
            device->Release();
            enumerator->Release();
            return false;
        }
        
        if (SUCCEEDED(hr)) {
            hr = audioClient->GetService(__uuidof(IAudioRenderClient), (void**)&renderClient);
            if (FAILED(hr)) {
                std::cerr << "Failed to get render client. HRESULT: 0x" << std::hex << hr << std::endl;
            }
        }
        
        CoTaskMemFree(deviceFormat);
        device->Release();
        enumerator->Release();
        
        if (SUCCEEDED(hr)) {
            std::cout << "Audio stream opened successfully!" << std::endl;
            return true;
        } else {
            std::cerr << "Failed to initialize audio stream" << std::endl;
            return false;
        }
    }
    
    std::string wstringToString(const std::wstring& wstr) const {
        if (wstr.empty()) return std::string();
        int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &result[0], size, nullptr, nullptr);
        return result;
    }
#endif
    
    void audioThreadFunc() {
        // Set real-time priority
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif
        
        std::vector<float> outputBuffer(config.bufferSize * config.outputChannels);
        dawg::core::AudioBuffer inputAudioBuffer(config.bufferSize, 0);
        dawg::core::AudioBuffer outputAudioBuffer(config.bufferSize, config.outputChannels);
        
        auto lastTime = std::chrono::high_resolution_clock::now();
        
        while (isRunning) {
#ifdef _WIN32
            if (audioEvent) {
                // Wait for WASAPI buffer ready event
                DWORD waitResult = WaitForSingleObject(audioEvent, 1000); // 1 second timeout
                if (waitResult != WAIT_OBJECT_0) {
                    if (waitResult == WAIT_TIMEOUT) {
                        std::cerr << "Audio event timeout" << std::endl;
                        xruns++;
                    }
                    continue;
                }
            }
#endif
            
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration<double>(currentTime - lastTime).count();
            
            // Calculate CPU load
            double expectedTime = (double)config.bufferSize / config.sampleRate;
            cpuLoad = (elapsed / expectedTime) * 100.0;
            
            // Call user callback
            if (callback) {
                try {
                    callback(inputAudioBuffer, outputAudioBuffer, config.bufferSize);
                } catch (...) {
                    // Handle callback exceptions
                    xruns++;
                }
            }
            
#ifdef _WIN32
            // Output to WASAPI
            if (renderClient && audioClient) {
                UINT32 padding;
                if (SUCCEEDED(audioClient->GetCurrentPadding(&padding))) {
                    UINT32 bufferSize;
                    if (SUCCEEDED(audioClient->GetBufferSize(&bufferSize))) {
                        UINT32 available = bufferSize - padding;
                        if (available > 0) {
                            BYTE* data;
                            if (SUCCEEDED(renderClient->GetBuffer(available, &data))) {
                                // Copy audio data
                                float* floatData = reinterpret_cast<float*>(data);
                                outputAudioBuffer.copyToInterleaved(floatData, available);
                                renderClient->ReleaseBuffer(available, 0);
                            }
                        }
                    }
                }
            }
#else
            // Fallback timing for non-Windows
            std::this_thread::sleep_for(std::chrono::microseconds(
                (int64_t)(expectedTime * 1000000 * 0.8)  // 80% of buffer time
            ));
#endif
            
            lastTime = currentTime;
        }
    }
    
    AudioStreamConfig config;
    AudioCallback callback;
    std::atomic<bool> isRunning;
    std::thread audioThread;
    std::atomic<double> cpuLoad;
    std::atomic<uint32_t> xruns;
};

// DeviceManager public interface
DeviceManager::DeviceManager() : pImpl(std::make_unique<Impl>()) {}
DeviceManager::~DeviceManager() = default;

std::vector<AudioDevice> DeviceManager::getInputDevices() const {
    return {}; // TODO: Implement input devices
}

std::vector<AudioDevice> DeviceManager::getOutputDevices() const {
    return pImpl->getOutputDevices();
}

AudioDevice DeviceManager::getDefaultOutputDevice() const {
    auto devices = getOutputDevices();
    return devices.empty() ? AudioDevice{} : devices[0];
}

bool DeviceManager::openStream(const AudioStreamConfig& config, AudioCallback callback) {
    return pImpl->openStream(config, callback);
}

bool DeviceManager::startStream() {
    return pImpl->startStream();
}

bool DeviceManager::stopStream() {
    return pImpl->stopStream();
}

bool DeviceManager::closeStream() {
    return pImpl->closeStream();
}

bool DeviceManager::isStreamRunning() const {
    return pImpl->isStreamRunning();
}

double DeviceManager::getCpuLoad() const {
    return pImpl->getCpuLoad();
}

uint32_t DeviceManager::getXruns() const {
    return pImpl->getXruns();
}

double DeviceManager::getStreamLatency() const {
    return 2.67; // Default latency estimate
}

uint32_t DeviceManager::getCurrentBufferSize() const {
    return 128; // Default buffer size
}

uint32_t DeviceManager::getCurrentSampleRate() const {
    return 48000; // Default sample rate
}

AudioDevice DeviceManager::getDefaultInputDevice() const {
    return AudioDevice{}; // Empty device for now
}

void DeviceManager::setCallback(AudioCallback callback) {
    // TODO: Implement
}

bool DeviceManager::setBufferSize(uint32_t bufferSize) {
    return true; // TODO: Implement
}

bool DeviceManager::setSampleRate(uint32_t sampleRate) {
    return true; // TODO: Implement
}

void DeviceManager::clearErrors() {
    // TODO: Implement
}

bool DeviceManager::isStreamActive() const {
    return pImpl->isStreamRunning();
}

} // namespace io
} // namespace dawg
