#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace nexus::utility::security {

class SecureWipe {
public:
    struct WipeRecord { void* address; size_t size; std::string context; };

    static SecureWipe& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    static void wipe(void* ptr, size_t size);
    static void wipeString(std::string& str);
    template<typename T> static void wipeObject(T& obj) { wipe(&obj, sizeof(T)); }

    void recordWipe(void* addr, size_t size, const std::string& ctx = "");
    std::vector<WipeRecord> getHistory() const;
    size_t getWipeCount() const;
    size_t getTotalBytesWiped() const;
    void clear();

private:
    SecureWipe() = default;
    ~SecureWipe() = default;
    bool enabled_ = false;
    std::vector<WipeRecord> history_;
    size_t totalBytes_ = 0;
};
} // namespace nexus::utility::security
