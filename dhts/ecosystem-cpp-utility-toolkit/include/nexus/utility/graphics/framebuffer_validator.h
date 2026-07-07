#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <sstream>
#include <algorithm>

namespace nexus::utility::graphics {

/**
 * @brief Validates framebuffer attachment configuration before creation.
 *
 * Checks that all color/depth/stencil attachments share matching dimensions
 * and surfaces actionable fix recommendations for common mistakes.
 */
class FramebufferValidator {
public:
    struct Attachment {
        std::string kind;    // "color", "depth", "stencil"
        int slot = -1;       // color slot, -1 for depth/stencil
        int width = 0;
        int height = 0;
        std::string format;
    };

    FramebufferValidator() = default;

    void addColorAttachment(int slot, int width, int height, const std::string& format) {
        // Replace any existing attachment on the same slot.
        for (auto& a : attachments_) {
            if (a.kind == "color" && a.slot == slot) {
                a.width = width; a.height = height; a.format = format;
                return;
            }
        }
        attachments_.push_back({"color", slot, width, height, format});
    }

    void setDepthAttachment(int width, int height, const std::string& format) {
        setSingular("depth", width, height, format);
    }

    void setStencilAttachment(int width, int height, const std::string& format) {
        setSingular("stencil", width, height, format);
    }

    /**
     * @brief Returns true if the framebuffer is valid (all dims match, >=1 attachment).
     */
    bool validate() {
        errors_.clear();

        if (attachments_.empty()) {
            errors_.push_back("No attachments defined; a framebuffer needs at least one.");
            return false;
        }

        bool hasColor = false;
        int refW = 0, refH = 0;
        bool haveRef = false;
        for (const auto& a : attachments_) {
            if (a.kind == "color") hasColor = true;

            if (a.width <= 0 || a.height <= 0) {
                std::ostringstream os;
                os << describe(a) << " has invalid dimensions "
                   << a.width << "x" << a.height << ".";
                errors_.push_back(os.str());
            }
            if (a.format.empty()) {
                errors_.push_back(describe(a) + " has an empty format.");
            }

            if (!haveRef) { refW = a.width; refH = a.height; haveRef = true; }
            else if (a.width != refW || a.height != refH) {
                std::ostringstream os;
                os << describe(a) << " dimensions " << a.width << "x" << a.height
                   << " do not match " << refW << "x" << refH << ".";
                errors_.push_back(os.str());
            }
        }

        int colorSlots = countColorSlots();
        int uniqueSlots = countUniqueColorSlots();
        if (colorSlots != uniqueSlots) {
            errors_.push_back("Duplicate color attachment slot detected.");
        }
        (void)hasColor;

        return errors_.empty();
    }

    const std::vector<std::string>& getErrors() const { return errors_; }

    /**
     * @brief Human-readable suggestions to resolve current validation errors.
     */
    std::vector<std::string> recommendFixes() const {
        std::vector<std::string> fixes;
        for (const auto& e : errors_) {
            if (e.find("do not match") != std::string::npos) {
                fixes.push_back("Resize all attachments to identical width/height.");
            } else if (e.find("invalid dimensions") != std::string::npos) {
                fixes.push_back("Set positive, non-zero width and height for every attachment.");
            } else if (e.find("empty format") != std::string::npos) {
                fixes.push_back("Assign a valid pixel format (e.g. RGBA8, DEPTH24_STENCIL8).");
            } else if (e.find("No attachments") != std::string::npos) {
                fixes.push_back("Add at least one color attachment before validating.");
            } else if (e.find("Duplicate") != std::string::npos) {
                fixes.push_back("Assign each color attachment a unique slot index.");
            }
        }
        std::sort(fixes.begin(), fixes.end());
        fixes.erase(std::unique(fixes.begin(), fixes.end()), fixes.end());
        return fixes;
    }

    std::size_t getAttachmentCount() const { return attachments_.size(); }

    void clear() {
        attachments_.clear();
        errors_.clear();
    }

private:
    void setSingular(const std::string& kind, int width, int height, const std::string& format) {
        for (auto& a : attachments_) {
            if (a.kind == kind) {
                a.width = width; a.height = height; a.format = format;
                return;
            }
        }
        attachments_.push_back({kind, -1, width, height, format});
    }

    int countColorSlots() const {
        int n = 0;
        for (const auto& a : attachments_) if (a.kind == "color") ++n;
        return n;
    }

    int countUniqueColorSlots() const {
        std::vector<int> slots;
        for (const auto& a : attachments_) if (a.kind == "color") slots.push_back(a.slot);
        std::sort(slots.begin(), slots.end());
        slots.erase(std::unique(slots.begin(), slots.end()), slots.end());
        return static_cast<int>(slots.size());
    }

    static std::string describe(const Attachment& a) {
        if (a.kind == "color") return "Color attachment " + std::to_string(a.slot);
        return a.kind == "depth" ? "Depth attachment" : "Stencil attachment";
    }

    std::vector<Attachment> attachments_;
    std::vector<std::string> errors_;
};

} // namespace nexus::utility::graphics
