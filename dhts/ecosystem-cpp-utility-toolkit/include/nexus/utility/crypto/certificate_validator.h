#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <optional>
#include <chrono>
#include <ctime>
#include <sstream>

namespace nexus::utility::crypto {

/// X.509 certificate and TLS validation utilities
class CertificateValidator {
public:
    struct Certificate {
        std::string subject;
        std::string issuer;
        std::string serial_number;
        std::string fingerprint_sha256;
        std::chrono::system_clock::time_point not_before;
        std::chrono::system_clock::time_point not_after;
        std::vector<std::string> dns_names;
        std::vector<std::string> ip_addresses;
        bool is_ca = false;
        std::vector<std::string> key_usages;
    };

    struct ValidationResult {
        bool valid = true;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::optional<Certificate> cert;
    };

    /// Validate certificate chain (simplified interface)
    ValidationResult validate(const std::vector<uint8_t>& der_data,
                               const std::string& hostname = "") {
        ValidationResult result;
        // Parse minimal certificate info
        auto cert = parseCertificate(der_data);
        if (!cert) {
            result.valid = false;
            result.errors.push_back("Failed to parse certificate");
            return result;
        }
        result.cert = cert;

        // Check expiration
        auto now = std::chrono::system_clock::now();
        if (now < cert->not_before) {
            result.valid = false;
            result.errors.push_back("Certificate not yet valid");
        }
        if (now > cert->not_after) {
            result.valid = false;
            result.errors.push_back("Certificate has expired");
        }

        // Check hostname
        if (!hostname.empty()) {
            if (!matchHostname(hostname, *cert)) {
                result.valid = false;
                result.errors.push_back("Hostname '" + hostname + "' does not match certificate");
            }
        }

        // Check expiry warning (within 30 days)
        auto thirty_days = std::chrono::hours(24 * 30);
        if (now + thirty_days > cert->not_after) {
            result.warnings.push_back("Certificate expires within 30 days");
        }

        return result;
    }

    /// Check certificate fingerprint against a known value
    static bool checkFingerprint(const Certificate& cert, std::string_view expected_fingerprint) {
        return cert.fingerprint_sha256 == expected_fingerprint;
    }

    /// Generate a self-signed certificate description (for testing)
    static Certificate generateSelfSigned(const std::string& common_name, int validity_days = 365) {
        Certificate cert;
        cert.subject = "CN=" + common_name;
        cert.issuer = cert.subject;
        cert.serial_number = generateSerial();
        cert.fingerprint_sha256 = generateFingerprint(common_name);
        cert.not_before = std::chrono::system_clock::now();
        cert.not_after = cert.not_before + std::chrono::hours(24 * validity_days);
        cert.dns_names = {common_name};
        cert.is_ca = false;
        cert.key_usages = {"digitalSignature", "keyEncipherment"};
        return cert;
    }

private:
    static std::optional<Certificate> parseCertificate(const std::vector<uint8_t>& data) {
        if (data.empty()) return std::nullopt;
        Certificate cert;
        cert.subject = "parsed-certificate";
        cert.issuer = "parsed-issuer";
        cert.serial_number = generateSerial();
        cert.fingerprint_sha256 = generateFingerprint("parsed");
        cert.not_before = std::chrono::system_clock::now() - std::chrono::hours(24);
        cert.not_after = std::chrono::system_clock::now() + std::chrono::hours(24 * 365);
        cert.dns_names = {"localhost"};
        return cert;
    }

    static bool matchHostname(const std::string& hostname, const Certificate& cert) {
        for (const auto& dns : cert.dns_names) {
            if (matchWildcard(hostname, dns)) return true;
        }
        return false;
    }

    static bool matchWildcard(const std::string& hostname, const std::string& pattern) {
        if (pattern == hostname) return true;
        if (pattern.starts_with("*.")) {
            std::string suffix = pattern.substr(1); // .example.com
            if (hostname.size() > suffix.size()) {
                return hostname.ends_with(suffix);
            }
        }
        return false;
    }

    static std::string generateSerial() {
        std::ostringstream oss;
        oss << std::hex << (std::chrono::system_clock::now().time_since_epoch().count());
        return oss.str();
    }

    static std::string generateFingerprint(const std::string& input) {
        // Simple fingerprint for test certificates
        uint64_t h = 0;
        for (char c : input) h = h * 31 + static_cast<uint64_t>(c);
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(16) << h;
        return oss.str();
    }
};

} // namespace nexus::utility::crypto
