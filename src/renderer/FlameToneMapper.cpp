#include "renderer/FlameToneMapper.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace flame {

void FlameToneMapper::createFilter(int oversample, double filterRadius) {
    oversample_ = std::max(1, oversample);
    const int fw = static_cast<int>(std::trunc(
        2.0 * FlameFractal::kFilterCutoff * oversample_ * filterRadius));
    filterSize_ = fw + 1;

    // Make sure filterSize has same parity as oversample
    if (((filterSize_ + oversample_) & 1) != 0) {
        ++filterSize_;
    }

    const double adjust =
        (fw > 0) ? (1.0 * FlameFractal::kFilterCutoff * filterSize_) /
                       static_cast<double>(fw)
                 : 1.0;

    filter_.assign(static_cast<size_t>(filterSize_) * filterSize_, 0.0);
    double sum = 0.0;
    for (int i = 0; i < filterSize_; ++i) {
        for (int j = 0; j < filterSize_; ++j) {
            const double ii = ((2.0 * i + 1.0) / filterSize_ - 1.0) * adjust;
            const double jj = ((2.0 * j + 1.0) / filterSize_ - 1.0) * adjust;
            const double val = std::exp(-2.0 * (ii * ii + jj * jj));
            filter_[static_cast<size_t>(i) * filterSize_ + j] = val;
            sum += val;
        }
    }

    if (sum > 0.0) {
        for (double& v : filter_) {
            v /= sum;
        }
    }
}

FlameImage FlameToneMapper::createImage(const FlameFractal& cp,
                                        double actualDensity,
                                        const std::vector<Bucket64>& buckets,
                                        int bucketWidth,
                                        int bucketHeight) const {
    FlameImage img(cp.width, cp.height);
    if (cp.width <= 0 || cp.height <= 0 || buckets.empty()) {
        return img;
    }

    const double gamma = (cp.gamma == 0.0) ? 0.0 : (1.0 / cp.gamma);
    const int64_t vib = apoRound(cp.vibrancy * 256.0);
    const int64_t notvib = 256 - vib;

    double funcval = 0.0;
    if (cp.gammaThreshold != 0.0) {
        funcval = std::pow(cp.gammaThreshold, gamma - 1.0);
    }

    const int64_t bgi[3] = {cp.background[0], cp.background[1],
                            cp.background[2]};
    const PixelRGBA bgOpaque{
        static_cast<uint8_t>(std::clamp<int64_t>(bgi[0], 0, 255)),
        static_cast<uint8_t>(std::clamp<int64_t>(bgi[1], 0, 255)),
        static_cast<uint8_t>(std::clamp<int64_t>(bgi[2], 0, 255)), 255};
    const PixelRGBA zeroBg{0, 0, 0, 0};

    const int gutterWidth = bucketWidth - oversample_ * cp.width;
    const bool useFastBucket = (filterSize_ <= gutterWidth / 2);

    auto getBucket = [&](int x, int y) noexcept -> const Bucket64& {
        if (!useFastBucket) {
            x = std::clamp(x, 0, bucketWidth - 1);
            y = std::clamp(y, 0, bucketHeight - 1);
        }
        return buckets[static_cast<size_t>(y) * bucketWidth + x];
    };

    const double zoomScale = std::pow(2.0, cp.zoom);
    double sampleDensity = actualDensity * zoomScale * zoomScale;
    if (sampleDensity == 0.0) {
        sampleDensity = 0.001;
    }

    constexpr double kPrefilterWhite =
        static_cast<double>(FlameFractal::kPrefilterWhite);
    const double k1 = (cp.contrast * FlameFractal::kBrightAdjust *
                       cp.brightness * 268.0 * kPrefilterWhite) /
                      256.0;
    const double area =
        (static_cast<double>(cp.width) * static_cast<double>(cp.height)) /
        (cp.ppux() * cp.ppuy());
    const double k2 = static_cast<double>(oversample_ * oversample_) /
                      (cp.contrast * area * static_cast<double>(cp.whiteLevel) *
                       sampleDensity);

    std::array<double, 1025> lsa{};
    lsa[0] = 0.0;
    for (int i = 1; i <= 1024; ++i) {
        const double wl_i =
            static_cast<double>(cp.whiteLevel) * static_cast<double>(i);
        lsa[static_cast<size_t>(i)] = (k1 * std::log10(1.0 + wl_i * k2)) / wl_i;
    }

    int by = 0;
    for (int i = 0; i < cp.height; ++i) {
        int bx = 0;
        for (int j = 0; j < cp.width; ++j) {
            double fp[4] = {0.0, 0.0, 0.0, 0.0};

            if (filterSize_ > 1) {
                for (int ii = 0; ii < filterSize_; ++ii) {
                    const size_t filterRowOffset =
                        static_cast<size_t>(ii) * filterSize_;
                    for (int jj = 0; jj < filterSize_; ++jj) {
                        const double filterValue =
                            filter_[filterRowOffset + jj];
                        const Bucket64& b = getBucket(bx + jj, by + ii);
                        if (b.count == 0)
                            continue;

                        double ls = 0.0;
                        if (b.count < 1024) {
                            ls = lsa[static_cast<size_t>(b.count)];
                        } else {
                            const double wl_c =
                                static_cast<double>(cp.whiteLevel) *
                                static_cast<double>(b.count);
                            ls = (k1 * std::log10(1.0 + wl_c * k2)) / wl_c;
                        }

                        const double fls = filterValue * ls;
                        fp[0] += fls * static_cast<double>(b.red);
                        fp[1] += fls * static_cast<double>(b.green);
                        fp[2] += fls * static_cast<double>(b.blue);
                        fp[3] += fls * static_cast<double>(b.count);
                    }
                }
                fp[0] /= kPrefilterWhite;
                fp[1] /= kPrefilterWhite;
                fp[2] /= kPrefilterWhite;
                fp[3] = static_cast<double>(cp.whiteLevel) * fp[3] /
                        kPrefilterWhite;
            } else {
                const Bucket64& b = getBucket(bx, by);
                double ls = 0.0;
                if (b.count > 0) {
                    if (b.count < 1024) {
                        ls =
                            lsa[static_cast<size_t>(b.count)] / kPrefilterWhite;
                    } else {
                        const double wl_c = static_cast<double>(cp.whiteLevel) *
                                            static_cast<double>(b.count);
                        ls = (k1 * std::log10(1.0 + wl_c * k2)) / wl_c /
                             kPrefilterWhite;
                    }
                }
                fp[0] = ls * static_cast<double>(b.red);
                fp[1] = ls * static_cast<double>(b.green);
                fp[2] = ls * static_cast<double>(b.blue);
                fp[3] = ls * static_cast<double>(b.count) *
                        static_cast<double>(cp.whiteLevel);
            }

            bx += oversample_;

            if (cp.transparency) {
                if (fp[3] > 0.0) {
                    double alpha = 0.0;
                    if (fp[3] <= cp.gammaThreshold) {
                        const double frac = fp[3] / cp.gammaThreshold;
                        alpha = (1.0 - frac) * fp[3] * funcval +
                                frac * std::pow(fp[3], gamma);
                    } else {
                        alpha = std::pow(fp[3], gamma);
                    }

                    const double ls = static_cast<double>(vib) * alpha / fp[3];
                    int64_t ai = apoRound(alpha * 256.0);
                    if (ai <= 0) {
                        img.setPixel(j, i, zeroBg);
                        continue;
                    }
                    if (ai > 255)
                        ai = 255;

                    int64_t ri = 0, gi = 0, bi = 0;
                    if (notvib > 0) {
                        ri =
                            apoRound(ls * fp[0] +
                                     static_cast<double>(notvib) *
                                         std::pow(std::max(0.0, fp[0]), gamma));
                        gi =
                            apoRound(ls * fp[1] +
                                     static_cast<double>(notvib) *
                                         std::pow(std::max(0.0, fp[1]), gamma));
                        bi =
                            apoRound(ls * fp[2] +
                                     static_cast<double>(notvib) *
                                         std::pow(std::max(0.0, fp[2]), gamma));
                    } else {
                        ri = apoRound(ls * fp[0]);
                        gi = apoRound(ls * fp[1]);
                        bi = apoRound(ls * fp[2]);
                    }

                    ri = (ri * 255) / ai;
                    gi = (gi * 255) / ai;
                    bi = (bi * 255) / ai;

                    ri = std::clamp<int64_t>(ri, 0, 255);
                    gi = std::clamp<int64_t>(gi, 0, 255);
                    bi = std::clamp<int64_t>(bi, 0, 255);

                    img.setPixel(j, i,
                                 PixelRGBA{static_cast<uint8_t>(ri),
                                           static_cast<uint8_t>(gi),
                                           static_cast<uint8_t>(bi),
                                           static_cast<uint8_t>(ai)});
                } else {
                    img.setPixel(j, i, zeroBg);
                }
            } else {
                if (fp[3] > 0.0) {
                    double alpha = 0.0;
                    if (fp[3] <= cp.gammaThreshold) {
                        const double frac = fp[3] / cp.gammaThreshold;
                        alpha = (1.0 - frac) * fp[3] * funcval +
                                frac * std::pow(fp[3], gamma);
                    } else {
                        alpha = std::pow(fp[3], gamma);
                    }

                    const double ls = static_cast<double>(vib) * alpha / fp[3];
                    int64_t ai = apoRound(alpha * 256.0);
                    ai = std::clamp<int64_t>(ai, 0, 255);
                    const int64_t ia = 255 - ai;

                    int64_t ri = 0, gi = 0, bi = 0;
                    if (notvib > 0) {
                        ri =
                            apoRound(ls * fp[0] +
                                     static_cast<double>(notvib) *
                                         std::pow(std::max(0.0, fp[0]), gamma));
                        gi =
                            apoRound(ls * fp[1] +
                                     static_cast<double>(notvib) *
                                         std::pow(std::max(0.0, fp[1]), gamma));
                        bi =
                            apoRound(ls * fp[2] +
                                     static_cast<double>(notvib) *
                                         std::pow(std::max(0.0, fp[2]), gamma));
                    } else {
                        ri = apoRound(ls * fp[0]);
                        gi = apoRound(ls * fp[1]);
                        bi = apoRound(ls * fp[2]);
                    }

                    ri += (ia * bgi[0]) >> 8;
                    gi += (ia * bgi[1]) >> 8;
                    bi += (ia * bgi[2]) >> 8;

                    ri = std::clamp<int64_t>(ri, 0, 255);
                    gi = std::clamp<int64_t>(gi, 0, 255);
                    bi = std::clamp<int64_t>(bi, 0, 255);

                    img.setPixel(j, i,
                                 PixelRGBA{static_cast<uint8_t>(ri),
                                           static_cast<uint8_t>(gi),
                                           static_cast<uint8_t>(bi), 255});
                } else {
                    img.setPixel(j, i, bgOpaque);
                }
            }
        }
        by += oversample_;
    }

    return img;
}

RenderStats FlameToneMapper::computeBucketStats(
    const std::vector<Bucket64>& buckets,
    int64_t totalSamples,
    double renderTimeSeconds) noexcept {
    RenderStats stats{};
    stats.totalSamples = totalSamples;
    stats.renderTimeSeconds = renderTimeSeconds;

    int64_t maxR = 0, maxG = 0, maxB = 0, maxA = 0, totalA = 0;
    for (const auto& b : buckets) {
        maxR = std::max(maxR, b.red);
        maxG = std::max(maxG, b.green);
        maxB = std::max(maxB, b.blue);
        maxA = std::max(maxA, b.count);
        totalA += b.count;
    }

    stats.totalHits = totalA;
    if (totalSamples > 0) {
        stats.hitRatioPercent = 100.0 * static_cast<double>(totalA) /
                                static_cast<double>(totalSamples);
    }
    if (renderTimeSeconds > 0.0) {
        stats.iterationsPerSecond =
            static_cast<double>(totalSamples) / renderTimeSeconds;
    }
    stats.maxRedBits = (maxR > 0) ? std::log2(static_cast<double>(maxR)) : 0.0;
    stats.maxGreenBits =
        (maxG > 0) ? std::log2(static_cast<double>(maxG)) : 0.0;
    stats.maxBlueBits = (maxB > 0) ? std::log2(static_cast<double>(maxB)) : 0.0;
    stats.maxCountBits =
        (maxA > 0) ? std::log2(static_cast<double>(maxA)) : 0.0;
    return stats;
}

}  // namespace flame
