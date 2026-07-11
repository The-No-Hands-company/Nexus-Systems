#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus GFX — ISwapchain
//
//  Manages surface presentation.  Acquire → record commands → present.
//  Handles resize / HDR / VSync settings.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include <cstdint>

namespace nexus::gfx {

// ── Present mode ─────────────────────────────────────────────────────────────
enum class PresentMode : uint8_t {
    Immediate,    // no vsync — tearing possible (best for dev/benchmark)
    Fifo,         // vsync on — no tearing (default)
    FifoRelaxed,  // vsync but allows tearing if frame is late
    Mailbox,      // triple-buffer — low latency + no tearing
};

// ── HDR metadata ─────────────────────────────────────────────────────────────
struct HdrMetadata {
    float maxLuminance  = 1000.f;   // nits
    float minLuminance  = 0.001f;
    float maxCll        = 1000.f;   // max content light level
    float maxFall       = 400.f;    // max frame average light level
};

// ── Swapchain descriptor ──────────────────────────────────────────────────────
struct SwapchainDesc {
    void*       nativeWindowHandle = nullptr;  // HWND / xcb_window_t / NSWindow*
    void*       nativeDisplayHandle= nullptr;  // X11 Display* / nullptr elsewhere
    void*       preCreatedSurface  = nullptr;  // opaque VkSurfaceKHR (e.g. from glfwCreateWindowSurface);
                                               // if set, the backend adopts it instead of building its own
    Extent2D    extent;
    Format      colorFormat  = Format::B8G8R8A8_Srgb;
    uint32_t    imageCount   = 3;              // triple-buffer default
    PresentMode presentMode  = PresentMode::Fifo;
    bool        hdrEnabled   = false;
    HdrMetadata hdrMeta;
};

// ── Acquire result ────────────────────────────────────────────────────────────
enum class AcquireResult : uint8_t {
    Ok,
    Suboptimal,   // still usable — resize next frame
    OutOfDate,    // must recreate immediately
};

struct AcquiredFrame {
    AcquireResult result       = AcquireResult::Ok;
    uint32_t      imageIndex   = 0;
    TextureHandle colorTarget;           // bound render target for this frame
    SemaphoreHandle imageAvailable;      // wait on this before writing
    SemaphoreHandle renderFinished;      // signal this when done; present waits on it
};

// ── Present result ────────────────────────────────────────────────────────────
enum class PresentResult : uint8_t { Ok, Suboptimal, OutOfDate };

// ── ISwapchain ────────────────────────────────────────────────────────────────
class ISwapchain {
public:
    virtual ~ISwapchain() = default;

    [[nodiscard]] virtual AcquiredFrame acquire()          = 0;
    [[nodiscard]] virtual PresentResult present(uint32_t imageIndex, SemaphoreHandle waitSemaphore) = 0;

    virtual void resize(Extent2D newExtent) = 0;

    [[nodiscard]] virtual Extent2D  extent()      const noexcept = 0;
    [[nodiscard]] virtual Format    colorFormat() const noexcept = 0;
    [[nodiscard]] virtual uint32_t  imageCount()  const noexcept = 0;
    [[nodiscard]] virtual bool      isHdrActive() const noexcept = 0;
};

} // namespace nexus::gfx
