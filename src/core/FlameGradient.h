#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace flame {

struct RGBColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    constexpr bool operator==(const RGBColor& other) const noexcept = default;
};

struct ScaledColor {
    int32_t r = 0;
    int32_t g = 0;
    int32_t b = 0;
};

class FlameGradient {
   public:
    static constexpr size_t kPaletteSize = 256;

    FlameGradient();

    const RGBColor& operator[](size_t index) const noexcept {
        return colors_[index & 0xFF];
    }
    RGBColor& operator[](size_t index) noexcept {
        return colors_[index & 0xFF];
    }

    const std::array<RGBColor, kPaletteSize>& colors() const noexcept {
        return colors_;
    }
    void setColors(const std::array<RGBColor, kPaletteSize>& colors) noexcept {
        colors_ = colors;
    }

    // Parse compact hex format from <palette count="256" format="RGB"> or
    // <colors data="..."> When hasAlphaPrefix is true (as in <colors
    // data="00RRGGBB...">), each color is 8 hex chars (2 ignored + 6 RGB).
    bool parseHex(std::string_view hexData,
                  bool hasAlphaPrefix = false,
                  size_t count = kPaletteSize);

    // Serialize to compact RGB hex string (6 hex chars per entry, 8 entries per
    // line)
    std::string toHex(bool includeAlphaPrefix = false) const;

    // Apply hue rotation in [0, 1] matching Apophysis rgb2hsv / hsv2rgb
    void applyHueRotation(double hueRotation);

    // Pre-scale color map by whiteLevel (default 200 in Apophysis): (c *
    // whiteLevel) / 256
    std::array<ScaledColor, kPaletteSize> createScaledColorMap(
        int32_t whiteLevel = 200) const;

    // Generate a smooth random gradient
    static FlameGradient createRandom(uint64_t seed = 0);

   private:
    std::array<RGBColor, kPaletteSize> colors_{};
};

}  // namespace flame
