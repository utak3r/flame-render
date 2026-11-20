#include "renderer/FlameCpuRenderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

namespace flame {

namespace {

struct CameraParams {
    double camX0 = 0.0;
    double camY0 = 0.0;
    double camX1 = 0.0;
    double camY1 = 0.0;
    double camW = 0.0;
    double camH = 0.0;
    double bws = 0.0;
    double bhs = 0.0;
    double cosa = 1.0;
    double sina = 0.0;
    double rcX = 0.0;
    double rcY = 0.0;
    double sampleDensity = 50.0;
    bool hasRotation = false;
};

CameraParams createCamera(const FlameFractal& cp,
                          int bucketWidth,
                          int bucketHeight,
                          int gutterWidth,
                          int maxGutterWidth) noexcept {
    CameraParams cam{};
    const double scale = std::pow(2.0, cp.zoom);
    cam.sampleDensity = cp.quality * scale * scale;
    double ppux = cp.scale * scale;
    double ppuy = cp.scale * scale;
    if (!std::isfinite(ppux) || std::abs(ppux) < 1e-12) {
        ppux = 1.0;
    }
    if (!std::isfinite(ppuy) || std::abs(ppuy) < 1e-12) {
        ppuy = 1.0;
    }

    const double cornerX =
        cp.center[0] - static_cast<double>(cp.width) / ppux / 2.0;
    const double cornerY =
        cp.center[1] - static_cast<double>(cp.height) / ppuy / 2.0;
    const double t0 = static_cast<double>(gutterWidth) / (cp.oversample * ppux);
    const double t1 = static_cast<double>(gutterWidth) / (cp.oversample * ppuy);
    const double t2 = static_cast<double>(2 * maxGutterWidth - gutterWidth) /
                      (cp.oversample * ppux);
    const double t3 = static_cast<double>(2 * maxGutterWidth - gutterWidth) /
                      (cp.oversample * ppuy);

    cam.camX0 = cornerX - t0;
    cam.camY0 = cornerY - t1;
    cam.camX1 = cornerX + static_cast<double>(cp.width) / ppux + t2;
    cam.camY1 = cornerY + static_cast<double>(cp.height) / ppuy + t3;

    cam.camW = cam.camX1 - cam.camX0;
    const double xSize = (std::abs(cam.camW) > 0.01) ? (1.0 / cam.camW) : 1.0;
    cam.camH = cam.camY1 - cam.camY0;
    const double ySize = (std::abs(cam.camH) > 0.01) ? (1.0 / cam.camH) : 1.0;

    cam.bws = (static_cast<double>(bucketWidth) - 0.5) * xSize;
    cam.bhs = (static_cast<double>(bucketHeight) - 0.5) * ySize;

    if (cp.angle != 0.0) {
        cam.hasRotation = true;
        cam.cosa = std::cos(cp.angle);
        cam.sina = std::sin(cp.angle);
        cam.rcX = cp.center[0] * (1.0 - cam.cosa) - cp.center[1] * cam.sina -
                  cam.camX0;
        cam.rcY = cp.center[1] * (1.0 - cam.cosa) + cp.center[0] * cam.sina -
                  cam.camY0;
    }
    return cam;
}

std::vector<PreparedTransform> buildPreparedTransforms(const FlameFractal& cp,
                                                       FastRng& rng) {
    const size_t n = cp.xforms.size();
    std::vector<PreparedTransform> prepared(n);
    for (size_t i = 0; i < n; ++i) {
        prepared[i].prepare(cp.xforms[i], rng);
    }

    if (cp.soloXform >= 0 && static_cast<size_t>(cp.soloXform) < n) {
        for (size_t i = 0; i < n; ++i) {
            prepared[i].plotMode = -1;
        }
        prepared[static_cast<size_t>(cp.soloXform)].plotMode = 1;
    }

    // Build 1024-entry PropTable for each transform
    std::vector<double> tp(n, 0.0);
    for (size_t k = 0; k < n; ++k) {
        double totValue = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const double mw = (i < cp.xforms[k].modWeights.size())
                                  ? cp.xforms[k].modWeights[i]
                                  : 1.0;
            tp[i] = cp.xforms[i].weight * mw;
            totValue += tp[i];
        }

        if (totValue > 0.0) {
            double loopValue = 0.0;
            const double step =
                totValue /
                static_cast<double>(PreparedTransform::kPropTableSize);
            for (size_t i = 0; i < PreparedTransform::kPropTableSize; ++i) {
                double propsum = 0.0;
                size_t j = 0;
                for (; j < n; ++j) {
                    propsum += tp[j];
                    if (propsum > loopValue || j == n - 1) {
                        break;
                    }
                }
                prepared[k].propTable[i] = static_cast<uint16_t>(j);
                loopValue += step;
            }
        } else {
            prepared[k].propTable.fill(0);
        }
    }

    return prepared;
}

}  // namespace

void FlameCpuRenderer::setFractal(const FlameFractal* fractal) {
    fractal_ = fractal;
}

void FlameCpuRenderer::stop() {
    stopRequested_.store(true, std::memory_order_relaxed);
}

FlameFractal FlameCpuRenderer::resolveEffectiveFractal(
    const FlameFractal& base, const RenderOptions& options) {
    FlameFractal cp = base;

    const int targetW = (options.width > 0) ? options.width : cp.width;
    const int targetH = (options.height > 0) ? options.height : cp.height;
    if (targetW != cp.width || targetH != cp.height) {
        cp.adjustScale(targetW, targetH);
    }

    if (options.quality > 0.0) {
        cp.quality = options.quality;
    }
    if (options.oversample > 0) {
        cp.oversample = options.oversample;
    }
    if (options.filterRadius > 0.0) {
        cp.filterRadius = options.filterRadius;
    }
    if (options.transparent.has_value()) {
        cp.transparency = *options.transparent;
    }

    cp.oversample = std::clamp(cp.oversample, 1, kMaxFilterWidth);
    if (!std::isfinite(cp.filterRadius) || cp.filterRadius < 0.0) {
        cp.filterRadius = 0.0;
    }
    if (!std::isfinite(cp.scale) || std::abs(cp.scale) < 1e-12) {
        cp.scale = 50.0;
    }
    if (!std::isfinite(cp.contrast) || cp.contrast <= 0.0) {
        cp.contrast = 1.0;
    }
    if (cp.whiteLevel <= 0) {
        cp.whiteLevel = FlameFractal::kDefaultWhiteLevel;
    }
    if (!std::isfinite(cp.gammaThreshold) || cp.gammaThreshold < 0.0) {
        cp.gammaThreshold = 0.0;
    }
    if (!std::isfinite(cp.quality) || cp.quality <= 0.0) {
        cp.quality = 1.0;
    }

    return cp;
}

FlameImage FlameCpuRenderer::render(const RenderOptions& options) {
    if (!fractal_) {
        throw std::runtime_error(
            "FlameCpuRenderer::render called without a FlameFractal set");
    }

    stopRequested_.store(false, std::memory_order_relaxed);
    FlameFractal cp = resolveEffectiveFractal(*fractal_, options);

    if (cp.xforms.empty() || cp.width <= 0 || cp.height <= 0) {
        return FlameImage(cp.width, cp.height);
    }

    FlameToneMapper toneMapper;
    toneMapper.createFilter(cp.oversample, cp.filterRadius);

    // CalcBufferSize
    const int maxGutterWidth = (kMaxFilterWidth - cp.oversample) / 2;
    const int gutterWidth = (toneMapper.filterSize() - cp.oversample) / 2;
    const int bucketWidth = cp.oversample * cp.width + 2 * maxGutterWidth;
    const int bucketHeight = cp.oversample * cp.height + 2 * maxGutterWidth;
    const int64_t bucketSize =
        static_cast<int64_t>(bucketWidth) * static_cast<int64_t>(bucketHeight);

    const CameraParams cam = createCamera(cp, bucketWidth, bucketHeight,
                                          gutterWidth, maxGutterWidth);
    cp.gradient.applyHueRotation(cp.hueRotation);
    const auto colorMap = cp.gradient.createScaledColorMap(cp.whiteLevel);

    // Calculate batch count
    const int64_t nSamples =
        apoRound(cam.sampleDensity * static_cast<double>(bucketSize) /
                 static_cast<double>(cp.oversample * cp.oversample));
    int64_t numBatches =
        apoRound(static_cast<double>(nSamples) /
                 static_cast<double>(std::max(1, cp.nbatches) *
                                     FlameFractal::kSubBatchSize));
    if (numBatches <= 0)
        numBatches = 1;

    uint32_t threadCount = options.numThreads;
    if (threadCount == 0) {
        threadCount = std::max(1u, std::thread::hardware_concurrency());
    }
    threadCount =
        static_cast<uint32_t>(std::min<int64_t>(threadCount, numBatches));

    const uint64_t baseSeed =
        options.seed.has_value() ? *options.seed : std::random_device{}();
    const bool useFinal = cp.useFinalXform();

    const auto startTime = std::chrono::steady_clock::now();

    std::vector<std::vector<Bucket64>> threadBuckets(
        threadCount, std::vector<Bucket64>(static_cast<size_t>(bucketSize)));
    std::atomic<int64_t> nextBatch{0};
    std::atomic<int64_t> completedBatches{0};

    auto workerFunc = [&](uint32_t tid) {
        FastRng rng(baseSeed +
                    static_cast<uint64_t>(tid) * 0x9e3779b97f4a7c15ULL);
        auto preparedXforms = buildPreparedTransforms(cp, rng);
        PreparedTransform preparedFinal;
        if (useFinal) {
            preparedFinal.prepare(*cp.finalXform, rng);
        }

        auto& localBuckets = threadBuckets[tid];
        int64_t deterministicBatch = static_cast<int64_t>(tid);

        while (!stopRequested_.load(std::memory_order_relaxed)) {
            int64_t batchIdx = 0;
            if (options.seed.has_value()) {
                batchIdx = deterministicBatch;
                deterministicBatch += static_cast<int64_t>(threadCount);
            } else {
                batchIdx = nextBatch.fetch_add(1, std::memory_order_relaxed);
            }
            if (batchIdx >= numBatches) {
                break;
            }

            // Initialize batch starting point
            FlamePoint p{rng.nextSigned(), rng.nextSigned(), rng.next01()};

            size_t xfIdx = 0;
            preparedXforms[xfIdx].nextPoint(p, rng);
            for (int i = 0; i <= FlameFractal::kFuseIterations; ++i) {
                xfIdx = preparedXforms[xfIdx].propTable[rng.nextInt(
                    PreparedTransform::kPropTableSize)];
                preparedXforms[xfIdx].nextPoint(p, rng);
            }

            FlamePoint q{};
            for (int i = 0; i < FlameFractal::kSubBatchSize; ++i) {
                xfIdx = preparedXforms[xfIdx].propTable[rng.nextInt(
                    PreparedTransform::kPropTableSize)];
                auto& xf = preparedXforms[xfIdx];
                xf.nextPoint(p, rng);

                if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
                    // Recover from math overflow
                    break;
                }

                if (xf.plotMode < 0)
                    continue;
                if (xf.plotMode == 0 && rng.next01() > xf.opacity)
                    continue;

                const FlamePoint* plotPt = &p;
                if (useFinal) {
                    preparedFinal.nextPointTo(p, q, rng);
                    if (!std::isfinite(q.x) || !std::isfinite(q.y)) {
                        break;
                    }
                    plotPt = &q;
                }

                double px = 0.0;
                double py = 0.0;
                if (!cam.hasRotation) {
                    px = plotPt->x - cam.camX0;
                    if (px < 0.0 || px > cam.camW)
                        continue;
                    py = plotPt->y - cam.camY0;
                    if (py < 0.0 || py > cam.camH)
                        continue;
                } else {
                    px = plotPt->x * cam.cosa + plotPt->y * cam.sina + cam.rcX;
                    if (px < 0.0 || px > cam.camW)
                        continue;
                    py = plotPt->y * cam.cosa - plotPt->x * cam.sina + cam.rcY;
                    if (py < 0.0 || py > cam.camH)
                        continue;
                }

                const int64_t bx = apoRound(cam.bws * px);
                const int64_t by = apoRound(cam.bhs * py);
                if (bx < 0 || bx >= bucketWidth || by < 0 || by >= bucketHeight)
                    continue;

                const int64_t cIdx =
                    std::clamp<int64_t>(apoRound(plotPt->c * 255.0), 0, 255);
                const ScaledColor& mc = colorMap[static_cast<size_t>(cIdx)];

                Bucket64& b =
                    localBuckets[static_cast<size_t>(by) * bucketWidth +
                                 static_cast<size_t>(bx)];
                b.red += mc.r;
                b.green += mc.g;
                b.blue += mc.b;
                b.count += 1;
            }

            const int64_t done =
                completedBatches.fetch_add(1, std::memory_order_relaxed) + 1;
            if (tid == 0 && options.onProgress &&
                ((done & 0x1F) == 0 || done == numBatches)) {
                options.onProgress(static_cast<double>(done) /
                                   static_cast<double>(numBatches));
            }
        }
    };

    if (threadCount == 1) {
        workerFunc(0);
    } else {
        std::vector<std::thread> workers;
        workers.reserve(threadCount);
        for (uint32_t t = 0; t < threadCount; ++t) {
            workers.emplace_back(workerFunc, t);
        }
        for (auto& w : workers) {
            w.join();
        }
    }

    // Merge thread-local buckets into threadBuckets[0]
    auto& mergedBuckets = threadBuckets[0];
    for (uint32_t t = 1; t < threadCount; ++t) {
        const auto& src = threadBuckets[t];
        for (size_t idx = 0; idx < mergedBuckets.size(); ++idx) {
            mergedBuckets[idx].red += src[idx].red;
            mergedBuckets[idx].green += src[idx].green;
            mergedBuckets[idx].blue += src[idx].blue;
            mergedBuckets[idx].count += src[idx].count;
        }
    }

    const auto endTime = std::chrono::steady_clock::now();
    const double elapsedSeconds =
        std::chrono::duration<double>(endTime - startTime).count();

    const int64_t actualBatches = std::min(completedBatches.load(), numBatches);
    const double actualDensity = cp.quality *
                                 static_cast<double>(actualBatches) /
                                 static_cast<double>(numBatches);

    lastStats_ = FlameToneMapper::computeBucketStats(
        mergedBuckets, actualBatches * FlameFractal::kSubBatchSize,
        elapsedSeconds);

    if (options.onProgress) {
        options.onProgress(1.0);
    }

    FlameImage result = toneMapper.createImage(cp, actualDensity, mergedBuckets,
                                               bucketWidth, bucketHeight);

    if (!options.outputFilename.empty() &&
        !result.savePng(options.outputFilename, cp.transparency)) {
        const auto u8 = options.outputFilename.u8string();
        throw std::runtime_error(
            "Failed to save rendered PNG to: " +
            std::string(reinterpret_cast<const char*>(u8.data()), u8.size()));
    }

    return result;
}

}  // namespace flame
