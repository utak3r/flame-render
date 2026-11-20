#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/FlameGradient.h"
#include "core/FlameTransform.h"

namespace flame {

class FlameFractal {
   public:
    static constexpr double kBrightAdjust = 2.3;
    static constexpr double kFilterCutoff = 1.8;
    static constexpr int64_t kPrefilterWhite = (1LL << 26);
    static constexpr int kDefaultWhiteLevel = 200;
    static constexpr int kSubBatchSize = 10000;
    static constexpr int kFuseIterations = 15;

    FlameFractal();

    std::string name = "untitled";
    std::string version = "Apophysis Flame Renderer";
    std::string nick;
    std::string url;
    double time = 0.0;

    int width = 100;
    int height = 100;
    std::array<double, 2> center{0.0, 0.0};
    double scale = 50.0;  // pixels_per_unit in Apophysis
    double zoom = 0.0;
    double angle = 0.0;  // FAngle in radians (rotate = -180 * angle / pi)

    int oversample = 1;         // spatial_oversample
    double filterRadius = 0.5;  // spatial_filter_radius
    double quality = 50.0;      // sample_density
    int nbatches = 1;
    int whiteLevel = kDefaultWhiteLevel;

    std::array<int, 3> background{0, 0, 0};  // 0..255 integer RGB
    double brightness = 1.0;  // Fbrightness (unscaled; multiplied by
                              // kBrightAdjust in FlameToneMapper)
    double contrast = 1.0;
    double gamma = 1.0;
    double gammaThreshold = 0.01;
    double vibrancy = 1.0;
    double hueRotation = 1.0;
    int soloXform = -1;
    bool transparency = false;

    double estimator = 9.0;
    double estimatorMin = 0.0;
    double estimatorCurve = 0.4;
    int jitters = 1;

    FlameGradient gradient;
    std::vector<FlameTransform> xforms;
    std::optional<FlameTransform> finalXform;
    bool finalXformEnabled = false;

    double rotateDegrees() const noexcept {
        return -180.0 * angle / std::numbers::pi;
    }
    void setRotateDegrees(double deg) noexcept {
        angle = -std::numbers::pi * deg / 180.0;
    }

    double ppux() const noexcept;
    double ppuy() const noexcept;

    bool hasFinalXform() const noexcept;
    bool useFinalXform() const noexcept;

    void adjustScale(int newWidth, int newHeight) noexcept;

    int addSymmetry(int sym);

    void equalizeWeights() noexcept;
    void normalizeWeights() noexcept;

    static FlameFractal createRandom(int minXforms = 2,
                                     int maxXforms = 5,
                                     uint64_t seed = 0);

    static std::vector<FlameFractal> loadFromXmlFile(
        const std::filesystem::path& path);
    static std::vector<FlameFractal> loadFromXmlString(
        std::string_view xmlContent);
    std::string toXmlString() const;
    static bool saveToXmlFile(const std::vector<FlameFractal>& fractals,
                              const std::filesystem::path& path,
                              std::string_view collectionName = "renders");
};

}  // namespace flame
