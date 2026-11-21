// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Piotr Borys ("utak3r") <piotr.borys@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
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
