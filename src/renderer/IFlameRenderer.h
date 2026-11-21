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

#include "core/FlameFractal.h"
#include "renderer/FlameImage.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>

namespace flame {

struct RenderOptions {
    int width = 0;         // If > 0, overrides fractal width (and adjusts scale
                           // proportionally)
    int height = 0;        // If > 0, overrides fractal height
    double quality = 0.0;  // If > 0, overrides fractal sample density (quality)
    int oversample = 0;    // If > 0, overrides spatial oversample
    double filterRadius = 0.0;  // If > 0, overrides spatial filter radius
    std::optional<bool> transparent =
        true;  // Default true (RGBA matching Apophysis golden PNGs); if set,
               // overrides fractal transparency
    std::filesystem::path outputFilename;  // If non-empty, saves the rendered
                                           // PNG directly to this path
    uint32_t numThreads = 0;               // 0 = auto (hardware_concurrency)
    std::optional<uint64_t>
        seed;  // Optional fixed RNG seed for deterministic testing
    std::function<void(double)>
        onProgress;  // Progress callback receiving [0.0 .. 1.0]
};

struct Bucket64 {
    int64_t red = 0;
    int64_t green = 0;
    int64_t blue = 0;
    int64_t count = 0;
};

struct RenderStats {
    int64_t totalSamples = 0;
    int64_t totalHits = 0;
    double hitRatioPercent = 0.0;
    double renderTimeSeconds = 0.0;
    double iterationsPerSecond = 0.0;
    double maxRedBits = 0.0;
    double maxGreenBits = 0.0;
    double maxBlueBits = 0.0;
    double maxCountBits = 0.0;
};

class IFlameRenderer {
   public:
    virtual ~IFlameRenderer() = default;

    virtual void setFractal(const FlameFractal* fractal) = 0;
    virtual FlameImage render(const RenderOptions& options = {}) = 0;
    virtual void stop() = 0;
    virtual RenderStats getLastStats() const = 0;
};

}  // namespace flame
