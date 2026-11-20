#include <gtest/gtest.h>
#include <cmath>
#include <filesystem>

#include "core/FlameFractal.h"
#include "core/FlameXmlSerializer.h"
#include "renderer/FlameCpuRenderer.h"
#include "renderer/FlameToneMapper.h"

using namespace flame;

TEST(FlameToneMapperTest, FilterNormalizationAndParity) {
    FlameToneMapper tm;
    // Oversample = 2, filterRadius = 0.4 -> fw = trunc(2.0 * 1.8 * 2 * 0.4) =
    // trunc(2.88) = 2 filterSize = fw + 1 = 3; parity adjusted with oversample
    // (2) -> filterSize = 4
    tm.createFilter(2, 0.4);
    EXPECT_EQ(tm.filterSize(), 4);
    ASSERT_EQ(tm.filterKernel().size(), 16u);

    double sum = 0.0;
    for (double v : tm.filterKernel()) {
        EXPECT_GT(v, 0.0);
        sum += v;
    }
    EXPECT_NEAR(sum, 1.0, 1e-12);

    // Oversample = 1, filterRadius = 0.0 -> width = 1, single 1.0 entry
    tm.createFilter(1, 0.0);
    EXPECT_EQ(tm.filterSize(), 1);
    ASSERT_EQ(tm.filterKernel().size(), 1u);
    EXPECT_DOUBLE_EQ(tm.filterKernel()[0], 1.0);
}

TEST(FlameToneMapperTest, TransparentAndOpaqueModes) {
    FlameFractal f = FlameFractal::createRandom(2, 4, 99);
    f.width = 4;
    f.height = 4;
    f.oversample = 1;
    f.filterRadius = 0.0;
    f.background = {10, 20, 30};

    FlameToneMapper tm;
    tm.createFilter(f.oversample, f.filterRadius);

    std::vector<Bucket64> buckets(16);
    // Leave bucket 0 empty, populate bucket (1, 1) -> index 5 with strong
    // red/green/count
    buckets[5] = Bucket64{50000, 25000, 0, 500};

    f.transparency = true;
    FlameImage imgTrans = tm.createImage(f, 100.0, buckets, 4, 4);
    ASSERT_EQ(imgTrans.width(), 4);
    ASSERT_EQ(imgTrans.height(), 4);
    // Empty pixel in transparent mode must have alpha = 0 and RGB = 0
    EXPECT_EQ(imgTrans.getPixel(0, 0).a, 0);
    EXPECT_EQ(imgTrans.getPixel(0, 0).r, 0);
    // Populated pixel in transparent mode must have positive alpha and red >
    // green
    EXPECT_GT(imgTrans.getPixel(1, 1).a, 0);
    EXPECT_GT(imgTrans.getPixel(1, 1).r, imgTrans.getPixel(1, 1).g);

    f.transparency = false;
    FlameImage imgOpaque = tm.createImage(f, 100.0, buckets, 4, 4);
    // Empty pixel in opaque mode must equal background color with alpha = 255
    EXPECT_EQ(imgOpaque.getPixel(0, 0).r, 10);
    EXPECT_EQ(imgOpaque.getPixel(0, 0).g, 20);
    EXPECT_EQ(imgOpaque.getPixel(0, 0).b, 30);
    EXPECT_EQ(imgOpaque.getPixel(0, 0).a, 255);
}

TEST(FlameCpuRendererTest, RenderOptionsOverridesAndDeterminism) {
    FlameFractal f = FlameFractal::createRandom(2, 4, 101);
    f.width = 520;
    f.height = 390;
    f.quality = 500.0;
    f.oversample = 2;
    f.filterRadius = 0.4;

    FlameCpuRenderer renderer;
    renderer.setFractal(&f);

    const std::filesystem::path tempOut =
        std::filesystem::temp_directory_path() / "flame_test_override.png";
    std::error_code ec;
    std::filesystem::remove(tempOut, ec);

    RenderOptions opts;
    opts.width = 160;
    opts.height = 120;
    opts.quality = 10.0;
    opts.oversample = 1;
    opts.filterRadius = 0.2;
    opts.transparent = true;
    opts.numThreads = 2;
    opts.seed = 777777ULL;
    opts.outputFilename = tempOut;

    double lastProgress = -1.0;
    opts.onProgress = [&](double p) { lastProgress = p; };

    FlameImage img1 = renderer.render(opts);
    const RenderStats stats1 = renderer.getLastStats();
    EXPECT_EQ(img1.width(), 160);
    EXPECT_EQ(img1.height(), 120);
    EXPECT_DOUBLE_EQ(lastProgress, 1.0);
    EXPECT_GT(stats1.totalSamples, 0LL);
    EXPECT_TRUE(std::filesystem::exists(tempOut));

    // Verify saved PNG dimensions match override
    FlameImage loaded = FlameImage::loadPng(tempOut);
    EXPECT_EQ(loaded.width(), 160);
    EXPECT_EQ(loaded.height(), 120);
    std::filesystem::remove(tempOut, ec);

    // Deterministic render check with identical seed and thread count
    opts.outputFilename.clear();
    opts.onProgress = nullptr;
    FlameImage img2 = renderer.render(opts);
    ASSERT_EQ(img1.rawRGBA().size(), img2.rawRGBA().size());
    EXPECT_EQ(img1.rawRGBA(), img2.rawRGBA());
}

TEST(FlameCpuRendererTest, AppliesHueRotationWithoutMutatingSourceFractal) {
    FlameFractal f = FlameFractal::createRandom(2, 4, 101);
    // Solid pure red gradient so a 1/3 hue rotation shifts it to pure green.
    for (size_t i = 0; i < FlameGradient::kPaletteSize; ++i) {
        f.gradient[i] = RGBColor{255, 0, 0};
    }

    RenderOptions opts;
    opts.width = 64;
    opts.height = 64;
    opts.quality = 10.0;
    opts.oversample = 1;
    opts.filterRadius = 0.0;
    opts.numThreads = 1;
    opts.seed = 424242ULL;

    FlameCpuRenderer renderer;

    f.hueRotation = 1.0;
    renderer.setFractal(&f);
    const FlameImage imgUnrotated = renderer.render(opts);

    f.hueRotation = 1.0 / 3.0;
    const FlameGradient originalGradient = f.gradient;
    const FlameImage imgHueRotated = renderer.render(opts);

    // Source fractal gradient must remain unchanged after rendering.
    EXPECT_EQ(f.gradient.colors(), originalGradient.colors());

    // Hue-rotated render must differ from unrotated render.
    EXPECT_NE(imgHueRotated.rawRGBA(), imgUnrotated.rawRGBA());

    // Rendering with hueRotation = 1/3 must match pre-rotating the gradient.
    FlameFractal preRotated = f;
    preRotated.hueRotation = 1.0;
    preRotated.gradient.applyHueRotation(1.0 / 3.0);
    renderer.setFractal(&preRotated);
    const FlameImage imgPreRotated = renderer.render(opts);
    EXPECT_EQ(imgHueRotated.rawRGBA(), imgPreRotated.rawRGBA());
}

TEST(FlameImageTest, SaveAndLoadPngWithUnicodePath) {
    FlameImage img(3, 2, PixelRGBA{10, 20, 30, 255});
    img.setPixel(1, 0, PixelRGBA{200, 100, 50, 128});
    img.setPixel(2, 1, PixelRGBA{0, 255, 127, 255});

    const std::filesystem::path unicodePath =
        std::filesystem::temp_directory_path() /
        std::filesystem::path(u8"flame_unicode_テスト_αβγ.png");
    std::error_code ec;
    std::filesystem::remove(unicodePath, ec);

    ASSERT_TRUE(img.savePng(unicodePath, true));
    ASSERT_TRUE(std::filesystem::exists(unicodePath));

    const FlameImage loaded = FlameImage::loadPng(unicodePath);
    EXPECT_EQ(loaded.width(), img.width());
    EXPECT_EQ(loaded.height(), img.height());
    EXPECT_EQ(loaded.rawRGBA(), img.rawRGBA());

    std::filesystem::remove(unicodePath, ec);

    const std::filesystem::path missingUnicodePath =
        std::filesystem::temp_directory_path() /
        std::filesystem::path(u8"flame_missing_テスト_αβγ.png");
    EXPECT_THROW(FlameImage::loadPng(missingUnicodePath), std::runtime_error);
}

TEST(FlameImageTest, NegativeDimensionsAndOutOfBoundsAccess) {
    FlameImage negW(-10, 5);
    EXPECT_TRUE(negW.empty());
    EXPECT_EQ(negW.width(), 0);
    EXPECT_EQ(negW.height(), 0);

    FlameImage negH(5, -10);
    EXPECT_TRUE(negH.empty());
    EXPECT_EQ(negH.width(), 0);
    EXPECT_EQ(negH.height(), 0);

    FlameImage img(2, 2, PixelRGBA{1, 2, 3, 255});
    img.setPixel(-1, 0, PixelRGBA{99, 99, 99, 255});
    img.setPixel(2, 0, PixelRGBA{99, 99, 99, 255});
    img.setPixel(0, -1, PixelRGBA{99, 99, 99, 255});
    img.setPixel(0, 2, PixelRGBA{99, 99, 99, 255});
    EXPECT_EQ(img.getPixel(-1, 0), (PixelRGBA{0, 0, 0, 0}));
    EXPECT_EQ(img.getPixel(2, 2), (PixelRGBA{0, 0, 0, 0}));
    EXPECT_EQ(img.getPixel(0, 0), (PixelRGBA{1, 2, 3, 255}));
}

TEST(FlameToneMapperTest, HandlesInvalidParametersGracefully) {
    FlameToneMapper tm;
    tm.createFilter(0, -2.5);
    EXPECT_EQ(tm.filterSize(), 1);
    ASSERT_EQ(tm.filterKernel().size(), 1u);
    EXPECT_DOUBLE_EQ(tm.filterKernel()[0], 1.0);

    FlameFractal f = FlameFractal::createRandom(2, 3, 77);
    f.width = 4;
    f.height = 4;
    f.scale = 0.0;
    f.contrast = 0.0;
    f.whiteLevel = 0;
    f.gammaThreshold = -0.5;
    f.transparency = false;

    std::vector<Bucket64> buckets(16);
    buckets[5] = Bucket64{1000, 2000, 3000, 10};
    FlameImage img = tm.createImage(f, 10.0, buckets, 4, 4);
    EXPECT_EQ(img.width(), 4);
    EXPECT_EQ(img.height(), 4);
}

TEST(FlameCpuRendererTest, ValidatesAndClampsInvalidParameters) {
    FlameFractal f = FlameFractal::createRandom(2, 3, 88);
    f.width = 32;
    f.height = 32;
    f.oversample = 0;
    f.filterRadius = -1.5;
    f.scale = 0.0;
    f.contrast = 0.0;
    f.whiteLevel = -10;
    f.gammaThreshold = -0.25;
    f.quality = -5.0;

    RenderOptions opts;
    opts.numThreads = 1;
    opts.seed = 12345ULL;

    const FlameFractal resolvedLow =
        FlameCpuRenderer::resolveEffectiveFractal(f, opts);
    EXPECT_EQ(resolvedLow.oversample, 1);
    EXPECT_GE(resolvedLow.filterRadius, 0.0);
    EXPECT_GT(resolvedLow.scale, 0.0);
    EXPECT_GT(resolvedLow.contrast, 0.0);
    EXPECT_GT(resolvedLow.whiteLevel, 0);
    EXPECT_GE(resolvedLow.gammaThreshold, 0.0);
    EXPECT_GT(resolvedLow.quality, 0.0);

    f.oversample = FlameCpuRenderer::kMaxFilterWidth + 10;
    const FlameFractal resolvedHigh =
        FlameCpuRenderer::resolveEffectiveFractal(f, opts);
    EXPECT_EQ(resolvedHigh.oversample, FlameCpuRenderer::kMaxFilterWidth);

    FlameCpuRenderer renderer;
    renderer.setFractal(&f);
    FlameImage img = renderer.render(opts);
    EXPECT_EQ(img.width(), 32);
    EXPECT_EQ(img.height(), 32);

    f.width = -4;
    f.height = 16;
    FlameImage emptyImg = renderer.render(opts);
    EXPECT_TRUE(emptyImg.empty());
}
