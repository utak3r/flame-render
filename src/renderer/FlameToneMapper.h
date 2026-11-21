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
#include "renderer/IFlameRenderer.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace flame {

// Banker's rounding (round half to even), matching Delphi's System.Round() /
// x87 FISTP
inline int64_t apoRound(double x) noexcept {
    if (!std::isfinite(x)) {
        return 0;
    }
    return static_cast<int64_t>(std::nearbyint(x));
}

class FlameToneMapper {
   public:
    FlameToneMapper() = default;

    void createFilter(int oversample, double filterRadius);

    int filterSize() const noexcept { return filterSize_; }
    const std::vector<double>& filterKernel() const noexcept { return filter_; }

    FlameImage createImage(const FlameFractal& cp,
                           double actualDensity,
                           const std::vector<Bucket64>& buckets,
                           int bucketWidth,
                           int bucketHeight) const;

    static RenderStats computeBucketStats(const std::vector<Bucket64>& buckets,
                                          int64_t totalSamples,
                                          double renderTimeSeconds) noexcept;

   private:
    int oversample_ = 1;
    int filterSize_ = 1;
    std::vector<double> filter_;  // filterSize_ x filterSize_ row-major
};

}  // namespace flame
