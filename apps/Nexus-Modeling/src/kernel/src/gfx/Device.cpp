// ─────────────────────────────────────────────────────────────────────────────
//  Nexus GFX — IDevice factory (createDevice free function)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <stdexcept>

#ifdef NEXUS_BACKEND_VULKAN
#  include "backend/vulkan/VulkanDevice.h"
#endif
#ifdef NEXUS_BACKEND_NULL
#  include "backend/null/NullDevice.h"
#endif

namespace nexus::gfx {

std::unique_ptr<IDevice> createDevice(Backend preferred)
{
#ifdef NEXUS_BACKEND_VULKAN
    if (preferred == Backend::Vulkan) {
        auto dev = std::make_unique<VulkanDevice>();
        RenderContextDesc defaults{};
        dev->init(defaults);
        return dev;
    }
#endif

#ifdef NEXUS_BACKEND_NULL
    if (preferred == Backend::Null) {
        return std::make_unique<NullDevice>();
    }
#endif

    (void)preferred;  // silence -Wunused-parameter when all backends are guarded
    throw std::runtime_error("createDevice: requested backend not compiled in");
}

} // namespace nexus::gfx
