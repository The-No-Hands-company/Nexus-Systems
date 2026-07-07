#pragma once
#include <string>
#include <vector>
namespace nexus::utility::graphics {
class VulkanValidationWrapper {
public:
    struct VkMessage { std::string layer; int severity; std::string message; };
    static VulkanValidationWrapper& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordMessage(const std::string& layer, int severity, const std::string& msg);
    std::vector<VkMessage> getMessages() const;
    std::vector<VkMessage> getErrors() const;
    std::vector<VkMessage> getWarnings() const;
    size_t getMessageCount() const;
    void clear();
private:
    VulkanValidationWrapper()=default; ~VulkanValidationWrapper()=default; bool enabled_=false;
    std::vector<VkMessage> messages_;
};
} // namespace nexus::utility::graphics
