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
