#pragma once

#include <string>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <ostream>

namespace nexus::utility::graphics {

/// @brief OpenGL error checking utility
class OpenGLErrorChecker {
public:
    /// Check for OpenGL errors and log them
    static bool checkErrors(const char* file, int line, const char* function);

    /// Get string description of OpenGL error code
    static std::string errorCodeToString(unsigned int error_code);
};

/// @brief Validate framebuffer completeness
class FramebufferValidator {
public:
    /// Check if framebuffer is complete
    static bool isComplete(unsigned int framebuffer_id);

    /// Get detailed status message
    static std::string getStatusMessage(unsigned int framebuffer_id);
};

/// @brief Track GPU resource allocations
class GPUResourceTracker {
public:
    enum class ResourceType {
        Texture,
        Buffer,
        Shader,
        Program,
        Framebuffer,
        Renderbuffer,
        VertexArray
    };

    void recordAllocation(ResourceType type, unsigned int id, size_t size_bytes,
                         const std::string& name = "");
    void recordDeallocation(ResourceType type, unsigned int id);

    struct ResourceStats {
        size_t count = 0;
        size_t total_bytes = 0;
    };

    ResourceStats getStats(ResourceType type) const;
    void printReport(std::ostream& os) const;

    /// Detect leaks (resources not freed)
    std::vector<std::pair<ResourceType, unsigned int>> detectLeaks() const;

private:
    struct ResourceInfo {
        ResourceType type;
        unsigned int id;
        size_t size_bytes;
        std::string name;
    };

    mutable std::mutex mutex_;
    std::unordered_map<unsigned int, ResourceInfo> resources_;
};

/// @brief Shader compilation error parser
class ShaderCompilerErrorParser {
public:
    struct ShaderError {
        int line_number = -1;
        std::string error_message;
        std::string source_line;
    };

    /// Parse shader compiler error log
    static std::vector<ShaderError> parseErrorLog(const std::string& log,
                                                  const std::string& source_code = "");

    /// Pretty print shader errors
    static void printErrors(const std::vector<ShaderError>& errors, std::ostream& os);
};

/// @brief Pipeline state dumper
class PipelineStateDumper {
public:
    /// Dump current OpenGL state
    static void dumpState(std::ostream& os);

    /// Dump specific state categories
    static void dumpBlendState(std::ostream& os);
    static void dumpDepthState(std::ostream& os);
    static void dumpStencilState(std::ostream& os);
    static void dumpRasterizerState(std::ostream& os);
};

} // namespace nexus::utility::graphics

/// @brief Convenient macro for OpenGL error checking
#ifdef NEXUS_DEBUG_OPENGL
    #define NEXUS_GL_CHECK() \
        nexus::utility::graphics::OpenGLErrorChecker::checkErrors(__FILE__, __LINE__, __FUNCTION__)
#else
    #define NEXUS_GL_CHECK() ((void)0)
#endif
