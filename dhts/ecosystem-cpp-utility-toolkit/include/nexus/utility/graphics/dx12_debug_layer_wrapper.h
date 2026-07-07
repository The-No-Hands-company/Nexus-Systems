#pragma once
#include <string>
#include <vector>
namespace nexus::utility::graphics {
class Dx12DebugLayerWrapper {
public:
    struct Dx12Message { std::string category; int severity; std::string message; };
    static Dx12DebugLayerWrapper& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordMessage(const std::string& cat, int sev, const std::string& msg);
    std::vector<Dx12Message> getMessages() const;
    size_t getMessageCount() const;
    size_t getErrorCount() const;
    void clear();
private:
    Dx12DebugLayerWrapper()=default; ~Dx12DebugLayerWrapper()=default; bool enabled_=false;
    std::vector<Dx12Message> messages_;
};
} // namespace nexus::utility::graphics