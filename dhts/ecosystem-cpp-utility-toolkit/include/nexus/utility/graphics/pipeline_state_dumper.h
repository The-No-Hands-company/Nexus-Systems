#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstdint>

namespace nexus::utility::graphics {

/**
 * @brief Captures and pretty-prints a GPU pipeline state for debugging.
 */
class PipelineStateDumper {
public:
    enum class BlendFactor {
        Zero, One,
        SrcColor, InvSrcColor,
        DstColor, InvDstColor,
        SrcAlpha, InvSrcAlpha,
        DstAlpha, InvDstAlpha
    };

    enum class CompareFunc {
        Never, Less, Equal, LessEqual,
        Greater, NotEqual, GreaterEqual, Always
    };

    enum class CullMode { None, Front, Back };
    enum class FillMode { Solid, Wireframe, Point };
    enum class FrontFace { Clockwise, CounterClockwise };

    PipelineStateDumper() = default;

    void setBlendState(BlendFactor srcRGB, BlendFactor dstRGB,
                       BlendFactor srcAlpha, BlendFactor dstAlpha) {
        blend_.set = true;
        blend_.srcRGB = srcRGB;
        blend_.dstRGB = dstRGB;
        blend_.srcAlpha = srcAlpha;
        blend_.dstAlpha = dstAlpha;
    }

    void setDepthState(bool enabled, bool writeMask, CompareFunc compareFunc) {
        depth_.set = true;
        depth_.enabled = enabled;
        depth_.writeMask = writeMask;
        depth_.compareFunc = compareFunc;
    }

    void setRasterState(CullMode cullMode, FillMode fillMode, FrontFace frontFace) {
        raster_.set = true;
        raster_.cullMode = cullMode;
        raster_.fillMode = fillMode;
        raster_.frontFace = frontFace;
    }

    void setViewport(float x, float y, float w, float h,
                     float minDepth, float maxDepth) {
        viewport_.set = true;
        viewport_.x = x; viewport_.y = y; viewport_.w = w; viewport_.h = h;
        viewport_.minDepth = minDepth; viewport_.maxDepth = maxDepth;
    }

    void setScissor(int x, int y, int w, int h) {
        scissor_.set = true;
        scissor_.x = x; scissor_.y = y; scissor_.w = w; scissor_.h = h;
    }

    void setShader(const std::string& stage, const std::string& source) {
        for (auto& s : shaders_) {
            if (s.stage == stage) { s.source = source; return; }
        }
        shaders_.push_back({stage, source});
    }

    std::string dumpToString() const {
        std::ostringstream os;
        os << "=== Pipeline State ===\n";

        os << "[Blend] ";
        if (blend_.set) {
            os << "srcRGB=" << toString(blend_.srcRGB)
               << " dstRGB=" << toString(blend_.dstRGB)
               << " srcAlpha=" << toString(blend_.srcAlpha)
               << " dstAlpha=" << toString(blend_.dstAlpha) << "\n";
        } else {
            os << "(unset)\n";
        }

        os << "[Depth] ";
        if (depth_.set) {
            os << "enabled=" << boolStr(depth_.enabled)
               << " write=" << boolStr(depth_.writeMask)
               << " func=" << toString(depth_.compareFunc) << "\n";
        } else {
            os << "(unset)\n";
        }

        os << "[Raster] ";
        if (raster_.set) {
            os << "cull=" << toString(raster_.cullMode)
               << " fill=" << toString(raster_.fillMode)
               << " front=" << toString(raster_.frontFace) << "\n";
        } else {
            os << "(unset)\n";
        }

        os << "[Viewport] ";
        if (viewport_.set) {
            os << "x=" << viewport_.x << " y=" << viewport_.y
               << " w=" << viewport_.w << " h=" << viewport_.h
               << " depth=[" << viewport_.minDepth << ", " << viewport_.maxDepth << "]\n";
        } else {
            os << "(unset)\n";
        }

        os << "[Scissor] ";
        if (scissor_.set) {
            os << "x=" << scissor_.x << " y=" << scissor_.y
               << " w=" << scissor_.w << " h=" << scissor_.h << "\n";
        } else {
            os << "(unset)\n";
        }

        os << "[Shaders] " << shaders_.size() << " stage(s)\n";
        for (const auto& s : shaders_) {
            os << "  - " << s.stage << " (" << s.source.size() << " bytes)\n";
        }
        return os.str();
    }

    bool dumpToFile(const std::string& path) const {
        std::ofstream out(path);
        if (!out.is_open()) return false;
        out << dumpToString();
        return out.good();
    }

    void clear() {
        blend_ = BlendState{};
        depth_ = DepthState{};
        raster_ = RasterState{};
        viewport_ = Viewport{};
        scissor_ = Scissor{};
        shaders_.clear();
    }

private:
    struct BlendState {
        bool set = false;
        BlendFactor srcRGB = BlendFactor::One, dstRGB = BlendFactor::Zero;
        BlendFactor srcAlpha = BlendFactor::One, dstAlpha = BlendFactor::Zero;
    };
    struct DepthState {
        bool set = false;
        bool enabled = true, writeMask = true;
        CompareFunc compareFunc = CompareFunc::Less;
    };
    struct RasterState {
        bool set = false;
        CullMode cullMode = CullMode::Back;
        FillMode fillMode = FillMode::Solid;
        FrontFace frontFace = FrontFace::CounterClockwise;
    };
    struct Viewport {
        bool set = false;
        float x = 0, y = 0, w = 0, h = 0, minDepth = 0, maxDepth = 1;
    };
    struct Scissor {
        bool set = false;
        int x = 0, y = 0, w = 0, h = 0;
    };
    struct Shader { std::string stage; std::string source; };

    static const char* boolStr(bool b) { return b ? "true" : "false"; }

    static const char* toString(BlendFactor f) {
        switch (f) {
            case BlendFactor::Zero: return "Zero";
            case BlendFactor::One: return "One";
            case BlendFactor::SrcColor: return "SrcColor";
            case BlendFactor::InvSrcColor: return "InvSrcColor";
            case BlendFactor::DstColor: return "DstColor";
            case BlendFactor::InvDstColor: return "InvDstColor";
            case BlendFactor::SrcAlpha: return "SrcAlpha";
            case BlendFactor::InvSrcAlpha: return "InvSrcAlpha";
            case BlendFactor::DstAlpha: return "DstAlpha";
            case BlendFactor::InvDstAlpha: return "InvDstAlpha";
        }
        return "Unknown";
    }

    static const char* toString(CompareFunc f) {
        switch (f) {
            case CompareFunc::Never: return "Never";
            case CompareFunc::Less: return "Less";
            case CompareFunc::Equal: return "Equal";
            case CompareFunc::LessEqual: return "LessEqual";
            case CompareFunc::Greater: return "Greater";
            case CompareFunc::NotEqual: return "NotEqual";
            case CompareFunc::GreaterEqual: return "GreaterEqual";
            case CompareFunc::Always: return "Always";
        }
        return "Unknown";
    }

    static const char* toString(CullMode m) {
        switch (m) {
            case CullMode::None: return "None";
            case CullMode::Front: return "Front";
            case CullMode::Back: return "Back";
        }
        return "Unknown";
    }

    static const char* toString(FillMode m) {
        switch (m) {
            case FillMode::Solid: return "Solid";
            case FillMode::Wireframe: return "Wireframe";
            case FillMode::Point: return "Point";
        }
        return "Unknown";
    }

    static const char* toString(FrontFace f) {
        switch (f) {
            case FrontFace::Clockwise: return "Clockwise";
            case FrontFace::CounterClockwise: return "CounterClockwise";
        }
        return "Unknown";
    }

    BlendState blend_;
    DepthState depth_;
    RasterState raster_;
    Viewport viewport_;
    Scissor scissor_;
    std::vector<Shader> shaders_;
};

} // namespace nexus::utility::graphics
