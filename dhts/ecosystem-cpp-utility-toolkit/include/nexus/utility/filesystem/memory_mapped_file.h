#pragma once

#include <string>
#include <span>
#include <cstddef>
#include <stdexcept>
#include <system_error>
#include <filesystem>

// POSIX only for this implementation phase
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#else
    #error "MemoryMappedFile currently supports POSIX only"
#endif

namespace nexus::utility::filesystem {

enum class MapMode {
    ReadOnly,
    ReadWrite,
    Private // Copy-on-write
};

/**
 * @brief RAII wrapper for memory mapped files.
 */
class MemoryMappedFile {
public:
    MemoryMappedFile(const std::filesystem::path& path, MapMode mode = MapMode::ReadOnly) {
        int flags = O_RDONLY;
        if (mode == MapMode::ReadWrite) flags = O_RDWR;
        
        fd_ = open(path.c_str(), flags);
        if (fd_ == -1) {
            throw std::system_error(errno, std::generic_category(), "Failed to open file for mmap: " + path.string());
        }

        struct stat sb;
        if (fstat(fd_, &sb) == -1) {
            close(fd_);
            throw std::system_error(errno, std::generic_category(), "Failed to stat file: " + path.string());
        }
        
        size_ = static_cast<size_t>(sb.st_size);

        // Handle empty file
        if (size_ == 0) {
            mapped_data_ = nullptr;
            return;
        }

        int prot = PROT_READ;
        if (mode == MapMode::ReadWrite) prot |= PROT_WRITE;

        int map_flags = MAP_SHARED;
        if (mode == MapMode::Private) {
            map_flags = MAP_PRIVATE;
            // Private mapping might need W permission if we want to modify the copy
            if (mode == MapMode::Private) prot |= PROT_WRITE; 
        }

        mapped_data_ = mmap(nullptr, size_, prot, map_flags, fd_, 0);
        if (mapped_data_ == MAP_FAILED) {
            close(fd_);
            throw std::system_error(errno, std::generic_category(), "Failed to mmap file");
        }
    }

    ~MemoryMappedFile() {
        if (mapped_data_ && size_ > 0) {
            munmap(mapped_data_, size_);
        }
        if (fd_ != -1) {
            close(fd_);
        }
    }

    MemoryMappedFile(const MemoryMappedFile&) = delete;
    MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;

    MemoryMappedFile(MemoryMappedFile&& other) noexcept 
        : fd_(other.fd_), size_(other.size_), mapped_data_(other.mapped_data_) {
        other.fd_ = -1;
        other.size_ = 0;
        other.mapped_data_ = nullptr;
    }

    MemoryMappedFile& operator=(MemoryMappedFile&& other) noexcept {
        if (this != &other) {
            if (mapped_data_ && size_ > 0) munmap(mapped_data_, size_);
            if (fd_ != -1) close(fd_);
            
            fd_ = other.fd_;
            size_ = other.size_;
            mapped_data_ = other.mapped_data_;
            
            other.fd_ = -1;
            other.size_ = 0;
            other.mapped_data_ = nullptr;
        }
        return *this;
    }

    void* data() const { return mapped_data_; }
    size_t size() const { return size_; }

    [[nodiscard]] std::span<std::byte> as_bytes() const {
        if (!mapped_data_) return {};
        return std::span<std::byte>(static_cast<std::byte*>(mapped_data_), size_);
    }

    [[nodiscard]] std::span<char> as_chars() const {
        if (!mapped_data_) return {};
        return std::span<char>(static_cast<char*>(mapped_data_), size_);
    }

    // Helper to sync to disk (msync)
    void flush() {
        if (mapped_data_ && size_ > 0) {
            if (msync(mapped_data_, size_, MS_SYNC) == -1) {
                throw std::system_error(errno, std::generic_category(), "Failed to msync");
            }
        }
    }

private:
    int fd_ = -1;
    size_t size_ = 0;
    void* mapped_data_ = nullptr;
};

} // namespace nexus::utility::filesystem
