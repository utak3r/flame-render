#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>

#include "core/FlameFractal.h"
#include "core/FlameXmlSerializer.h"
#include "renderer/FlameCpuRenderer.h"
#include "renderer/FlameImage.h"

using namespace flame;

namespace {

std::string trimName(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

struct ImageComparisonMetrics {
    double maePremulR = 0.0;
    double maePremulG = 0.0;
    double maePremulB = 0.0;
    double maeAlpha = 0.0;
    double maeOverall = 0.0;
    double psnrPremul = 0.0;
    double meanAlphaActual = 0.0;
    double meanAlphaRef = 0.0;
};

// Compares two RGBA images in premultiplied-alpha space as well as alpha
// channel directly. In transparent PNGs, pixels with alpha == 0 or very low
// alpha can have noisy un-premultiplied RGB values, whereas visual appearance
// is governed by premultiplied RGB (r * a / 255, g * a / 255, b * a / 255) and
// alpha.
ImageComparisonMetrics compareImages(const FlameImage& actual,
                                     const FlameImage& reference) {
    ImageComparisonMetrics m{};
    const int w = actual.width();
    const int h = actual.height();
    const std::size_t n =
        static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    if (n == 0 || w != reference.width() || h != reference.height()) {
        return m;
    }

    double sumAbsR = 0.0, sumAbsG = 0.0, sumAbsB = 0.0, sumAbsA = 0.0;
    double sumSqErr = 0.0;
    double sumAlphaAct = 0.0, sumAlphaRef = 0.0;

    const auto& rawA = actual.rawRGBA();
    const auto& rawR = reference.rawRGBA();

    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t idx = i * 4;
        const uint8_t arByte = rawA[idx + 0];
        const uint8_t agByte = rawA[idx + 1];
        const uint8_t abByte = rawA[idx + 2];
        const uint8_t aaByte = rawA[idx + 3];

        const uint8_t rrByte = rawR[idx + 0];
        const uint8_t rgByte = rawR[idx + 1];
        const uint8_t rbByte = rawR[idx + 2];
        const uint8_t raByte = rawR[idx + 3];

        const double aa = aaByte / 255.0;
        const double ar = raByte / 255.0;

        const double ra = arByte * aa;
        const double ga = agByte * aa;
        const double ba = abByte * aa;

        const double rr = rrByte * ar;
        const double gr = rgByte * ar;
        const double br = rbByte * ar;

        const double dr = std::abs(ra - rr);
        const double dg = std::abs(ga - gr);
        const double db = std::abs(ba - br);
        const double da =
            std::abs(static_cast<double>(aaByte) - static_cast<double>(raByte));

        sumAbsR += dr;
        sumAbsG += dg;
        sumAbsB += db;
        sumAbsA += da;

        sumSqErr += (dr * dr + dg * dg + db * db + da * da) * 0.25;
        sumAlphaAct += aaByte;
        sumAlphaRef += raByte;
    }

    const double invN = 1.0 / static_cast<double>(n);
    m.maePremulR = sumAbsR * invN;
    m.maePremulG = sumAbsG * invN;
    m.maePremulB = sumAbsB * invN;
    m.maeAlpha = sumAbsA * invN;
    m.maeOverall =
        (m.maePremulR + m.maePremulG + m.maePremulB + m.maeAlpha) * 0.25;

    const double mse = sumSqErr * invN;
    m.psnrPremul =
        (mse > 1e-12) ? (10.0 * std::log10((255.0 * 255.0) / mse)) : 99.0;
    m.meanAlphaActual = sumAlphaAct * invN;
    m.meanAlphaRef = sumAlphaRef * invN;

    return m;
}

// Compute 4x4 block-downsampled PSNR to measure structural/perceptual agreement
// independent of per-pixel Monte Carlo grain
double computeBlockPsnr(const FlameImage& actual,
                        const FlameImage& reference,
                        int blockSize = 4) {
    const int bw = actual.width() / blockSize;
    const int bh = actual.height() / blockSize;
    double sumSq = 0.0;
    int count = 0;

    for (int by = 0; by < bh; ++by) {
        for (int bx = 0; bx < bw; ++bx) {
            double ra = 0, ga = 0, ba = 0, aa = 0;
            double rr = 0, gr = 0, br = 0, ar = 0;
            for (int dy = 0; dy < blockSize; ++dy) {
                for (int dx = 0; dx < blockSize; ++dx) {
                    const PixelRGBA pa = actual.getPixel(bx * blockSize + dx,
                                                         by * blockSize + dy);
                    const PixelRGBA pr = reference.getPixel(
                        bx * blockSize + dx, by * blockSize + dy);
                    const double fa = pa.a / 255.0;
                    const double fr = pr.a / 255.0;
                    ra += pa.r * fa;
                    ga += pa.g * fa;
                    ba += pa.b * fa;
                    aa += pa.a;
                    rr += pr.r * fr;
                    gr += pr.g * fr;
                    br += pr.b * fr;
                    ar += pr.a;
                }
            }
            const double inv = 1.0 / (blockSize * blockSize);
            ra *= inv;
            ga *= inv;
            ba *= inv;
            aa *= inv;
            rr *= inv;
            gr *= inv;
            br *= inv;
            ar *= inv;

            const double dr = ra - rr;
            const double dg = ga - gr;
            const double db = ba - br;
            const double da = aa - ar;
            sumSq += (dr * dr + dg * dg + db * db + da * da) * 0.25;
            ++count;
        }
    }
    const double mse = sumSq / std::max(1, count);
    return (mse > 1e-12) ? (10.0 * std::log10((255.0 * 255.0) / mse)) : 99.0;
}

}  // namespace

TEST(GoldenSamplesTest, All13ExamplesMatchReferencePngs) {
    const std::filesystem::path examplesDir(FLAME_EXAMPLES_DIR);
    const std::filesystem::path flameFile = examplesDir / "renders.flame";
    ASSERT_TRUE(std::filesystem::exists(flameFile))
        << "Missing: " << flameFile.string();

    const auto fractals = FlameXmlSerializer::loadFromFile(flameFile);
    ASSERT_EQ(fractals.size(), 13u);

    FlameCpuRenderer renderer;

    for (std::size_t i = 0; i < fractals.size(); ++i) {
        const auto& fractal = fractals[i];
        const std::string trimmed = trimName(fractal.name);
        const std::filesystem::path refPath = examplesDir / (trimmed + ".png");
        ASSERT_TRUE(std::filesystem::exists(refPath))
            << "Missing reference PNG: " << refPath.string();

        const FlameImage refImg = FlameImage::loadPng(refPath);
        ASSERT_EQ(refImg.width(), fractal.width);
        ASSERT_EQ(refImg.height(), fractal.height);

        renderer.setFractal(&fractal);
        RenderOptions opts;
        opts.seed = 20260417ULL + i;
        opts.transparent = true;

        const FlameImage actualImg = renderer.render(opts);
        ASSERT_EQ(actualImg.width(), refImg.width());
        ASSERT_EQ(actualImg.height(), refImg.height());

        const ImageComparisonMetrics metrics = compareImages(actualImg, refImg);
        const double blockPsnr = computeBlockPsnr(actualImg, refImg, 4);

        std::cout << "[Golden " << std::setw(2) << (i + 1) << "/13] \""
                  << trimmed << "\""
                  << " | MAE(overall)=" << std::fixed << std::setprecision(2)
                  << metrics.maeOverall << " | MAE(alpha)=" << metrics.maeAlpha
                  << " | PSNR(pixel)=" << metrics.psnrPremul << " dB"
                  << " | PSNR(4x4)=" << blockPsnr << " dB"
                  << " | MeanAlpha(act/ref)=" << metrics.meanAlphaActual << "/"
                  << metrics.meanAlphaRef << "\n";

        EXPECT_LT(metrics.maeOverall, 12.0)
            << "Fractal \"" << trimmed << "\" exceeded overall MAE threshold";
        EXPECT_GT(blockPsnr, 24.0)
            << "Fractal \"" << trimmed << "\" 4x4 block PSNR too low";
        EXPECT_NEAR(metrics.meanAlphaActual, metrics.meanAlphaRef, 12.0)
            << "Fractal \"" << trimmed
            << "\" mean alpha diverged from reference";
    }
}
