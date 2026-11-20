#include "renderer/FlameImage.h"

#include <lodepng.h>
#include <stdexcept>

namespace flame {

FlameImage::FlameImage(int width, int height, PixelRGBA fill)
    : width_(width),
      height_(height),
      data_(static_cast<size_t>(width) * height * 4) {
    for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
        data_[i * 4 + 0] = fill.r;
        data_[i * 4 + 1] = fill.g;
        data_[i * 4 + 2] = fill.b;
        data_[i * 4 + 3] = fill.a;
    }
}

PixelRGBA FlameImage::getPixel(int x, int y) const noexcept {
    const size_t idx = (static_cast<size_t>(y) * width_ + x) * 4;
    return PixelRGBA{data_[idx + 0], data_[idx + 1], data_[idx + 2],
                     data_[idx + 3]};
}

void FlameImage::setPixel(int x, int y, PixelRGBA p) noexcept {
    const size_t idx = (static_cast<size_t>(y) * width_ + x) * 4;
    data_[idx + 0] = p.r;
    data_[idx + 1] = p.g;
    data_[idx + 2] = p.b;
    data_[idx + 3] = p.a;
}

bool FlameImage::savePng(const std::filesystem::path& path,
                         bool includeAlpha) const {
    if (empty())
        return false;

    std::vector<uint8_t> pngBytes;
    unsigned error = 0;
    if (includeAlpha) {
        error = lodepng::encode(pngBytes, data_, static_cast<unsigned>(width_),
                                static_cast<unsigned>(height_), LCT_RGBA, 8);
    } else {
        std::vector<uint8_t> rgbData(static_cast<size_t>(width_) * height_ * 3);
        for (size_t i = 0; i < static_cast<size_t>(width_) * height_; ++i) {
            rgbData[i * 3 + 0] = data_[i * 4 + 0];
            rgbData[i * 3 + 1] = data_[i * 4 + 1];
            rgbData[i * 3 + 2] = data_[i * 4 + 2];
        }
        error =
            lodepng::encode(pngBytes, rgbData, static_cast<unsigned>(width_),
                            static_cast<unsigned>(height_), LCT_RGB, 8);
    }
    if (error != 0)
        return false;

    if (path.has_parent_path() && !path.parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    return lodepng::save_file(pngBytes, path.string()) == 0;
}

FlameImage FlameImage::loadPng(const std::filesystem::path& path) {
    std::vector<uint8_t> pngBytes;
    unsigned error = lodepng::load_file(pngBytes, path.string());
    if (error != 0) {
        throw std::runtime_error("Failed to load PNG file: " + path.string() +
                                 " (" + lodepng_error_text(error) + ")");
    }
    std::vector<uint8_t> decoded;
    unsigned w = 0, h = 0;
    error = lodepng::decode(decoded, w, h, pngBytes, LCT_RGBA, 8);
    if (error != 0) {
        throw std::runtime_error("Failed to decode PNG file: " + path.string() +
                                 " (" + lodepng_error_text(error) + ")");
    }
    FlameImage img(static_cast<int>(w), static_cast<int>(h));
    img.data_ = std::move(decoded);
    return img;
}

}  // namespace flame
