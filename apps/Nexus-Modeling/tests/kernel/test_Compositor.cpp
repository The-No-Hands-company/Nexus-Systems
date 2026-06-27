#include <gtest/gtest.h>
#include "nexus/compositor/Compositor.h"

using namespace nexus::compositor;

TEST(Compositor, CreateBuffer) {
    auto buf = Compositor::createBuffer(64, 64);
    EXPECT_TRUE(buf.valid());
    EXPECT_EQ(buf.width, 64u);
    EXPECT_EQ(buf.height, 64u);
}

TEST(ImageBuffer, SetAndGetPixel) {
    auto buf = Compositor::createBuffer(32, 32);
    buf.setPixel(10, 10, 0.5f, 0.25f, 0.75f);
    EXPECT_FLOAT_EQ(buf.sample(10, 10, 0), 0.5f);
    EXPECT_FLOAT_EQ(buf.sample(10, 10, 1), 0.25f);
    EXPECT_FLOAT_EQ(buf.sample(10, 10, 2), 0.75f);
}

TEST(Compositor, CompositeSingleLayer) {
    auto layer = Compositor::createBuffer(64, 64);
    for (uint32_t y = 0; y < 64; ++y)
        for (uint32_t x = 0; x < 64; ++x)
            layer.setPixel(x, y, 1, 0, 0);

    CompositeLayer cl;
    cl.image = layer;

    auto result = Compositor::composite({cl}, 64, 64);
    EXPECT_FLOAT_EQ(result.sample(10, 10, 0), 1.0f);
}

TEST(Compositor, MultipleLayers) {
    auto red = Compositor::createBuffer(32, 32);
    auto blue = Compositor::createBuffer(32, 32);
    for (uint32_t y = 0; y < 32; ++y)
        for (uint32_t x = 0; x < 32; ++x) {
            red.setPixel(x, y, 1, 0, 0);
            blue.setPixel(x, y, 0, 0, 1);
        }

    CompositeLayer lr{red, BlendMode::Normal, 0.5f};
    CompositeLayer lb{blue, BlendMode::Normal, 0.5f};

    auto result = Compositor::composite({lr, lb}, 32, 32);
    EXPECT_GT(result.sample(10, 10, 0), 0.0f);
    EXPECT_GT(result.sample(10, 10, 2), 0.0f);
}

TEST(Compositor, ApplyBlur) {
    auto img = Compositor::createBuffer(32, 32);
    img.setPixel(16, 16, 1, 1, 1);
    auto blurred = Compositor::applyBlur(img, 2.0f);
    EXPECT_TRUE(blurred.valid());
}

TEST(Compositor, ApplyColorAdjustment) {
    auto img = Compositor::createBuffer(16, 16);
    for (uint32_t y = 0; y < 16; ++y)
        for (uint32_t x = 0; x < 16; ++x)
            img.setPixel(x, y, 0.5f, 0.5f, 0.5f);

    ColorAdjustment adj;
    adj.operation = ColorOp::Invert;
    auto inverted = Compositor::applyColorAdjustment(img, adj);
    EXPECT_NEAR(inverted.sample(5, 5, 0), 0.5f, 0.01f);  // 1-0.5 = 0.5
}

TEST(Compositor, EdgeDetect) {
    auto img = Compositor::createBuffer(32, 32);
    img.setPixel(10, 10, 1, 1, 1);
    auto edges = Compositor::applyEdgeDetect(img);
    EXPECT_TRUE(edges.valid());
}

TEST(Compositor, Vignette) {
    auto img = Compositor::createBuffer(32, 32);
    for (uint32_t y = 0; y < 32; ++y)
        for (uint32_t x = 0; x < 32; ++x)
            img.setPixel(x, y, 1, 1, 1);

    auto vignette = Compositor::applyVignette(img, 0.5f);
    EXPECT_GE(img.sample(16, 16, 0), vignette.sample(0, 0, 0));
}

TEST(Compositor, ResizeImageBuffer) {
    auto img = Compositor::createBuffer(8, 8);
    img.setPixel(4, 4, 1, 0, 0);
    auto resized = img.resize(16, 16);
    EXPECT_EQ(resized.width, 16u);
    EXPECT_EQ(resized.height, 16u);
}
