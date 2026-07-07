#pragma once

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

// Note: This is a wrapper for stb_image which should be included separately
// #define STB_IMAGE_IMPLEMENTATION
// #include "stb_image.h"

namespace nexus::utility::graphics {

/**
 * @brief RAII wrapper for image loading (designed for stb_image).
 */
class ImageLoaderWrapper {
public:
    struct ImageData {
        int width = 0;
        int height = 0;
        int channels = 0;
        std::vector<unsigned char> pixels;
        
        bool isValid() const { return !pixels.empty(); }
        size_t size() const { return pixels.size(); }
    };

    /**
     * @brief Loads image from file.
     * Note: Requires stb_image to be included in implementation.
     */
    static ImageData loadFromFile(const std::string& filename, int desiredChannels = 0) {
        ImageData data;
        
        // Placeholder implementation - actual implementation would use stb_image
        // unsigned char* pixels = stbi_load(filename.c_str(), &data.width, &data.height, &data.channels, desiredChannels);
        // if (pixels) {
        //     size_t size = data.width * data.height * (desiredChannels ? desiredChannels : data.channels);
        //     data.pixels.assign(pixels, pixels + size);
        //     stbi_image_free(pixels);
        // }
        
        return data;
    }

    /**
     * @brief Loads image from memory.
     */
    static ImageData loadFromMemory(const unsigned char* buffer, size_t length, int desiredChannels = 0) {
        ImageData data;
        
        // Placeholder - would use stbi_load_from_memory
        
        return data;
    }

    /**
     * @brief Converts image to different channel count.
     */
    static ImageData convertChannels(const ImageData& src, int targetChannels) {
        if (src.channels == targetChannels) return src;
        
        ImageData result;
        result.width = src.width;
        result.height = src.height;
        result.channels = targetChannels;
        result.pixels.resize(src.width * src.height * targetChannels);
        
        // Simple conversion logic
        for (int i = 0; i < src.width * src.height; ++i) {
            int srcIdx = i * src.channels;
            int dstIdx = i * targetChannels;
            
            if (targetChannels >= 1 && src.channels >= 1) result.pixels[dstIdx] = src.pixels[srcIdx];
            if (targetChannels >= 2 && src.channels >= 2) result.pixels[dstIdx + 1] = src.pixels[srcIdx + 1];
            if (targetChannels >= 3 && src.channels >= 3) result.pixels[dstIdx + 2] = src.pixels[srcIdx + 2];
            if (targetChannels >= 4) result.pixels[dstIdx + 3] = (src.channels >= 4) ? src.pixels[srcIdx + 3] : 255;
        }
        
        return result;
    }

    /**
     * @brief Flips image vertically.
     */
    static void flipVertical(ImageData& image) {
        int rowSize = image.width * image.channels;
        std::vector<unsigned char> temp(rowSize);
        
        for (int y = 0; y < image.height / 2; ++y) {
            int topRow = y * rowSize;
            int bottomRow = (image.height - 1 - y) * rowSize;
            
            std::copy(image.pixels.begin() + topRow, image.pixels.begin() + topRow + rowSize, temp.begin());
            std::copy(image.pixels.begin() + bottomRow, image.pixels.begin() + bottomRow + rowSize, 
                     image.pixels.begin() + topRow);
            std::copy(temp.begin(), temp.end(), image.pixels.begin() + bottomRow);
        }
    }

    /**
     * @brief Resizes image (nearest neighbor).
     */
    static ImageData resize(const ImageData& src, int newWidth, int newHeight) {
        ImageData result;
        result.width = newWidth;
        result.height = newHeight;
        result.channels = src.channels;
        result.pixels.resize(newWidth * newHeight * src.channels);
        
        float xRatio = static_cast<float>(src.width) / newWidth;
        float yRatio = static_cast<float>(src.height) / newHeight;
        
        for (int y = 0; y < newHeight; ++y) {
            for (int x = 0; x < newWidth; ++x) {
                int srcX = static_cast<int>(x * xRatio);
                int srcY = static_cast<int>(y * yRatio);
                
                int srcIdx = (srcY * src.width + srcX) * src.channels;
                int dstIdx = (y * newWidth + x) * src.channels;
                
                for (int c = 0; c < src.channels; ++c) {
                    result.pixels[dstIdx + c] = src.pixels[srcIdx + c];
                }
            }
        }
        
        return result;
    }
};

} // namespace nexus::utility::graphics
