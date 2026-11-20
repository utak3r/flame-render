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
