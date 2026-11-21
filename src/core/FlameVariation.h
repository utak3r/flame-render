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

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace flame {

enum class VariationId : uint8_t {
    Linear = 0,
    Sinusoidal = 1,
    Spherical = 2,
    Swirl = 3,
    Horseshoe = 4,
    Polar = 5,
    Handkerchief = 6,
    Heart = 7,
    Disc = 8,
    Spiral = 9,
    Hyperbolic = 10,
    Diamond = 11,
    Ex = 12,
    Julia = 13,
    Bent = 14,
    Waves = 15,
    Fisheye = 16,
    Popcorn = 17,
    Exponential = 18,
    Power = 19,
    Cosine = 20,
    Rings = 21,
    Fan = 22,
    Eyefish = 23,
    Bubble = 24,
    Cylinder = 25,
    Noise = 26,
    Blur = 27,
    GaussianBlur = 28,
    PreBlur = 29,
    Blob = 30,
    Curl = 31,
    Fan2 = 32,
    Julian = 33,
    JuliaScope = 34,
    Pdj = 35,
    Perspective = 36,
    RadialBlur = 37,
    Rectangles = 38,
    Rings2 = 39,

    Count = 40
};

constexpr size_t kNumVariations = static_cast<size_t>(VariationId::Count);

struct VariationInfo {
    VariationId id;
    std::string_view name;
    bool isPreVariation;
    bool needsAngle;
    bool needsSinCos;
    std::vector<std::string_view> parameterNames;
};

// All variation parameters for a transform, with some defaults
struct VariationParams {
    // blob
    double blob_low = 0.2;
    double blob_high = 1.0;
    double blob_waves = 4.0;

    // curl
    double curl_c1 = 0.0;
    double curl_c2 = 0.0;

    // fan2
    double fan2_x = 0.0;
    double fan2_y = 0.0;

    // julian
    double julian_power = 2.0;
    double julian_dist = 1.0;

    // juliascope
    double juliascope_power = 2.0;
    double juliascope_dist = 1.0;

    // pdj
    double pdj_a = 0.0;
    double pdj_b = 0.0;
    double pdj_c = 0.0;
    double pdj_d = 0.0;

    // perspective
    double perspective_angle = 0.0;
    double perspective_dist = 2.0;

    // radial_blur
    double radial_blur_angle = 0.0;

    // rectangles
    double rectangles_x = 1.0;
    double rectangles_y = 1.0;

    // rings2
    double rings2_val = 0.0;

    bool setParam(std::string_view name, double value) noexcept;
    std::optional<double> getParam(std::string_view name) const noexcept;
};

class FlameVariation {
   public:
    FlameVariation() = default;
    FlameVariation(VariationId id, double weight) noexcept
        : id_(id), weight_(weight) {}

    VariationId id() const noexcept { return id_; }
    double weight() const noexcept { return weight_; }
    void setWeight(double w) noexcept { weight_ = w; }

    std::string_view name() const noexcept;

    static const VariationInfo& getInfo(VariationId id) noexcept;
    static std::optional<VariationId> findByName(
        std::string_view name) noexcept;
    static const std::array<std::string_view, 21>& allParameterNames() noexcept;
    static bool isParameterName(std::string_view name) noexcept;

   private:
    VariationId id_ = VariationId::Linear;
    double weight_ = 0.0;
};

}  // namespace flame
