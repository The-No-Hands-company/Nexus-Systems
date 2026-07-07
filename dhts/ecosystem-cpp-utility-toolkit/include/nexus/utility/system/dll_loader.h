#pragma once

#include <string>
#include <dlfcn.h>
#include <stdexcept>
#include <memory>

namespace nexus::utility::system {

/**
 * @brief RAII-based dynamic library loader.
 */
class DllLoader {
public:
    /**
     * @brief Loads a shared library.
     */
    explicit DllLoader(const std::string& path, int flags = RTLD_LAZY) {
        handle_ = dlopen(path.c_str(), flags);
        if (!handle_) {
            throw std::runtime_error("Failed to load library: " + std::string(dlerror()));
        }
    }

    ~DllLoader() {
        if (handle_) {
            dlclose(handle_);
        }
    }

    // Non-copyable, movable
    DllLoader(const DllLoader&) = delete;
    DllLoader& operator=(const DllLoader&) = delete;
    
    DllLoader(DllLoader&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    DllLoader& operator=(DllLoader&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                dlclose(handle_);
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief Resolves a symbol with type safety.
     */
    template<typename T>
    T* getSymbol(const std::string& name) {
        // Clear any existing error
        dlerror();
        
        void* symbol = dlsym(handle_, name.c_str());
        const char* error = dlerror();
        
        if (error) {
            throw std::runtime_error("Failed to resolve symbol '" + name + "': " + error);
        }
        
        return reinterpret_cast<T*>(symbol);
    }

    /**
     * @brief Checks if a symbol exists.
     */
    bool hasSymbol(const std::string& name) const {
        dlerror(); // Clear error
        void* symbol = dlsym(handle_, name.c_str());
        return dlerror() == nullptr;
    }

    /**
     * @brief Gets the raw handle.
     */
    void* handle() const { return handle_; }

private:
    void* handle_ = nullptr;
};

} // namespace nexus::utility::system
