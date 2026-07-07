#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <sstream>
#include <cctype>
#include <algorithm>

namespace nexus::utility::graphics {

/**
 * @brief Parses shader compiler diagnostic output (GLSL and HLSL styles).
 *
 * Supported formats:
 *   GLSL: "ERROR: 0:42: 'foo' : undeclared identifier"
 *         "WARNING: 0:12: implicit cast ..."
 *   HLSL: "shader.hlsl(42,10): error X3000: syntax error"
 *         "shader.hlsl(12,5): warning X3206: implicit truncation"
 */
class ShaderCompilerErrorParser {
public:
    struct ShaderError {
        std::size_t line = 0;
        std::size_t column = 0;
        std::string severity;    // "error" or "warning"
        std::string message;
        std::string sourceFile;
    };

    ShaderCompilerErrorParser() = default;

    /**
     * @brief Parses a full compiler log into individual diagnostics.
     *        Results are cached and also returned.
     */
    std::vector<ShaderError> parseErrors(const std::string& compilerOutput) {
        errors_.clear();
        std::istringstream stream(compilerOutput);
        std::string line;
        while (std::getline(stream, line)) {
            const std::string trimmed = trim(line);
            if (trimmed.empty()) continue;
            ShaderError err;
            if (parseHlslLine(trimmed, err) || parseGlslLine(trimmed, err)) {
                errors_.push_back(std::move(err));
            }
        }
        return errors_;
    }

    const std::vector<ShaderError>& getErrors() const { return errors_; }

    std::size_t getErrorCount() const {
        return count("error");
    }

    std::size_t getWarningCount() const {
        return count("warning");
    }

    bool hasErrors() const {
        return getErrorCount() > 0;
    }

    /**
     * @brief Returns cached diagnostics that reference the given source file.
     */
    std::vector<ShaderError> filterByFile(const std::string& filename) const {
        std::vector<ShaderError> out;
        for (const auto& e : errors_) {
            if (e.sourceFile == filename) out.push_back(e);
        }
        return out;
    }

    void clear() { errors_.clear(); }

private:
    std::size_t count(const std::string& sev) const {
        std::size_t n = 0;
        for (const auto& e : errors_) if (e.severity == sev) ++n;
        return n;
    }

    static std::string trim(const std::string& s) {
        std::size_t a = 0, b = s.size();
        while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
        return s.substr(a, b - a);
    }

    static std::string toLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    // HLSL: file.hlsl(LINE,COL): error X3000: message
    static bool parseHlslLine(const std::string& line, ShaderError& err) {
        const std::size_t open = line.find('(');
        if (open == std::string::npos || open == 0) return false;
        const std::size_t comma = line.find(',', open);
        if (comma == std::string::npos) return false;
        const std::size_t close = line.find(')', comma);
        if (close == std::string::npos) return false;
        const std::size_t colon = line.find(':', close);
        if (colon == std::string::npos) return false;

        const std::string file = line.substr(0, open);
        const std::string lineStr = line.substr(open + 1, comma - open - 1);
        const std::string colStr = line.substr(comma + 1, close - comma - 1);
        if (!isNumber(lineStr) || !isNumber(colStr)) return false;

        std::string rest = trim(line.substr(colon + 1));
        const std::string sev = leadingSeverity(rest);
        if (sev.empty()) return false;

        err.sourceFile = trim(file);
        err.line = toSize(lineStr);
        err.column = toSize(colStr);
        err.severity = sev;
        err.message = rest; // remainder after severity token stripped by leadingSeverity
        return true;
    }

    // GLSL: SEVERITY: FILE:LINE: message  (column omitted)
    static bool parseGlslLine(const std::string& line, ShaderError& err) {
        const std::size_t sevEnd = line.find(':');
        if (sevEnd == std::string::npos) return false;
        std::string sevTok = toLower(trim(line.substr(0, sevEnd)));
        if (sevTok != "error" && sevTok != "warning") return false;

        // After "SEVERITY:" expect "FILE:LINE:" where FILE is often "0".
        std::string rest = trim(line.substr(sevEnd + 1));
        const std::size_t fileEnd = rest.find(':');
        if (fileEnd == std::string::npos) return false;
        const std::string file = trim(rest.substr(0, fileEnd));

        std::string afterFile = rest.substr(fileEnd + 1);
        const std::size_t lineEnd = afterFile.find(':');
        if (lineEnd == std::string::npos) return false;
        const std::string lineStr = trim(afterFile.substr(0, lineEnd));
        if (!isNumber(lineStr)) return false;

        err.severity = sevTok;
        err.sourceFile = file;
        err.line = toSize(lineStr);
        err.column = 0;
        err.message = trim(afterFile.substr(lineEnd + 1));
        return true;
    }

    // Reads a leading "error XNNNN" or "warning" token, strips it from rest,
    // and returns the normalized severity (or empty if none present).
    static std::string leadingSeverity(std::string& rest) {
        const std::string low = toLower(rest);
        std::string sev;
        std::size_t skip = 0;
        if (low.rfind("error", 0) == 0) { sev = "error"; skip = 5; }
        else if (low.rfind("warning", 0) == 0) { sev = "warning"; skip = 7; }
        else return {};

        std::string tail = trim(rest.substr(skip));
        // Drop an optional error code token (e.g. "X3000") up to the next colon.
        const std::size_t colon = tail.find(':');
        if (colon != std::string::npos) {
            tail = trim(tail.substr(colon + 1));
        }
        rest = tail;
        return sev;
    }

    static bool isNumber(const std::string& s) {
        if (s.empty()) return false;
        return std::all_of(s.begin(), s.end(),
                           [](unsigned char c) { return std::isdigit(c) != 0; });
    }

    static std::size_t toSize(const std::string& s) {
        std::size_t v = 0;
        for (char c : s) v = v * 10 + static_cast<std::size_t>(c - '0');
        return v;
    }

    std::vector<ShaderError> errors_;
};

} // namespace nexus::utility::graphics
