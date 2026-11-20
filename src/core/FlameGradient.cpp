#include "core/FlameGradient.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <random>
#include <sstream>

namespace flame {

namespace {

int hexValue(char c) noexcept {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

void rgb2hsv(const double rgb[3], double hsv[3]) noexcept {
    const double maxval = std::max({rgb[0], rgb[1], rgb[2]});
    const double minval = std::min({rgb[0], rgb[1], rgb[2]});

    hsv[2] = maxval;
    if (maxval > 0.0 && maxval != minval) {
        const double del = maxval - minval;
        hsv[1] = del / maxval;
        hsv[0] = 0.0;
        if (rgb[0] > rgb[1] && rgb[0] > rgb[2]) {
            hsv[0] = (rgb[1] - rgb[2]) / del;
        } else if (rgb[1] > rgb[2]) {
            hsv[0] = 2.0 + (rgb[2] - rgb[0]) / del;
        } else {
            hsv[0] = 4.0 + (rgb[0] - rgb[1]) / del;
        }
        if (hsv[0] < 0.0) {
            hsv[0] += 6.0;
        }
    } else {
        hsv[0] = 0.0;
        hsv[1] = 0.0;
    }
}

void hsv2rgb(const double hsv[3], double rgb[3]) noexcept {
    double h = hsv[0];
    while (h >= 6.0)
        h -= 6.0;
    while (h < 0.0)
        h += 6.0;
    const int j = static_cast<int>(std::floor(h));
    const double f = h - j;
    const double v = hsv[2];
    const double p = hsv[2] * (1.0 - hsv[1]);
    const double q = hsv[2] * (1.0 - hsv[1] * f);
    const double t = hsv[2] * (1.0 - hsv[1] * (1.0 - f));
    switch (j % 6) {
        case 0:
            rgb[0] = v;
            rgb[1] = t;
            rgb[2] = p;
            break;
        case 1:
            rgb[0] = q;
            rgb[1] = v;
            rgb[2] = p;
            break;
        case 2:
            rgb[0] = p;
            rgb[1] = v;
            rgb[2] = t;
            break;
        case 3:
            rgb[0] = p;
            rgb[1] = q;
            rgb[2] = v;
            break;
        case 4:
            rgb[0] = t;
            rgb[1] = p;
            rgb[2] = v;
            break;
        case 5:
            rgb[0] = v;
            rgb[1] = p;
            rgb[2] = q;
            break;
        default:
            rgb[0] = v;
            rgb[1] = t;
            rgb[2] = p;
            break;
    }
}

}  // namespace

FlameGradient::FlameGradient() {
    for (size_t i = 0; i < kPaletteSize; ++i) {
        const auto v = static_cast<uint8_t>(i);
        colors_[i] = RGBColor{v, v, v};
    }
}

bool FlameGradient::parseHex(std::string_view hexData,
                             bool hasAlphaPrefix,
                             size_t count) {
    std::string cleaned;
    cleaned.reserve(hexData.size());
    for (char c : hexData) {
        if (hexValue(c) >= 0) {
            cleaned.push_back(c);
        }
    }

    const size_t stride = hasAlphaPrefix ? 8 : 6;
    const size_t numEntries = std::min(count, kPaletteSize);
    if (cleaned.size() < numEntries * stride) {
        return false;
    }

    for (size_t i = 0; i < numEntries; ++i) {
        const size_t pos = i * stride + (hasAlphaPrefix ? 2 : 0);
        const int r =
            (hexValue(cleaned[pos + 0]) << 4) | hexValue(cleaned[pos + 1]);
        const int g =
            (hexValue(cleaned[pos + 2]) << 4) | hexValue(cleaned[pos + 3]);
        const int b =
            (hexValue(cleaned[pos + 4]) << 4) | hexValue(cleaned[pos + 5]);
        colors_[i] = RGBColor{static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                              static_cast<uint8_t>(b)};
    }
    return true;
}

std::string FlameGradient::toHex(bool includeAlphaPrefix) const {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    for (size_t i = 0; i < kPaletteSize; ++i) {
        if (!includeAlphaPrefix && (i % 8 == 0)) {
            if (i > 0)
                oss << "\n";
            oss << "      ";
        }
        if (includeAlphaPrefix) {
            oss << std::setw(2) << 0;
        }
        oss << std::setw(2) << static_cast<int>(colors_[i].r) << std::setw(2)
            << static_cast<int>(colors_[i].g) << std::setw(2)
            << static_cast<int>(colors_[i].b);
    }
    return oss.str();
}

void FlameGradient::applyHueRotation(double hueRotation) {
    if (std::abs(hueRotation) < 1e-12 || std::abs(hueRotation - 1.0) < 1e-12) {
        return;
    }
    for (size_t i = 0; i < kPaletteSize; ++i) {
        double rgb[3] = {colors_[i].r / 255.0, colors_[i].g / 255.0,
                         colors_[i].b / 255.0};
        double hsv[3] = {0.0, 0.0, 0.0};
        rgb2hsv(rgb, hsv);
        hsv[0] += hueRotation * 6.0;
        hsv2rgb(hsv, rgb);
        colors_[i].r = static_cast<uint8_t>(std::clamp(
            static_cast<int>(std::nearbyint(rgb[0] * 255.0)), 0, 255));
        colors_[i].g = static_cast<uint8_t>(std::clamp(
            static_cast<int>(std::nearbyint(rgb[1] * 255.0)), 0, 255));
        colors_[i].b = static_cast<uint8_t>(std::clamp(
            static_cast<int>(std::nearbyint(rgb[2] * 255.0)), 0, 255));
    }
}

std::array<ScaledColor, FlameGradient::kPaletteSize>
FlameGradient::createScaledColorMap(int32_t whiteLevel) const {
    std::array<ScaledColor, kPaletteSize> map{};
    for (size_t i = 0; i < kPaletteSize; ++i) {
        // Exact integer division, as in Apophysis:
        // (CMap[i][c] * white_level) div 256
        map[i].r = (static_cast<int32_t>(colors_[i].r) * whiteLevel) / 256;
        map[i].g = (static_cast<int32_t>(colors_[i].g) * whiteLevel) / 256;
        map[i].b = (static_cast<int32_t>(colors_[i].b) * whiteLevel) / 256;
    }
    return map;
}

FlameGradient FlameGradient::createRandom(uint64_t seed) {
    std::mt19937_64 rng(seed ? seed : std::random_device{}());
    std::uniform_int_distribution<int> colDist(0, 255);

    FlameGradient grad;
    constexpr size_t kNumStops = 4;
    std::array<RGBColor, kNumStops> stops{};
    for (auto& s : stops) {
        s = RGBColor{static_cast<uint8_t>(colDist(rng)),
                     static_cast<uint8_t>(colDist(rng)),
                     static_cast<uint8_t>(colDist(rng))};
    }

    for (size_t i = 0; i < kPaletteSize; ++i) {
        const double pos = (static_cast<double>(i) / kPaletteSize) * kNumStops;
        const size_t idx0 = static_cast<size_t>(pos) % kNumStops;
        const size_t idx1 = (idx0 + 1) % kNumStops;
        const double frac = pos - std::floor(pos);
        grad[i] =
            RGBColor{static_cast<uint8_t>(std::nearbyint(
                         stops[idx0].r * (1.0 - frac) + stops[idx1].r * frac)),
                     static_cast<uint8_t>(std::nearbyint(
                         stops[idx0].g * (1.0 - frac) + stops[idx1].g * frac)),
                     static_cast<uint8_t>(std::nearbyint(
                         stops[idx0].b * (1.0 - frac) + stops[idx1].b * frac))};
    }
    return grad;
}

}  // namespace flame
