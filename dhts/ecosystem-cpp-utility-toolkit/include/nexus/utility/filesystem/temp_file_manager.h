#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <random>
#include <vector>
#include <stdexcept>

namespace nexus::utility::filesystem {

namespace fs = std::filesystem;

/**
 * @brief Manages temporary files and directories with RAII cleanup.
 * NOT thread safe for the same instance.
 */
class TempFileManager {
public:
    TempFileManager() = default;

    /**
     * @brief Cleanup all tracked temporary files/dirs on destruction.
     */
    ~TempFileManager() {
        cleanup();
    }

    /**
     * @brief Creates a unique temporary file.
     * @return Path to the created file.
     */
    fs::path createTempFile(const std::string& extension = ".tmp") {
        fs::path tempDir = fs::temp_directory_path();
        fs::path path;
        
        // Simple retry mechanism for uniqueness
        for (int i = 0; i < 100; ++i) {
            path = tempDir / ("nexus_tmp_" + randomString(8) + extension);
            if (!fs::exists(path)) {
                // Touch the file to reserve it and ensure creation
                std::ofstream ofs(path);
                if (ofs.is_open()) {
                    ofs.close();
                    tracked_files_.push_back(path);
                    return path;
                }
            }
        }
        throw std::runtime_error("Failed to create unique temporary file");
    }

    /**
     * @brief Creates a unique temporary directory.
     * @return Path to the created directory.
     */
    fs::path createTempDirectory() {
        fs::path tempDir = fs::temp_directory_path();
        fs::path path;
        
        for (int i = 0; i < 100; ++i) {
            path = tempDir / ("nexus_tmp_dir_" + randomString(8));
            if (!fs::exists(path)) {
                if (fs::create_directory(path)) {
                    tracked_dirs_.push_back(path);
                    return path;
                }
            }
        }
        throw std::runtime_error("Failed to create unique temporary directory");
    }

    /**
     * @brief Manually cleanup resources.
     */
    void cleanup() {
        for (const auto& file : tracked_files_) {
            std::error_code ec;
            fs::remove(file, ec); // Ignore errors
        }
        tracked_files_.clear();

        for (const auto& dir : tracked_dirs_) {
            std::error_code ec;
            fs::remove_all(dir, ec); // Recursive remove
        }
        tracked_dirs_.clear();
    }

    // Move-only
    TempFileManager(const TempFileManager&) = delete;
    TempFileManager& operator=(const TempFileManager&) = delete;

    TempFileManager(TempFileManager&& other) noexcept 
        : tracked_files_(std::move(other.tracked_files_)), 
          tracked_dirs_(std::move(other.tracked_dirs_)) {}

    TempFileManager& operator=(TempFileManager&& other) noexcept {
        if (this != &other) {
            cleanup();
            tracked_files_ = std::move(other.tracked_files_);
            tracked_dirs_ = std::move(other.tracked_dirs_);
        }
        return *this;
    }

private:
    std::vector<fs::path> tracked_files_;
    std::vector<fs::path> tracked_dirs_;

    std::string randomString(size_t length) {
        static const char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        static thread_local std::mt19937 rg{std::random_device{}()};
        static thread_local std::uniform_int_distribution<size_t> pick(0, sizeof(charset) - 2);

        std::string s;
        s.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            s += charset[pick(rg)];
        }
        return s;
    }
};

} // namespace nexus::utility::filesystem
