// ─────────────────────────────────────────────────────────────────────────────
//  NeuralRenderer — factory implementation
//  Priority: DLSS4 → XeSS → OIDN → deterministic software fallback
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/neural/NeuralRenderer.h>
#include <nexus/gfx/Device.h>
#include "OIDNDenoiser.h"
#include "DLSSPlugin.h"
#include "XeSSPlugin.h"
#include <memory>

#if defined(NEXUS_BACKEND_VULKAN)
#include "../backend/vulkan/VulkanDevice.h"
#endif

namespace nexus::neural {

namespace {

#include <nexus/neural/NeuralRenderer.h>
#include <nexus/gfx/Device.h>
#include "OIDNDenoiser.h"
#include "DLSSPlugin.h"
#include "XeSSPlugin.h"
#include <algorithm>
#include <cmath>
#include <memory>

#if defined(NEXUS_BACKEND_VULKAN)
#include "../backend/vulkan/VulkanDevice.h"
#endif

namespace nexus::neural {

namespace {

class DeterministicFallbackNeuralRenderer final : public INeuralRenderer {
public:
    [[nodiscard]] DenoiserBackend activeDenoiser() const noexcept override {
        return DenoiserBackend::None;
    }

    [[nodiscard]] UpscalerBackend activeUpscaler() const noexcept override {
        return UpscalerBackend::Bilinear;
    }

    void denoise(nexus::gfx::CmdBufHandle,
                 const DenoiserInput& in,
                 DenoiserOutput& out) override {
        const int patchSize = 3;
        const float spatialSigma = 0.5f;
        const float colorSigma = 0.1f;

        size_t pixelCount = static_cast<size_t>(in.width * in.height);
        out.color.resize(pixelCount * 3, 0.0f);

        for (uint32_t y = 0; y < in.height; ++y) {
            for (uint32_t x = 0; x < in.width; ++x) {
                size_t ci = (static_cast<size_t>(y) * in.width + x) * 3;

                float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;
                float sumW = 0.0f;

                float centerR = in.color[ci];
                float centerG = in.color[ci + 1];
                float centerB = in.color[ci + 2];

                for (int dy = -patchSize; dy <= patchSize; ++dy) {
                    for (int dx = -patchSize; dx <= patchSize; ++dx) {
                        int nx = static_cast<int>(x) + dx;
                        int ny = static_cast<int>(y) + dy;
                        if (nx < 0 || ny < 0 ||
                            static_cast<uint32_t>(nx) >= in.width ||
                            static_cast<uint32_t>(ny) >= in.height) continue;

                        size_t ni = (static_cast<size_t>(ny) * in.width + nx) * 3;

                        float dR = in.color[ni] - centerR;
                        float dG = in.color[ni + 1] - centerG;
                        float dB = in.color[ni + 2] - centerB;
                        float colorDist = std::sqrt(dR * dR + dG * dG + dB * dB);
                        float spatialDist = std::sqrt(static_cast<float>(dx * dx + dy * dy));

                        float w = std::exp(-spatialDist * spatialDist / (2.0f * spatialSigma * spatialSigma) -
                                            colorDist * colorDist / (2.0f * colorSigma * colorSigma));

                        sumR += in.color[ni] * w;
                        sumG += in.color[ni + 1] * w;
                        sumB += in.color[ni + 2] * w;
                        sumW += w;
                    }
                }

                if (sumW > 1e-10f) {
                    out.color[ci] = sumR / sumW;
                    out.color[ci + 1] = sumG / sumW;
                    out.color[ci + 2] = sumB / sumW;
                } else {
                    out.color[ci] = centerR;
                    out.color[ci + 1] = centerG;
                    out.color[ci + 2] = centerB;
                }
            }
        }
    }

    void upscale(nexus::gfx::CmdBufHandle,
                 const UpscalerInput& in,
                 UpscalerOutput& out) override {
        size_t outPixels = static_cast<size_t>(in.targetWidth * in.targetHeight);
        out.color.resize(outPixels * 3, 0.0f);

        float scaleX = static_cast<float>(in.sourceWidth) / static_cast<float>(in.targetWidth);
        float scaleY = static_cast<float>(in.sourceHeight) / static_cast<float>(in.targetHeight);

        for (uint32_t y = 0; y < in.targetHeight; ++y) {
            for (uint32_t x = 0; x < in.targetWidth; ++x) {
                float srcX = (static_cast<float>(x) + 0.5f) * scaleX - 0.5f;
                float srcY = (static_cast<float>(y) + 0.5f) * scaleY - 0.5f;

                int x0 = static_cast<int>(std::floor(srcX));
                int y0 = static_cast<int>(std::floor(srcY));
                int x1 = x0 + 1;
                int y1 = y0 + 1;

                x0 = std::clamp(x0, 0, static_cast<int>(in.sourceWidth) - 1);
                y0 = std::clamp(y0, 0, static_cast<int>(in.sourceHeight) - 1);
                x1 = std::clamp(x1, 0, static_cast<int>(in.sourceWidth) - 1);
                y1 = std::clamp(y1, 0, static_cast<int>(in.sourceHeight) - 1);

                float fx = srcX - std::floor(srcX);
                float fy = srcY - std::floor(srcY);

                size_t i00 = (static_cast<size_t>(y0) * in.sourceWidth + x0) * 3;
                size_t i10 = (static_cast<size_t>(y0) * in.sourceWidth + x1) * 3;
                size_t i01 = (static_cast<size_t>(y1) * in.sourceWidth + x0) * 3;
                size_t i11 = (static_cast<size_t>(y1) * in.sourceWidth + x1) * 3;

                size_t oi = (static_cast<size_t>(y) * in.targetWidth + x) * 3;
                for (int c = 0; c < 3; ++c) {
                    float v00 = in.color[i00 + c];
                    float v10 = in.color[i10 + c];
                    float v01 = in.color[i01 + c];
                    float v11 = in.color[i11 + c];

                    out.color[oi + c] = v00 * (1.0f - fx) * (1.0f - fy) +
                                         v10 * fx * (1.0f - fy) +
                                         v01 * (1.0f - fx) * fy +
                                         v11 * fx * fy;
                }
            }
        }
    }
};

[[nodiscard]] bool hasExecutablePath(const INeuralRenderer& renderer) noexcept
{
    return renderer.activeDenoiser() != DenoiserBackend::None
        || renderer.activeUpscaler() != UpscalerBackend::None;
}

} // namespace

std::unique_ptr<INeuralRenderer> createNeuralRenderer(
    nexus::gfx::IDevice& device,
    bool preferDLSS,
    bool preferXeSS,
    bool preferFSR)
{
    (void)preferFSR;

    // Obtain Vulkan handles if the backend is Vulkan.
    VkInstance       instance  = VK_NULL_HANDLE;
    VkPhysicalDevice physDev   = VK_NULL_HANDLE;
    VkDevice         vkDevice  = VK_NULL_HANDLE;

    if (device.backend() == nexus::gfx::Backend::Vulkan) {
#if defined(NEXUS_BACKEND_VULKAN)
        if (auto* vd = dynamic_cast<nexus::gfx::VulkanDevice*>(&device)) {
            instance = vd->instance();
            physDev  = vd->physical();
            vkDevice = vd->logical();
        }
#endif
    }

    // ── DLSS 4 ────────────────────────────────────────────────────────────────
    if (preferDLSS) {
        auto dlss = std::make_unique<DLSSPlugin>(instance, physDev, vkDevice);
        if (dlss->available()) return dlss;
    }

    // ── XeSS ─────────────────────────────────────────────────────────────────
    if (preferXeSS) {
        auto xess = std::make_unique<XeSSPlugin>(vkDevice, physDev);
        if (xess->available()) return xess;
    }

    // ── OIDN (CPU fallback candidate) ─────────────────────────────────────────
    {
        auto oidn = std::make_unique<OIDNDenoiser>();
        if (hasExecutablePath(*oidn)) return oidn;
    }

    // Deterministic baseline renderer for unsupported or unavailable runtimes.
    return std::make_unique<DeterministicFallbackNeuralRenderer>();
}

} // namespace nexus::neural

