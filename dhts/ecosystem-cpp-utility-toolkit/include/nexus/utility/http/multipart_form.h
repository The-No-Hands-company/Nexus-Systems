#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <sstream>
#include <cstdint>
#include <random>

namespace nexus::utility::http {

/// Multipart form data builder for HTTP file uploads
class MultipartForm {
public:
    struct Part {
        std::string name;
        std::string filename;
        std::string content_type;
        std::vector<uint8_t> data;
    };

    MultipartForm() : boundary_(generateBoundary()) {}

    /// Add a text field
    void addField(const std::string& name, const std::string& value) {
        Part part;
        part.name = name;
        part.content_type = "text/plain";
        part.data.assign(value.begin(), value.end());
        parts_.push_back(std::move(part));
    }

    /// Add a file upload
    void addFile(const std::string& field_name, const std::string& filename,
                 const std::string& content_type, const std::vector<uint8_t>& data) {
        Part part;
        part.name = field_name;
        part.filename = filename;
        part.content_type = content_type;
        part.data = data;
        parts_.push_back(std::move(part));
    }

    /// Build the complete multipart body
    std::string build() const {
        std::ostringstream oss;

        for (const auto& part : parts_) {
            oss << "--" << boundary_ << "\r\n";
            oss << "Content-Disposition: form-data; name=\"" << part.name << "\"";
            if (!part.filename.empty()) {
                oss << "; filename=\"" << part.filename << "\"";
            }
            oss << "\r\n";
            if (!part.content_type.empty()) {
                oss << "Content-Type: " << part.content_type << "\r\n";
            }
            oss << "\r\n";
            oss.write(reinterpret_cast<const char*>(part.data.data()), part.data.size());
            oss << "\r\n";
        }

        oss << "--" << boundary_ << "--\r\n";
        return oss.str();
    }

    /// Get the boundary string for the Content-Type header
    const std::string& boundary() const { return boundary_; }

    /// Get Content-Type header value
    std::string contentType() const {
        return "multipart/form-data; boundary=" + boundary_;
    }

    /// Get total content length
    size_t contentLength() const {
        return build().size();
    }

    /// Number of parts
    size_t partCount() const { return parts_.size(); }

private:
    std::string boundary_;
    std::vector<Part> parts_;

    static std::string generateBoundary() {
        static const char* chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::string boundary = "----NexusFormBoundary";
        for (int i = 0; i < 16; ++i) {
            boundary += chars[(i * 1234567 + i * i) % 62];
        }
        return boundary;
    }
};

} // namespace nexus::utility::http
