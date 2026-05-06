#pragma once
#include <nexus/gfx/Swapchain.h>

namespace nexus::gfx {

class NullSwapchain final : public ISwapchain {
public:
    explicit NullSwapchain(Extent2D extent) : m_extent(extent) {}

    [[nodiscard]] AcquiredFrame acquire() override {
        AcquiredFrame f{};
        f.result     = AcquireResult::Ok;
        f.imageIndex = m_frame++ % 3;
        return f;
    }
    [[nodiscard]] PresentResult present(uint32_t, SemaphoreHandle) override { return PresentResult::Ok; }
    void resize(Extent2D e) override { m_extent = e; }

    [[nodiscard]] Extent2D  extent()      const noexcept override { return m_extent; }
    [[nodiscard]] Format    colorFormat() const noexcept override { return Format::B8G8R8A8_Srgb; }
    [[nodiscard]] uint32_t  imageCount()  const noexcept override { return 3; }
    [[nodiscard]] bool      isHdrActive() const noexcept override { return false; }

private:
    Extent2D m_extent;
    uint32_t m_frame = 0;
};

} // namespace nexus::gfx
