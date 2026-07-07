#pragma once
#include <string>
#include <vector>
#include <unordered_set>

namespace nexus::utility::cpp26 {

class TextEncodingValidator {
public:
    struct EncodingInfo {
        std::string name;
        int mibEnum;
        bool isRegistered;
    };

    bool registerEncoding(const std::string& name, int mib) {
        if (registered_.count(name)) return false;
        registered_.insert(name);
        encodings_.push_back({name, mib, true});
        return true;
    }

    bool isKnownEncoding(const std::string& name) const {
        return registered_.count(name) > 0;
    }

    std::vector<EncodingInfo> getRegisteredEncodings() const {
        return encodings_;
    }

    size_t countEncodings() const { return encodings_.size(); }

    void clear() { encodings_.clear(); registered_.clear(); }

private:
    std::vector<EncodingInfo> encodings_;
    std::unordered_set<std::string> registered_;
};

} // namespace nexus::utility::cpp26
