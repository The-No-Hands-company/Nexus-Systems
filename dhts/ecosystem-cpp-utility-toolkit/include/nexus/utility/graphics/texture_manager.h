#pragma once

#include <nexus/utility/graphics/image_loader_wrapper.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <mutex>

namespace nexus::utility::graphics {

/**
 * @brief Texture resource manager with caching and reference counting.
 */
class TextureManager {
public:
    using TextureID = size_t;
    
    struct Texture {
        TextureID id;
        ImageLoaderWrapper::ImageData data;
        std::string name;
        size_t referenceCount = 0;
    };

    /**
     * @brief Loads or retrieves cached texture.
     */
    static TextureID load(const std::string& filename) {
        std::lock_guard lock(mutex_);
        
        // Check if already loaded
        auto it = nameToId_.find(filename);
        if (it != nameToId_.end()) {
            textures_[it->second].referenceCount++;
            return it->second;
        }
        
        // Load new texture
        auto data = ImageLoaderWrapper::loadFromFile(filename);
        if (!data.isValid()) {
            return 0; // Invalid texture ID
        }
        
        TextureID id = nextId_++;
        Texture tex;
        tex.id = id;
        tex.data = std::move(data);
        tex.name = filename;
        tex.referenceCount = 1;
        
        textures_[id] = std::move(tex);
        nameToId_[filename] = id;
        
        return id;
    }

    /**
     * @brief Gets texture by ID.
     */
    static const Texture* get(TextureID id) {
        std::lock_guard lock(mutex_);
        auto it = textures_.find(id);
        return (it != textures_.end()) ? &it->second : nullptr;
    }

    /**
     * @brief Releases texture reference.
     */
    static void release(TextureID id) {
        std::lock_guard lock(mutex_);
        auto it = textures_.find(id);
        if (it != textures_.end()) {
            it->second.referenceCount--;
            if (it->second.referenceCount == 0) {
                nameToId_.erase(it->second.name);
                textures_.erase(it);
            }
        }
    }

    /**
     * @brief Gets reference count.
     */
    static size_t getReferenceCount(TextureID id) {
        std::lock_guard lock(mutex_);
        auto it = textures_.find(id);
        return (it != textures_.end()) ? it->second.referenceCount : 0;
    }

    /**
     * @brief Clears all textures.
     */
    static void clear() {
        std::lock_guard lock(mutex_);
        textures_.clear();
        nameToId_.clear();
    }

    /**
     * @brief Gets total loaded texture count.
     */
    static size_t getLoadedCount() {
        std::lock_guard lock(mutex_);
        return textures_.size();
    }

    /**
     * @brief RAII texture handle.
     */
    class TextureHandle {
    public:
        explicit TextureHandle(TextureID id = 0) : id_(id) {}
        
        ~TextureHandle() {
            if (id_ != 0) {
                release(id_);
            }
        }

        // Non-copyable, movable
        TextureHandle(const TextureHandle&) = delete;
        TextureHandle& operator=(const TextureHandle&) = delete;

        TextureHandle(TextureHandle&& other) noexcept : id_(other.id_) {
            other.id_ = 0;
        }

        TextureHandle& operator=(TextureHandle&& other) noexcept {
            if (this != &other) {
                if (id_ != 0) release(id_);
                id_ = other.id_;
                other.id_ = 0;
            }
            return *this;
        }

        TextureID id() const { return id_; }
        const Texture* get() const { return TextureManager::get(id_); }

    private:
        TextureID id_;
    };

private:
    static inline std::mutex mutex_;
    static inline std::unordered_map<TextureID, Texture> textures_;
    static inline std::unordered_map<std::string, TextureID> nameToId_;
    static inline TextureID nextId_ = 1;
};

} // namespace nexus::utility::graphics
