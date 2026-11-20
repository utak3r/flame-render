#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include "core/FlameFractal.h"
#include "core/FlameGradient.h"
#include "core/FlameTransform.h"
#include "core/FlameVariation.h"
#include "core/FlameXmlSerializer.h"

using namespace flame;

TEST(FlameGradientTest, ParseHexPalette) {
    FlameGradient grad;
    std::string hex;
    for (int i = 0; i < 256; ++i) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02X%02X%02X", i, 255 - i,
                      (i * 2) % 256);
        hex += buf;
    }

    ASSERT_TRUE(grad.parseHex(hex, false));
    EXPECT_EQ(grad[0].r, 0);
    EXPECT_EQ(grad[0].g, 255);
    EXPECT_EQ(grad[0].b, 0);

    EXPECT_EQ(grad[255].r, 255);
    EXPECT_EQ(grad[255].g, 0);
    EXPECT_EQ(grad[255].b, (255 * 2) % 256);

    const std::string outHex = grad.toHex(false);
    FlameGradient grad2;
    ASSERT_TRUE(grad2.parseHex(outHex, false));
    for (int i = 0; i < 256; ++i) {
        EXPECT_EQ(grad[i].r, grad2[i].r);
        EXPECT_EQ(grad[i].g, grad2[i].g);
        EXPECT_EQ(grad[i].b, grad2[i].b);
    }
}

TEST(FlameGradientTest, HueRotationAndWhiteLevelScaling) {
    FlameGradient grad;
    for (int i = 0; i < 256; ++i) {
        grad[i] = RGBColor{255, 128, 0};
    }

    const auto scaled = grad.createScaledColorMap(200);
    // (255 * 200) / 256 = 199
    EXPECT_EQ(scaled[0].r, (255 * 200) / 256);
    EXPECT_EQ(scaled[0].g, (128 * 200) / 256);
    EXPECT_EQ(scaled[0].b, 0);

    FlameGradient rotatedFull = grad;
    rotatedFull.applyHueRotation(1.0);
    EXPECT_NEAR(rotatedFull[0].r, grad[0].r, 2);
    EXPECT_NEAR(rotatedFull[0].g, grad[0].g, 2);
    EXPECT_NEAR(rotatedFull[0].b, grad[0].b, 2);

    FlameGradient rotatedHalf = grad;
    rotatedHalf.applyHueRotation(0.5);
    EXPECT_NE(rotatedHalf[0].r, grad[0].r);
}

TEST(FlameVariationTest, RegistryCoversAll40Variations) {
    ASSERT_EQ(kNumVariations, 40u);

    for (std::size_t i = 0; i < kNumVariations; ++i) {
        const auto id = static_cast<VariationId>(i);
        const auto& info = FlameVariation::getInfo(id);
        EXPECT_EQ(info.id, id);
        const auto byName = FlameVariation::findByName(info.name);
        ASSERT_TRUE(byName.has_value())
            << "Failed to find variation by name: " << info.name;
        EXPECT_EQ(*byName, id);
    }

    // Parametric variations check
    EXPECT_EQ(FlameVariation::getInfo(VariationId::Blob).parameterNames.size(),
              3u);
    EXPECT_EQ(FlameVariation::getInfo(VariationId::Pdj).parameterNames.size(),
              4u);
    EXPECT_EQ(FlameVariation::getInfo(VariationId::Fan2).parameterNames.size(),
              2u);
    EXPECT_EQ(
        FlameVariation::getInfo(VariationId::Rings2).parameterNames.size(), 1u);
    EXPECT_EQ(
        FlameVariation::getInfo(VariationId::Perspective).parameterNames.size(),
        2u);
    EXPECT_EQ(
        FlameVariation::getInfo(VariationId::Julian).parameterNames.size(), 2u);
    EXPECT_EQ(
        FlameVariation::getInfo(VariationId::JuliaScope).parameterNames.size(),
        2u);
    EXPECT_EQ(
        FlameVariation::getInfo(VariationId::RadialBlur).parameterNames.size(),
        1u);
}

TEST(FlameTransformTest, AllVariationsProduceFiniteOutput) {
    FastRng rng(123456789ULL);
    for (std::size_t i = 0; i < kNumVariations; ++i) {
        const auto id = static_cast<VariationId>(i);
        const auto& info = FlameVariation::getInfo(id);

        FlameTransform xform;
        xform.weight = 1.0;
        xform.coefs = {{{1.0, 0.2}, {-0.1, 0.9}, {0.3, -0.4}}};
        xform.clearVariations();
        xform.setVariation(id, 1.0);

        if (id == VariationId::Blob) {
            xform.params.setParam("blob_high", 1.2);
            xform.params.setParam("blob_low", 0.4);
            xform.params.setParam("blob_waves", 5.0);
        } else if (id == VariationId::Pdj) {
            xform.params.setParam("pdj_a", 1.1);
            xform.params.setParam("pdj_b", -0.7);
            xform.params.setParam("pdj_c", 0.5);
            xform.params.setParam("pdj_d", 2.1);
        } else if (id == VariationId::Fan2) {
            xform.params.setParam("fan2_x", 0.5);
            xform.params.setParam("fan2_y", 0.8);
        } else if (id == VariationId::Rings2) {
            xform.params.setParam("rings2_val", 0.6);
        } else if (id == VariationId::Perspective) {
            xform.params.setParam("perspective_angle", 0.4);
            xform.params.setParam("perspective_dist", 2.0);
        } else if (id == VariationId::Julian) {
            xform.params.setParam("julian_power", 3.0);
            xform.params.setParam("julian_dist", 1.0);
        } else if (id == VariationId::JuliaScope) {
            xform.params.setParam("juliascope_power", -4.0);
            xform.params.setParam("juliascope_dist", 1.5);
        } else if (id == VariationId::RadialBlur) {
            xform.params.setParam("radial_blur_angle", 0.5);
        }

        PreparedTransform prep;
        prep.prepare(xform, rng);

        FlamePoint pt{0.5, -0.3, 0.25};
        prep.nextPoint(pt, rng);

        EXPECT_TRUE(std::isfinite(pt.x))
            << "Non-finite X for variation: " << info.name;
        EXPECT_TRUE(std::isfinite(pt.y))
            << "Non-finite Y for variation: " << info.name;
        EXPECT_TRUE(std::isfinite(pt.c))
            << "Non-finite C for variation: " << info.name;
    }
}

TEST(FlameTransformTest, AffineAndPostTransformOperations) {
    FlameTransform xform;
    xform.coefs = {{{1.0, 0.0}, {0.0, 1.0}, {0.0, 0.0}}};
    xform.translate(2.0, -3.0);
    EXPECT_DOUBLE_EQ(xform.coefs[2][0], 2.0);
    EXPECT_DOUBLE_EQ(xform.coefs[2][1], -3.0);

    xform.scale(2.0);
    EXPECT_DOUBLE_EQ(xform.coefs[0][0], 2.0);
    EXPECT_DOUBLE_EQ(xform.coefs[1][1], 2.0);

    xform.post = {{{0.0, 1.0}, {-1.0, 0.0}, {1.0, 2.0}}};
    EXPECT_TRUE(xform.hasPostTransform());

    FastRng rng(42);
    PreparedTransform prep;
    prep.prepare(xform, rng);

    FlamePoint pt{1.0, 0.5, 0.5};
    prep.nextPoint(pt, rng);
    // FTx = 2*1 + 2 = 4, FTy = 2*0.5 - 3 = -2
    // Linear weight = 1 -> FPx = 4, FPy = -2
    // Post: px = 0*4 + (-1)*(-2) + 1 = 3, py = 1*4 + 0*(-2) + 2 = 6
    EXPECT_DOUBLE_EQ(pt.x, 3.0);
    EXPECT_DOUBLE_EQ(pt.y, 6.0);
}

TEST(FlameXmlSerializerTest, LoadExamplesAndRoundTrip) {
    const std::filesystem::path xmlPath =
        std::filesystem::path(FLAME_EXAMPLES_DIR) / "renders.flame";
    ASSERT_TRUE(std::filesystem::exists(xmlPath))
        << "Missing: " << xmlPath.string();

    const auto fractals = FlameXmlSerializer::loadFromFile(xmlPath);
    ASSERT_EQ(fractals.size(), 13u);

    EXPECT_EQ(fractals[0].name, "VirtualAlchemy_external");
    EXPECT_EQ(fractals[0].width, 520);
    EXPECT_EQ(fractals[0].height, 390);
    EXPECT_DOUBLE_EQ(fractals[0].quality, 500.0);
    EXPECT_EQ(fractals[0].oversample, 2);
    EXPECT_DOUBLE_EQ(fractals[0].filterRadius, 0.4);
    EXPECT_GT(fractals[0].xforms.size(), 0u);

    // Check finalxform and parametric variations exist in the loaded set
    bool foundFinal = false;
    bool foundJuliaN = false;
    bool foundJuliaScope = false;
    bool foundRadialBlur = false;
    bool foundPDJ = false;
    for (const auto& f : fractals) {
        if (f.useFinalXform()) {
            foundFinal = true;
            if (f.finalXform->variationWeight(VariationId::JuliaScope) != 0.0)
                foundJuliaScope = true;
        }
        for (const auto& xf : f.xforms) {
            if (xf.variationWeight(VariationId::Julian) != 0.0)
                foundJuliaN = true;
            if (xf.variationWeight(VariationId::JuliaScope) != 0.0)
                foundJuliaScope = true;
            if (xf.variationWeight(VariationId::RadialBlur) != 0.0)
                foundRadialBlur = true;
            if (xf.variationWeight(VariationId::Pdj) != 0.0)
                foundPDJ = true;
        }
    }
    EXPECT_TRUE(foundFinal);
    EXPECT_TRUE(foundJuliaN);
    EXPECT_TRUE(foundJuliaScope);
    EXPECT_TRUE(foundRadialBlur);
    EXPECT_TRUE(foundPDJ);

    // XML round-trip serialization test
    const std::string xmlStr = FlameXmlSerializer::toXmlString(fractals);
    const auto reloaded = FlameXmlSerializer::loadFromString(xmlStr);
    ASSERT_EQ(reloaded.size(), fractals.size());

    for (std::size_t i = 0; i < fractals.size(); ++i) {
        EXPECT_EQ(reloaded[i].name, fractals[i].name);
        EXPECT_EQ(reloaded[i].width, fractals[i].width);
        EXPECT_EQ(reloaded[i].height, fractals[i].height);
        EXPECT_NEAR(reloaded[i].center[0], fractals[i].center[0], 1e-5);
        EXPECT_NEAR(reloaded[i].center[1], fractals[i].center[1], 1e-5);
        EXPECT_NEAR(reloaded[i].scale, fractals[i].scale, 1e-5);
        EXPECT_NEAR(reloaded[i].brightness, fractals[i].brightness, 1e-5);
        EXPECT_NEAR(reloaded[i].gamma, fractals[i].gamma, 1e-5);
        EXPECT_EQ(reloaded[i].xforms.size(), fractals[i].xforms.size());
        EXPECT_EQ(reloaded[i].finalXformEnabled, fractals[i].finalXformEnabled);
    }
}

TEST(FlameXmlSerializerTest, EscapesAttributesAndPreservesAllFields) {
    FlameFractal f = FlameFractal::createRandom(2, 3, 2026);
    f.name = R"(Flame "A" & <B> > 'C')";
    f.version = R"(Apo "7X" & <test>)";
    f.nick = R"(author & "friend" <1>)";
    f.url = R"(https://example.com/?a=1&b="2")";
    f.time = 12.5;
    f.hueRotation = 0.35;
    f.contrast = 1.75;

    FlameTransform finalXf;
    finalXf.color = 0.4;
    finalXf.setVariation(VariationId::Spherical, 1.0);
    f.finalXform = finalXf;
    f.finalXformEnabled = false;

    const std::string singleXml = FlameXmlSerializer::toXmlString(f);
    const auto reloadedSingle = FlameXmlSerializer::loadFromString(singleXml);
    ASSERT_EQ(reloadedSingle.size(), 1u);
    EXPECT_EQ(reloadedSingle[0].name, f.name);
    EXPECT_EQ(reloadedSingle[0].version, f.version);
    EXPECT_EQ(reloadedSingle[0].nick, f.nick);
    EXPECT_EQ(reloadedSingle[0].url, f.url);
    EXPECT_DOUBLE_EQ(reloadedSingle[0].time, f.time);
    EXPECT_DOUBLE_EQ(reloadedSingle[0].hueRotation, f.hueRotation);
    EXPECT_DOUBLE_EQ(reloadedSingle[0].contrast, f.contrast);
    EXPECT_TRUE(reloadedSingle[0].hasFinalXform());
    EXPECT_FALSE(reloadedSingle[0].finalXformEnabled);
    EXPECT_FALSE(reloadedSingle[0].useFinalXform());

    const std::string collectionXml = FlameXmlSerializer::toXmlString(
        {f}, R"(Collection "1" & <All>)");
    const auto reloadedCollection =
        FlameXmlSerializer::loadFromString(collectionXml);
    ASSERT_EQ(reloadedCollection.size(), 1u);
    EXPECT_EQ(reloadedCollection[0].name, f.name);
    EXPECT_FALSE(reloadedCollection[0].finalXformEnabled);

    // Default values should be omitted from serialized XML
    FlameFractal defaults = FlameFractal::createRandom(2, 2, 7);
    const std::string defaultXml = FlameXmlSerializer::toXmlString(defaults);
    EXPECT_EQ(defaultXml.find("nick="), std::string::npos);
    EXPECT_EQ(defaultXml.find("url="), std::string::npos);
    EXPECT_EQ(defaultXml.find("time="), std::string::npos);
    EXPECT_EQ(defaultXml.find("hue="), std::string::npos);
    EXPECT_EQ(defaultXml.find("contrast="), std::string::npos);
}

TEST(FlameFractalTest, AdjustScaleAndRandomCreation) {
    FlameFractal f = FlameFractal::createRandom(2, 5, 12345);
    EXPECT_GE(f.xforms.size(), 2u);
    f.width = 520;
    f.height = 390;

    const double origScale = f.scale;
    f.adjustScale(1040, 780);
    EXPECT_EQ(f.width, 1040);
    EXPECT_EQ(f.height, 780);
    EXPECT_NEAR(f.scale, origScale * 2.0, 1e-9);
}
