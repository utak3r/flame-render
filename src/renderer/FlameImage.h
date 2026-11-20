#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace flame {

struct PixelRGBA {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    constexpr bool operator==(const PixelRGBA& other) const noexcept = default;
};

class FlameImage {
   public:
    FlameImage() = default;
    FlameImage(int width, int height, PixelRGBA fill = {0, 0, 0, 0});

    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    bool empty() const noexcept {
        return width_ <= 0 || height_ <= 0 || data_.empty();
    }

    PixelRGBA getPixel(int x, int y) const noexcept;
    void setPixel(int x, int y, PixelRGBA p) noexcept;

    const std::vector<uint8_t>& rawRGBA() const noexcept { return data_; }
    std::vector<uint8_t>& rawRGBA() noexcept { return data_; }

    bool savePng(const std::filesystem::path& path,
                 bool includeAlpha = true) const;
    static FlameImage loadPng(const std::filesystem::path& path);

   private:
    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> data_;  // 4 bytes per pixel: R, G, B, A
};

}  // namespace flame
