#include <gtest/gtest.h>
#include "nexus/canvas/ImageCanvas.h"

using namespace nexus::canvas;

TEST(ImageCanvas, CreateHasCorrectDimensions) {
    ImageCanvas canvas(64, 48);
    EXPECT_EQ(canvas.width(), 64u);
    EXPECT_EQ(canvas.height(), 48u);
}

TEST(ImageCanvas, ClearFillsWithColor) {
    ImageCanvas canvas(16, 16);
    canvas.clear(CanvasColor{1, 0, 0, 1});
    auto c = canvas.getPixel(5, 5);
    EXPECT_FLOAT_EQ(c.r, 1.0f);
    EXPECT_FLOAT_EQ(c.g, 0.0f);
}

TEST(ImageCanvas, SetAndGetPixel) {
    ImageCanvas canvas(32, 32);
    canvas.setPixel(10, 10, CanvasColor{0.5f, 0.25f, 0.75f});
    auto c = canvas.getPixel(10, 10);
    EXPECT_FLOAT_EQ(c.r, 0.5f);
    EXPECT_FLOAT_EQ(c.g, 0.25f);
    EXPECT_FLOAT_EQ(c.b, 0.75f);
}

TEST(ImageCanvas, DrawLine) {
    ImageCanvas canvas(64, 64);
    canvas.clear();
    canvas.drawLine(10, 10, 50, 50, CanvasColor{1, 1, 1});
    auto c = canvas.getPixel(30, 30);
    EXPECT_GT(c.r, 0.0f);
}

TEST(ImageCanvas, DrawCircleFilled) {
    ImageCanvas canvas(64, 64);
    canvas.clear();
    canvas.drawCircle(32, 32, 10, CanvasColor{1, 0, 0}, true);
    auto center = canvas.getPixel(32, 32);
    EXPECT_GT(center.r, 0.0f);
    auto outside = canvas.getPixel(0, 0);
    EXPECT_FLOAT_EQ(outside.r, 0.0f);
}

TEST(ImageCanvas, DrawRect) {
    ImageCanvas canvas(64, 64);
    canvas.clear();
    canvas.drawRect(10, 10, 20, 20, CanvasColor{0, 1, 0}, true);
    auto inside = canvas.getPixel(15, 15);
    EXPECT_GT(inside.g, 0.0f);
    auto outside = canvas.getPixel(5, 5);
    EXPECT_FLOAT_EQ(outside.g, 0.0f);
}

TEST(ImageCanvas, ApplyGaussianBlur) {
    ImageCanvas canvas(32, 32);
    canvas.setPixel(16, 16, CanvasColor{1, 1, 1});
    canvas.applyGaussianBlur(2.0f);
    EXPECT_GT(canvas.getPixel(16, 16).r, 0.0f);
}

TEST(ImageCanvas, ApplyBoxBlur) {
    ImageCanvas canvas(32, 32);
    canvas.setPixel(16, 16, CanvasColor{1, 1, 1});
    canvas.applyBoxBlur(2);
    EXPECT_GT(canvas.getPixel(16, 16).r, 0.0f);
}

TEST(ImageCanvas, Resize) {
    ImageCanvas canvas(8, 8);
    canvas.clear(CanvasColor{1, 0, 0});
    auto resized = canvas.resize(16, 16);
    EXPECT_EQ(resized.width(), 16u);
    EXPECT_EQ(resized.height(), 16u);
}

TEST(ImageCanvas, Crop) {
    ImageCanvas canvas(64, 64);
    canvas.clear(CanvasColor{0, 1, 0});
    auto cropped = canvas.crop(10, 10, 20, 20);
    EXPECT_EQ(cropped.width(), 20u);
    EXPECT_EQ(cropped.height(), 20u);
}

TEST(ImageCanvas, Paste) {
    ImageCanvas dest(32, 32);
    dest.clear(CanvasColor{0, 0, 0});
    ImageCanvas src(8, 8);
    src.clear(CanvasColor{1, 0, 0});
    dest.paste(src, 12, 12);
    EXPECT_GT(dest.getPixel(15, 15).r, 0.0f);
}

TEST(ImageCanvas, FillFlood) {
    ImageCanvas canvas(32, 32);
    canvas.clear(CanvasColor{0, 0, 0});
    canvas.drawRect(5, 5, 20, 20, CanvasColor{1, 0, 0}, true);
    canvas.drawRect(8, 8, 14, 14, CanvasColor{0, 0, 0}, true);
    canvas.fillFlood(10, 10, CanvasColor{0, 1, 0});
    EXPECT_GT(canvas.getPixel(10, 10).g, 0.0f);
}

TEST(CanvasColor, FromRGBA) {
    auto c = CanvasColor::fromRGBA(128, 64, 32, 255);
    EXPECT_FLOAT_EQ(c.r, 128.0f/255.0f);
    EXPECT_FLOAT_EQ(c.g, 64.0f/255.0f);
}
