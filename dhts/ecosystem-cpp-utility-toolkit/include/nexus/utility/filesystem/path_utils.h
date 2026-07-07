#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <optional>
#include <string_view>

namespace nexus::utility::filesystem {

namespace fs = std::filesystem;

class PathUtils {
public:
    /**
     * @brief Normalizes a path string.
     */
    static std::string normalize(const fs::path& path) {
        return fs::weakly_canonical(path).string();
    }

    /**
     * @brief Joins multiple path segments.
     */
    template<typename... Args>
    static fs::path join(const Args&... args) {
        fs::path result;
        ((result /= args), ...);
        return result;
    }

    /**
     * @brief Checks if a path exists.
     */
    static bool exists(const fs::path& path) {
        std::error_code ec;
        return fs::exists(path, ec);
    }

    /**
     * @brief Checks if a path is a regular file.
     */
    static bool isFile(const fs::path& path) {
        std::error_code ec;
        return fs::is_regular_file(path, ec);
    }

    /**
     * @brief Checks if a path is a directory.
     */
    static bool isDirectory(const fs::path& path) {
        std::error_code ec;
        return fs::is_directory(path, ec);
    }

    /**
     * @brief Gets the extension of a file (check if file exists optional).
     */
    static std::string getExtension(const fs::path& path) {
        return path.extension().string();
    }

    /**
     * @brief Replaces the extension of a path.
     */
    static fs::path replaceExtension(const fs::path& path, const std::string& newExt) {
        fs::path p = path;
        return p.replace_extension(newExt);
    }
};

} // namespace nexus::utility::filesystem
