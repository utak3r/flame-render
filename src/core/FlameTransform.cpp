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
#include "core/FlameTransform.h"

#include <cmath>
#include <numbers>

namespace flame {

namespace {

constexpr double kEps = 1e-300;
constexpr double kPi = std::numbers::pi;

using Matrix3x3 = std::array<std::array<double, 3>, 3>;

Matrix3x3 identityMatrix() noexcept {
    return {{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
}

// Replicates Apophysis TXForm.Mul33 (including its exact row/col indexing)
Matrix3x3 mul33(const Matrix3x3& M1, const Matrix3x3& M2) noexcept {
    Matrix3x3 res{};
    res[0][0] = M1[0][0] * M2[0][0] + M1[0][1] * M2[1][0] + M1[0][2] * M2[2][0];
    res[0][1] = M1[0][0] * M2[0][1] + M1[0][1] * M2[1][1] + M1[0][2] * M2[2][1];
    res[0][2] = M1[0][0] * M2[0][2] + M1[0][1] * M2[1][2] + M1[0][2] * M2[2][2];
    res[1][0] = M1[1][0] * M2[0][0] + M1[1][1] * M2[1][0] + M1[1][2] * M2[2][0];
    res[1][1] = M1[1][0] * M2[0][1] + M1[1][1] * M2[1][1] + M1[1][2] * M2[2][1];
    res[1][2] = M1[1][0] * M2[0][2] + M1[1][1] * M2[1][2] + M1[1][2] * M2[2][2];
    res[2][0] = M1[2][0] * M2[0][0] + M1[2][1] * M2[1][0] + M1[2][2] * M2[2][0];
    res[2][1] = M1[2][0] * M2[0][1] + M1[2][1] * M2[1][1] + M1[2][2] * M2[2][1];
    res[2][2] = M1[2][0] * M2[0][2] + M1[2][1] * M2[1][2] + M1[2][2] * M2[2][2];
    return res;
}

void applyAffineMatrix(decltype(FlameTransform::coefs)& coefs,
                       const Matrix3x3& M1) noexcept {
    Matrix3x3 matrix = identityMatrix();
    matrix[0][0] = coefs[0][0];
    matrix[0][1] = coefs[0][1];
    matrix[1][0] = coefs[1][0];
    matrix[1][1] = coefs[1][1];
    matrix[0][2] = coefs[2][0];
    matrix[1][2] = coefs[2][1];

    matrix = mul33(matrix, M1);
    coefs[0][0] = matrix[0][0];
    coefs[0][1] = matrix[0][1];
    coefs[1][0] = matrix[1][0];
    coefs[1][1] = matrix[1][1];
    coefs[2][0] = matrix[0][2];
    coefs[2][1] = matrix[1][2];
}

}  // namespace

FlameTransform::FlameTransform() {
    clear();
}

void FlameTransform::clear() {
    weight = 0.0;
    color = 0.0;
    symmetry = 0.0;
    opacity = 1.0;
    plotMode = 1;
    postXswap = false;

    coefs = {{{1.0, 0.0}, {0.0, 1.0}, {0.0, 0.0}}};
    post = {{{1.0, 0.0}, {0.0, 1.0}, {0.0, 0.0}}};

    modWeights.assign(kMaxModWeights, 1.0);
    params = VariationParams{};
    variations_.clear();
    variations_.emplace_back(VariationId::Linear, 1.0);
}

void FlameTransform::setVariation(VariationId id, double w) {
    for (auto it = variations_.begin(); it != variations_.end(); ++it) {
        if (it->id() == id) {
            if (w == 0.0) {
                variations_.erase(it);
            } else {
                it->setWeight(w);
            }
            return;
        }
    }
    if (w != 0.0) {
        variations_.emplace_back(id, w);
    }
}

double FlameTransform::variationWeight(VariationId id) const noexcept {
    for (const auto& v : variations_) {
        if (v.id() == id)
            return v.weight();
    }
    return 0.0;
}

bool FlameTransform::hasPostTransform() const noexcept {
    return post[0][0] != 1.0 || post[0][1] != 0.0 || post[1][0] != 0.0 ||
           post[1][1] != 1.0 || post[2][0] != 0.0 || post[2][1] != 0.0;
}

bool FlameTransform::isIdentityFinal() const noexcept {
    if (coefs[0][0] != 1.0 || coefs[0][1] != 0.0 || coefs[1][0] != 0.0 ||
        coefs[1][1] != 1.0 || coefs[2][0] != 0.0 || coefs[2][1] != 0.0) {
        return false;
    }
    if (hasPostTransform())
        return false;
    if (symmetry != 1.0)
        return false;
    if (variationWeight(VariationId::Linear) != 1.0)
        return false;
    for (const auto& v : variations_) {
        if (v.id() != VariationId::Linear && v.weight() != 0.0)
            return false;
    }
    return true;
}

void FlameTransform::rotate(double degrees) noexcept {
    const double r = degrees * kPi / 180.0;
    Matrix3x3 M1 = identityMatrix();
    M1[0][0] = std::cos(r);
    M1[0][1] = -std::sin(r);
    M1[1][0] = std::sin(r);
    M1[1][1] = std::cos(r);
    applyAffineMatrix(coefs, M1);
}

void FlameTransform::translate(double x, double y) noexcept {
    Matrix3x3 M1 = identityMatrix();
    M1[0][2] = x;
    M1[1][2] = y;
    applyAffineMatrix(coefs, M1);
}

void FlameTransform::multiply(double a, double b, double c, double d) noexcept {
    Matrix3x3 M1 = identityMatrix();
    M1[0][0] = a;
    M1[0][1] = b;
    M1[1][0] = c;
    M1[1][1] = d;
    applyAffineMatrix(coefs, M1);
}

void FlameTransform::scale(double s) noexcept {
    multiply(s, 0.0, 0.0, s);
}

void PreparedTransform::prepare(const FlameTransform& xf, FastRng& rng) {
    c00_ = xf.coefs[0][0];
    c01_ = xf.coefs[0][1];
    c10_ = xf.coefs[1][0];
    c11_ = xf.coefs[1][1];
    c20_ = xf.coefs[2][0];
    c21_ = xf.coefs[2][1];

    colorC1_ = (1.0 + xf.symmetry) * 0.5;
    colorC2_ = xf.color * (1.0 - xf.symmetry) * 0.5;

    opacity = xf.opacity;
    if (xf.plotMode < 0 || opacity <= 0.0) {
        plotMode = -1;
    } else if (opacity >= 1.0) {
        plotMode = 1;
    } else {
        plotMode = 0;
    }

    hasPreBlur_ = false;
    preBlurWeight_ = 0.0;
    calcAngle_ = false;
    calcSinCos_ = false;
    activeVars_.clear();

    // Sort active variations in canonical VariationId order (0..39)
    std::array<double, kNumVariations> weights{};
    for (const auto& v : xf.variations()) {
        weights[static_cast<size_t>(v.id())] = v.weight();
    }

    for (size_t i = 0; i < kNumVariations; ++i) {
        const double w = weights[i];
        if (w == 0.0)
            continue;
        const auto id = static_cast<VariationId>(i);
        const auto& info = FlameVariation::getInfo(id);
        if (info.isPreVariation) {
            hasPreBlur_ = true;
            preBlurWeight_ = w;
        } else {
            activeVars_.push_back(ActiveVar{id, w});
        }
        if (info.needsAngle)
            calcAngle_ = true;
        if (info.needsSinCos)
            calcSinCos_ = true;
    }

    waves_f1_ = 1.0 / (c20_ * c20_ + kEps);
    waves_f2_ = 1.0 / (c21_ * c21_ + kEps);
    rings_dx_ = c20_ * c20_ + kEps;
    fan_dx_ = kPi * (c20_ * c20_ + kEps);
    fan_dx2_ = fan_dx_ * 0.5;
    polar_vpi_ = weights[static_cast<size_t>(VariationId::Polar)] / kPi;
    disc_vpi_ = weights[static_cast<size_t>(VariationId::Disc)] / kPi;

    // Blob precalc
    const double wBlob = weights[static_cast<size_t>(VariationId::Blob)];
    blob_vheight_ = wBlob * (xf.params.blob_high - xf.params.blob_low) * 0.5;
    blob_vlow_ = wBlob * xf.params.blob_low + blob_vheight_;
    blob_waves_ = xf.params.blob_waves;

    // Curl precalc
    curl_c1_ = xf.params.curl_c1;
    curl_c2_ = xf.params.curl_c2;
    curl_c2x2_ = 2.0 * curl_c2_;

    // Fan2 precalc
    fan2_dy_ = xf.params.fan2_y;
    fan2_dx_ = kPi * (xf.params.fan2_x * xf.params.fan2_x + 1e-10);
    fan2_dx2_ = fan2_dx_ * 0.5;

    // Julian precalc
    julian_N_ = static_cast<int>(std::nearbyint(xf.params.julian_power));
    if (julian_N_ == 0)
        julian_N_ = 1;
    julian_absN_ = std::abs(julian_N_);
    julian_cN_ = xf.params.julian_dist / static_cast<double>(julian_N_) / 2.0;

    // JuliaScope precalc
    juliascope_power_ =
        static_cast<int>(std::nearbyint(xf.params.juliascope_power));
    if (juliascope_power_ == 0)
        juliascope_power_ = 1;
    juliascope_rN_ = std::abs(juliascope_power_);
    juliascope_invDistPower_ = xf.params.juliascope_dist /
                               static_cast<double>(juliascope_power_) / 2.0;

    // PDJ precalc
    pdj_a_ = xf.params.pdj_a;
    pdj_b_ = xf.params.pdj_b;
    pdj_c_ = xf.params.pdj_c;
    pdj_d_ = xf.params.pdj_d;

    // Perspective precalc
    const double wPersp =
        weights[static_cast<size_t>(VariationId::Perspective)];
    persp_focus_ = xf.params.perspective_dist;
    persp_vsin_ = std::sin(xf.params.perspective_angle * kPi * 0.5);
    persp_vf_ = wPersp * persp_focus_;
    persp_vfcos_ =
        persp_vf_ * std::cos(xf.params.perspective_angle * kPi * 0.5);

    // RadialBlur precalc
    const double wRBlur = weights[static_cast<size_t>(VariationId::RadialBlur)];
    rblur_spin_ = wRBlur * std::sin(xf.params.radial_blur_angle * kPi * 0.5);
    rblur_zoom_ = wRBlur * std::cos(xf.params.radial_blur_angle * kPi * 0.5);

    // Rectangles precalc
    rect_x_ = xf.params.rectangles_x;
    rect_y_ = xf.params.rectangles_y;

    // Rings2 precalc
    rings2_dx_ = xf.params.rings2_val * xf.params.rings2_val + 1e-10;

    for (int i = 0; i < 4; ++i) {
        gaussRnd_[i] = rng.next01();
        radialRnd_[i] = rng.next01();
    }
    gaussN_ = 0;
    radialN_ = 0;

    hasPost_ = xf.hasPostTransform();
    if (hasPost_) {
        p00_ = xf.post[0][0];
        p01_ = xf.post[0][1];
        p10_ = xf.post[1][0];
        p11_ = xf.post[1][1];
        p20_ = xf.post[2][0];
        p21_ = xf.post[2][1];
    }
}

void PreparedTransform::nextPoint(FlamePoint& p, FastRng& rng) noexcept {
    p.c = p.c * colorC1_ + colorC2_;
    double outX = 0.0, outY = 0.0;
    applyInternal(p.x, p.y, outX, outY, rng);
    p.x = outX;
    p.y = outY;
}

void PreparedTransform::nextPointTo(const FlamePoint& in,
                                    FlamePoint& out,
                                    FastRng& rng) noexcept {
    out.c = in.c * colorC1_ + colorC2_;
    applyInternal(in.x, in.y, out.x, out.y, rng);
}

void PreparedTransform::applyInternal(
    double inX, double inY, double& outX, double& outY, FastRng& rng) noexcept {
    double FTx = c00_ * inX + c10_ * inY + c20_;
    double FTy = c01_ * inX + c11_ * inY + c21_;

    double FPx = 0.0;
    double FPy = 0.0;

    // 1. Pre-variations (pre_blur)
    if (hasPreBlur_) {
        const double r = preBlurWeight_ * (gaussRnd_[0] + gaussRnd_[1] +
                                           gaussRnd_[2] + gaussRnd_[3] - 2.0);
        const double u = rng.next01();
        gaussRnd_[gaussN_] = u;
        gaussN_ = (gaussN_ + 1) & 3;
        const double a = u * 2.0 * kPi;
        FTx += r * std::cos(a);
        FTy += r * std::sin(a);
    }

    // 2. Precalc Angle & SinCos
    double FAngle = 0.0;
    double FLength = 0.0;
    double FSinA = 0.0;
    double FCosA = 0.0;

    if (calcAngle_) {
        FAngle = std::atan2(FTx, FTy);
    }
    if (calcSinCos_) {
        FLength = std::sqrt(FTx * FTx + FTy * FTy) + kEps;
        FSinA = FTx / FLength;
        FCosA = FTy / FLength;
    }

    // 3. Normal variations
    for (const auto& av : activeVars_) {
        const double w = av.weight;
        switch (av.id) {
            case VariationId::Linear:
                {
                    FPx += w * FTx;
                    FPy += w * FTy;
                    break;
                }
            case VariationId::Sinusoidal:
                {
                    FPx += w * std::sin(FTx);
                    FPy += w * std::sin(FTy);
                    break;
                }
            case VariationId::Spherical:
                {
                    const double r = w / (FTx * FTx + FTy * FTy + kEps);
                    FPx += FTx * r;
                    FPy += FTy * r;
                    break;
                }
            case VariationId::Swirl:
                {
                    const double r2 = FTx * FTx + FTy * FTy;
                    const double sinr = std::sin(r2);
                    const double cosr = std::cos(r2);
                    FPx += w * (sinr * FTx - cosr * FTy);
                    FPy += w * (cosr * FTx + sinr * FTy);
                    break;
                }
            case VariationId::Horseshoe:
                {
                    const double r =
                        w / (std::sqrt(FTx * FTx + FTy * FTy) + kEps);
                    FPx += (FTx - FTy) * (FTx + FTy) * r;
                    FPy += (2.0 * FTx * FTy) * r;
                    break;
                }
            case VariationId::Polar:
                {
                    FPx += polar_vpi_ * FAngle;
                    FPy += w * (std::sqrt(FTx * FTx + FTy * FTy) - 1.0);
                    break;
                }
            case VariationId::Handkerchief:
                {
                    const double r = std::sqrt(FTx * FTx + FTy * FTy);
                    FPx += w * std::sin(FAngle + r) * r;
                    FPy += w * std::cos(FAngle - r) * r;
                    break;
                }
            case VariationId::Heart:
                {
                    const double r = std::sqrt(FTx * FTx + FTy * FTy);
                    const double a = r * FAngle;
                    const double rw = r * w;
                    FPx += rw * std::sin(a);
                    FPy -= rw * std::cos(a);
                    break;
                }
            case VariationId::Disc:
                {
                    const double a = kPi * std::sqrt(FTx * FTx + FTy * FTy);
                    const double r = disc_vpi_ * FAngle;
                    FPx += std::sin(a) * r;
                    FPy += std::cos(a) * r;
                    break;
                }
            case VariationId::Spiral:
                {
                    const double r0 = FLength + kEps;
                    const double sinr = std::sin(r0);
                    const double cosr = std::cos(r0);
                    const double r = w / r0;
                    FPx += (FCosA + sinr) * r;
                    FPy += (FSinA - cosr) * r;
                    break;
                }
            case VariationId::Hyperbolic:
                {
                    FPx += w * FTx / (FTx * FTx + FTy * FTy + kEps);
                    FPy += w * FTy;
                    break;
                }
            case VariationId::Diamond:
                {
                    const double sinr = std::sin(FLength);
                    const double cosr = std::cos(FLength);
                    FPx += w * FSinA * cosr;
                    FPy += w * FCosA * sinr;
                    break;
                }
            case VariationId::Ex:
                {
                    const double r = std::sqrt(FTx * FTx + FTy * FTy);
                    const double n0 = std::sin(FAngle + r);
                    const double n1 = std::cos(FAngle - r);
                    const double m0 = n0 * n0 * n0;
                    const double m1 = n1 * n1 * n1;
                    const double rw = r * w;
                    FPx += rw * (m0 + m1);
                    FPy += rw * (m0 - m1);
                    break;
                }
            case VariationId::Julia:
                {
                    const double a = FAngle * 0.5 + (rng.nextBit() ? kPi : 0.0);
                    const double r =
                        w * std::sqrt(std::sqrt(FTx * FTx + FTy * FTy));
                    FPx += r * std::cos(a);
                    FPy += r * std::sin(a);
                    break;
                }
            case VariationId::Bent:
                {
                    FPx += w * (FTx < 0.0 ? FTx * 2.0 : FTx);
                    FPy += w * (FTy < 0.0 ? FTy * 0.5 : FTy);
                    break;
                }
            case VariationId::Waves:
                {
                    FPx += w * (FTx + c10_ * std::sin(FTy * waves_f1_));
                    FPy += w * (FTy + c11_ * std::sin(FTx * waves_f2_));
                    break;
                }
            case VariationId::Fisheye:
                {
                    const double r =
                        2.0 * w / (std::sqrt(FTx * FTx + FTy * FTy) + 1.0);
                    FPx += r * FTy;
                    FPy += r * FTx;
                    break;
                }
            case VariationId::Popcorn:
                {
                    const double dx = std::tan(3.0 * FTy);
                    const double dy = std::tan(3.0 * FTx);
                    FPx +=
                        w * (FTx + c20_ * std::sin(std::isnan(dx) ? 0.0 : dx));
                    FPy +=
                        w * (FTy + c21_ * std::sin(std::isnan(dy) ? 0.0 : dy));
                    break;
                }
            case VariationId::Exponential:
                {
                    const double d = w * std::exp(FTx - 1.0);
                    const double a = kPi * FTy;
                    FPx += std::cos(a) * d;
                    FPy += std::sin(a) * d;
                    break;
                }
            case VariationId::Power:
                {
                    const double r = w * std::pow(FLength, FSinA);
                    FPx += r * FCosA;
                    FPy += r * FSinA;
                    break;
                }
            case VariationId::Cosine:
                {
                    const double a = FTx * kPi;
                    FPx += w * std::cos(a) * std::cosh(FTy);
                    FPy -= w * std::sin(a) * std::sinh(FTy);
                    break;
                }
            case VariationId::Rings:
                {
                    const double r =
                        w *
                        (2.0 * FLength -
                         rings_dx_ *
                             (std::trunc((FLength / rings_dx_ + 1.0) * 0.5) *
                                  2.0 +
                              FLength));
                    FPx += r * FCosA;
                    FPy += r * FSinA;
                    break;
                }
            case VariationId::Fan:
                {
                    const double t = (FAngle + c21_) / fan_dx_;
                    const double frac = t - std::trunc(t);
                    const double a = (frac > 0.5) ? (FAngle - fan_dx2_)
                                                  : (FAngle + fan_dx2_);
                    const double r = w * std::sqrt(FTx * FTx + FTy * FTy);
                    FPx += r * std::cos(a);
                    FPy += r * std::sin(a);
                    break;
                }
            case VariationId::Eyefish:
                {
                    const double r =
                        2.0 * w / (std::sqrt(FTx * FTx + FTy * FTy) + 1.0);
                    FPx += r * FTx;
                    FPy += r * FTy;
                    break;
                }
            case VariationId::Bubble:
                {
                    const double r = w / ((FTx * FTx + FTy * FTy) * 0.25 + 1.0);
                    FPx += r * FTx;
                    FPy += r * FTy;
                    break;
                }
            case VariationId::Cylinder:
                {
                    FPx += w * std::sin(FTx);
                    FPy += w * FTy;
                    break;
                }
            case VariationId::Noise:
                {
                    const double r = w * rng.next01();
                    const double a = rng.next01() * 2.0 * kPi;
                    FPx += FTx * r * std::cos(a);
                    FPy += FTy * r * std::sin(a);
                    break;
                }
            case VariationId::Blur:
                {
                    const double r = w * rng.next01();
                    const double a = rng.next01() * 2.0 * kPi;
                    FPx += r * std::cos(a);
                    FPy += r * std::sin(a);
                    break;
                }
            case VariationId::GaussianBlur:
                {
                    const double r = w * (gaussRnd_[0] + gaussRnd_[1] +
                                          gaussRnd_[2] + gaussRnd_[3] - 2.0);
                    const double u = rng.next01();
                    gaussRnd_[gaussN_] = u;
                    gaussN_ = (gaussN_ + 1) & 3;
                    const double a = u * 2.0 * kPi;
                    FPx += r * std::cos(a);
                    FPy += r * std::sin(a);
                    break;
                }
            case VariationId::PreBlur:
                break;  // Handled in step 1

            case VariationId::Blob:
                {
                    const double r =
                        blob_vlow_ +
                        blob_vheight_ *
                            std::sin(blob_waves_ * std::atan2(FTx, FTy));
                    FPx += r * FTx;
                    FPy += r * FTy;
                    break;
                }
            case VariationId::Curl:
                {
                    const double re = 1.0 + curl_c1_ * FTx +
                                      curl_c2_ * (FTx * FTx - FTy * FTy);
                    const double im = curl_c1_ * FTy + curl_c2x2_ * FTx * FTy;
                    const double r = w / (re * re + im * im + kEps);
                    FPx += (FTx * re + FTy * im) * r;
                    FPy += (FTy * re - FTx * im) * r;
                    break;
                }
            case VariationId::Fan2:
                {
                    const double angle = std::atan2(FTx, FTy);
                    const double t = (angle + fan2_dy_) / fan2_dx_;
                    const double frac = t - std::trunc(t);
                    const double a = (frac > 0.5) ? (angle - fan2_dx2_)
                                                  : (angle + fan2_dx2_);
                    const double r = w * std::sqrt(FTx * FTx + FTy * FTy);
                    // Note: varFan2.pas x87 FSINCOS stores cos into sinr and
                    // sin into cosr, resulting in:
                    FPx += r * std::sin(a);
                    FPy += r * std::cos(a);
                    break;
                }
            case VariationId::Julian:
                {
                    const uint32_t k =
                        rng.nextInt(static_cast<uint32_t>(julian_absN_));
                    const double a = (std::atan2(FTy, FTx) +
                                      2.0 * kPi * static_cast<double>(k)) /
                                     static_cast<double>(julian_N_);
                    const double r2 = FTx * FTx + FTy * FTy;
                    const double r =
                        (r2 > 0.0) ? (w * std::pow(r2, julian_cN_)) : 0.0;
                    FPx += r * std::cos(a);
                    FPy += r * std::sin(a);
                    break;
                }
            case VariationId::JuliaScope:
                {
                    const uint32_t rnd =
                        rng.nextInt(static_cast<uint32_t>(juliascope_rN_));
                    const double baseAngle = std::atan2(FTy, FTx);
                    const double a =
                        ((rnd & 1U) == 0U)
                            ? ((2.0 * kPi * static_cast<double>(rnd) +
                                baseAngle) /
                               static_cast<double>(juliascope_power_))
                            : ((2.0 * kPi * static_cast<double>(rnd) -
                                baseAngle) /
                               static_cast<double>(juliascope_power_));
                    const double r2 = FTx * FTx + FTy * FTy;
                    const double r =
                        (r2 > 0.0)
                            ? (w * std::pow(r2, juliascope_invDistPower_))
                            : 0.0;
                    FPx += r * std::cos(a);
                    FPy += r * std::sin(a);
                    break;
                }
            case VariationId::Pdj:
                {
                    FPx +=
                        w * (std::sin(pdj_a_ * FTy) - std::cos(pdj_b_ * FTx));
                    FPy +=
                        w * (std::sin(pdj_c_ * FTx) - std::cos(pdj_d_ * FTy));
                    break;
                }
            case VariationId::Perspective:
                {
                    const double t = persp_focus_ - persp_vsin_ * FTy;
                    if (std::abs(t) > kEps) {
                        FPx += persp_vf_ * FTx / t;
                        FPy += persp_vfcos_ * FTy / t;
                    }
                    break;
                }
            case VariationId::RadialBlur:
                {
                    const double rndG = radialRnd_[0] + radialRnd_[1] +
                                        radialRnd_[2] + radialRnd_[3] - 2.0;
                    radialRnd_[radialN_] = rng.next01();
                    radialN_ = (radialN_ + 1) & 3;
                    const double ra = std::sqrt(FTx * FTx + FTy * FTy);
                    const double a = std::atan2(FTy, FTx) + rblur_spin_ * rndG;
                    const double rz = rblur_zoom_ * rndG - 1.0;
                    FPx += ra * std::cos(a) + rz * FTx;
                    FPy += ra * std::sin(a) + rz * FTy;
                    break;
                }
            case VariationId::Rectangles:
                {
                    FPx += w * ((std::abs(rect_x_) < 1e-12)
                                    ? FTx
                                    : ((2.0 * std::floor(FTx / rect_x_) + 1.0) *
                                           rect_x_ -
                                       FTx));
                    FPy += w * ((std::abs(rect_y_) < 1e-12)
                                    ? FTy
                                    : ((2.0 * std::floor(FTy / rect_y_) + 1.0) *
                                           rect_y_ -
                                       FTy));
                    break;
                }
            case VariationId::Rings2:
                {
                    const double len = std::sqrt(FTx * FTx + FTy * FTy) + kEps;
                    const double r =
                        w *
                        (2.0 - rings2_dx_ *
                                   (std::trunc((len / rings2_dx_ + 1.0) * 0.5) *
                                        2.0 / len +
                                    1.0));
                    FPx += r * FTx;
                    FPy += r * FTy;
                    break;
                }
            case VariationId::Count:
                break;
        }
    }

    // 4. Post-affine transform
    if (hasPost_) {
        const double tmpX = FPx;
        FPx = p00_ * tmpX + p10_ * FPy + p20_;
        FPy = p01_ * tmpX + p11_ * FPy + p21_;
    }

    outX = FPx;
    outY = FPy;
}

}  // namespace flame
