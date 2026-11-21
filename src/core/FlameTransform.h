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

#include "core/FlameVariation.h"

#include <array>
#include <cstdint>
#include <vector>

namespace flame {

struct FlamePoint {
    double x = 0.0;
    double y = 0.0;
    double c = 0.0;
};

// Fast Xoshiro256** PRNG for Chaos Game iteration
// https://en.wikipedia.org/wiki/Xorshift
class FastRng {
   public:
    explicit FastRng(uint64_t seed = 0x853c49e6748fea9bULL) noexcept {
        seedWith(seed);
    }

    void seedWith(uint64_t seed) noexcept {
        // SplitMix64 initialization
        uint64_t z = seed;
        for (int i = 0; i < 4; ++i) {
            z += 0x9e3779b97f4a7c15ULL;
            uint64_t x = z;
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
            s_[i] = x ^ (x >> 31);
        }
    }

    uint64_t nextU64() noexcept {
        const uint64_t result = rotl(s_[1] * 5, 7) * 9;
        const uint64_t t = s_[1] << 17;
        s_[2] ^= s_[0];
        s_[3] ^= s_[1];
        s_[1] ^= s_[2];
        s_[0] ^= s_[3];
        s_[2] ^= t;
        s_[3] = rotl(s_[3], 45);
        return result;
    }

    // Uniform double in [0, 1)
    double next01() noexcept {
        return static_cast<double>(nextU64() >> 11) * 0x1.0p-53;
    }

    // Uniform double in [-1, 1)
    double nextSigned() noexcept { return next01() * 2.0 - 1.0; }

    // Uniform integer in [0, maxExclusive)
    uint32_t nextInt(uint32_t maxExclusive) noexcept {
        if (maxExclusive <= 1)
            return 0;
        return static_cast<uint32_t>(nextU64() % maxExclusive);
    }

    bool nextBit() noexcept { return (nextU64() & 1ULL) != 0; }

   private:
    static constexpr uint64_t rotl(const uint64_t x, int k) noexcept {
        return (x << k) | (x >> (64 - k));
    }
    uint64_t s_[4]{};
};

class FlameTransform {
   public:
    static constexpr size_t kMaxModWeights = 100;

    FlameTransform();

    void clear();

    double weight = 0.0;
    double color = 0.0;
    double symmetry = 0.0;  // color_speed in Apophysis
    double opacity = 1.0;
    int plotMode = 1;  // -1 = neverPlot, 0 = opacityPlot, 1 = alwaysPlot

    // Affine transform coefficients: FTx = c[0][0]*x + c[1][0]*y + c[2][0], FTy
    // = c[0][1]*x + c[1][1]*y + c[2][1]
    std::array<std::array<double, 2>, 3> coefs{
        {{1.0, 0.0}, {0.0, 1.0}, {0.0, 0.0}}};

    // Post-transform coefficients: FPx' = p[0][0]*FPx + p[1][0]*FPy + p[2][0],
    // FPy' = p[0][1]*FPx + p[1][1]*FPy + p[2][1]
    std::array<std::array<double, 2>, 3> post{
        {{1.0, 0.0}, {0.0, 1.0}, {0.0, 0.0}}};
    bool postXswap = false;

    // Chaos / Xaos transition weights (defaults to all 1.0)
    std::vector<double> modWeights;

    // Variation parameters (for julian, juliascope, pdj, blob, curl, etc.)
    VariationParams params;

    // Active variations
    const std::vector<FlameVariation>& variations() const noexcept {
        return variations_;
    }
    void clearVariations() noexcept { variations_.clear(); }
    void setVariation(VariationId id, double weight);
    double variationWeight(VariationId id) const noexcept;

    bool hasPostTransform() const noexcept;
    bool isIdentityFinal() const noexcept;

    // Affine matrix operations matching Apophysis TXForm
    void rotate(double degrees) noexcept;
    void translate(double x, double y) noexcept;
    void multiply(double a, double b, double c, double d) noexcept;
    void scale(double s) noexcept;

   private:
    std::vector<FlameVariation> variations_;
};

// Precompiled fast execution structure for a single FlameTransform
class PreparedTransform {
   public:
    static constexpr size_t kPropTableSize = 1024;

    PreparedTransform() = default;
    void prepare(const FlameTransform& xf, FastRng& rng);

    // Transform in-place
    void nextPoint(FlamePoint& p, FastRng& rng) noexcept;

    // Transform into separate output point
    void nextPointTo(const FlamePoint& in,
                     FlamePoint& out,
                     FastRng& rng) noexcept;

    int plotMode = 1;
    double opacity = 1.0;
    std::array<uint16_t, kPropTableSize>
        propTable{};  // Indices of next transform in prepared list

   private:
    void applyInternal(double inX,
                       double inY,
                       double& outX,
                       double& outY,
                       FastRng& rng) noexcept;

    double c00_ = 1.0, c01_ = 0.0, c10_ = 0.0, c11_ = 1.0, c20_ = 0.0,
           c21_ = 0.0;
    double p00_ = 1.0, p01_ = 0.0, p10_ = 0.0, p11_ = 1.0, p20_ = 0.0,
           p21_ = 0.0;
    double colorC1_ = 0.5;
    double colorC2_ = 0.0;

    bool hasPreBlur_ = false;
    double preBlurWeight_ = 0.0;
    bool calcAngle_ = false;
    bool calcSinCos_ = false;
    bool hasPost_ = false;

    // Precalculated variation constants
    double waves_f1_ = 0.0, waves_f2_ = 0.0;
    double rings_dx_ = 0.0;
    double fan_dx_ = 0.0, fan_dx2_ = 0.0;
    double polar_vpi_ = 0.0;
    double disc_vpi_ = 0.0;

    // Blob precalc
    double blob_vlow_ = 0.0, blob_vheight_ = 0.0, blob_waves_ = 0.0;
    // Curl precalc
    double curl_c1_ = 0.0, curl_c2_ = 0.0, curl_c2x2_ = 0.0;
    // Fan2 precalc
    double fan2_dx_ = 0.0, fan2_dx2_ = 0.0, fan2_dy_ = 0.0;
    // Julian precalc
    int julian_N_ = 2, julian_absN_ = 2;
    double julian_cN_ = 0.25;
    // JuliaScope precalc
    int juliascope_power_ = 2, juliascope_rN_ = 2;
    double juliascope_invDistPower_ = 0.25;
    // PDJ precalc
    double pdj_a_ = 0.0, pdj_b_ = 0.0, pdj_c_ = 0.0, pdj_d_ = 0.0;
    // Perspective precalc
    double persp_focus_ = 2.0, persp_vsin_ = 0.0, persp_vf_ = 0.0,
           persp_vfcos_ = 0.0;
    // RadialBlur precalc
    double rblur_spin_ = 0.0, rblur_zoom_ = 0.0;
    // Rectangles precalc
    double rect_x_ = 1.0, rect_y_ = 1.0;
    // Rings2 precalc
    double rings2_dx_ = 1e-10;

    // Rolling 4-sample Gaussian state
    std::array<double, 4> gaussRnd_{};
    int gaussN_ = 0;
    std::array<double, 4> radialRnd_{};
    int radialN_ = 0;

    struct ActiveVar {
        VariationId id;
        double weight;
    };
    std::vector<ActiveVar> activeVars_;
};

}  // namespace flame
