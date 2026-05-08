#include <nexus/gfx/Device.h>
#include <nexus/neural/NeuralRenderer.h>

#include <gtest/gtest.h>

using namespace nexus::gfx;
using namespace nexus::neural;

TEST(NeuralRenderer, FallbackRendererIsDeterministicOnNullBackend)
{
    std::unique_ptr<IDevice> dev = createDevice(Backend::Null);
    ASSERT_NE(dev, nullptr);

    std::unique_ptr<INeuralRenderer> renderer = createNeuralRenderer(*dev);
    ASSERT_NE(renderer, nullptr);

    EXPECT_EQ(renderer->activeDenoiser(), DenoiserBackend::None);
    EXPECT_EQ(renderer->activeUpscaler(), UpscalerBackend::Bilinear);

    DenoiserInput dIn{};
    dIn.color = TextureHandle{7};
    DenoiserOutput dOut{};
    renderer->denoise({}, dIn, dOut);
    EXPECT_EQ(dOut.color.id, dIn.color.id);

    UpscalerInput uIn{};
    uIn.color = TextureHandle{11};
    uIn.renderResolution = {640, 360};
    uIn.outputResolution = {1280, 720};
    UpscalerOutput uOut{};
    renderer->upscale({}, uIn, uOut);
    EXPECT_EQ(uOut.color.id, uIn.color.id);
}

TEST(NeuralRenderer, PreferenceFlagsStillReturnDeterministicFallbackWithoutRuntimePlugins)
{
    std::unique_ptr<IDevice> dev = createDevice(Backend::Null);
    ASSERT_NE(dev, nullptr);

    std::unique_ptr<INeuralRenderer> renderer = createNeuralRenderer(*dev,
                                                                      true,
                                                                      true,
                                                                      true);
    ASSERT_NE(renderer, nullptr);

    EXPECT_EQ(renderer->activeDenoiser(), DenoiserBackend::None);
    EXPECT_EQ(renderer->activeUpscaler(), UpscalerBackend::Bilinear);
}
