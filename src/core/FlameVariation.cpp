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
#include "core/FlameVariation.h"

#include <cmath>

namespace flame {

namespace {

const std::array<VariationInfo, kNumVariations> kVariationTable = {
    {{VariationId::Linear, "linear", false, false, false, {}},
     {VariationId::Sinusoidal, "sinusoidal", false, false, false, {}},
     {VariationId::Spherical, "spherical", false, false, false, {}},
     {VariationId::Swirl, "swirl", false, false, false, {}},
     {VariationId::Horseshoe, "horseshoe", false, false, false, {}},
     {VariationId::Polar, "polar", false, true, false, {}},
     {VariationId::Handkerchief, "handkerchief", false, true, false, {}},
     {VariationId::Heart, "heart", false, true, false, {}},
     {VariationId::Disc, "disc", false, true, false, {}},
     {VariationId::Spiral, "spiral", false, false, true, {}},
     {VariationId::Hyperbolic, "hyperbolic", false, false, false, {}},
     {VariationId::Diamond, "diamond", false, false, true, {}},
     {VariationId::Ex, "ex", false, true, false, {}},
     {VariationId::Julia, "julia", false, true, false, {}},
     {VariationId::Bent, "bent", false, false, false, {}},
     {VariationId::Waves, "waves", false, false, false, {}},
     {VariationId::Fisheye, "fisheye", false, false, false, {}},
     {VariationId::Popcorn, "popcorn", false, false, false, {}},
     {VariationId::Exponential, "exponential", false, false, false, {}},
     {VariationId::Power, "power", false, false, true, {}},
     {VariationId::Cosine, "cosine", false, false, false, {}},
     {VariationId::Rings, "rings", false, true, true, {}},
     {VariationId::Fan, "fan", false, true, false, {}},
     {VariationId::Eyefish, "eyefish", false, false, false, {}},
     {VariationId::Bubble, "bubble", false, false, false, {}},
     {VariationId::Cylinder, "cylinder", false, false, false, {}},
     {VariationId::Noise, "noise", false, false, false, {}},
     {VariationId::Blur, "blur", false, true, false, {}},
     {VariationId::GaussianBlur, "gaussian_blur", false, false, false, {}},
     {VariationId::PreBlur, "pre_blur", true, false, false, {}},
     {VariationId::Blob,
      "blob",
      false,
      false,
      false,
      {"blob_low", "blob_high", "blob_waves"}},
     {VariationId::Curl, "curl", false, false, false, {"curl_c1", "curl_c2"}},
     {VariationId::Fan2, "fan2", false, false, false, {"fan2_x", "fan2_y"}},
     {VariationId::Julian,
      "julian",
      false,
      false,
      false,
      {"julian_power", "julian_dist"}},
     {VariationId::JuliaScope,
      "juliascope",
      false,
      false,
      false,
      {"juliascope_power", "juliascope_dist"}},
     {VariationId::Pdj,
      "pdj",
      false,
      false,
      false,
      {"pdj_a", "pdj_b", "pdj_c", "pdj_d"}},
     {VariationId::Perspective,
      "perspective",
      false,
      false,
      false,
      {"perspective_angle", "perspective_dist"}},
     {VariationId::RadialBlur,
      "radial_blur",
      false,
      false,
      false,
      {"radial_blur_angle"}},
     {VariationId::Rectangles,
      "rectangles",
      false,
      false,
      false,
      {"rectangles_x", "rectangles_y"}},
     {VariationId::Rings2, "rings2", false, false, false, {"rings2_val"}}}};

constexpr std::array<std::string_view, 21> kAllParamNames = {
    "blob_low",
    "blob_high",
    "blob_waves",
    "curl_c1",
    "curl_c2",
    "fan2_x",
    "fan2_y",
    "julian_power",
    "julian_dist",
    "juliascope_power",
    "juliascope_dist",
    "pdj_a",
    "pdj_b",
    "pdj_c",
    "pdj_d",
    "perspective_angle",
    "perspective_dist",
    "radial_blur_angle",
    "rectangles_x",
    "rectangles_y",
    "rings2_val"};

}  // namespace

bool VariationParams::setParam(std::string_view name, double value) noexcept {
    if (name == "blob_low") {
        blob_low = value;
        return true;
    }
    if (name == "blob_high") {
        blob_high = value;
        return true;
    }
    if (name == "blob_waves") {
        blob_waves = std::nearbyint(value);
        return true;
    }
    if (name == "curl_c1") {
        curl_c1 = value;
        return true;
    }
    if (name == "curl_c2") {
        curl_c2 = value;
        return true;
    }
    if (name == "fan2_x") {
        fan2_x = value;
        return true;
    }
    if (name == "fan2_y") {
        fan2_y = value;
        return true;
    }
    if (name == "julian_power") {
        int n = static_cast<int>(std::nearbyint(value));
        if (n == 0)
            n = 1;
        julian_power = static_cast<double>(n);
        return true;
    }
    if (name == "julian_dist") {
        julian_dist = value;
        return true;
    }
    if (name == "juliascope_power") {
        int n = static_cast<int>(std::nearbyint(value));
        if (n == 0)
            n = 1;
        juliascope_power = static_cast<double>(n);
        return true;
    }
    if (name == "juliascope_dist") {
        juliascope_dist = value;
        return true;
    }
    if (name == "pdj_a") {
        pdj_a = value;
        return true;
    }
    if (name == "pdj_b") {
        pdj_b = value;
        return true;
    }
    if (name == "pdj_c") {
        pdj_c = value;
        return true;
    }
    if (name == "pdj_d") {
        pdj_d = value;
        return true;
    }
    if (name == "perspective_angle") {
        perspective_angle = value;
        return true;
    }
    if (name == "perspective_dist") {
        perspective_dist = value;
        return true;
    }
    if (name == "radial_blur_angle") {
        radial_blur_angle = value;
        return true;
    }
    if (name == "rectangles_x") {
        rectangles_x = value;
        return true;
    }
    if (name == "rectangles_y") {
        rectangles_y = value;
        return true;
    }
    if (name == "rings2_val") {
        rings2_val = value;
        return true;
    }
    return false;
}

std::optional<double> VariationParams::getParam(
    std::string_view name) const noexcept {
    if (name == "blob_low")
        return blob_low;
    if (name == "blob_high")
        return blob_high;
    if (name == "blob_waves")
        return blob_waves;
    if (name == "curl_c1")
        return curl_c1;
    if (name == "curl_c2")
        return curl_c2;
    if (name == "fan2_x")
        return fan2_x;
    if (name == "fan2_y")
        return fan2_y;
    if (name == "julian_power")
        return julian_power;
    if (name == "julian_dist")
        return julian_dist;
    if (name == "juliascope_power")
        return juliascope_power;
    if (name == "juliascope_dist")
        return juliascope_dist;
    if (name == "pdj_a")
        return pdj_a;
    if (name == "pdj_b")
        return pdj_b;
    if (name == "pdj_c")
        return pdj_c;
    if (name == "pdj_d")
        return pdj_d;
    if (name == "perspective_angle")
        return perspective_angle;
    if (name == "perspective_dist")
        return perspective_dist;
    if (name == "radial_blur_angle")
        return radial_blur_angle;
    if (name == "rectangles_x")
        return rectangles_x;
    if (name == "rectangles_y")
        return rectangles_y;
    if (name == "rings2_val")
        return rings2_val;
    return std::nullopt;
}

std::string_view FlameVariation::name() const noexcept {
    return getInfo(id_).name;
}

const VariationInfo& FlameVariation::getInfo(VariationId id) noexcept {
    return kVariationTable[static_cast<size_t>(id)];
}

std::optional<VariationId> FlameVariation::findByName(
    std::string_view name) noexcept {
    for (const auto& info : kVariationTable) {
        if (info.name == name) {
            return info.id;
        }
    }
    return std::nullopt;
}

const std::array<std::string_view, 21>&
FlameVariation::allParameterNames() noexcept {
    return kAllParamNames;
}

bool FlameVariation::isParameterName(std::string_view name) noexcept {
    for (auto p : kAllParamNames) {
        if (p == name)
            return true;
    }
    return false;
}

}  // namespace flame
