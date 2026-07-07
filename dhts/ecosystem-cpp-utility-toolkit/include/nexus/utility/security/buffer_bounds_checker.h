#pragma once

#include <string>
#include <cstring>
#include <stdexcept>

namespace nexus::utility::security {

/// @brief Runtime buffer bounds checking
class BufferBoundsChecker {
public:
    /// Check if access is within bounds
    static void checkBounds(const void* buffer, size_t buffer_size,
                           size_t offset, size_t access_size);

    /// Checked memory copy
    static void checkedMemcpy(void* dest, size_t dest_size,
                             const void* src, size_t src_size);

    /// Checked string copy
    static void checkedStrcpy(char* dest, size_t dest_size, const char* src);
};

/// @brief Validate file paths for traversal attacks
class PathTraversalValidator {
public:
    /// Check if path contains traversal patterns
    static bool isPathSafe(const std::string& path);

    /// Resolve to canonical path and validate
    static std::string sanitizePath(const std::string& path);

    /// Check if path escapes base directory
    static bool isWithinBaseDir(const std::string& path, const std::string& base_dir);
};

/// @brief Prevent secrets from appearing in logs/dumps
class SecretLeakDetector {
public:
    /// Register a secret pattern (regex)
    static void registerSecretPattern(const std::string& pattern_name,
                                     const std::string& regex);

    /// Scan text for secrets
    static bool containsSecrets(const std::string& text);

    /// Redact secrets from text
    static std::string redactSecrets(const std::string& text);
};

/// @brief Secure memory wiping
class SecureWipe {
public:
    /// Securely zero memory (prevents optimization)
    static void wipe(void* ptr, size_t size);

    /// Securely wipe and free
    template<typename T>
    static void wipeAndDelete(T* ptr) {
        if (ptr) {
            wipe(ptr, sizeof(T));
            delete ptr;
        }
    }
};

} // namespace nexus::utility::security
